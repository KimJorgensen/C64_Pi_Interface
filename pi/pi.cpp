//
// Raspberry Pi specific code
// Based on
// https://github.com/fenlogic/IDE_trial
// https://github.com/Wallacoloo/Raspberry-Pi-DMA-Example
// https://github.com/juj/fbcp-ili9341
//

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PAGE_SIZE   (4*1024)
#define BLOCK_SIZE  (4*1024)

static void cleanup_smi();

// Highjack DMA channel 1 (should be free)
static const uint8_t dma_ch = 1;

static int mem_fd = -1;
static int vcio_fd = -1;

// I/O access
static volatile uint32_t *dma = 0;
static volatile uint32_t *clk = 0;
static volatile uint32_t *gpio = 0;
static volatile uint32_t *smi = 0;

// DMA control block needs to be on 32 byte boundary
typedef struct
{
	uint32_t info;
	uint32_t src;
	uint32_t dst;
	uint32_t length;
	uint32_t stride;
	uint32_t next;
	uint32_t pad[2]; // to fill alignment
} dma_cb_type;

typedef struct
{
  uint32_t handle;
  volatile uint8_t *virt_addr;
  uintptr_t bus_addr;
  size_t bytes;
} gpu_memory;

static gpu_memory dma_buffer = {};

static const uint32_t chunck_size = 32760; // 130 raster lines (PAL)
static uint16_t *chunk_virt_addr[4];
static uint32_t dma_cb_bus_addr[5];

static uint32_t cb_index, current_cb;

//
// Map the physical address of a peripheral into virtual address space.
//
static volatile uint32_t *map_to_virt(size_t length, off_t addr)
{
    void *mapped = mmap(
      NULL,
      length,
      PROT_READ | PROT_WRITE,
      MAP_SHARED,
      mem_fd,
      addr);

    if (mapped == MAP_FAILED)
    {
      fprintf(stderr, "Could not mmap address %p\n", (void *)addr);
      exit(-1);
    }

    return (volatile uint32_t *)mapped;
}

static uint32_t send_mailbox(uint32_t tag, uint32_t num_args, ...)
{
  uint32_t mailbox[9], i;
  va_list args;
  int ret;

  if (num_args > 3)
  {
    fprintf(stderr, "Too many args for send_mailbox %u\n", num_args);
    exit(1);
  }

  mailbox[MAILBOX_BUFFER_SIZE] = sizeof(mailbox);
  mailbox[MAILBOX_RE_CODE] = 0;
  mailbox[MAILBOX_TAG_ID] = tag;
  mailbox[MAILBOX_TAG_BUF_SIZE] = num_args*sizeof(uint32_t);

  va_start(args, num_args);
  for (i=0; i<num_args; i++)
  {
    uint32_t param = va_arg(args, uint32_t);
    mailbox[MAILBOX_TAG_BUFFER + i] = param;
  }

  va_end(args);
  mailbox[MAILBOX_TAG_BUFFER + num_args] = 0;  // end tag

  ret = ioctl(vcio_fd, _IOWR(/*MAJOR_NUM=*/100, 0, char *), mailbox);
  if (ret < 0)
  {
    fprintf(stderr, "send_mailbox failed in ioctl\n");
    exit(1);
  }

  if (mailbox[MAILBOX_RE_CODE] != BUF_REQ_SUCCESSFUL)
  {
    fprintf(stderr, "send_mailbox failed with response code %x\n",
      mailbox[MAILBOX_RE_CODE]);
    exit(1);
  }

  if ((mailbox[MAILBOX_TAG_RE_CODE] & TAG_RE_BIT_MASK) == 0)
  {
    fprintf(stderr, "send_mailbox failed. Tag response bit not set\n");
    exit(1);
  }

  if ((mailbox[MAILBOX_TAG_RE_CODE] & ~TAG_RE_BIT_MASK) < 4)
  {
    fprintf(stderr, "send_mailbox failed. Unsupported response length %u\n",
      mailbox[MAILBOX_TAG_RE_CODE] & ~TAG_RE_BIT_MASK);
    exit(1);
  }

  return mailbox[MAILBOX_TAG_BUFFER];
}

static void free_gpu_mem(gpu_memory *memory)
{
  uint32_t status;

  if (memory)
  {
    if (memory->virt_addr)
    {
      munmap((void *)memory->virt_addr, memory->bytes);
      memory->virt_addr = 0;
    }

    if (memory->bus_addr)
    {
      status = send_mailbox(TAG_UNLOCK_MEMORY, 1, memory->handle);
      if (!status)
      {
        memory->bus_addr = 0;
      }
      else
      {
        fprintf(stderr, "Failed to unlock GPU memory\n");
      }
    }

    if (memory->handle)
    {
      status = send_mailbox(TAG_RELEASE_MEMORY, 1, memory->handle);
      if (!status)
      {
        memory->handle = 0;
      }
      else
      {
        fprintf(stderr, "Failed to release GPU memory\n");
      }
    }
  }
}

