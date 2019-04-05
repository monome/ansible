#include <string.h> //memset
#include <stdlib.h> //abs

#include "print_funcs.h"
#include "flashc.h"
#include "gpio.h"

#include "dac.h"
#include "monome.h"
#include "i2c.h"
#include "music.h"
#include "ii.h"
#include "i2c.h"
#include "init_common.h"

#include "main.h"
#include "ansible_arc.h"

static uint8_t arc_preset_mode;
static uint8_t arc_preset;
static uint8_t arc_preset_select;

static int16_t enc_count[4];

void (*arc_refresh)(void);



////////////////////////////////////////////////////////////////////////////////
// LEVELS

static void levels_pattern_next(void);
static void levels_play_next(void);

static void levels_dac_refresh(void);

static void levels_timer_volt0(void* o);
static void levels_timer_volt1(void* o);
static void levels_timer_volt2(void* o);
static void levels_timer_volt3(void* o);
static void levels_timer_note0(void* o);
static void levels_timer_note1(void* o);
static void levels_timer_note2(void* o);
static void levels_timer_note3(void* o);

static levels_data_t l;
static cycles_data_t c;

static uint8_t mode;
static uint8_t mode_config;
static uint8_t play;
static uint8_t pattern_pos;
static uint8_t pattern_pos_play;
static int8_t pattern_read;
static int8_t pattern_write;
static uint8_t levels_scales[4][29];
static bool tr_state[4];
static uint16_t tr_time[4];
static int16_t tr_time_pw[4];

#define LEVELS_CM_MODE 0
#define LEVELS_CM_RANGE 1
#define LEVELS_CM_OFFSET 2
#define LEVELS_CM_SLEW 3

////////////////////////////////////////////////////////////////////////////////
// CYCLES



void set_mode_arc(void) {
	switch(ansible_mode) {
	case mArcLevels:
		// print_dbg("\r\n> mode arc levels");
		app_event_handlers[kEventKey] = &handler_LevelsKey;
		app_event_handlers[kEventTr] = &handler_LevelsTr;
		app_event_handlers[kEventTrNormal] = &handler_LevelsTrNormal;
		app_event_handlers[kEventMonomeRingEnc] = &handler_LevelsEnc;
		app_event_handlers[kEventMonomeRefresh] = &handler_LevelsRefresh;
		clock = &clock_null;
		// clock = &clock_levels;
		// clock_set(f.levels_state.clock_period);
		init_i2c_slave(II_LV_ADDR);
		process_ii = &ii_levels;
		resume_levels();
		update_leds(1);
		break;
	case mArcCycles:
		// print_dbg("\r\n> mode arc cycles");
		app_event_handlers[kEventKey] = &handler_CyclesKey;
		app_event_handlers[kEventTr] = &handler_CyclesTr;
		app_event_handlers[kEventTrNormal] = &handler_CyclesTrNormal;
		app_event_handlers[kEventMonomeRingEnc] = &handler_CyclesEnc;
		app_event_handlers[kEventMonomeRefresh] = &handler_CyclesRefresh;
		clock = &clock_cycles;
		// 24
		clock_set(DAC_RATE_CV << 3);
		init_i2c_slave(II_CY_ADDR);
		process_ii = &ii_cycles;
		resume_cycles();
		update_leds(2);
		break;
	default:
		break;
	}

	// if(connected == conARC) {
	// 	app_event_handlers[kEventFrontShort] = &handler_ArcFrontShort;
	// 	app_event_handlers[kEventFrontLong] = &handler_ArcFrontLong;
	// }

	flashc_memset32((void*)&(f.state.none_mode), ansible_mode, 4, true);
	flashc_memset32((void*)&(f.state.arc_mode), ansible_mode, 4, true);
}


static inline void arc_leave_preset(void) {
	arc_preset_mode = 0;

	switch(ansible_mode) {
	case mArcLevels:
		mode = 0;
		app_event_handlers[kEventMonomeRingEnc] = &handler_LevelsEnc;
		app_event_handlers[kEventKey] = &handler_LevelsKey;
		arc_refresh = &refresh_levels;
		break;
	case mArcCycles:

		app_event_handlers[kEventMonomeRingEnc] = &handler_CyclesEnc;
		app_event_handlers[kEventKey] = &handler_CyclesKey;
		mode = 0;
		arc_refresh = &refresh_cycles;
		break;
	default:
		break;
	}
}

void handler_ArcFrontShort(s32 data) {
	// print_dbg("\r\n> PRESET ");
	// print_dbg_ulong(arc_preset);
	if(arc_preset_mode) {
		arc_leave_preset();
	}
	else {
		enc_count[0] = 0;
		enc_count[1] = 0;
		enc_count[2] = 0;
		enc_count[3] = 0;

		arc_preset_mode = 1;
		arc_preset_select = arc_preset;

		app_event_handlers[kEventKey] = &handler_ArcPresetKey;
		app_event_handlers[kEventMonomeRingEnc] = &handler_ArcPresetEnc;
		arc_refresh = &refresh_arc_preset;
	}

	monomeFrameDirty++;
}

void handler_ArcFrontLong(s32 data) {
	if(ansible_mode == mArcLevels)
		set_mode(mArcCycles);
	else
		set_mode(mArcLevels);
}



static void key_long_levels(uint8_t key);
static void key_long_cycles(uint8_t key);

static void generate_scales(uint8_t n);

static void arc_draw_point(uint8_t n, uint16_t p);
static void arc_draw_point_dark(uint8_t n, uint16_t p);

static bool ext_clock;
static bool ext_reset;

uint8_t key_count_arc[2];

// https://en.wikipedia.org/wiki/Triangular_number
const uint8_t delta_acc[16] = {0, 1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 66, 78, 91, 105, 120};


