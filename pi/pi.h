//
// Raspberry Pi specific constants
// Based on
// https://github.com/fenlogic/IDE_trial
// https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
// https://github.com/raspberrypi/documentation/files/1888662/BCM2837-ARM-Peripherals.-.Revised.-.V2-1.pdf
//

#if 0
  #define BCM2708_PERI_BASE     0x20000000 // For Pi1/Zero
#else
  #define BCM2708_PERI_BASE     0x3F000000 // For Pi2/3
#endif

#define DMA_BASE                (BCM2708_PERI_BASE + 0x007000)
#define CLOCK_BASE              (BCM2708_PERI_BASE + 0x101000)
#define GPIO_BASE               (BCM2708_PERI_BASE + 0x200000)
#define SMI_BASE                (BCM2708_PERI_BASE + 0x600000)

#define SMI_BASE_BUS            0x7E600000

//
//  SMI
//

// Registers offset in CPU map 
// Assuming we access the register through a 4K MMU mapped address range
// As such the register addresses are the offsets in that range
//======
#define SMI_CS_REG              0           //** Control & Status register 
#define SMI_CS_RXFF_FULL        0x80000000  // RX FIFO full
#define SMI_CS_TXFF_EMPTY       0x40000000  // TX FIFO not empty
#define SMI_CS_RXFF_DATA        0x20000000  // RX FIFO holds data
#define SMI_CS_TXFF_SPACE       0x10000000  // TX FIFO can accept data
#define SMI_CS_RXFF_HIGH        0x08000000  // RX FIFO >= 3/4 full
#define SMI_CS_TXFF_LOW         0x04000000  // TX FIFO <  1/4 full
#define SMI_CS_AFERR            0x02000000  // AXI FIFO unsderflow/overflow
                                            // Write with bit set to clear
#define SMI_CS_EDREQ            0x00008000  // External DREQ received
#define SMI_CS_PXLDAT           0x00004000  // Pixel mode on
#define SMI_CS_SETERR           0x00002000  // Setup reg. written & used error
#define SMI_CS_PVMODE           0x00001000  // Pixel Valve mode on
#define SMI_CS_RXIRQ            0x00000800  // Receive interrupt
#define SMI_CS_TXIRQ            0x00000400  // Transmit interrupt
#define SMI_CS_DONIRQ           0x00000200  // Done interrupt
#define SMI_CS_TEEN             0x00000100  // Tear enable
#define SMI_CS_BYTPAD           0x000000C0  // Number of PAD byte
#define SMI_CS_WRITE            0x00000020  // 1= Write to ext. devices
                                            // 0= Read from ext. devices
#define SMI_CS_FFCLR            0x00000010  // Write to 1 to clear FIFO
#define SMI_CS_START            0x00000008  // Write to 1 to start transfer(s)
#define SMI_CS_BUSY             0x00000004  // Set if transfer is taking place
#define SMI_CS_DONE             0x00000002  // Set if transfer has finished
                                            // For reads only set after FF emptied
                                            // Write with bit set to clear
#define SMI_CS_ENABLE           0x00000001  // Set to enable SMI 
                                         
#define SMI_LENGTH_REG          1           //** transfer length register 
                                        
#define SMI_ADRS_REG            2           //** transfer address register
#define SMI_ADRS_DEV_MSK        0x00000300  // Which device timing to use 
#define SMI_ADRS_DEV_LS         8
#define SMI_ADRS_DEV0           0x00000000  // Use read0/write0 timing  
#define SMI_ADRS_DEV1           0x00000100  // Use read1/write1 timing 
#define SMI_ADRS_DEV2           0x00000200  // Use read2/write2 timing 
#define SMI_ADRS_DEV3           0x00000300  // Use read3/write3 timing 
#define SMI_ADRS_MSK            0x0000003F  // Address bits 0-5 to use 
#define SMI_ADRS_LS             0           // Address LS bit

#define SMI_DATA_REG            3           //** transfer data register
                                        
