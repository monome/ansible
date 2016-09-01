/*

config:
	modal view (don't display stuff in mixed mode)
	a. range (smashes offset) end point with shading
	b. scale -- note intervals. includes "none". two octaves (25 notes)
	a. offset (smashes range) start point with shading
	b. offset (0-12) note interval. shaded blocks with highlight.

implement offset and range
implmement octave offset

tr output in VOLT mode
tr new note in NOTE mode

presets



/////

default slew for editing?? or same as transition?
should note range be 48?

loop trapping
SLEW NEEDS LOG/etc scaling
tune acceleration

future features?
	live slew indication
	share scales with kria/mp?
*/

#include "print_funcs.h"
#include "flashc.h"
#include "gpio.h"

#include "dac.h"
#include "monome.h"
#include "i2c.h"
#include "music.h"

#include "main.h"
#include "ansible_arc.h"



void set_mode_arc(void) {
	switch(f.state.mode) {
	case mArcLevels:
		print_dbg("\r\n> mode arc levels");
		app_event_handlers[kEventKey] = &handler_LevelsKey;
		app_event_handlers[kEventTr] = &handler_LevelsTr;
		app_event_handlers[kEventTrNormal] = &handler_LevelsTrNormal;
		app_event_handlers[kEventMonomeRingEnc] = &handler_LevelsEnc;
		app_event_handlers[kEventMonomeRefresh] = &handler_LevelsRefresh;
		clock = &clock_levels;
		clock_set(f.levels_state.clock_period);
		process_ii = &ii_levels;
		resume_levels();
		update_leds(1);
		break;
	case mArcCycles:
		print_dbg("\r\n> mode arc cycles");
		app_event_handlers[kEventKey] = &handler_CyclesKey;
		app_event_handlers[kEventTr] = &handler_CyclesTr;
		app_event_handlers[kEventTrNormal] = &handler_CyclesTrNormal;
		app_event_handlers[kEventMonomeRingEnc] = &handler_CyclesEnc;
		app_event_handlers[kEventMonomeRefresh] = &handler_CyclesRefresh;
		clock = &clock_cycles;
		clock_set(f.cycles_state.clock_period);
		process_ii = &ii_cycles;
		update_leds(2);
		break;
	default:
		break;
	}
	
	if(connected == conARC) {
		app_event_handlers[kEventFrontShort] = &handler_ArcFrontShort;
		app_event_handlers[kEventFrontLong] = &handler_ArcFrontLong;
	}

	flashc_memset32((void*)&(f.state.none_mode), f.state.mode, 4, true);
	flashc_memset32((void*)&(f.state.arc_mode), f.state.mode, 4, true);
}


void handler_ArcFrontShort(s32 data) {
	print_dbg("\r\n> PRESET");
}

void handler_ArcFrontLong(s32 data) {
	if(f.state.mode == mArcLevels)
		set_mode(mArcCycles);
	else
		set_mode(mArcLevels);
}

uint8_t key_count_arc[2];

void (*arc_refresh)(void);

const uint8_t delta_acc[32] = {0, 1, 3, 5, 7, 9, 10, 12, 14, 16, 18, 19, 21, 23, 25, 27, 28, 30, 32, 34, 36, 37, 39, 41, 43,
45, 46, 48, 50, 52, 54, 55 };

static void key_long_levels(uint8_t key);
static void key_long_cycles(uint8_t key);

static uint8_t calc_note(uint8_t s, uint8_t i);
static void generate_scales(uint8_t n);


void arc_keytimer(void) {
	if(key_count_arc[0]) {
		if(key_count_arc[0] == 1) {
			switch(f.state.mode) {
			case mArcLevels:
				key_long_levels(0);
				break;
			case mArcCycles:
				key_long_cycles(0);
				break;
			default:
				break;
			}
		}
		key_count_arc[0]--;
	}

	if(key_count_arc[1]) {
		if(key_count_arc[1] == 1) {
			switch(f.state.mode) {
			case mArcLevels:
				key_long_levels(1);
				break;
			case mArcCycles:
				key_long_cycles(1);
				break;
			default:
				break;
			}
		}
		key_count_arc[1]--;
	}
}

static void arc_draw_point(uint8_t n, uint16_t p);
static void arc_draw_point_dark(uint8_t n, uint16_t p);

