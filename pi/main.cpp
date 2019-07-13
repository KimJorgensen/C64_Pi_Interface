#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/resource.h>
#include "display.h"
#include "6581.h"
#include "pi.h"

#include "6502.cpp"

#undef FAKE_PI
//#define FAKE_PI

// CPU cycle counter
static uint32_t cycle_counter;

static uint8_t ram[0x10000];
static uint8_t color_ram[0x0400];
static uint8_t char_rom[0x1000];

// VIC-II stuff
static bool vic_ba_Low;
static bool vic_in_sync = false;

static void vic_trigger_irq()
{
    // Ignore
}

static void vic_clear_irq()
{
    // Ignore
}

static bool quit_requested = false;
static bool debug_turbo = true;

#include "6569.cpp"
#include "sound.cpp"
#include "6581.cpp"
#include "display.cpp"

#ifndef FAKE_PI
  #include "pi.cpp"
#else
  #include "fake_pi.cpp"
#endif

static uint8_t ddr_6510 = 0x00;
static uint8_t dr_6510 = 0x3f;
static bool io_visible = false;

inline static void cpu_changed_port()
{
    uint8_t port = ~ddr_6510 | dr_6510;
    io_visible = (port & 0x03) && (port & 0x04);
}

static uint8_t pra_6526_2 = 0x00;
static uint8_t ddra_6526_2 = 0x00;

inline static void cia2_changed_va()
{
	// VA14/15
    changed_va_6569(~(pra_6526_2 | ~ddra_6526_2) & 3);
}

static void reset6526_2()
{
	pra_6526_2 = 0;
	ddra_6526_2 = 0;
	cia2_changed_va();
}

static void reset_sim()
{
    cycle_counter = -1;
    ddr_6510 = 0x00;
    dr_6510 = 0x3f;
    cpu_changed_port();
    reset6502();

    reset6526_2();
    reset6569();
    reset6581();
}

static void write_register_6526_2(uint8_t address, uint8_t data)
{
	switch (address)
	{
        case 0x0:
            pra_6526_2 = data;
            cia2_changed_va();
            break;
        case 0x2:
            ddra_6526_2 = data;
            cia2_changed_va();
            break;
    }
}

static void mem_write(uint16_t address, uint8_t data)
{
    if (address < 0xd000 || address >= 0xe000 || !io_visible)
    {
        ram[address] = data;
    }
    else
    {
		switch ((address >> 8) & 0x0f)
		{
			case 0x0:	// VIC
			case 0x1:
			case 0x2:
			case 0x3:
				write_register_6569(address & 0x3f, data);
				break;
			case 0x4:	// SID
			case 0x5:
			case 0x6:
			case 0x7:
				write_register_6581(address & 0x1f, data);
				break;
			case 0x8:	// Color RAM
			case 0x9:
			case 0xa:
			case 0xb:
				color_ram[address & 0x03ff] = data & 0x0f;
				break;
			case 0xc:	// CIA 1
				break;
			case 0xd:	// CIA 2
				write_register_6526_2(address & 0x0f, data);
				break;
			case 0xe:	// I/O 1
                break;
			case 0xf:	// I/O 2
				break;
		}
	}
}

struct smi_stream
{
    uint16_t *chunk_at;
    uint32_t remaining;
    uint16_t *next_chunk;
};

static void get_next_chunk(smi_stream *stream)
{
    if (stream->next_chunk)
    {
        stream->chunk_at = stream->next_chunk;
        stream->next_chunk = 0;
    }
    else
    {
        stream->chunk_at = get_next_smi_chunk();
    }

    stream->remaining = chunck_size/sizeof(uint16_t);
}

static uint16_t get(smi_stream *stream)
{
    uint16_t result = *stream->chunk_at++;

    if (--stream->remaining == 0)
    {
       get_next_chunk(stream);
    }

    return result;
}

static uint16_t peek(smi_stream *stream, uint32_t pos)
{
    uint16_t result;

    if (pos >= stream->remaining)
    {
        pos -= stream->remaining;
        if (!stream->next_chunk)
        {
            stream->next_chunk = get_next_smi_chunk();
        }

        result = stream->next_chunk[pos];
    }
    else
    {
        result = stream->chunk_at[pos];
    }

    return result;
}