#define SMI_READ0_REG           4           //** Read  settings 0 reg  
#define SMI_WRITE0_REG          5           //** Write settings 0 reg  
#define SMI_READ1_REG           6           //** Read  settings 1 reg  
#define SMI_WRITE1_REG          7           //** Write settings 1 reg  
#define SMI_READ2_REG           8           //** Read  settings 2 reg  
#define SMI_WRITE2_REG          9           //** Write settings 2 reg  
#define SMI_READ3_REG           10          //** Read  settings 3 reg  
#define SMI_WRITE3_REG          11          //** Write settings 3 reg  

// SMI read and write register fields applicable
// to the 8 register listed above 
// Beware two fields are different between read and write 
#define SMI_RW_WIDTH_MSK        0xC0000000  // Data width mask 
#define SMI_RW_WID8             0x00000000  // Data width 8 bits
#define SMI_RW_WID16            0x40000000  // Data width 16 bits
#define SMI_RW_WID18            0x80000000  // Data width 18 bits
#define SMI_RW_WID9             0xC0000000  // Data width 9 bits
#define SMI_RW_SETUP_MSK        0x3F000000  // Setup cycles (6 bits)
#define SMI_RW_SETUP_LS         24          // Setup cycles LS bit (shift) 
#define SMI_RW_MODE68           0x00800000  // Run cycle motorola mode 
#define SMI_RW_MODE80           0x00000000  // Run cycle intel mode 
#define SMI_READ_FSETUP         0x00400000  //++ READ! Setup only for first cycle
#define SMI_WRITE_SWAP          0x00400000  //++ Write! swap pixel data
#define SMI_RW_HOLD_MSK         0x003F0000  // Hold cycles (6 bits)
#define SMI_RW_HOLD_LS          16          // Hold cycles LS bit (shift) 
#define SMI_RW_PACEALL          0x00008000  // Apply pacing always 
          // Clear: no pacing if switching to a different read/write settings
#define SMI_RW_PACE_MSK         0x00007F00  // Pace cycles (7 bits)
#define SMI_RW_PACE_LS          8           // Pace cycles LS bit (shift) 
#define SMI_RW_DREQ             0x00000080  // Use DMA req on read/write
                                            // Use with SMI_DMAC_DMAP
#define SMI_RW_STROBE_MSK       0x0000007F  // Strobe cycles (7 bits)
#define SMI_RW_STROBE_LS        0           // Strobe cycles LS bit (shift) 

// DMA control register 
#define SMI_DMAC_REG            12          //** DMA control register
#define SMI_DMAC_ENABLE         0x10000000  // DMA enable 
#define SMI_DMAC_DMAP           0x01000000  // D16/17 are resp DMA_REQ/DMA_ACK
#define SMI_DMAC_RXPANIC_MSK    0x00FC0000  // Read Panic threshold (6 bits)
#define SMI_DMAC_RXPANIC_LS     18          // Read Panic threshold LS bit (shift)
#define SMI_DMAC_TXPANIC_MSK    0x0003F000  // Write Panic threshold (6 bits)
#define SMI_DMAC_TXPANIC_LS     12          // Write Panic threshold LS bit (shift)
#define SMI_DMAC_RXTHRES_MSK    0x00000FC0  // Read DMA threshold (6 bits)
#define SMI_DMAC_RXTHRES_LS     6           // Read DMA threshold LS bit (shift)
#define SMI_DMAC_TXTHRES_MSK    0x0000003F  // Write DMA threshold (6 bits)
#define SMI_DMAC_TXTHRES_LS     0           // Read DMA threshold LS bit (shift)
//
// Direct access registers
//
#define SMI_DIRCS_REG            13          //** Direct control register
// The following fields are identical in the SMI_DC register
#define SMI_DIRCS_WRITE         0x00000008  // 1= Write to ext. devices
#define SMI_DIRCS_DONE          0x00000004  // Set if transfer has finished
                                            // Write with bit set to clear
#define SMI_DIRCS_START         0x00000002  // Write to 1 to start transfer(s)
// Found to make no difference!!
#define SMI_DIRCS_ENABLE        0x00000001  // Set to enable SMI 
                                            // 0= Read from ext. devices 