static bool ext_clock;
static bool ext_reset;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static void levels_pattern_next(void);
static void levels_play_next(void);

static void levels_dac_refresh(void);

static void levels_at1(void* o);

static uint8_t mode;
static uint8_t ch_edit;
static bool mixed_modes;
static uint8_t play;
static int8_t pattern_read;
static int8_t pattern_write;
static int16_t enc_count[4];
static levels_data_t l;
static uint8_t levels_scales[4][25];
static bool tr_state[4];

void default_levels() {
	uint8_t i1,i2;

	l.now = 0;
	l.start = 0;
	l.len = 3;
	l.dir = 0;

	for(i1=0;i1<4;i1++) {
		l.mode[i1] = 0;
		l.scale[i1] = 1;
		l.octave[i1] = 0;
		l.offset[i1] = 0;
		l.range[i1] = 10;
		l.slew[i1] = 20;
		for(i2=0;i2<16;i2++)
			l.pattern[i1][i2] = 0;
	}

	flashc_memcpy((void *)&f.levels_state.l, &l, sizeof(l), true);
	flashc_memset32((void*)&(f.levels_state.clock_period), 250, 4, true);
}

void init_levels() {
	ch_edit = 0;
}

void resume_levels() {
	uint8_t i1;

	l.now = f.levels_state.l.now;
	l.start = f.levels_state.l.start;
	l.len = f.levels_state.l.len;
	l.dir = f.levels_state.l.dir;

	for(i1=0;i1<4;i1++) {
		l.pattern[i1][l.now] = f.levels_state.l.pattern[i1][l.now];
		l.mode[i1] = f.levels_state.l.mode[i1];
		l.scale[i1] = f.levels_state.l.scale[i1];
		l.octave[i1] = f.levels_state.l.octave[i1];
		l.offset[i1] = f.levels_state.l.offset[i1];
		l.range[i1] = f.levels_state.l.range[i1];
		l.slew[i1] = f.levels_state.l.slew[i1];

		dac_set_slew(i1,l.slew[i1]);
	}

	arc_refresh = &refresh_levels;

	reset_dacs();

	mode = 0;

	generate_scales(0);
	generate_scales(1);
	generate_scales(2);
	generate_scales(3);

	key_count_arc[0] = 0;
	key_count_arc[1] = 0;

	ext_clock = !gpio_get_pin_value(B10);
	ext_reset = false;

	timer_add(&auxTimer[0], l.pattern[0][l.now], &levels_at1, NULL );

	monomeFrameDirty++;
}

static void levels_at1(void* o) {
	if(tr_state[0])
		set_tr(TR1);
	else
		clr_tr(TR1);

	tr_state[0] ^= 1;
}

void clock_levels(uint8_t phase) {
	;;
	// if(phase)
	// 	set_tr(TR1);
	// else
	// 	clr_tr(TR1);
}

void ii_levels(uint8_t *d, uint8_t l) {
	;;
}