int main()
{
    // Catch all signals (like ctrl+c, ctrl+z, ...) to ensure DMA is disabled
    for (int i = 0; i < 64; i++)
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = cleanup_smi_and_exit;
        sigaction(i, &sa, NULL);
    }

    id_t pid = getpid();  
    setpriority(PRIO_PROCESS, pid, -20);

    FILE *rom_file = fopen("char.rom", "r");
    if (rom_file != NULL)
    {
        if (fread(char_rom, sizeof(char_rom), 1, rom_file) != 1)
        {
            fprintf(stderr, "Failed to read data from char.rom file\n");
            return 1;
        }
        fclose(rom_file);
    }
    else
    {
        fprintf(stderr, "Failed to open char.rom file for reading\n%s\n",
            strerror(errno));
        return 1;
    }

	if (!display_init())
    {
        fprintf(stderr, "Failed to init display\n");
        return 1;
    }

    init6581();
    sound_init();

    reset_sim();
    bool interrupt = false;

    if (!start_smi_dma())
    {
        sound_close();
        display_close();
        return 1;
    }

    display_draw_string(96, 120, "WAITING FOR C64 TO RESET", palette[14], palette[0]);
    display_update();

    smi_stream stream = {};
    get_next_chunk(&stream);

    // Wait for reset
    while (peek(&stream, 0) != 0xFFFC || peek(&stream, 2) != 0xFFFD)
    {
        get(&stream);
    }

    for (cycle_counter=0;; cycle_counter++)
    {
        uint16_t address = get(&stream);
        uint16_t status_data = get(&stream);

        uint8_t status = (uint8_t)(status_data >> 8);
        uint8_t data = (uint8_t)status_data;

        if (status & 0xFC)
        {
            fprintf(stderr, "Invalid status byte %02x at %d\n", status, cycle_counter);
            break;
        }

        bool ba = status & 0x02;
        bool write = status & 0x01;

        // Handle IRQ/NMI
        if (interrupt)
        {
            if (cpu.cycle == 1)
            {
                interrupt = false;
            }
            else if (cpu.cycle == 4)
            {
                cpu.data &= ~FLAG_BREAK;
            }
            else if (cpu.cycle == 5)
            {
                if (address == 0xfffa) // Check for NMI
                {
                    cpu.addr = address;
                }
            }
        }

        // Look ahead to detect interrupt -
        // 3 consecutive writes only occur during an interrupt or BRK
        if (!interrupt && cpu.cycle == 1 && (peek(&stream, 1) & 0x0100) &&
            (peek(&stream, 3) & 0x0100) && (peek(&stream, 5) & 0x0100))
        {
            if (cpu.opcode != 0x00) // Check for BRK instruction
            {
                cpu.opcode = 0x00;
                cpu.addr = --cpu.pc;
                --cpu.pc;
                interrupt = true;
            }
        }

        // TODO: Improve the VIC-II sync
        if (vic_in_sync || !vic_ba_Low)
        {
    		if (emulate_cycle_6569())
    		{
    			emulate_line_6581();
			}
        }
        else if (ba)
        {
            // The VIC-II emulator is in sync with the real C64
            vic_in_sync = true;
        }
        
        if (quit_requested)
        {
            printf("Quit requested (%d)\n", cycle_counter);
            break;
        }

        // Skip VIC-II cycle
        if (!ba || write)
        {
            // TODO: Fix this "+ 0x0100" hack. The 6502 seems to do calc during the VIC-II cycle
            if (address != cpu.addr && address != (uint16_t)(cpu.addr + 0x0100))
            {
                printf("%c %04x %02x - op: %02x%c(%u) - %d\n", write ? 'W' : 'R',
                    address, data, cpu.opcode, interrupt ? '*' : ' ', cpu.cycle, cycle_counter);
                fprintf(stderr, "Unexpected address: %04x. Expected: %04x   (%u)\n",
                    address, cpu.addr, cycle_counter);

                // TODO: Try to recover
                break;
            }

            if (write)
            {
                if (!cpu.write)
                {
                    fprintf(stderr, "Unexpected write. Expected read\n");
                    // TODO: Try to recover
                    break;
                }

                if (address == 0x0000)
                {
                    ddr_6510 = cpu.data & 0x3f;
                    cpu_changed_port();
                }
                else if (address == 0x0001)
                {
                    dr_6510 = cpu.data & 0x3f;
                    cpu_changed_port();
                }
                // Ignore upper nibble (which is "random" when read from color RAM)
                else if ((data & 0x0f) != (cpu.data & 0x0f))
                {
                    fprintf(stderr, "Unexpected data to write at %04x: %02x. Expected: %02x\n",
                        address, data, cpu.data);
                    // TODO: Try to recover
                    break;
                }

                mem_write(address, data);
            }
            else
            {
                if (cpu.write)
                {
                    fprintf(stderr, "Unexpected read. Expected write\n");
                    // TODO: Try to recover
                    break;
                }

                if (address == 0x0000)
                {
                    cpu.data = ddr_6510;
                }
                else if (address == 0x0001)
                {
                    // Assume sense is always high (no tape button pressed)
                    cpu.data = dr_6510 | 0x10;
                }
                else
                {
                    cpu.data = data;
                }
            }

            step6502();
        }
    }

    cleanup_smi();
    sound_close();
    display_close();

    return 0;
}