#define SMI_DIRADRS_REG         14          //** transfer address register
#define SMI_DIRADRS_DEV_MSK     0x00000300  // Which device timing to use 
#define SMI_DIRADRS_DEV_LS      8
#define SMI_DIRADRS_DEV0        0x00000000  // Use read0/write0 timing
#define SMI_DIRADRS_DEV1        0x00000100  // Use read1/write1 timing
#define SMI_DIRADRS_DEV2        0x00000200  // Use read2/write2 timing
#define SMI_DIRADRS_DEV3        0x00000300  // Use read3/write3 timing
#define SMI_DIRADRS_MSK         0x0000003F  // Address bits 0-5 to use
#define SMI_DIRADRS_LS          0           // Address LS bit

#define SMI_DIRDATA_REG         15          //** Direct access data register

// FIDO debug register
// There is terse and ambigous documentation
// e.g. How/when is high level reset
#define SMI_FIFODBG_REG         16          //** FIFO debug register
#define SMI_FIFODBG_HIGH_MSK    0x00003F00  // High level during last transfers
#define SMI_FIFODBG_HIGH_LS     8           // High level LS bit
#define SMI_FIFODBG_LEVL_MSK    0x0000003F  // Current level
#define SMI_FIFODBG_LEVL_LS     0           // Current level LS bit (shift)

//
//  DMA
//
#define DMA_CS_REG(ch)        (0x40*(ch)+0)   //** Control and Status register
#define DMA_CS_RESET          0x80000000      // Set to reset DMA
#define DMA_CS_ABORT          0x40000000      // Set to abort current control block
#define DMA_CS_DISDEBUG       0x20000000      // If set DMA won't be paused when debug signal is sent
#define DMA_CS_WAIT_WRITES    0x10000000      // If set DMA will wait for outstanding writes
#define DMA_CS_PANIC_PRI_MSK  0x00F00000      // AXI Panic Priority level (4 bits). 0 is lowest
#define DMA_CS_PANIC_PRI_LS   20              // AXI Panic Priority level LS bit (shift)
#define DMA_CS_PRIORITY_MSK   0x000F0000      // AXI normal Priority level (4 bits). 0 is lowest
#define DMA_CS_PRIORITY_LS    16              // AXI normal Priority level LS bit (shift)
#define DMA_CS_ERROR          0x00000100      // Set when error is encountered. Read only
#define DMA_CS_WAITING_WRITES 0x00000040      // Set when waiting for outstanding writes. Read only
#define DMA_CS_DREQ_STOPS_DMA 0x00000020      // Set when waiting for DREQ. Read only
#define DMA_CS_PAUSED         0x00000010      // Set when DMA is paused. Read only
#define DMA_CS_DREQ           0x00000008      // State of the selected DREQ signal. Read only
#define DMA_CS_INT            0x00000004      // Set when current CB ends and its INTEN=1
#define DMA_CS_END            0x00000002      // Set when transfer defined by current CB is complete
#define DMA_CS_ACTIVE         0x00000001      // Set to activate DMA (load CB before hand)

#define DMA_CONBLK_AD_REG(ch) (0x40*(ch)+1)   //** Control Block Address register

#define DMA_TI_REG(ch)        (0x40*(ch)+2)   //** Transfer Infomation register
#define DMA_TI_NO_WIDE_BURSTS 0x04000000      // Prevents the DMA from issuing wide writes
#define DMA_TI_WAITS_MSK      0x03E00000      // Add Wait Cycles (5 bits)
#define DMA_TI_WAITS_LS       21              // Add Wait Cycles LS bit (shift)
#define DMA_TI_PERMAP_MSK     0x001F0000      // Peripheral Mapping (5 bits)
#define DMA_TI_PERMAP_LS      16              // Peripheral Mapping LS bit (shift)
#define DMA_TI_BURST_LEN_MSK  0x0000F000      // Peripheral Mapping (4 bits)
#define DMA_TI_BURST_LEN_LS   12              // Peripheral Mapping LS bit (shift)
#define DMA_TI_SRC_IGNORE     0x00000800      // Prevents the DMA from issuing wide writes
#define DMA_TI_SRC_DREQ       0x00000400      // DREQ selected by PERMAP will gate the reads
#define DMA_TI_SRC_WIDTH      0x00000200      // 1 = Use 128-bit source read width. 0 = 32-bit
#define DMA_TI_SRC_INC        0x00000100      // Increment source address after each read
#define DMA_TI_DEST_IGNORE    0x00000080      // Do not perform destination writes
#define DMA_TI_DEST_DREQ      0x00000040      // DREQ selected by PERMAP will gate the writes
#define DMA_TI_DEST_WIDTH     0x00000020      // 1 = Use 128-bit destination write width. 0 = 32-bit
#define DMA_TI_DEST_INC       0x00000010      // Increment destination address after each read
#define DMA_TI_WAIT_RESP      0x00000008      // Wait for a write response
#define DMA_TI_TDMODE         0x00000002      // Set 2D mode
#define DMA_TI_INTEN          0x00000001      // Generate an interrupt when transfer completes