void handler_LevelsEnc(s32 data) { 
	uint8_t n;
	int8_t delta;
	int16_t i;

	monome_ring_enc_parse_event_data(data, &n, &delta);

	// print_dbg("\r\n levels enc \tn: "); 
	// print_dbg_ulong(n); 
	// print_dbg("\t delta: "); 
	// print_dbg_hex(delta);

	switch(mode) {
	// NORMAL
	case 0:
		// NOTE
		if(l.mode[n]) {
			enc_count[n] += delta;
			i = enc_count[n] >> 4;
			enc_count[n] -= i << 4;

			if(i) {
				i += l.note[n][l.now];
				if(i<0)
					i=0;
				else if(i>24 && l.scale[n])
					i=24;
				else if(i>41 && !l.scale[n])
					i=41;

				l.note[n][l.now] = i;

				if(!ext_clock) {
				// if(l.now == play) {
					if(l.scale[n]) 
						dac_set_value(n, ET[ levels_scales[n][i] ] << 2);
					else
						dac_set_value(n, ET[i] << 2);
				}
			}
		}
		// VOLT
		else {
			if(delta > 0)
				i = l.pattern[n][l.now] + delta_acc[delta];
			else 
				i = l.pattern[n][l.now] - delta_acc[-delta];
			if(i < 0)
				i = 0;
			else if(i > 0x3ff)
				i = 0x3ff;
			l.pattern[n][l.now] = i;

			timer_set(&auxTimer[0], 0x3ff - l.pattern[0][l.now] + 10);

			if(!ext_clock) {
			// if(l.now == play) {
				dac_set_value(n, i << 4);
			}
		}
		break;
	// LEFT KEY
	case 1:
		i = (enc_count[n] + delta) & 0xff;
		if(i >> 4 != enc_count[n] >> 4) {
			switch(n) {
			case 0:
				pattern_read = i >> 4;
				pattern_write = -1;
				enc_count[1] = (l.now << 4) + 8;
				break;
			case 1:
				pattern_write = i >> 4;
				pattern_read = -1;
				enc_count[0] = (l.now << 4) + 8;
				break;
			case 2:
				l.start = i >> 4;
				break;
			case 3:
				if((i>>4) == 0) {
					if(l.dir && l.len == 2)
						l.dir = 0;
					else if(l.dir == 0 && l.len == 2)
						l.dir = 1;
				}

				if(l.dir)
					l.len = ((16 - (i>>4)) & 0xf) + 1;
				else
					l.len = (i>>4) + 1;
				
				// print_dbg("\r\nlen: ");
				// print_dbg_ulong(l.len);
				// print_dbg("\t");
				// print_dbg_ulong(l.dir);
				// print_dbg("\t");
				// print_dbg_ulong(i>>4);
				break;
			default:
				break;
			}
			monomeFrameDirty++;
		}
		// print_dbg("\r\ni: ");
		// print_dbg_ulong(i >> 4);
		enc_count[n] = i;
		break;
	// RIGHT KEY
	case 2:
		switch(n) {
		// volt/note mode
		case 0:
			if(ch_edit) {
				l.mode[ch_edit - 1] = delta > 0;
				i = l.mode[0] + l.mode[1] + l.mode[2] + l.mode[3];
				mixed_modes = i>0 && i<4;
			}
			else {
				mixed_modes = false;
				for(uint8_t i1=0;i1<4;i1++)
					l.mode[i1] = delta > 0;
			}
			break;
		// range / scale
		case 1:
			if(mixed_modes == false) {
				enc_count[1] += delta;
				i = enc_count[1] >> 4;
				enc_count[1] -= i << 4;

				if(ch_edit) {
					if(l.mode[ch_edit-1] == 0) {
						// range
						if(i) {
							i += l.range[ch_edit-1];
							if(i < 1) i = 1;
							else if(i > 10) i = 10;

							l.range[ch_edit-1] = i;

							monomeFrameDirty++;
						}
					}
					else {
						// scale
						if(i) {
							i += l.scale[ch_edit-1];
							if(i < 0) i = 0;
							else if(i > 7) i = 7;

							l.scale[ch_edit-1] = i;

							generate_scales(ch_edit-1);

							monomeFrameDirty++;
						}
					}
				}
				else {
					// range
					if(l.mode[0] == 0) {
						if(i) {
							i += l.range[0];

							if(i < 1) i = 1;
							else if(i > 10) i = 10;

							l.range[0] = i;
							l.range[1] = i;
							l.range[2] = i;
							l.range[3] = i;

							monomeFrameDirty++;
						}
					}
					else {
						// scale
						if(i) {
							i += l.scale[0];

							if(i < 0) i = 0;
							else if(i > 7) i = 7;

							l.scale[0] = i;
							l.scale[1] = i;
							l.scale[2] = i;
							l.scale[3] = i;

							generate_scales(0);
							generate_scales(1);
							generate_scales(2);
							generate_scales(3);

							monomeFrameDirty++;
						}
					}
				}
			}
			break;
		// offset
		case 2:
			if(mixed_modes == false) {
				if(ch_edit) {
					// volt offset
					if(l.mode[ch_edit-1] == 0) {
						if(delta > 0)
							i = l.offset[ch_edit-1] + delta_acc[delta];
						else 
							i = l.offset[ch_edit-1] - delta_acc[-delta];
						if(i < 0)
							i = 0;
						else if(i > 0x3ff)
							i = 0x3ff;
						l.offset[ch_edit-1] = i;
					}
					// octave
					else {
						enc_count[2] += delta;
						i = enc_count[2] >> 4;
						enc_count[2] -= i << 4;

						i += l.octave[ch_edit-1];

						if(i < 0) i = 0;
						else if(i > 5) i = 5;

						l.octave[ch_edit-1] = i;
					}

				}
				else {
					// volt offset
					if(l.mode[0] == 0) {
						if(delta > 0)
							i = l.offset[0] + delta_acc[delta];
						else 
							i = l.offset[0] - delta_acc[-delta];
						if(i < 0)
							i = 0;
						else if(i > 0x3ff)
							i = 0x3ff;
						l.offset[0] = i;
						l.offset[1] = i;
						l.offset[2] = i;
						l.offset[3] = i;
					}
					// octave
					else {
						enc_count[2] += delta;
						i = enc_count[2] >> 4;
						enc_count[2] -= i << 4;

						i += l.octave[0];

						if(i < 0) i = 0;
						else if(i > 5) i = 5;

						l.octave[0] = i;
						l.octave[1] = i;
						l.octave[2] = i;
						l.octave[3] = i;
					}
				}
			}
			break;
		// slew
		case 3:
			if(ch_edit) {
				i = l.slew[ch_edit-1] + (delta * 8);
				if(i < 1) i = 1;
				else if(i > 4091) i = 4091;
				l.slew[ch_edit-1] = i;
				dac_set_slew(ch_edit-1, i);
			}
			else {
				i = l.slew[0] + (delta * 8);
				if(i < 1) i = 1;
				else if(i > 4091) i = 4091;

				for(uint8_t i1=0;i1<4;i1++) {
					l.slew[i1] = i;
					dac_set_slew(i1, i);
				}
				print_dbg("\r\nslew: ");
				print_dbg_ulong(i);
			}
			break;
		default:
			break;
		}
	default:
		break;
	}

	// print_dbg("\r\n accum: "); 
	// print_dbg_hex(i);

	monomeFrameDirty++;
}