void arc_keytimer(void) {
	if(key_count_arc[0]) {
		if(key_count_arc[0] == 1) {
			switch(ansible_mode) {
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
			switch(ansible_mode) {
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

void handler_ArcPresetEnc(s32 data) {
	uint8_t n;
	int8_t delta;
	int16_t i;

	monome_ring_enc_parse_event_data(data, &n, &delta);

	enc_count[n] += delta;
	i = enc_count[n] >> 4;
	enc_count[n] -= i << 4;

	if(i) {
		arc_preset_select = (arc_preset_select + i) & 0x7;
		monomeFrameDirty++;
	}
}

void handler_ArcPresetKey(s32 data) {
	switch(data) {
	case 1:
		arc_preset = arc_preset_select;
		// print_dbg("\r\nread preset: ");
		// print_dbg_ulong(arc_preset);
		switch(ansible_mode) {
		case mArcLevels:
			flashc_memset8((void*)&(f.levels_state.preset), arc_preset, 1, true);
			init_levels();
			arc_leave_preset();
			resume_levels();
			break;
		case mArcCycles:
			flashc_memset8((void*)&(f.cycles_state.preset), arc_preset, 1, true);
			init_cycles();
			arc_leave_preset();
			resume_cycles();
			break;
		default:
			break;
		}
		break;
	case 3:
		arc_preset = arc_preset_select;
		// print_dbg("\r\nwrite preset: ");
		// print_dbg_ulong(arc_preset);
		switch(ansible_mode) {
		case mArcLevels:
			flashc_memcpy((void *)&f.levels_state.l[arc_preset], &l, sizeof(l), true);
			flashc_memset8((void*)&(f.levels_state.preset), arc_preset, 1, true);
			arc_leave_preset();
			resume_levels();
			break;
		case mArcCycles:
			flashc_memcpy((void *)&f.cycles_state.c[arc_preset], &c, sizeof(c), true);
			flashc_memset8((void*)&(f.cycles_state.preset), arc_preset, 1, true);
			arc_leave_preset();
			resume_cycles();
			break;
		default:
			break;
		}

		break;
	default:
		break;
	}
}

void refresh_arc_preset(void) {
	uint16_t i1;
	for(i1=0;i1<256;i1++) {
		monomeLedBuffer[i1] = 0;
	}

	for(i1=0;i1<8;i1++) {
		monomeLedBuffer[arc_preset * 8 + i1] = 7;
		monomeLedBuffer[arc_preset_select * 8 + i1] = 15;
	}
}




////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static inline uint16_t get_tr_time(uint8_t n) {
	return ((0x3ff - l.pattern[n][play]) >> (2 - l.range[n])) + 40;
}

static inline int16_t get_tr_time_pw(uint8_t n) {
	int i;
	i = ((l.offset[n] >> 6) - 8);
	if(i<0) i++;
	return i * (tr_time[n] >> 3);
}

void default_levels() {
	uint8_t i1,i2;

	l.now = 0;
	l.start = 0;
	l.len = 3;
	l.dir = 0;

	for(i1=0;i1<4;i1++) {
		l.mode[i1] = 0;
		l.all[i1] = 0;
		l.scale[i1] = 0;
		l.octave[i1] = 0;
		l.offset[i1] = 0;
		l.range[i1] = 2;
		l.slew[i1] = 20;
		for(i2=0;i2<16;i2++) {
			l.pattern[i1][i2] = 0;
			l.note[i1][i2] = 0;
		}
	}

	for(i1=0;i1<8;i1++)
		flashc_memcpy((void *)&f.levels_state.l[i1], &l, sizeof(l), true);
	flashc_memset8((void*)&(f.levels_state.preset), arc_preset, 1, true);
	// flashc_memcpy_memset32((void*)&(f.levels_state.clock_period), 250, 4, true);
}

void init_levels() {
	uint8_t i1, i2;

	arc_preset = f.levels_state.preset;

	l.now = f.levels_state.l[arc_preset].now;
	l.start = f.levels_state.l[arc_preset].start;
	l.len = f.levels_state.l[arc_preset].len;
	l.dir = f.levels_state.l[arc_preset].dir;

	for(i1=0;i1<4;i1++) {
		for(i2=0;i2<16;i2++) {
			l.pattern[i1][i2] = f.levels_state.l[arc_preset].pattern[i1][i2];
			l.note[i1][i2] = f.levels_state.l[arc_preset].note[i1][i2];
		}

		l.mode[i1] = f.levels_state.l[arc_preset].mode[i1];
		l.all[i1] = f.levels_state.l[arc_preset].all[i1];
		l.scale[i1] = f.levels_state.l[arc_preset].scale[i1];
		l.octave[i1] = f.levels_state.l[arc_preset].octave[i1];
		l.offset[i1] = f.levels_state.l[arc_preset].offset[i1];
		l.range[i1] = f.levels_state.l[arc_preset].range[i1];
		l.slew[i1] = f.levels_state.l[arc_preset].slew[i1];
	}
}

void resume_levels() {
	uint8_t i1;

	mode = 0;
	mode_config = 0;
	pattern_pos = l.now;
	pattern_pos_play = l.now;
	play = l.now;

	arc_refresh = &refresh_levels;

	arc_preset = f.levels_state.preset;

	for(i1=0;i1<4;i1++) {
		dac_set_slew(i1,l.slew[i1]);
		generate_scales(i1);
		tr_state[i1] = 0;
	}

	if(!l.mode[0])
		timer_add(&auxTimer[0], 1, &levels_timer_volt0, NULL );
	if(!l.mode[1])
		timer_add(&auxTimer[1], 1, &levels_timer_volt1, NULL );
	if(!l.mode[2])
		timer_add(&auxTimer[2], 1, &levels_timer_volt2, NULL );
	if(!l.mode[3])
		timer_add(&auxTimer[3], 1, &levels_timer_volt3, NULL );

	key_count_arc[0] = 0;
	key_count_arc[1] = 0;

	ext_clock = !gpio_get_pin_value(B10);
	ext_reset = false;

	// reset_dacs();
	levels_dac_refresh();

	monomeFrameDirty++;

	// print_dbg("\r\nresume levels");
}

static void levels_timer_volt0(void* o) {
	if(tr_state[0]) {
		clr_tr(TR1);
		timer_reset_set(&auxTimer[0], tr_time[0] - tr_time_pw[0]);
	}
	else {
		set_tr(TR1);
		tr_time[0] = get_tr_time(0);
		tr_time_pw[0] = get_tr_time_pw(0);
		timer_reset_set(&auxTimer[0], tr_time[0] + tr_time_pw[0]);
	}

	tr_state[0] ^= 1;
}

static void levels_timer_volt1(void* o) {
	if(tr_state[1]) {
		clr_tr(TR2);
		timer_reset_set(&auxTimer[1], tr_time[1] - tr_time_pw[1]);
	}
	else {
		tr_time[1] = get_tr_time(1);
		tr_time_pw[1] = get_tr_time_pw(1);
		set_tr(TR2);
		timer_reset_set(&auxTimer[1], tr_time[1] + tr_time_pw[1]);
	}

	tr_state[1] ^= 1;
}

static void levels_timer_volt2(void* o) {
	if(tr_state[2]) {
		clr_tr(TR3);
		timer_reset_set(&auxTimer[2], tr_time[2] - tr_time_pw[2]);
	}
	else {
		tr_time[2] = get_tr_time(2);
		tr_time_pw[2] = get_tr_time_pw(2);
		set_tr(TR3);
		timer_reset_set(&auxTimer[2], tr_time[2] + tr_time_pw[2]);
	}

	tr_state[2] ^= 1;
}

static void levels_timer_volt3(void* o) {
	if(tr_state[3]) {
		clr_tr(TR4);
		timer_reset_set(&auxTimer[3], tr_time[3] - tr_time_pw[3]);
	}
	else {
		tr_time[3] = get_tr_time(3);
		tr_time_pw[3] = get_tr_time_pw(3);
		set_tr(TR4);
		timer_reset_set(&auxTimer[3], tr_time[3] + tr_time_pw[3]);
	}

	tr_state[3] ^= 1;
}

static void levels_timer_note0(void* o) {
	timer_remove( &auxTimer[0]);
	clr_tr(TR1);
	tr_state[0] = 0;
}

static inline void tr_note0(void) {
	if(tr_state[0] == 0) {
		timer_add(&auxTimer[0], l.offset[0] + 10, &levels_timer_note0, NULL );
		set_tr(TR1);
		tr_state[0] = 1;
	}
	else
		timer_reset_set(&auxTimer[0], l.offset[0] + 10);
}

static void levels_timer_note1(void* o) {
	timer_remove( &auxTimer[1]);
	clr_tr(TR2);
	tr_state[1] = 0;
}

static inline void tr_note1(void) {
	if(tr_state[1] == 0) {
		timer_add(&auxTimer[1], l.offset[1] + 10, &levels_timer_note1, NULL );
		set_tr(TR2);
		tr_state[1] = 1;
	}
	else
		timer_reset_set(&auxTimer[1], l.offset[1] + 10);
}

static void levels_timer_note2(void* o) {
	timer_remove( &auxTimer[2]);
	clr_tr(TR3);
	tr_state[2] = 0;
}

static inline void tr_note2(void) {
	if(tr_state[2] == 0) {
		timer_add(&auxTimer[2], l.offset[2] + 10, &levels_timer_note2, NULL );
		set_tr(TR3);
		tr_state[2] = 1;
	}
	else
		timer_reset_set(&auxTimer[2], l.offset[2] + 10);
}

static void levels_timer_note3(void* o) {
	timer_remove( &auxTimer[3]);
	clr_tr(TR4);
	tr_state[3] = 0;
}

static inline void tr_note3(void) {
	if(tr_state[3] == 0) {
		timer_add(&auxTimer[3], l.offset[3] + 10, &levels_timer_note3, NULL );
		set_tr(TR4);
		tr_state[3] = 1;
	}
	else
		timer_reset_set(&auxTimer[3], l.offset[3] + 10);
}

void clock_levels(uint8_t phase) {
	;;
	// if(phase)
	// 	set_tr(TR1);
	// else
	// 	clr_tr(TR1);
}

void ii_levels(uint8_t *d, uint8_t len) {
	if(len) {
		switch(d[0]) {
		case II_LV_PRESET:
			if(d[1] > -1 && d[1] < 8) {
				arc_preset = d[1];
				flashc_memset8((void*)&(f.levels_state.preset), arc_preset, 1, true);
				init_levels();
				monomeFrameDirty++;
			}
			break;
		case II_LV_PRESET + II_GET:
			ii_tx_queue(arc_preset);
			break;
		case II_LV_POS:
			if(d[1] > -1 && d[1] < 16) {
				pattern_pos_play = d[1] % l.len;
				if(l.dir)
					play = (16 + l.start - pattern_pos_play) & 0xf;
				else
					play = (pattern_pos_play + l.start) & 0xf;
				levels_dac_refresh();
				if(l.mode[0])
					tr_note0();
				if(l.mode[1])
					tr_note1();
				if(l.mode[2])
					tr_note2();
				if(l.mode[3])
					tr_note3();
				monomeFrameDirty++;
			}
			break;
		case II_LV_POS + II_GET:
			ii_tx_queue(play);
			break;
		case II_LV_L_ST:
			if(d[1] > -1 && d[1] < 16) {
				l.start = d[1];
				monomeFrameDirty++;
			}
			break;
		case II_LV_L_ST + II_GET:
			ii_tx_queue(l.start);
			break;
		case II_LV_L_LEN:
			if(d[1] > 0 && d[1] < 17) {
				l.len = d[1];
				monomeFrameDirty++;
			}
			break;
		case II_LV_L_LEN + II_GET:
			ii_tx_queue(l.len);
			break;
		case II_LV_L_DIR:
			if(d[1] > -1 && d[1] < 2) {
				l.dir = d[1];
				monomeFrameDirty++;
			}
			break;
		case II_LV_L_DIR + II_GET:
			ii_tx_queue(l.dir);
			break;
		case II_LV_RESET:
			if(d[0] == 0)
				ext_reset = true;
			else {
				play = l.start;
				levels_dac_refresh();
				if(l.mode[0])
					tr_note0();
				if(l.mode[1])
					tr_note1();
				if(l.mode[2])
					tr_note2();
				if(l.mode[3])
					tr_note3();
				monomeFrameDirty++;
			}
			break;
		case II_LV_CV + II_GET:
			if (d[1] > 3) {
				ii_tx_queue(0);
				ii_tx_queue(0);
				break;
			}
			ii_tx_queue(dac_get_value(d[1]) >> 8);
			ii_tx_queue(dac_get_value(d[1]) & 0xff);
			break;
		default:
			break;
		}
	}
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
				else if(i>28 && l.scale[n])
					i=28;
				else if(i>48 && !l.scale[n])
					i=48;

				l.note[n][l.now] = i;

				if(l.all[n])
					memset(l.note[n], i, 16);

				if(!ext_clock)
					levels_dac_refresh();
			}
		}
		// VOLT
		else {
			if(delta > 15) delta = 15;
			if(delta < -15) delta = -15;
			if(delta > 0)
				i = l.pattern[n][l.now] + delta_acc[delta];
			else
				i = l.pattern[n][l.now] - delta_acc[-delta];
			if(i < 0)
				i = 0;
			else if(i > 0x3ff)
				i = 0x3ff;
			l.pattern[n][l.now] = i;

			if(l.all[n])
				for(uint8_t i1=0;i1<16;i1++)
					l.pattern[n][i1] = i;

			// print_dbg("\r\n time: ");
			// print_dbg_ulong(tr_time[n] + tr_time_pw[n]);
			// print_dbg(" / ");
			// print_dbg_ulong(tr_time[n] - tr_time_pw[n]);
			// print_dbg(" ---- ");
			// print_dbg_ulong(tr_time[n]);


			// print_dbg("\r\ntr timer: ");
			// print_dbg_ulong(((0x3ff - l.pattern[n][l.now]) >> (2 - l.range[n])) + 10);

			if(!ext_clock)
				levels_dac_refresh();
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
		switch(mode_config) {
		// volt/note mode
		case LEVELS_CM_MODE:
			enc_count[n] += delta;
			i = enc_count[n] >> 6;
			enc_count[n] -= i << 6;

			if(i) {
				if(l.mode[n] == 0) {
					if(i < 0) {
						l.all[n] = 1;
					}
					else if(l.all[n]) {
						l.all[n] = 0;
					}
					else {
						l.mode[n] = 1;
					}
				}
				else {
					if(i > 0) {
						l.all[n] = 1;
					}
					else if(l.all[n]) {
						l.all[n] = 0;
					}
					else {
						l.mode[n] = 0;
					}
				}

				if(l.mode[n]) {
					timer_remove( &auxTimer[n]);
					clr_tr(TR1 + n);
					tr_state[n] = 0;
				}
				else {
					tr_state[n] = 0;
					set_tr(TR1 + n);

					switch(n) {
					case 0:
						timer_add(&auxTimer[0], tr_time[0] - tr_time_pw[0], &levels_timer_volt0, NULL );
						break;
					case 1:
						timer_add(&auxTimer[1], tr_time[1] - tr_time_pw[1], &levels_timer_volt1, NULL );
						break;
					case 2:
						timer_add(&auxTimer[2], tr_time[2] - tr_time_pw[2], &levels_timer_volt2, NULL );
						break;
					case 3:
						timer_add(&auxTimer[3], tr_time[3] - tr_time_pw[3], &levels_timer_volt3, NULL );
						break;
					}
				}
			}
			break;
		// range / scale
		case LEVELS_CM_RANGE:
			enc_count[n] += delta;
			i = enc_count[n] >> 4;
			enc_count[n] -= i << 4;

			if(l.mode[n] == 0) {
				// range
				if(i) {
					i += l.range[n];
					if(i < 0) i = 0;
					else if(i > 2) i = 2;

					l.range[n] = i;
				}
			}
			else {
				// scale
				if(i) {
					i += l.scale[n];
					if(i < 0) i = 0;
					else if(i > 7) i = 7;

					l.scale[n] = i;

					generate_scales(n);
				}
			}

			if(!ext_clock)
				levels_dac_refresh();
			break;
		// offset
		case LEVELS_CM_OFFSET:
			// volt offset
			if(l.mode[n] == 0) {
				if(delta > 15) delta = 15;
				if(delta < -15) delta = -15;
				if(delta > 0)
					i = l.offset[n] + delta_acc[delta];
				else
					i = l.offset[n] - delta_acc[-delta];
				if(i < 0)
					i = 0;
				else if(i > 0x3ff)
					i = 0x3ff;
				l.offset[n] = i;
			}
			// octave
			else {
				enc_count[2] += delta;
				i = enc_count[2] >> 4;
				enc_count[2] -= i << 4;

				i += l.octave[n];

				if(i < 0) i = 0;
				else if(i > 5) i = 5;

				l.octave[n] = i;
			}

			if(!ext_clock)
				levels_dac_refresh();

			break;
		// slew
		case LEVELS_CM_SLEW:
			if(delta > 15) delta = 15;
			if(delta < -15) delta = -15;
			if(delta > 0)
				i = delta_acc[delta];
			else
				i = -delta_acc[-delta];
			i += l.slew[n];

			if(i < 1) i = 1;
			else if(i > 2047) i = 2047;
			l.slew[n] = i;
			dac_set_slew(n, i);

			// print_dbg("\r\nslew: ");
			// print_dbg_ulong(l.slew[n]);
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
	memset(monomeLedBuffer,0,MONOME_MAX_LED_BYTES);

	for(i1=0;i1<4;i1++) {
		if(l.mode[i1]) {
			// note map
			if(l.scale[i1]) {
				// show note map
				for(i2=0;i2<29;i2++) {
					monomeLedBuffer[i1*64 + ((32 + levels_scales[i1][i2]) & 0x3f)] = 3;
				}
				monomeLedBuffer[i1*64 + ((32 + 0) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 12) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 24) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 36) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 48) & 0x3f)] = 7;
				// show note
				monomeLedBuffer[i1*64 + ((32 + levels_scales[i1][l.note[i1][play]]) & 0x3f)] = 10;
				monomeLedBuffer[i1*64 + ((32 + levels_scales[i1][l.note[i1][l.now]]) & 0x3f)] = 15;
			}
			// all semitones
			else {
				for(i2=0;i2<48;i2++)
					monomeLedBuffer[i1*64 + ((32 + i2) & 0x3f)] = 3;
				monomeLedBuffer[i1*64 + ((32 + 0) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 12) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 24) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 36) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 48) & 0x3f)] = 7;
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
	memset(monomeLedBuffer,0,MONOME_MAX_LED_BYTES);

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
	uint16_t i1, i2, i3;
	memset(monomeLedBuffer,0,MONOME_MAX_LED_BYTES);

	switch(mode_config) {
	case LEVELS_CM_MODE:
		for(i1=0;i1<4;i1++) {
			for(i2=0;i2<14;i2++) {
				if(l.mode[i1])
					monomeLedBuffer[i1*64 + 32 - i2] = (i2 & 1) * 7;
				else
					monomeLedBuffer[i1*64 + 32 - i2] = i2;
			}

			if(l.all[i1]) {
				monomeLedBuffer[i1*64 + 12] = 7;
				monomeLedBuffer[i1*64 + 13] = 7;
			}
		}
		break;
	case LEVELS_CM_RANGE:
		for(i1=0;i1<4;i1++) {
			// note scale
			if(l.mode[i1]) {
				// show note map
				if(l.scale[i1]) {
					for(i2=0;i2<29;i2++)
						monomeLedBuffer[i1*64 + ((32 + levels_scales[i1][i2]) & 0x3f)] = 3;
				}
				// all semis
				else {
					for(i2=0;i2<48;i2++)
						monomeLedBuffer[i1*64 + ((32 + i2) & 0x3f)] = 3;
				}

				// note octave markers
				monomeLedBuffer[i1*64 + ((32 + 0) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 12) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 24) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 36) & 0x3f)] = 7;
				monomeLedBuffer[i1*64 + ((32 + 48) & 0x3f)] = 7;
			}
			// volt range
			else {
				i2 = (48 >> (2-l.range[i1]));
				for(i3=0;i3<i2;i3++)
					monomeLedBuffer[i1*64 + ((40 + i3) & 0x3f)] = 3;
			}
		}
		break;

	case LEVELS_CM_OFFSET:
		for(i1=0;i1<4;i1++) {
			// note octave
			if(l.mode[i1]) {
				monomeLedBuffer[i1*64 + 41 + 0 * 3] = 3;
				monomeLedBuffer[i1*64 + 41 + 1 * 3] = 3;
				monomeLedBuffer[i1*64 + 41 + 2 * 3] = 3;
				monomeLedBuffer[i1*64 + 41 + 3 * 3] = 3;
				monomeLedBuffer[i1*64 + 41 + 4 * 3] = 3;
				monomeLedBuffer[i1*64 + 41 + 5 * 3] = 3;

				monomeLedBuffer[i1*64 + 40 + l.octave[i1] * 3 + 0] = 15;
				monomeLedBuffer[i1*64 + 40 + l.octave[i1] * 3 + 1] = 15;
				monomeLedBuffer[i1*64 + 40 + l.octave[i1] * 3 + 2] = 15;
			}
			// volt offset
			else {
				monomeLedBuffer[i1*64 + 40] = 3;
				monomeLedBuffer[i1*64 + 0] = 3;
				monomeLedBuffer[i1*64 + 24] = 3;

				i2 = (l.offset[i1] * 3) >> 2;
				i2 = (i2 + 640) & 0x3ff;
				arc_draw_point(i1, i2);
			}
		}
		break;
	case LEVELS_CM_SLEW:
		// slew
		for(i1=0;i1<4;i1++) {
			i2 = (l.slew[i1] * 3) >> 3;
			i2 = (i2 + 640) & 0x3ff;
			arc_draw_point_dark(i1, i2);

			for(i2=0;i2<48;i2++)
				monomeLedBuffer[i1*64 + ((40 + i2) & 0x3f)] += 3;
		}
		break;
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
			if(mode == 2) {
				mode_config = (mode_config + 1) & 3;
				// print_dbg("\r\nmode: ");
				// print_dbg_ulong(mode_config);
				monomeFrameDirty++;
			}
			else {
				levels_pattern_next();
				if(!ext_clock) {
					play = l.now;
					levels_dac_refresh();

					if(l.mode[0])
						tr_note0();
					if(l.mode[1])
						tr_note1();
					if(l.mode[2])
						tr_note2();
					if(l.mode[3])
						tr_note3();
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

					if(l.mode[0])
						tr_note0();
					if(l.mode[1])
						tr_note1();
					if(l.mode[2])
						tr_note2();
					if(l.mode[3])
						tr_note3();
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
	// print_dbg("\r\nLONG PRESS >>>>>>> ");
	// print_dbg_ulong(key);

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
	pattern_pos = (pattern_pos + 1) % l.len;
	if(l.dir)
		l.now = (16 + l.start - pattern_pos) % 0xf;
	else
		l.now = (pattern_pos + l.start) & 0xf;

	monomeFrameDirty++;
}

static void levels_play_next() {
	pattern_pos_play = (pattern_pos_play + 1) % l.len;
	if(l.dir)
		play = (16 + l.start - pattern_pos_play) & 0xf;
	else
		play = (pattern_pos_play + l.start) & 0xf;

	if(l.mode[0])
		tr_note0();
	if(l.mode[1])
		tr_note1();
	if(l.mode[2])
		tr_note2();
	if(l.mode[3])
		tr_note3();

	monomeFrameDirty++;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// const uint8_t friction_map[33] = { 0, 4, 8, 12, 16, 20, 23, 26, 29, 32, 34, 36, 38, 40, 42, 44,
	// 46, 48, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64};

const uint8_t friction_map[25] = { 0, 12, 20, 26, 30, 34, 39, 42, 45,
	48, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64};

const uint16_t div_map[4] = {0x2000,0x1000,0x0800,0x0400};

static uint16_t friction;
static int8_t add_force[4];
static bool cycle_dir[4];

void default_cycles() {
	uint8_t i1;

	c.mode = 0;
	c.shape = 0;
	c.friction = 0;
	c.force = 1;

	for(i1=0;i1<4;i1++) {
		c.pos[i1] = 0;
		c.speed[i1] = 0;
		c.mult[i1] = 0;
		c.range[i1] = 64;
		c.div[i1] = 0;
	}

	for(i1=0;i1<8;i1++)
		flashc_memcpy((void *)&f.cycles_state.c[i1], &c, sizeof(c), true);

	flashc_memset32((void*)&(f.cycles_state.preset), 0, 4, true);

}

void init_cycles() {
	uint8_t i1;

	arc_preset = f.cycles_state.preset;

	c.mode = f.cycles_state.c[arc_preset].mode;
	c.shape = f.cycles_state.c[arc_preset].shape;
	c.friction = f.cycles_state.c[arc_preset].friction;
	c.force = f.cycles_state.c[arc_preset].force;

	for(i1=0;i1<4;i1++) {
		c.pos[i1] = f.cycles_state.c[arc_preset].pos[i1];
		c.speed[i1] = f.cycles_state.c[arc_preset].speed[i1];
		c.mult[i1] = f.cycles_state.c[arc_preset].mult[i1];
		c.range[i1] = f.cycles_state.c[arc_preset].range[i1];
		c.div[i1] = f.cycles_state.c[arc_preset].div[i1];
	}
}

void resume_cycles() {
	uint8_t i1;

	mode = 0;

	arc_refresh = &refresh_cycles;

	arc_preset = f.cycles_state.preset;

	for(i1=0;i1<4;i1++) {
		dac_set_slew(i1,DAC_RATE_CV << 3);
		tr_state[i1] = 0;
		cycle_dir[i1] = 1;
		add_force[i1] = 0;
	}

	key_count_arc[0] = 0;
	key_count_arc[1] = 0;

	friction = friction_map[24 - c.friction] + 192;

	ext_clock = !gpio_get_pin_value(B10);

	monomeFrameDirty++;
}



void clock_cycles(uint8_t phase) {
	uint8_t i1;

	if(c.mode) {
		c.speed[0] += add_force[0];

		if(c.mult[1] > -1)
			c.speed[1] = c.speed[0] >> c.mult[1];
		else
			c.speed[1] = c.speed[0] << -c.mult[1];

		if(c.mult[2] > -1)
			c.speed[2] = c.speed[0] >> c.mult[2];
		else
			c.speed[2] = c.speed[0] << -c.mult[2];

		if(c.mult[3] > -1)
			c.speed[3] = c.speed[0] >> c.mult[3];
		else
			c.speed[3] = c.speed[0] << -c.mult[3];
	}

	for(i1=0;i1<4;i1++) {
		if(c.mode == 0)
			c.speed[i1] += add_force[i1];
		c.pos[i1] = (c.pos[i1] + c.speed[i1]) & 0x3fff;
		c.speed[i1] = (c.speed[i1] * (friction)) / 256;
		// c.speed[i1] = (c.speed[i1] * (friction)) >> 8;

		if(c.pos[i1] & div_map[c.div[i1]])
			set_tr(TR1 + i1);
		else
			clr_tr(TR1 + i1);

		if(c.shape)
			dac_set_value(i1, (c.pos[i1] * c.range[i1]) >> 6);
			// dac_set_value(i1, c.pos[i1] << 2);
		else
			dac_set_value(i1, (abs(0x2000 - c.pos[i1]) << 1) * c.range[i1] >> 6);
	}

	monomeFrameDirty++;
}

void ii_cycles(uint8_t *d, uint8_t len) {
	if(len) {
		switch(d[0]) {
		case II_CY_PRESET:
			if(d[1] > -1 && d[1] < 8) {
				arc_preset = d[1];
				flashc_memset8((void*)&(f.cycles_state.preset), arc_preset, 1, true);
				init_cycles();
				monomeFrameDirty++;
			}
			break;
		case II_CY_PRESET + II_GET:
			ii_tx_queue(arc_preset);
			break;
		case II_CY_RESET:
			if(d[1] == 0) {
				c.pos[0] = 0;
				c.pos[1] = 0;
				c.pos[2] = 0;
				c.pos[3] = 0;
			}
			else if(d[1] < 5)
				c.pos[d[1]-1] = 0;
			break;
		case II_CY_POS:
			if(d[1] == 0) {
				c.pos[0] = d[2] << 6;
				c.pos[1] = d[2] << 6;
				c.pos[2] = d[2] << 6;
				c.pos[3] = d[2] << 6;
			}
			else if(d[1] < 5)
				c.pos[d[1]-1] = d[2] << 6;
			monomeFrameDirty++;
			break;
		case II_CY_POS + II_GET:
			if(d[1] == 0)
				ii_tx_queue(((c.pos[0] >> 6) + (c.pos[1] >> 6) + (c.pos[2] >> 6) + (c.pos[3] >> 6)) >> 2);
			if(d[1] < 5)
				ii_tx_queue(c.pos[d[1]-1] >> 6);
			break;
		case II_CY_REV:
			if(d[1] == 0) {
				c.speed[0] = -c.speed[0];
				c.speed[1] = -c.speed[1];
				c.speed[2] = -c.speed[2];
				c.speed[3] = -c.speed[3];
			}
			else if(d[1] < 5)
				c.speed[d[1]-1] = -c.speed[d[1]-1];
			break;
		case II_CY_CV + II_GET:
			if (d[1] > 3) {
				ii_tx_queue(0);
				ii_tx_queue(0);
				break;
			}
			ii_tx_queue(dac_get_value(d[1]) >> 8);
			ii_tx_queue(dac_get_value(d[1]) & 0xff);
			break;
		default:
			break;
		}
	}
}

#define MAX_SPEED 2000

void handler_CyclesEnc(s32 data) {
	uint8_t n;
	int16_t i;
	int8_t delta;
	int32_t s;

	monome_ring_enc_parse_event_data(data, &n, &delta);

	switch(mode) {
	case 0:
		// sync
		if(c.mode) {
			if(n == 0) {
				s = c.speed[0] + (delta << c.force);

				if(s > MAX_SPEED)
					s = MAX_SPEED;
				if(s < -MAX_SPEED)
					s = -MAX_SPEED;
				c.speed[0] = s;

				cycle_dir[0] = delta > 0;
				cycle_dir[1] = delta > 0;
				cycle_dir[2] = delta > 0;
				cycle_dir[3] = delta > 0;

				if(c.mult[1] > -1)
					c.speed[1] = c.speed[0] >> c.mult[1];
				else
					c.speed[1] = c.speed[0] << -c.mult[1];

				if(c.mult[2] > -1)
					c.speed[2] = c.speed[0] >> c.mult[2];
				else
					c.speed[2] = c.speed[0] << -c.mult[2];

				if(c.mult[3] > -1)
					c.speed[3] = c.speed[0] >> c.mult[3];
				else
					c.speed[3] = c.speed[0] << -c.mult[3];

			}
			else {
				enc_count[n] += delta;
				i = enc_count[n] >> 4;
				enc_count[n] -= i << 4;

				if(i) {
					i += c.mult[n];
					if(i < -4)
						i = -4;
					else if(i > 1)
						i = 1;
					c.mult[n] = i;

					if(i > -1)
						c.speed[n] = c.speed[0] >> i;
					else
						c.speed[n] = c.speed[0] << -i;
				}
			}
		}
		// free
		else {
			s = c.speed[n] + (delta << c.force);

			if(s > MAX_SPEED)
				s = MAX_SPEED;
			if(s < -MAX_SPEED)
				s = -MAX_SPEED;
			c.speed[n] = s;

			cycle_dir[n] = delta > 0;

			// print_dbg("\r\n");
			// print_dbg_ulong(n);
			// print_dbg(" ");
			// print_dbg_ulong(s);
		}
		break;

	case 1:
		switch(n) {
		// mode
		case 0:
			if(delta > 0)
				c.mode = 1;
			else
				c.mode = 0;
			break;
		// shape
		case 1:
			if(delta > 0)
				c.shape = 1;
			else
				c.shape = 0;
			break;
		// force
		case 2:
			enc_count[n] += delta;
			i = enc_count[n] >> 4;
			enc_count[n] -= i << 4;

			i += c.force;
			if(i < 1)
				i = 1;
			else if(i > 4)
				i = 4;
			c.force = i;
			break;
		// friction
		case 3:
			enc_count[n] += delta;
			i = enc_count[n] >> 4;
			enc_count[n] -= i << 4;

			if(i) {
				i += c.friction;
				if(i < 0)
					i = 0;
				else if(i > 24)
					i = 24;
				c.friction = i;

				friction = friction_map[24 - c.friction] + 192;

				// print_dbg("\r\nfriction: ");
				// print_dbg_ulong(friction);
			}

			break;
		}
		monomeFrameDirty++;
		break;
	case 2:
		// range
		enc_count[n] += delta;
		i = enc_count[n] >> 4;
		enc_count[n] -= i << 4;

		if(i) {
			i += c.range[n];
			if(i < 1)
				i = 1;
			else if(i > 64)
				i = 64;
			c.range[n] = i;
		}
		break;
	case 3:
		// div
		enc_count[n] += delta;
		i = enc_count[n] >> 6;
		enc_count[n] -= i << 6;

		if(i) {
			i += c.div[n];
			if(i < 0)
				i = 0;
			else if(i > 3)
				i = 3;
			c.div[n] = i;
		}
		break;
	default: break;
	}
}


void handler_CyclesRefresh(s32 data) {
	if(monomeFrameDirty) {
		arc_refresh();

		monome_set_quadrant_flag(0);
		monome_set_quadrant_flag(1);
		monome_set_quadrant_flag(2);
		monome_set_quadrant_flag(3);
		(*monome_refresh)();
	}
}

void handler_CyclesKey(s32 data) {
	// print_dbg("\r\n> cycles key ");
	// print_dbg_ulong(data);

	switch(data) {
	// key 1 UP
	case 0:
		if(key_count_arc[0]) {
			key_count_arc[0] = 0;
			// SHORT PRESS
			// add_force = c.force;
		}
		// LONG UP
		else {
			friction = friction_map[24 - c.friction] + 192;
		}
		break;
	// key 1 DOWN
	case 1:
		if(mode == 1) {
			mode = 2;
			enc_count[0] = 0;
			enc_count[1] = 0;
			enc_count[2] = 0;
			enc_count[3] = 0;
			arc_refresh = &refresh_cycles_config_range;
			monomeFrameDirty++;
		}
		else if(mode == 2) {
			mode = 3;
			enc_count[0] = 0;
			enc_count[1] = 0;
			enc_count[2] = 0;
			enc_count[3] = 0;
			arc_refresh = &refresh_cycles_config_div;
			monomeFrameDirty++;
		}
		else if(mode == 3){
			mode = 1;
			enc_count[0] = 0;
			enc_count[1] = 0;
			enc_count[2] = 0;
			enc_count[3] = 0;
			arc_refresh = &refresh_cycles_config;
			monomeFrameDirty++;
		} else {
			key_count_arc[0] = KEY_HOLD_TIME;
		}
		break;
	// key 2 UP
	case 2:
		if(key_count_arc[1]) {
			key_count_arc[1] = 0;
			// SHORT PRESS
			c.pos[0] = 0;
			c.pos[1] = 0;
			c.pos[2] = 0;
			c.pos[3] = 0;
		}
		else {
			// LONG RELEASE
			mode = 0;
			enc_count[0] = 0;
			enc_count[1] = 0;
			enc_count[2] = 0;
			enc_count[3] = 0;
			arc_refresh = &refresh_cycles;
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

void handler_CyclesTr(s32 data) {
	// print_dbg("\r\n> cycles tr ");
	// print_dbg_ulong(data);

	switch(data) {
	case 0:
		friction = friction_map[24 - c.friction] + 192;
		break;
	case 1:
		friction = friction_map[(24 - c.friction) >> 1] + 192;
		break;
	case 2:
		if(ext_clock) {
			add_force[0] = 0;
			add_force[1] = 0;
			add_force[2] = 0;
			add_force[3] = 0;
		}
		break;
	case 3:
		if(ext_clock) {
			add_force[0] = (cycle_dir[0] * 2 - 1) * (1 << (c.force - 1));
			add_force[1] = (cycle_dir[1] * 2 - 1) * (1 << (c.force - 1));
			add_force[2] = (cycle_dir[2] * 2 - 1) * (1 << (c.force - 1));
			add_force[3] = (cycle_dir[3] * 2 - 1) * (1 << (c.force - 1));
		}
		else {
			c.pos[0] = 0;
			c.pos[1] = 0;
			c.pos[2] = 0;
			c.pos[3] = 0;
		}
		break;
	}
}

void handler_CyclesTrNormal(s32 data) {
	// print_dbg("\r\n> cycles tr normal ");
	// print_dbg_ulong(data);

	ext_clock = data;
}

static void key_long_cycles(uint8_t key) {
	// print_dbg("\r\nLONG PRESS >>>>>>> ");
	// print_dbg_ulong(key);

	if(key == 1) {
		mode = 1;
		enc_count[0] = 0;
		enc_count[1] = 0;
		enc_count[2] = 0;
		enc_count[3] = 0;
		arc_refresh = &refresh_cycles_config;
		monomeFrameDirty++;
	}
	else {
		friction = friction_map[(24 - c.friction) >> 1] + 192;
	}
}

void refresh_cycles(void) {
	uint8_t i1;
	memset(monomeLedBuffer,0,MONOME_MAX_LED_BYTES);

	if(c.mode) {
		for(i1=1;i1<4;i1++) {
			monomeLedBuffer[i1*64 + 30 + c.mult[i1]*4] = 3;
			monomeLedBuffer[i1*64 + 31 + c.mult[i1]*4] = 3;
			monomeLedBuffer[i1*64 + 32 + c.mult[i1]*4] = 3;
			monomeLedBuffer[i1*64 + 33 + c.mult[i1]*4] = 3;
		}
	}

	for(i1=0;i1<4;i1++)
		arc_draw_point(i1,c.pos[i1] >> 4);
}

void refresh_cycles_config(void) {
	uint8_t i1;
	memset(monomeLedBuffer,0,MONOME_MAX_LED_BYTES);

	if(c.mode) {
		monomeLedBuffer[36] = 7;
		monomeLedBuffer[39] = 7;
		monomeLedBuffer[42] = 7;
		monomeLedBuffer[45] = 7;
	}
	else {
		monomeLedBuffer[35] = 7;
		monomeLedBuffer[42] = 7;
		monomeLedBuffer[44] = 7;
		monomeLedBuffer[48] = 7;
	}

	if(c.shape) {
		for(i1=0;i1<16;i1++) {
			monomeLedBuffer[64 + i1*4 + 0] = i1;
			monomeLedBuffer[64 + i1*4 + 1] = i1;
			monomeLedBuffer[64 + i1*4 + 2] = i1;
			monomeLedBuffer[64 + i1*4 + 3] = i1;
		}
	}
	else {
		for(i1=0;i1<16;i1++) {
			monomeLedBuffer[64 + i1*2] = 15-i1;
			monomeLedBuffer[64 + i1*2 + 1] = 15-i1;
			monomeLedBuffer[64 + 32 + i1*2] = i1;
			monomeLedBuffer[64 + 32 + i1*2 + 1] = i1;
		}
	}

	for(i1=0;i1<8;i1++)
		monomeLedBuffer[128 + 40 + i1] = 3;

	monomeLedBuffer[128 + 38 + (c.force * 2)] = 15;
	monomeLedBuffer[128 + 39 + (c.force * 2)] = 15;

	monomeLedBuffer[192] = 3;
	memset(monomeLedBuffer + 232, 3, 24);
	monomeLedBuffer[192 + ((c.friction + 40) & 0x3f)] = 15;
	// uint16_t i = (c.friction * 3);
	// i = (i + 640) & 0x3ff;
	// arc_draw_point(3, i);
}

void refresh_cycles_config_range(void) {
	uint8_t i1, i2;
	memset(monomeLedBuffer,0,MONOME_MAX_LED_BYTES);

	for(i1=0;i1<4;i1++)
		for(i2=0;i2<c.range[i1];i2++)
			monomeLedBuffer[i1*64 + ((32 + i2) & 0x3f)] = 3;
}

void refresh_cycles_config_div(void) {
	uint8_t i1, i2;
	memset(monomeLedBuffer,0,MONOME_MAX_LED_BYTES);

	for(i1=0;i1<4;i1++) {
		for(i2=0; i2<(1<<c.div[i1]); i2++) {
			memset(i1*64 + monomeLedBuffer + i2*(64 >> c.div[i1]), 5, (64 >> (c.div[i1]+1)));
		}
	}
}



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////



static void arc_draw_point(uint8_t n, uint16_t p) {
	int c;

	// c = p / 16;
	c = p >> 4;

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


static void generate_scales(uint8_t n) {
	levels_scales[n][0] = 0;
	for(uint8_t i1=0;i1<28;i1++) {
		levels_scales[n][i1+1] = levels_scales[n][i1] + SCALE_INT[l.scale[n]-1][i1 % 7];
	}
}

static void levels_dac_refresh(void) {
	if(l.mode[0]) {
		if(l.scale[0])
			dac_set_value(0, ET[ levels_scales[0][ l.note[0][play] ] + l.octave[0]*12] << 2);
		else
			dac_set_value(0, ET[ l.note[0][play] + l.octave[0]*12] << 2);
	}
	else
		dac_set_value(0, (l.pattern[0][play] + l.offset[0]) << (2 + l.range[0]));
	if(l.mode[1]) {
		if(l.scale[1])
			dac_set_value(1, ET[ levels_scales[1][ l.note[1][play] ] + l.octave[1]*12] << 2);
		else
			dac_set_value(1, ET[ l.note[1][play] + l.octave[1]*12] << 2);
	}
	else
		dac_set_value(1, (l.pattern[1][play] + l.offset[1]) << (2 + l.range[1]));
	if(l.mode[2]) {
		if(l.scale[2])
			dac_set_value(2, ET[ levels_scales[2][ l.note[2][play] ] + l.octave[2]*12] << 2);
		else
			dac_set_value(2, ET[ l.note[2][play] + l.octave[2]*12] << 2);
	}
	else
		dac_set_value(2, (l.pattern[2][play] + l.offset[2]) << (2 + l.range[2]));
	if(l.mode[3]) {
		if(l.scale[3])
			dac_set_value(3, ET[ levels_scales[3][ l.note[3][play] ] + l.octave[3]*12] << 2);
		else
			dac_set_value(3, ET[ l.note[3][play] + l.octave[3]*12] << 2);
	}
	else
		dac_set_value(3, (l.pattern[3][play] + l.offset[3]) << (2 + l.range[3]));
}