#define DMA_SOURCE_AD_REG(ch) (0x40*(ch)+3)   //** Source Address register
#define DMA_DEST_AD_REG(ch)   (0x40*(ch)+4)   //** Destination Address register
#define DMA_TXFR_LN_REG(ch)   (0x40*(ch)+5)   //** Transfer Length register
#define DMA_STRIDE_REG(ch)    (0x40*(ch)+6)   //** 2D Stride register
#define DMA_NEXTCONBK_REG(ch) (0x40*(ch)+7)   //** Next Control Block Address register

#define DMA_DEBUG_REG(ch)     (0x40*(ch)+8)   //** Debug register
#define DMA_DEBUG_READ_ERROR  0x00000004      // Set if the read operation returned an error
#define DMA_DEBUG_FIFO_ERROR  0x00000002      // Set if the read Fifo records an error condition
#define DMA_DEBUG_SET_ERROR   0x00000001      // Set if the AXI read last signal was not set when expected

#define DMA_ENABLE_REG        0x3FC           //** Global DMA enable register

//
//  GPIO
//

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g)         *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g)         *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a)   *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET0           *(gpio+7)  // Set GPIO high bits 0-31
#define GPIO_SET1           *(gpio+8)  // Set GPIO high bits 32-53

#define GPIO_CLR0           *(gpio+10) // Set GPIO low bits 0-31
#define GPIO_CLR1           *(gpio+11) // Set GPIO low bits 32-53

#define GPIO_LEV0           *(gpio+13) // Get GPIO level low bits 0-31

#define GPIO_EDS0           *(gpio+16) // Get GPIO event detect status low bits 0-31

#define GPIO_REN0           *(gpio+19) // Get GPIO rising edge detect enable low bits 0-31

#define GPIO_PULL           *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0       *(gpio+38) // Pull up/pull down clock

//
//  Mailbox
//
#define MAILBOX_BUFFER_SIZE     0
#define MAILBOX_RE_CODE         1
#define MAILBOX_TAG_ID          2
#define MAILBOX_TAG_BUF_SIZE    3
#define MAILBOX_TAG_RE_CODE     4
#define MAILBOX_TAG_BUFFER      5

// Message IDs for different mailbox GPU memory allocation messages
#define TAG_ALLOCATE_MEMORY     0x3000C // This message is 3 u32s: size, alignment and flags
#define TAG_LOCK_MEMORY         0x3000D // 1 u32: handle
#define TAG_UNLOCK_MEMORY       0x3000E // 1 u32: handle
#define TAG_RELEASE_MEMORY      0x3000F // This message is 1 u32: handle

// Memory allocation flags
#define MEM_FLAG_DIRECT         (1 << 2)  // 0xC alias. Uncached
#define MEM_FLAG_COHERENT       (1 << 3)  // 0x8 alias. Non-allocating in L2 but coherent
#define MEM_FLAG_L1_NONALLOCATING (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT) // Allocating in L2
#define MEM_FLAG_ZERO           (1 << 4)  // initialise buffer to all zeros
#define MEM_FLAG_NO_INIT        (1 << 5)  // don't initialise (default is initialise to all ones
#define MEM_FLAG_HINT_PERMALOCK (1 << 6)  // Likely to be locked for long periods of time

// Response codes
#define BUF_REQ_SUCCESSFUL      0x80000000
#define TAG_RE_BIT_MASK         0x80000000

#define BUS_TO_PHYS(x)          ((x) & ~0xC0000000)
#define PHYS_TO_BUS(x)          ((x) |  0xC0000000)