void handler_LevelsRefresh(s32 data) {
	if(monomeFrameDirty) {
		arc_refresh();

		monome_set_quadrant_flag(0);
		monome_set_quadrant_flag(1);
		monome_set_quadrant_flag(2);
		monome_set_quadrant_flag(3);
		(*monome_refresh)();
	}
}

void refresh_levels() {
	uint16_t i1, i2;
	for(i1=0;i1<256;i1++) {
		monomeLedBuffer[i1] = 0;
	}

	for(i1=0;i1<4;i1++) {
		if(l.mode[i1]) {
			// note map
			if(l.scale[i1]) {
				// show note map
				for(i2=0;i2<24;i2++) {
					monomeLedBuffer[i1*64 + ((32 + levels_scales[i1][i2]) & 0x3f)] = 3;
				}
				monomeLedBuffer[i1*64 + ((32 + 0) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 12) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 24) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 36) & 0x3f)] = 7;
				// show note
				monomeLedBuffer[i1*64 + ((32 + levels_scales[i1][l.note[i1][play]]) & 0x3f)] = 10;
				monomeLedBuffer[i1*64 + ((32 + levels_scales[i1][l.note[i1][l.now]]) & 0x3f)] = 15;
			}
			// all semitones
			else {
				for(i2=0;i2<42;i2++)
					monomeLedBuffer[i1*64 + ((32 + i2) & 0x3f)] = 3;
				monomeLedBuffer[i1*64 + ((32 + 0) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 12) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 24) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 36) & 0x3f)] = 7;
				// show note
				monomeLedBuffer[i1*64 + ((32 + l.note[i1][play]) & 0x3f)] = 10;
				monomeLedBuffer[i1*64 + ((32 + l.note[i1][l.now]) & 0x3f)] = 15;

			}
		}
		else {
			if(play == l.now) {
				i2 = (l.pattern[i1][l.now] * 3) >> 2;
				i2 = (i2 + 640) & 0x3ff;
				arc_draw_point(i1, i2);
			}
			else {
				i2 = (l.pattern[i1][play] * 3) >> 2;
				i2 = (i2 + 640) & 0x3ff;
				arc_draw_point_dark(i1, i2);

				i2 = (l.pattern[i1][l.now] * 3) >> 2;
				i2 = (i2 + 640) & 0x3ff;
				arc_draw_point(i1, i2);
			}
		}
	}
}