// Allocates the given number of bytes in GPU side memory, and returns the
// virtual address and physical bus address of the allocated memory block.
// The virtual address holds an uncached view to the allocated memory, so
// writes and reads to that memory address bypass the L1 and L2 caches. Use
// this kind of memory to pass data blocks over to the DMA controller to process.
static gpu_memory alloc_gpu_mem(size_t bytes)
{
  gpu_memory result = {};

  result.handle = send_mailbox(TAG_ALLOCATE_MEMORY, 3,
    bytes, PAGE_SIZE, MEM_FLAG_L1_NONALLOCATING | MEM_FLAG_ZERO);
  if (result.handle)
  {
    result.bus_addr = send_mailbox(TAG_LOCK_MEMORY, 1, result.handle);
    if (result.bus_addr)
    {
      result.virt_addr = (volatile uint8_t *)map_to_virt(bytes,
        BUS_TO_PHYS(result.bus_addr));

      result.bytes = bytes;
    }
    else
    {
      fprintf(stderr, "Failed to lock GPU memory\n");
      free_gpu_mem(&result);
    }
  }
  else
  {
    fprintf(stderr, "Failed to allocate GPU memory\n");
  }

  return result;
}

//
// Set up memory regions to access the peripherals.
//
static bool setup_io()
{
  if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0)
  {
    fprintf(stderr, "Can't open /dev/mem\n");
    fprintf(stderr, "Did you forget to run as root?\n");
    return false;
  }

  if ((vcio_fd = open("/dev/vcio", O_RDWR|O_SYNC)) < 0)
  {
    fprintf(stderr, "Can't open /dev/vcio\n");
    return false;
  }

  dma = map_to_virt(BLOCK_SIZE, DMA_BASE);
  clk = map_to_virt(BLOCK_SIZE, CLOCK_BASE);
  gpio = map_to_virt(BLOCK_SIZE, GPIO_BASE);
  smi = map_to_virt(BLOCK_SIZE, SMI_BASE);

  dma_buffer = alloc_gpu_mem(5*sizeof(dma_cb_type) + 4*chunck_size);
  if (dma_buffer.virt_addr == 0)
  {
    fprintf(stderr, "Alloc of DMA buffer failed\n");
    return false;
  }

  return true;
}

//
// Undo what we did above.
//
static void restore_io()
{
  if (smi)
  {
    munmap((void *)dma, BLOCK_SIZE);
    munmap((void *)clk, BLOCK_SIZE);
    munmap((void *)gpio, BLOCK_SIZE);
    munmap((void *)smi, BLOCK_SIZE);
  }

  free_gpu_mem(&dma_buffer);
  close(mem_fd);
  close(vcio_fd);
}

//
// Set up basic SMI mode
//
static void setup_smi()
{
  // Set GPIO 0..25 to SMI mode
  for (int i=0; i<26; i++)
  {
    INP_GPIO(i);
    SET_GPIO_ALT(i, 1);
  }

  // Disable SMI DMA
  smi[SMI_DMAC_REG] = 0;

  // Clear any errors and disable
  smi[SMI_CS_REG] = SMI_CS_AFERR|SMI_CS_SETERR|SMI_CS_FFCLR|SMI_CS_DONE;
  smi[SMI_DIRCS_REG] = SMI_DIRCS_DONE;

  // Set SMI clock
  // Use PLL-D as that is not changing (500MHz)
  volatile uint32_t *smi_clk  = clk + (0xB0>>2);
  *smi_clk = 0x5A000000;     // Off
  *(smi_clk+1) = 0x5A002000; // div 2 = 250MHz
  *smi_clk     = 0x5A000016; // PLLD & enabled

  // Initially set all timing registers to 16 bit intel mode
  // assuming a 250MHz SMI clock
  volatile uint32_t *read_reg = smi + SMI_READ0_REG;
  for (int i=0; i<8; i++)
  {
    read_reg[i] =
        SMI_RW_WID16   | // 16 bits
        SMI_RW_MODE80  | // intel mode
        SMI_RW_PACEALL | // always pace
        SMI_RW_DREQ    | // Use external DMA req on SD16/SD17 to pace reads/writes
        (8<<SMI_RW_SETUP_LS)   |  //  32 ns
        (15<<SMI_RW_STROBE_LS) |  //  60 ns
        (8<<SMI_RW_HOLD_LS)    |  //  32 ns
        (8<<SMI_RW_PACE_LS);      //  32 ns
  }

  smi[SMI_DMAC_REG] =
    SMI_DMAC_ENABLE  | // enable DMA
    SMI_DMAC_DMAP    | // use SMI D16/D17 as DREQ/DACK pins
    (0x20<<SMI_DMAC_RXPANIC_LS)  |
    (0x20<<SMI_DMAC_TXPANIC_LS)  |
    (0x01<<SMI_DMAC_RXTHRES_LS)  |
    (0x3F<<SMI_DMAC_TXTHRES_LS);
}

