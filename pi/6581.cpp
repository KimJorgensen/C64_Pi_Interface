/*
 *  Based on SID.cpp - 6581 emulation
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 *
 * Incompatibilities:
 * ------------------
 *
 *  - Lots of empirically determined constants in the filter calculations
 *  - Voice 3 cannot be muted
 */

#include <math.h>


/*
 *  Resonance frequency polynomials
 */

#define CALC_RESONANCE_LP(f) (227.755\
				- 1.7635 * f\
				- 0.0176385 * f * f\
				+ 0.00333484 * f * f * f\
				- 9.05683E-6 * f * f * f * f)

#define CALC_RESONANCE_HP(f) (366.374\
				- 14.0052 * f\
				+ 0.603212 * f * f\
				- 0.000880196 * f * f * f)


/*
 *  Random number generator for noise waveform
 */

static uint8_t sid_random(void);
static uint8_t sid_random(void)
{
	static uint32_t seed = 1;
	seed = seed * 1103515245 + 12345;
	return seed >> 16;
}


/*
 *  Constructor
 */

static void init6581()
{
	// Link voices together
	voice[0].mod_by = &voice[2];
	voice[1].mod_by = &voice[0];
	voice[2].mod_by = &voice[1];
	voice[0].mod_to = &voice[1];
	voice[1].mod_to = &voice[2];
	voice[2].mod_to = &voice[0];

	// Calculate triangle table
	for (int i=0; i<0x1000; i++) {
		TriTable[i] = (i << 4) | (i >> 8);
		TriTable[0x1fff-i] = (i << 4) | (i >> 8);
	}

	reset6581();

	// Stuff the current register values into the renderer
	for (int i=0; i<25; i++)
		write_register_6581(i, regs[i]);
}


/*
 *  Reset the SID
 */

static void reset6581(void)
{
	for (int i=0; i<32; i++)
		regs[i] = 0;
	last_sid_byte = 0;

	// Reset the renderer
	volume = 0;
	v3_mute = false;

	for (int v=0; v<3; v++) {
		voice[v].wave = WAVE_NONE;
		voice[v].eg_state = EG_IDLE;
		voice[v].count = voice[v].add = 0;
		voice[v].freq = voice[v].pw = 0;
		voice[v].eg_level = voice[v].s_level = 0;
		voice[v].a_add = voice[v].d_sub = voice[v].r_sub = EGTable[0];
		voice[v].gate = voice[v].ring = voice[v].test = false;
		voice[v].filter = voice[v].sync = false;
	}

	f_type = FILT_NONE;
	f_freq = f_res = 0;
	f_ampl = 1.0;
	d1 = d2 = g1 = g2 = 0.0;
	xn1 = xn2 = yn1 = yn2 = 0.0;

	sample_in_ptr = 0;
	memset(sample_buf, 0, SAMPLE_BUF_SIZE);
}

#if 0
/*
 *  Get SID state
 */

static void get_state_6581(MOS6581State *ss)
{
	ss->freq_lo_1 = regs[0];
	ss->freq_hi_1 = regs[1];
	ss->pw_lo_1 = regs[2];
	ss->pw_hi_1 = regs[3];
	ss->ctrl_1 = regs[4];
	ss->AD_1 = regs[5];
	ss->SR_1 = regs[6];

	ss->freq_lo_2 = regs[7];
	ss->freq_hi_2 = regs[8];
	ss->pw_lo_2 = regs[9];
	ss->pw_hi_2 = regs[10];
	ss->ctrl_2 = regs[11];
	ss->AD_2 = regs[12];
	ss->SR_2 = regs[13];

	ss->freq_lo_3 = regs[14];
	ss->freq_hi_3 = regs[15];
	ss->pw_lo_3 = regs[16];
	ss->pw_hi_3 = regs[17];
	ss->ctrl_3 = regs[18];
	ss->AD_3 = regs[19];
	ss->SR_3 = regs[20];

	ss->fc_lo = regs[21];
	ss->fc_hi = regs[22];
	ss->res_filt = regs[23];
	ss->mode_vol = regs[24];

	ss->pot_x = 0xff;
	ss->pot_y = 0xff;
	ss->osc_3 = 0;
	ss->env_3 = 0;
}
#endif

/*
 * Fill buffer (for Unix sound routines), sample volume (for sampled voice)
 */

inline static void emulate_line_6581(void)
{
	emulate_line_sound();
}


/*
 *  Read from register
 */

inline static uint8_t read_register_6581(uint16_t adr)
{
	// A/D converters
	if (adr == 0x19 || adr == 0x1a) {
		last_sid_byte = 0;
		return 0xff;
	}

	// Voice 3 oscillator/EG readout
	if (adr == 0x1b || adr == 0x1c) {
		last_sid_byte = 0;
		return rand();
	}

	// Write-only register: Return last value written to SID
	return last_sid_byte;
}


/*
 *  Write to register
 */