void refresh_levels_change() {
	uint16_t i1;
	for(i1=0;i1<256;i1++) {
			monomeLedBuffer[i1] = 0;
		}

	if(l.dir) {
		for(i1=0;i1<l.len * 4;i1++) {
			monomeLedBuffer[128 + ((((l.start - l.len + 1) * 4) + i1) & 63)] = 3;
			monomeLedBuffer[192 + ((((l.start - l.len + 1) * 4) + i1) & 63)] = 5;
		}
	}
	else {
		for(i1=0;i1<l.len * 4;i1++) {
			monomeLedBuffer[128 + (((l.start * 4) + i1) & 63)] = 3;
			monomeLedBuffer[192 + (((l.start * 4) + i1) & 63)] = 5;
		}
	}

	for(i1=0;i1<4;i1++) {
		// READ
		monomeLedBuffer[(l.now * 4) + i1] = 5;
		if(pattern_read >= 0)
			monomeLedBuffer[(pattern_read * 4) + i1] = 15;

		// WRITE
		monomeLedBuffer[64 + (l.now * 4) + i1] = 5;
		if(pattern_write >= 0)
			monomeLedBuffer[64 + (pattern_write * 4) + i1] = 15;

		// PATTERN START
		monomeLedBuffer[128 + (l.start * 4) + i1] = 15;

		// PLAY
		monomeLedBuffer[192 + (play * 4) + i1] = 11;		
	}


}

