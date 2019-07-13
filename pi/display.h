/*
 *  Based on Display.h - C64 graphics display, emulator window handling
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

// Display dimensions
static const int DISPLAY_X = 0x180;
static const int DISPLAY_Y = 0x110;

// C64 color palette in RGBA (from https://www.pepto.de/projects/colorvic/)
static const uint32_t palette[16] =
{
	0x000000ff, 0xffffffff, 0x813338ff, 0x75cec8ff,
	0x8e3c97ff, 0x56ac4dff, 0x2e2c9bff, 0xedf171ff,
	0x8e5029ff, 0x553800ff, 0xc46c71ff, 0x4a4a4aff,
	0x7b7b7bff, 0xa9ff9fff, 0x706debff, 0xb2b2b2ff
};

static uint32_t *display_bitmap_base;   // Pointer to bitmap data
static int display_bitmap_xmod;         // Number of bytes per row

static void display_update(void);
static void display_draw_string(int x, int y, const char *str, uint32_t front_color, uint32_t back_color);

static void vic_vblank();

