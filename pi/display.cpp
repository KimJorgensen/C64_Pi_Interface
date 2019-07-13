/*
 *  Based on
 *  Display.cpp   - C64 graphics display, emulator window handling
 *  Display_SDL.i - C64 graphics display, emulator window handling,
 *                  SDL specific stuff
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include <SDL.h>
#include <signal.h>
#include <sys/time.h>


// Display surface
static SDL_Surface *screen = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;

#ifdef FAKE_PI
static struct timeval tv_start;
static double speed_index;
static char speedometer_string[16]; // Speedometer text
#endif


/*
 *  Open window
 */

static int display_init(void)
{
    // Init SDL
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) != 0)
    {
        fprintf(stderr, "Couldn't initialize SDL (%s)\n", SDL_GetError());
        return 0;
    }

    SDL_ShowCursor(SDL_DISABLE);
    screen = SDL_CreateRGBSurface(0, DISPLAY_X, DISPLAY_Y, 32,
                                  0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

    display_bitmap_base = (uint32_t *)screen->pixels;
    display_bitmap_xmod = screen->pitch/sizeof(uint32_t);

    // Open window
    SDL_Window *window = SDL_CreateWindow("C64 - Pi",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          DISPLAY_X * 2, DISPLAY_Y * 2,
                                          SDL_WINDOW_RESIZABLE);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(renderer, DISPLAY_X, DISPLAY_Y);

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_RGBA8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                DISPLAY_X, DISPLAY_Y);
    display_update();

    return 1;
}


/*
 *  Close window
 */

static void display_close(void)
{
	SDL_Quit();
}


/*
 *  Redraw bitmap
 */

static void display_update(void)
{
    SDL_UpdateTexture(texture, NULL, screen->pixels, screen->pitch);

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}


/*
 *  Draw string into surface using the C64 ROM font
 */

static void display_draw_string(int x, int y, const char *str, uint32_t front_color, uint32_t back_color)
{
	SDL_Surface *s = screen;
	uint32_t *pb = (uint32_t *)s->pixels + s->pitch*y/sizeof(uint32_t) + x;
	char c;
	while ((c = *str++) != 0) {
		uint8_t *q = char_rom + c*8 + 0x800;
		uint32_t *p = pb;
		for (int y=0; y<8; y++) {
			uint8_t v = *q++;
			p[0] = (v & 0x80) ? front_color : back_color;
			p[1] = (v & 0x40) ? front_color : back_color;
			p[2] = (v & 0x20) ? front_color : back_color;
			p[3] = (v & 0x10) ? front_color : back_color;
			p[4] = (v & 0x08) ? front_color : back_color;
			p[5] = (v & 0x04) ? front_color : back_color;
			p[6] = (v & 0x02) ? front_color : back_color;
			p[7] = (v & 0x01) ? front_color : back_color;
			p += s->pitch/sizeof(uint32_t);
		}
		pb += 8;
	}
}

static void display_poll_keyboard()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			// Key pressed
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					case SDLK_F4:	    // F4: Quit
						quit_requested = true;
						break;

					case SDLK_KP_PLUS:	// '+' on keypad: turbo mode (debug)
						debug_turbo = !debug_turbo;
						break;

        			default:
		        	    break;
        	    }
				break;

			case SDL_QUIT:
				quit_requested = true; 
				break;

			default:
        	    break;
		}
	}
}

static void vic_vblank()
{
    display_poll_keyboard();

#ifdef FAKE_PI
    display_draw_string(0, DISPLAY_Y - 8, speedometer_string, palette[6], palette[0]);
#endif

	display_update();

#ifdef FAKE_PI
	// Calculate time between vblanks, display speedometer
	struct timeval tv;
	gettimeofday(&tv, NULL);
	if ((tv.tv_usec -= tv_start.tv_usec) < 0) {
		tv.tv_usec += 1000000;
		tv.tv_sec -= 1;
	}
	tv.tv_sec -= tv_start.tv_sec;
	double elapsed_time = (double)tv.tv_sec * 1000000 + tv.tv_usec;
	speed_index = 20000 / (elapsed_time + 1) * 100;

	// Limit speed to 100%
	if (!debug_turbo && speed_index > 100) {
		usleep((unsigned long)(20000 - elapsed_time));
		speed_index = 100;
	}

	gettimeofday(&tv_start, NULL);

	static int delay = 0;
    if (delay >= 20)
    {
	    delay = 0;
	    sprintf(speedometer_string, "%d%%", (int)speed_index);
    }
    else
	{
	    delay++;
    }
#endif
}