void refresh_levels_config() {
	uint16_t i1, i2;
	for(i1=0;i1<256;i1++) {
		monomeLedBuffer[i1] = 0;
	}

	for(i1=0;i1<4;i1++) {
		for(i2=0;i2<4;i2++) {
			if(l.mode[i1])
				monomeLedBuffer[46 - (i1*8) - i2] = (i2 & 1) * 10;
			else
				monomeLedBuffer[46 - (i1*8) - i2] = i2 * 3 + 1;

			if(i1 == (ch_edit - 1)) {
				monomeLedBuffer[46 - (i1*8) - i2] += 5;
			}
		}
	}

	if(ch_edit) {
		if(l.mode[ch_edit-1]) {
			// show note map
			if(l.scale[ch_edit-1]) {
				for(i1=0;i1<24;i1++)
					monomeLedBuffer[64 + ((32 + levels_scales[ch_edit-1][i1]) & 0x3f)] = 3;
			}
			// all semis
			else {
				for(i2=0;i2<42;i2++)
					monomeLedBuffer[64 + ((32 + i2) & 0x3f)] = 3;
			}

			// note octave markers
			monomeLedBuffer[64 + ((32 + 0) & 0x3f)] = 7;
			monomeLedBuffer[64 + ((32 + 12) & 0x3f)] = 7;
			monomeLedBuffer[64 + ((32 + 24) & 0x3f)] = 7;
			monomeLedBuffer[64 + ((32 + 36) & 0x3f)] = 7;

			// octave
			monomeLedBuffer[128 + 41 + 0 * 3] = 3;
			monomeLedBuffer[128 + 41 + 1 * 3] = 3;
			monomeLedBuffer[128 + 41 + 2 * 3] = 3;
			monomeLedBuffer[128 + 41 + 3 * 3] = 3;
			monomeLedBuffer[128 + 41 + 4 * 3] = 3;
			monomeLedBuffer[128 + 41 + 5 * 3] = 3;

			monomeLedBuffer[128 + 40 + l.octave[ch_edit-1] * 3 + 0] = 15;
			monomeLedBuffer[128 + 40 + l.octave[ch_edit-1] * 3 + 1] = 15;
			monomeLedBuffer[128 + 40 + l.octave[ch_edit-1] * 3 + 2] = 15;
		}
		else {
			// range
			for(i1=1;i1<11;i1++) {
				monomeLedBuffer[64 + 40 + i1] = 3;
				if(l.range[ch_edit-1] >= i1)
					monomeLedBuffer[64 + 40 + i1] +=3;
			}

			// offset
			monomeLedBuffer[128 + 39] = 3;
			monomeLedBuffer[128 + 0] = 3;
			monomeLedBuffer[128 + 24] = 3;

			i1 = (l.offset[ch_edit-1] * 3) >> 2;
			i1 = (i1 + 640) & 0x3ff;
			arc_draw_point(2, i1);
		}
		
		// slew
		for(i2=0;i2 < 1 + ((l.slew[ch_edit-1] * 3) >> 8);i2++)
			monomeLedBuffer[192 + ((40 + i2) & 0x3f)] += 3;
	}
	else {
		if(!mixed_modes) {
			if(l.mode[0]) {
				// scale
				// show note map
				if(l.scale[0]) {
					for(i1=0;i1<24;i1++)
						monomeLedBuffer[64 + ((32 + levels_scales[0][i1]) & 0x3f)] = 3;
				}
				// all semis
				else {
					for(i2=0;i2<42;i2++)
						monomeLedBuffer[64 + ((32 + i2) & 0x3f)] = 3;
				}

				// note octave markers
				monomeLedBuffer[64 + ((32 + 0) & 0x3f)] = 7;
				monomeLedBuffer[64 + ((32 + 12) & 0x3f)] = 7;
				monomeLedBuffer[64 + ((32 + 24) & 0x3f)] = 7;
				monomeLedBuffer[64 + ((32 + 36) & 0x3f)] = 7;

				// octave
				monomeLedBuffer[128 + 41 + 0 * 3] = 3;
				monomeLedBuffer[128 + 41 + 1 * 3] = 3;
				monomeLedBuffer[128 + 41 + 2 * 3] = 3;
				monomeLedBuffer[128 + 41 + 3 * 3] = 3;
				monomeLedBuffer[128 + 41 + 4 * 3] = 3;
				monomeLedBuffer[128 + 41 + 5 * 3] = 3;

				monomeLedBuffer[128 + 40 + l.octave[0] * 3 + 0] += 3;
				monomeLedBuffer[128 + 40 + l.octave[0] * 3 + 1] += 3;
				monomeLedBuffer[128 + 40 + l.octave[0] * 3 + 2] += 3;
				monomeLedBuffer[128 + 40 + l.octave[1] * 3 + 0] += 3;
				monomeLedBuffer[128 + 40 + l.octave[1] * 3 + 1] += 3;
				monomeLedBuffer[128 + 40 + l.octave[1] * 3 + 2] += 3;
				monomeLedBuffer[128 + 40 + l.octave[2] * 3 + 0] += 3;
				monomeLedBuffer[128 + 40 + l.octave[2] * 3 + 1] += 3;
				monomeLedBuffer[128 + 40 + l.octave[2] * 3 + 2] += 3;
				monomeLedBuffer[128 + 40 + l.octave[3] * 3 + 0] += 3;
				monomeLedBuffer[128 + 40 + l.octave[3] * 3 + 1] += 3;
				monomeLedBuffer[128 + 40 + l.octave[3] * 3 + 2] += 3;
			}
			else {
				// range
				for(i1=1;i1<11;i1++) {
					monomeLedBuffer[64 + 40 + i1] = 3;
					for(i2=0;i2<4;i2++)
						if(l.range[i2] >= i1)
							monomeLedBuffer[64 + 40 + i1] +=3;
				}

				// offset
				monomeLedBuffer[128 + 39] = 3;
				monomeLedBuffer[128 + 0] = 3;
				monomeLedBuffer[128 + 24] = 3;

				i1 = (l.offset[0] * 3) >> 2;
				i1 = (i1 + 640) & 0x3ff;
				arc_draw_point_dark(2, i1);
				i1 = (l.offset[1] * 3) >> 2;
				i1 = (i1 + 640) & 0x3ff;
				arc_draw_point_dark(2, i1);
				i1 = (l.offset[2] * 3) >> 2;
				i1 = (i1 + 640) & 0x3ff;
				arc_draw_point_dark(2, i1);
				i1 = (l.offset[3] * 3) >> 2;
				i1 = (i1 + 640) & 0x3ff;
				arc_draw_point_dark(2, i1);

			}

		}

		// slew
		for(i1=0;i1<4;i1++) {
			for(i2=0;i2 < 1 + ((l.slew[i1] * 3) >> 8);i2++)
				monomeLedBuffer[192 + ((40 + i2) & 0x3f)] += 3;
		}
	}


}