static void setup_dma()
{
    // Reset DMA channel
    dma[DMA_CS_REG(dma_ch)] = DMA_CS_RESET;
    while (dma[DMA_CS_REG(dma_ch)] & DMA_CS_ACTIVE);

    // Clear debug error flags
    dma[DMA_DEBUG_REG(dma_ch)] =
      DMA_DEBUG_READ_ERROR |
      DMA_DEBUG_FIFO_ERROR |
      DMA_DEBUG_SET_ERROR;

    // Setup DMA channel
    dma[DMA_CS_REG(dma_ch)] =
      DMA_CS_DISDEBUG |
      (0x0f<<DMA_CS_PANIC_PRI_LS) |
      (0x08<<DMA_CS_PRIORITY_LS)  |
      DMA_CS_INT |
      DMA_CS_END;

    dma[DMA_ENABLE_REG] |= (1<<dma_ch);  // enable DMA channel
}

static void setup_smi_read(uint32_t words)
{
  const uint32_t timing = 0;
  const uint32_t address = 0;

  // clear done bit, enable for reads, clear FIFO
  smi[SMI_CS_REG] = SMI_CS_DONE|SMI_CS_ENABLE|SMI_CS_FFCLR;

  // set address & length
  smi[SMI_ADRS_REG]   = ((timing<<SMI_ADRS_DEV_LS)&SMI_ADRS_DEV_MSK) |
                         (address&SMI_ADRS_MSK);
  smi[SMI_LENGTH_REG] = words;
}