inline static void write_register_6581(uint16_t adr, uint8_t byte)
{
	// Keep a local copy of the register values
	last_sid_byte = regs[adr] = byte;

	if (!ready)
		return;

	int v = adr/7;	// Voice number

	switch (adr) {
		case 0:
		case 7:
		case 14:
			voice[v].freq = (voice[v].freq & 0xff00) | byte;
			voice[v].add = (uint32_t)((float)voice[v].freq * SID_FREQ / SAMPLE_FREQ);
			break;

		case 1:
		case 8:
		case 15:
			voice[v].freq = (voice[v].freq & 0xff) | (byte << 8);
			voice[v].add = (uint32_t)((float)voice[v].freq * SID_FREQ / SAMPLE_FREQ);
			break;

		case 2:
		case 9:
		case 16:
			voice[v].pw = (voice[v].pw & 0x0f00) | byte;
			break;

		case 3:
		case 10:
		case 17:
			voice[v].pw = (voice[v].pw & 0xff) | ((byte & 0xf) << 8);
			break;

		case 4:
		case 11:
		case 18:
			voice[v].wave = (byte >> 4) & 0xf;
			if ((byte & 1) != voice[v].gate)
            {
				if (byte & 1)	// Gate turned on
					voice[v].eg_state = EG_ATTACK;
				else			// Gate turned off
					if (voice[v].eg_state != EG_IDLE)
						voice[v].eg_state = EG_RELEASE;
			}
			voice[v].gate = byte & 1;
			voice[v].mod_by->sync = byte & 2;
			voice[v].ring = byte & 4;
			if ((voice[v].test = byte & 8))
				voice[v].count = 0;
			break;

		case 5:
		case 12:
		case 19:
			voice[v].a_add = EGTable[byte >> 4];
			voice[v].d_sub = EGTable[byte & 0xf];
			break;

		case 6:
		case 13:
		case 20:
			voice[v].s_level = (byte >> 4) * 0x111111;
			voice[v].r_sub = EGTable[byte & 0xf];
			break;

		case 22:
			if (byte != f_freq) {
				f_freq = byte;
				if (SIDFilters)
					calc_filter();
			}
			break;

		case 23:
			voice[0].filter = byte & 1;
			voice[1].filter = byte & 2;
			voice[2].filter = byte & 4;
			if ((byte >> 4) != f_res) {
				f_res = byte >> 4;
				if (SIDFilters)
					calc_filter();
			}
			break;

		case 24:
			volume = byte & 0xf;
			v3_mute = byte & 0x80;
			if (((byte >> 4) & 7) != f_type) {
				f_type = (byte >> 4) & 7;
				xn1 = xn2 = yn1 = yn2 = 0.0;
				if (SIDFilters)
					calc_filter();
			}
			break;
	}
}


/*
 *  Calculate IIR filter coefficients
 */

static void calc_filter(void)
{
	float fr, arg;

	// Check for some trivial cases
	if (f_type == FILT_ALL) {
		d1 = 0.0; d2 = 0.0;
		g1 = 0.0; g2 = 0.0;
		f_ampl = 1.0;
		return;
	} else if (f_type == FILT_NONE) {
		d1 = 0.0; d2 = 0.0;
		g1 = 0.0; g2 = 0.0;
		f_ampl = 0.0;
		return;
	}

	// Calculate resonance frequency
	if (f_type == FILT_LP || f_type == FILT_LPBP)
		fr = CALC_RESONANCE_LP(f_freq);
	else
		fr = CALC_RESONANCE_HP(f_freq);

	// Limit to <1/2 sample frequency, avoid div by 0 in case FILT_BP below
	arg = fr / (float)(SAMPLE_FREQ >> 1);
	if (arg > 0.99)
		arg = 0.99;
	if (arg < 0.01)
		arg = 0.01;

	// Calculate poles (resonance frequency and resonance)
	g2 = 0.55 + 1.2 * arg * arg - 1.2 * arg + (float)f_res * 0.0133333333;
	g1 = -2.0 * sqrt(g2) * cos(M_PI * arg);

	// Increase resonance if LP/HP combined with BP
	if (f_type == FILT_LPBP || f_type == FILT_HPBP)
		g2 += 0.1;

	// Stabilize filter
	if (fabs(g1) >= g2 + 1.0)
    {
		if (g1 > 0.0)
			g1 = g2 + 0.99;
		else
			g1 = -(g2 + 0.99);
    }

	// Calculate roots (filter characteristic) and input attenuation
	switch (f_type) {

		case FILT_LPBP:
		case FILT_LP:
			d1 = 2.0; d2 = 1.0;
			f_ampl = 0.25 * (1.0 + g1 + g2);
			break;

		case FILT_HPBP:
		case FILT_HP:
			d1 = -2.0; d2 = 1.0;
			f_ampl = 0.25 * (1.0 - g1 + g2);
			break;

		case FILT_BP:
			d1 = 0.0; d2 = -1.0;
			f_ampl = 0.25 * (1.0 + g1 + g2) * (1 + cos(M_PI * arg)) / sin(M_PI * arg);
			break;

		case FILT_NOTCH:
			d1 = -2.0 * cos(M_PI * arg); d2 = 1.0;
			f_ampl = 0.25 * (1.0 + g1 + g2) * (1 + cos(M_PI * arg)) / (sin(M_PI * arg));
			break;

		default:
			break;
	}
}