void handler_LevelsKey(s32 data) { 
	// print_dbg("\r\n> levels key ");
	// print_dbg_ulong(data);

	switch(data) {
	// key 1 UP
	case 0:
		if(key_count_arc[0]) {
			key_count_arc[0] = 0;
			// SHORT PRESS
			if(mode == 2)
				ch_edit = (ch_edit + 1) % 5;
			else {
				levels_pattern_next();
				if(!ext_clock) {
					play = l.now;
					levels_dac_refresh();
				}
			}
			monomeFrameDirty++;
		}
		else if(mode == 1) {
			if(pattern_read != -1) {
				l.now = pattern_read;
				if(!ext_clock) {
					play = l.now;
					levels_dac_refresh();
				}
			}
			else if(pattern_write != -1) {
				l.pattern[0][pattern_write] = l.pattern[0][l.now];
				l.pattern[1][pattern_write] = l.pattern[1][l.now];
				l.pattern[2][pattern_write] = l.pattern[2][l.now];
				l.pattern[3][pattern_write] = l.pattern[3][l.now];
				l.note[0][pattern_write] = l.note[0][l.now];
				l.note[1][pattern_write] = l.note[1][l.now];
				l.note[2][pattern_write] = l.note[2][l.now];
				l.note[3][pattern_write] = l.note[3][l.now];
				l.now = pattern_write;
			}
			mode = 0;
			arc_refresh = &refresh_levels;
			monomeFrameDirty++;
		}
		break;
	// key 1 DOWN
	case 1:
		key_count_arc[0] = KEY_HOLD_TIME;	
		break;
	// key 2 UP
	case 2:
		if(key_count_arc[1]) {
			key_count_arc[1] = 0;

			if(mode == 0) {
				l.now = l.start;
				if(!ext_clock) {
					play = l.now;
					levels_dac_refresh();
				}
				monomeFrameDirty++;
			}
		}
		else if(mode == 2) {
			mode = 0;
			arc_refresh = &refresh_levels;
			monomeFrameDirty++;
		}
		break;
	// key 2 DOWN
	case 3:
		key_count_arc[1] = KEY_HOLD_TIME;
		break;
	default:
		break;
	}
}

static void key_long_levels(uint8_t key) {
	print_dbg("\r\nLONG PRESS >>>>>>> ");
	print_dbg_ulong(key);

	if(key == 0) {
		if(mode == 0) {
			mode = 1;
			pattern_read = -1;
			pattern_write = -1;
			arc_refresh = &refresh_levels_change;
			enc_count[0] = (l.now << 4) + 8;
			enc_count[1] = (l.now << 4) + 8;
			enc_count[2] = (l.start << 4) + 8;
			if(l.dir)
				enc_count[3] = ((17 - l.len) << 4) + 8;
			else
				enc_count[3] = ((l.len - 1) << 4) + 8;
			monomeFrameDirty++;
		}
	}
	else if(key == 1) {
		if(mode == 0) {
			mode = 2;
			enc_count[0] = 0;
			enc_count[1] = 0;
			enc_count[2] = 0;
			enc_count[3] = 0;
			arc_refresh = &refresh_levels_config;
			monomeFrameDirty++;
		}
	}
}

void handler_LevelsTr(s32 data) { 
	// print_dbg("\r\n> levels tr ");
	// print_dbg_ulong(data);

	switch(data) {
	case 1:
		if(ext_reset) {
			play = l.start;
			ext_reset = false;
			monomeFrameDirty++;
		}
		else
			levels_play_next();
			levels_dac_refresh();
		break;
	case 3:
		ext_reset = true;
		break;
	default:
		break;
	}

}

void handler_LevelsTrNormal(s32 data) { 
	print_dbg("\r\n> levels tr normal ");
	print_dbg_ulong(data);

	if(data) 
		ext_clock = true;
	else {
		ext_clock = false;
		play = l.now;
		monomeFrameDirty++;
	}
}


static void levels_pattern_next() {
	if(l.dir) { 
			if(l.now == ((l.start - l.len + 1) & 0xf))
				l.now = l.start;
			else 
				l.now = (l.now - 1) & 0xf;
		}
		else {
			if(l.now == ((l.start + l.len - 1) & 0xf))
				l.now = l.start;
			else
				l.now = (l.now + 1) & 0xf;
		}
	monomeFrameDirty++;
}