static bool start_smi_dma()
{
    dma_cb_type *rx1_from_smi, *rx2_from_smi, *rx3_from_smi;
    dma_cb_type *rx4_from_smi, *smi_rx_start;

    if(!setup_io())
    {
        return false;
    }

    // Use the first part of the buffer for the DMA control blocks
    rx1_from_smi = (dma_cb_type *)dma_buffer.virt_addr;
    rx2_from_smi = (dma_cb_type *)(dma_buffer.virt_addr + sizeof(dma_cb_type));
    rx3_from_smi = (dma_cb_type *)(dma_buffer.virt_addr + 2*sizeof(dma_cb_type));
    rx4_from_smi = (dma_cb_type *)(dma_buffer.virt_addr + 3*sizeof(dma_cb_type));
    smi_rx_start = (dma_cb_type *)(dma_buffer.virt_addr + 4*sizeof(dma_cb_type));

    dma_cb_bus_addr[0] = dma_buffer.bus_addr;
    dma_cb_bus_addr[1] = dma_buffer.bus_addr + sizeof(dma_cb_type);
    dma_cb_bus_addr[2] = dma_buffer.bus_addr + 2*sizeof(dma_cb_type);
    dma_cb_bus_addr[3] = dma_buffer.bus_addr + 3*sizeof(dma_cb_type);
    dma_cb_bus_addr[4] = dma_buffer.bus_addr + 4*sizeof(dma_cb_type);

    // Use the last part of the buffer for the chuncks
    chunk_virt_addr[0] = (uint16_t *)(dma_buffer.virt_addr + 5*sizeof(dma_cb_type));
    chunk_virt_addr[1] = chunk_virt_addr[0] + chunck_size/sizeof(uint16_t);
    chunk_virt_addr[2] = chunk_virt_addr[1] + chunck_size/sizeof(uint16_t);
    chunk_virt_addr[3] = chunk_virt_addr[2] + chunck_size/sizeof(uint16_t);

    uint32_t chunk_bus_addr1 = dma_buffer.bus_addr + 5*sizeof(dma_cb_type);
    uint32_t chunk_bus_addr2 = chunk_bus_addr1 + chunck_size;
    uint32_t chunk_bus_addr3 = chunk_bus_addr2 + chunck_size;
    uint32_t chunk_bus_addr4 = chunk_bus_addr3 + chunck_size;

    setup_smi();
    setup_dma();

    // DMA control block 0 - read chunk 1
    rx1_from_smi->info =
      (4<<DMA_TI_PERMAP_LS) | // Peripheral 4 = SMI
      DMA_TI_SRC_DREQ       |
      DMA_TI_DEST_WIDTH     |
      DMA_TI_DEST_INC;
    rx1_from_smi->src = SMI_BASE_BUS + SMI_DATA_REG*sizeof(uint32_t);
    rx1_from_smi->dst = chunk_bus_addr1;
    rx1_from_smi->length = chunck_size;
    rx1_from_smi->next = dma_cb_bus_addr[1];

    // DMA control block 1 - read chunk 2
    rx2_from_smi->info =
      (4<<DMA_TI_PERMAP_LS) | // Peripheral 4 = SMI
      DMA_TI_SRC_DREQ       |
      DMA_TI_DEST_WIDTH     |
      DMA_TI_DEST_INC;
    rx2_from_smi->src = SMI_BASE_BUS + SMI_DATA_REG*sizeof(uint32_t);
    rx2_from_smi->dst = chunk_bus_addr2;
    rx2_from_smi->length = chunck_size;
    rx2_from_smi->next = dma_cb_bus_addr[2];

    // DMA control block 2 - read chunk 3
    rx3_from_smi->info =
      (4<<DMA_TI_PERMAP_LS) | // Peripheral 4 = SMI
      DMA_TI_SRC_DREQ       |
      DMA_TI_DEST_WIDTH     |
      DMA_TI_DEST_INC;
    rx3_from_smi->src = SMI_BASE_BUS + SMI_DATA_REG*sizeof(uint32_t);
    rx3_from_smi->dst = chunk_bus_addr3;
    rx3_from_smi->length = chunck_size;
    rx3_from_smi->next = dma_cb_bus_addr[3];

    // DMA control block 3 - read chunk 4
    rx4_from_smi->info =
      (4<<DMA_TI_PERMAP_LS) | // Peripheral 4 = SMI
      DMA_TI_SRC_DREQ       |
      DMA_TI_DEST_WIDTH     |
      DMA_TI_DEST_INC;
    rx4_from_smi->src = SMI_BASE_BUS + SMI_DATA_REG*sizeof(uint32_t);
    rx4_from_smi->dst = chunk_bus_addr4;
    rx4_from_smi->length = chunck_size;
    rx4_from_smi->next = dma_cb_bus_addr[4];

    // DMA control block 4 - start SMI read
    smi_rx_start->info = 0;
    smi_rx_start->src = dma_cb_bus_addr[4] + 6*sizeof(uint32_t); // Point to pad[0]
    smi_rx_start->dst = SMI_BASE_BUS + SMI_CS_REG*sizeof(uint32_t);
    smi_rx_start->length = sizeof(uint32_t);
    smi_rx_start->pad[0] = SMI_CS_PXLDAT|SMI_CS_START|SMI_CS_ENABLE;
    smi_rx_start->next = dma_cb_bus_addr[0];

    setup_smi_read(0xFFFFFFFF);
    //setup_smi_read(256);

    current_cb = dma_cb_bus_addr[0];
    cb_index = 3;

    // DMA setup
    dma[DMA_CONBLK_AD_REG(dma_ch)] = dma_cb_bus_addr[4];

    // Start DMA
    dma[DMA_CS_REG(dma_ch)] |= DMA_CS_ACTIVE;
    while ((dma[DMA_CS_REG(dma_ch)] & DMA_CS_ACTIVE) == 0);

    return true;
}

static uint16_t *get_next_smi_chunk()
{
    cb_index++;
    if (cb_index > 3)
    {
        cb_index = 0;
    }

    if (dma_cb_bus_addr[cb_index] != current_cb)
    {
        return chunk_virt_addr[cb_index];
    }

    // TODO: Is it possible to use interrupts in userspace when DMA CB is done?
    while (dma[DMA_CS_REG(dma_ch)] & DMA_CS_ACTIVE)
    {
        uint32_t new_cb = dma[DMA_CONBLK_AD_REG(dma_ch)];
        if (new_cb != current_cb && new_cb != dma_cb_bus_addr[4])
        {
            current_cb = new_cb;
            return chunk_virt_addr[cb_index];
        }

        //usleep(1);
    }

    printf("DMA done\n");
    cleanup_smi();
    exit(1);
}

static void cleanup_smi()
{
  // Disable DMA. Otherwise, it will continue to run in the background,
  // potentially overwriting future user data.
  if (dma)
  {
    dma[DMA_CS_REG(dma_ch)] = DMA_CS_RESET;
    while (dma[DMA_CS_REG(dma_ch)] & DMA_CS_ACTIVE);
  }

  if (smi)
  {
    setup_smi();
  }

  restore_io();
}

static void cleanup_smi_and_exit(int sig)
{
  printf("\nExiting with error; caught signal: %i\n", sig);
  cleanup_smi();
  exit(1);
}