/*
 *  Fill one audio buffer with calculated SID sound
 */

static void calc_buffer(int16_t *buf, long count)
{
	// Get filter coefficients, so the emulator won't change
	// them in the middle of our calculations
	float cf_ampl = f_ampl;
	float cd1 = d1, cd2 = d2, cg1 = g1, cg2 = g2;

	// Index in sample_buf for reading, 16.16 fixed
	uint32_t sample_count = (sample_in_ptr + SAMPLE_BUF_SIZE/2) << 16;

	count >>= 1;	// 16 bit mono output, count is in bytes

	while (count--) {
		int32_t sum_output;
		int32_t sum_output_filter = 0;

		// Get current master volume from sample buffer,
		// calculate sampled voice
		uint8_t master_volume = sample_buf[(sample_count >> 16) % SAMPLE_BUF_SIZE];
		sample_count += ((0x138 * 50) << 16) / SAMPLE_FREQ;
		sum_output = SampleTab[master_volume] << 8;

		// Loop for all three voices
		for (int j=0; j<3; j++) {
			DRVoice *v = &voice[j];

			// Envelope generators
			uint16_t envelope;

			switch (v->eg_state) {
				case EG_ATTACK:
					v->eg_level += v->a_add;
					if (v->eg_level > 0xffffff) {
						v->eg_level = 0xffffff;
						v->eg_state = EG_DECAY;
					}
					break;
				case EG_DECAY:
					if (v->eg_level <= v->s_level || v->eg_level > 0xffffff)
						v->eg_level = v->s_level;
					else {
						v->eg_level -= v->d_sub >> EGDRShift[v->eg_level >> 16];
						if (v->eg_level <= v->s_level || v->eg_level > 0xffffff)
							v->eg_level = v->s_level;
					}
					break;
				case EG_RELEASE:
					v->eg_level -= v->r_sub >> EGDRShift[v->eg_level >> 16];
					if (v->eg_level > 0xffffff) {
						v->eg_level = 0;
						v->eg_state = EG_IDLE;
					}
					break;
				case EG_IDLE:
					v->eg_level = 0;
					break;
			}
			envelope = (v->eg_level * master_volume) >> 20;

			// Waveform generator
			uint16_t output;

			if (!v->test)
				v->count += v->add;

			if (v->sync && (v->count > 0x1000000))
				v->mod_to->count = 0;

			v->count &= 0xffffff;

			switch (v->wave) {
				case WAVE_TRI:
					if (v->ring)
						output = TriTable[(v->count ^ (v->mod_by->count & 0x800000)) >> 11];
					else
						output = TriTable[v->count >> 11];
					break;
				case WAVE_SAW:
					output = v->count >> 8;
					break;
				case WAVE_RECT:
					if (v->count > (uint32_t)(v->pw << 12))
						output = 0xffff;
					else
						output = 0;
					break;
				case WAVE_TRISAW:
					output = TriSawTable[v->count >> 16];
					break;
				case WAVE_TRIRECT:
					if (v->count > (uint32_t)(v->pw << 12))
						output = TriRectTable[v->count >> 16];
					else
						output = 0;
					break;
				case WAVE_SAWRECT:
					if (v->count > (uint32_t)(v->pw << 12))
						output = SawRectTable[v->count >> 16];
					else
						output = 0;
					break;
				case WAVE_TRISAWRECT:
					if (v->count > (uint32_t)(v->pw << 12))
						output = TriSawRectTable[v->count >> 16];
					else
						output = 0;
					break;
				case WAVE_NOISE:
					if (v->count > 0x100000) {
						output = v->noise = sid_random() << 8;
						v->count &= 0xfffff;
					} else
						output = v->noise;
					break;
				default:
					output = 0x8000;
					break;
			}
			if (v->filter)
				sum_output_filter += (int16_t)(output ^ 0x8000) * envelope;
			else
				sum_output += (int16_t)(output ^ 0x8000) * envelope;
		}

		// Filter
		if (SIDFilters) {
			float xn = (float)sum_output_filter * cf_ampl;
			float yn = xn + cd1 * xn1 + cd2 * xn2 - cg1 * yn1 - cg2 * yn2;
			yn2 = yn1; yn1 = yn; xn2 = xn1; xn1 = xn;
			sum_output_filter = (int32_t)yn;
		}

		// Write to buffer
		*buf++ = (sum_output + sum_output_filter) >> 10;
	}
}