static void levels_play_next() {
	if(l.dir) { 
		if(play == ((l.start - l.len + 1) & 0xf))
			play = l.start;
		else 
			play = (play - 1) & 0xf;
	}
	else {
		if(play == ((l.start + l.len - 1) & 0xf))
			play = l.start;
		else
			play = (play + 1) & 0xf;
	}
	monomeFrameDirty++;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void default_cycles() {
	flashc_memset32((void*)&(f.cycles_state.clock_period), 50, 4, true);
}

void clock_cycles(uint8_t phase) {
	if(phase)
		set_tr(TR1);
	else
		clr_tr(TR1);
}

void ii_cycles(uint8_t *d, uint8_t l) {
	;;
}

void handler_CyclesEnc(s32 data) { 
	uint8_t n;
	int8_t delta;

	monome_ring_enc_parse_event_data(data, &n, &delta);

	print_dbg("\r\n cycles enc \tn: "); 
	print_dbg_ulong(n); 
	print_dbg("\t delta: "); 
	print_dbg_hex(delta);
}


void handler_CyclesRefresh(s32 data) { 
	if(monomeFrameDirty) {
		////
		for(uint8_t i1=0;i1<16;i1++) {
			monomeLedBuffer[i1] = 0;
			monomeLedBuffer[16+i1] = 0;
			monomeLedBuffer[32+i1] = 4;
			monomeLedBuffer[48+i1] = 0;
		}

		monomeLedBuffer[0] = 15;

		////
		monome_set_quadrant_flag(0);
		monome_set_quadrant_flag(1);
		(*monome_refresh)();
	}
}

void handler_CyclesKey(s32 data) { 
	print_dbg("\r\n> cycles key ");
	print_dbg_ulong(data);
}

void handler_CyclesTr(s32 data) { 
	print_dbg("\r\n> cycles tr ");
	print_dbg_ulong(data);
}

void handler_CyclesTrNormal(s32 data) { 
	print_dbg("\r\n> cycles tr normal ");
	print_dbg_ulong(data);
}

static void key_long_cycles(uint8_t key) {
	print_dbg("\r\nLONG PRESS >>>>>>> ");
	print_dbg_ulong(key);
}





static void arc_draw_point(uint8_t n, uint16_t p) {
	int c;

	c = p / 16;

	monomeLedBuffer[(n * 64) + c] = 15;
	monomeLedBuffer[(n * 64) + ((c + 1) % 64)] = p % 16;
	monomeLedBuffer[(n * 64) + ((c + 63) % 64)] = 15 - (p % 16);
}

static void arc_draw_point_dark(uint8_t n, uint16_t p) {
	int c;

	c = p / 16;

	monomeLedBuffer[(n * 64) + c] = 7;
	monomeLedBuffer[(n * 64) + ((c + 1) % 64)] = (p % 16) >> 1;
	monomeLedBuffer[(n * 64) + ((c + 63) % 64)] = (15 - (p % 16)) >> 1;
}

static uint8_t calc_note(uint8_t s, uint8_t i) {
	uint8_t n, r;
	r = 0;
	for(n=0;n<i;n++)
		r += SCALE_INT[s][n];
	return r;
}

static void generate_scales(uint8_t n) {
	levels_scales[n][0] = 0;
	for(uint8_t i1=0;i1<24;i1++) {
		levels_scales[n][i1+1] = levels_scales[n][i1] + SCALE_INT[l.scale[n]-1][i1 % 7];
	}
}

static void levels_dac_refresh(void) {
	if(l.mode[0]) {
		if(l.scale[0])
			dac_set_value(0, ET[ levels_scales[0][ l.note[0][play] ] ] << 2);
		else
			dac_set_value(0, ET[ l.note[0][play] ] << 2);
	}
	else
		dac_set_value(0, l.pattern[0][play] << 4);
	if(l.mode[1]) {
		if(l.scale[1])
			dac_set_value(1, ET[ levels_scales[1][ l.note[1][play] ] ] << 2);
		else
			dac_set_value(1, ET[ l.note[1][play] ] << 2);
	}
	else
		dac_set_value(1, l.pattern[1][play] << 4);
	if(l.mode[2]) {
		if(l.scale[2])
			dac_set_value(2, ET[ levels_scales[2][ l.note[2][play] ] ] << 2);
		else
			dac_set_value(2, ET[ l.note[2][play] ] << 2);
	}
	else
		dac_set_value(2, l.pattern[2][play] << 4);
	if(l.mode[3]) {
		if(l.scale[3])
			dac_set_value(3, ET[ levels_scales[3][ l.note[3][play] ] ] << 2);
		else
			dac_set_value(3, ET[ l.note[3][play] ] << 2);
	}
	else
		dac_set_value(3, l.pattern[3][play] << 4);
}
