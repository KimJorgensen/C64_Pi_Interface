/*
 *  Based on VIC.h - 6569R5 emulation (line based)
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

// Total number of raster lines (PAL)
const unsigned TOTAL_RASTERS = 0x138;

// Screen refresh frequency (PAL)
const unsigned SCREEN_FREQ = 50;

static void reset6569();

static bool emulate_cycle_6569(void);
static void write_register_6569(uint16_t adr, uint8_t byte);
static void changed_va_6569(uint16_t new_va);	// CIA VA14/15 has changed

