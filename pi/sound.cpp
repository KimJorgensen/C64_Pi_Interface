/*
 *  Based on SID_Be.i - 6581 emulation, Be specific stuff
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include <SDL.h>

static uint32_t dev;

/*
 *  Callback function
 */

static void audio_callback(void *userdata, uint8_t *stream, int len)
{
    (void)userdata; // ignore
    calc_buffer((int16_t *)stream, len);
}

/*
 *  Initialization
 */

static void sound_init(void)
{
    SDL_AudioSpec want, have;
    ready = false;

    SDL_memset(&want, 0, sizeof(want));
    want.freq = SAMPLE_FREQ;
    want.format = AUDIO_S16LSB;
    want.channels = 1;
    want.samples = SAMPLE_FREQ / CALC_FREQ;
    want.callback = audio_callback;
    want.userdata = NULL;

    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev == 0)
    {
        fprintf(stderr, "Failed to open audio: %s", SDL_GetError());
        return;
    }

    SDL_PauseAudioDevice(dev, 0); // start audio playing
    ready = true;
}


/*
 *  Close audio device 
 */

static void sound_close()
{
    SDL_CloseAudioDevice(dev);
}


/*
 *  Sample volume (for sampled voice)
 */

void static emulate_line_sound(void)
{
    SDL_LockAudioDevice(dev);

    sample_buf[sample_in_ptr] = volume;
    sample_in_ptr = (sample_in_ptr + 1) % SAMPLE_BUF_SIZE;

    SDL_UnlockAudioDevice(dev);
}

