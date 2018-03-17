#include "string.h"

#include "print_funcs.h"
#include "flashc.h"
#include "gpio.h"

#include "monome.h"
#include "i2c.h"
#include "dac.h"
#include "util.h" // rnd
#include "music.h"
#include "init_common.h"
#include "ii.h"

#include "main.h"
#include "ansible_grid.h"

#include "init_ansible.h"


#define L2 12
#define L1 8
#define L0 4

#define GRID_KEY_HOLD_TIME 15
#define MAX_HELD_KEYS 32

#define ES_CHORD_THRESHOLD 30

bool preset_mode;
uint8_t preset;
	
u8 key_count = 0;
u8 held_keys[MAX_HELD_KEYS];
u8 key_times[128];

bool clock_external;
bool view_clock;
bool view_config;
uint32_t clock_period;
uint8_t clock_count;
uint8_t clock_mul;
uint8_t ext_clock_count;
uint8_t ext_clock_phase;

u64 last_ticks[4];
u32 clock_deltas[4];

uint8_t time_rough;
uint8_t time_fine;

uint8_t cue_div;
uint8_t cue_steps;

uint8_t meta;

uint8_t scale_data[16][8];

u8 cur_scale[8];
void calc_scale(uint8_t s);

void (*grid_refresh)(void);

// KRIA
kria_data_t k;

typedef enum {
	mTr, mNote, mOct, mDur, mScale, mPattern
} kria_modes_t;

typedef enum {
	modNone, modLoop, modTime, modProb
} kria_mod_modes_t;

kria_modes_t k_mode;
kria_mod_modes_t k_mod_mode;

bool kria_mutes[4];
bool kria_blinks[4];
softTimer_t blinkTimer[4] = {
	{ .next = NULL, .prev = NULL },
	{ .next = NULL, .prev = NULL },
	{ .next = NULL, .prev = NULL },
	{ .next = NULL, .prev = NULL }
};

// manually clocking via teletype
bool kria_tt_clocked[4];

// MP

mp_data_t m;
u8 sound;
u8 voice_mode;

// ES

es_data_t e;

void set_mode_grid() {
	switch(ansible_mode) {
	case mGridKria:
		// print_dbg("\r\n> mode grid kria");
		app_event_handlers[kEventKey] = &handler_KriaKey;
		app_event_handlers[kEventTr] = &handler_KriaTr;
		app_event_handlers[kEventTrNormal] = &handler_KriaTrNormal;
		app_event_handlers[kEventMonomeGridKey] = &handler_KriaGridKey;
		app_event_handlers[kEventMonomeRefresh] = &handler_KriaRefresh;
		clock = &clock_kria;
		clock_set(clock_period);
		init_i2c_slave(II_KR_ADDR);
		process_ii = &ii_kria;
		resume_kria();
		update_leds(1);
		break;
	case mGridMP:
		// print_dbg("\r\n> mode grid mp");
		app_event_handlers[kEventKey] = &handler_MPKey;
		app_event_handlers[kEventTr] = &handler_MPTr;
		app_event_handlers[kEventTrNormal] = &handler_MPTrNormal;
		app_event_handlers[kEventMonomeGridKey] = &handler_MPGridKey;
		app_event_handlers[kEventMonomeRefresh] = &handler_MPRefresh;
		clock = &clock_mp;
		clock_set(clock_period);
		init_i2c_slave(II_MP_ADDR);
		process_ii = &ii_mp;
		resume_mp();
		update_leds(2);
		break;
	case mGridES:
		// print_dbg("\r\n> mode grid es");
		app_event_handlers[kEventKey] = &handler_ESKey;
		app_event_handlers[kEventTr] = &handler_ESTr;
		app_event_handlers[kEventTrNormal] = &handler_ESTrNormal;
		app_event_handlers[kEventMonomeGridKey] = &handler_ESGridKey;
		app_event_handlers[kEventMonomeRefresh] = &handler_ESRefresh;
		clock = &clock_null;
		clock_set(clock_period);
		init_i2c_slave(II_ANSIBLE_ADDR);
		process_ii = &ii_null;
		resume_es();
		update_leds(3);
		break;
	default:
		break;
	}

	// if(connected == conGRID) {
	// 	app_event_handlers[kEventFrontShort] = &handler_GridFrontShort;
	// 	app_event_handlers[kEventFrontLong] = &handler_GridFrontLong;
	// }

	flashc_memset32((void*)&(f.state.none_mode), ansible_mode, 4, true);
	flashc_memset32((void*)&(f.state.grid_mode), ansible_mode, 4, true);
}


void handler_GridFrontShort(s32 data) {
	if(preset_mode) {
		// print_dbg("\r\n> PRESET EXIT");
		preset_mode = false;

		switch (ansible_mode) {
			case mGridKria:
				grid_refresh = &refresh_kria;
				break;
			case mGridMP:
				grid_refresh = &refresh_mp;
				break;
			case mGridES:
				grid_refresh = &refresh_es;
				break;
			default:
				break;
		}
			
		view_config = false;
		view_clock = false;
	}
	else {
		// print_dbg("\r\n> PRESET ENTER");
		preset_mode = true;
		grid_refresh = &refresh_preset;
		view_config = false;
		view_clock = false;
	}
	monomeFrameDirty++;
}

void handler_GridFrontLong(s32 data) {
	switch (ansible_mode) {
		case mGridKria:
			set_mode(mGridMP);
			break;
		case mGridMP:
			set_mode(mGridES);
			break;
		case mGridES:
			set_mode(mGridKria);
			break;
		default:
			break;
	}
	monomeFrameDirty++;
}

void refresh_preset(void) {
	u8 i1, i2;//, i3;

	memset(monomeLedBuffer,0,128);

	for(i1=0;i1<128;i1++)
		monomeLedBuffer[i1] = 0;

	monomeLedBuffer[preset * 16] = 11;

	switch(ansible_mode) {
	case mGridMP:
		for(i1=0;i1<8;i1++)
			for(i2=0;i2<8;i2++)
				if(m.glyph[i1] & (1<<i2))
					monomeLedBuffer[i1*16+i2+8] = 9;
		break;
	case mGridKria:
		for(i1=0;i1<8;i1++)
			for(i2=0;i2<8;i2++)
				if(k.glyph[i1] & (1<<i2))
					monomeLedBuffer[i1*16+i2+8] = 9;
		break;
	case mGridES:
		for(i1=0;i1<8;i1++)
			for(i2=0;i2<8;i2++)
				if(e.glyph[i1] & (1<<i2))
					monomeLedBuffer[i1*16+i2+8] = 9;
		break;
	default: break;
	}

	monome_set_quadrant_flag(0);
	monome_set_quadrant_flag(1);
}

void grid_keytimer(void) {
	for(uint8_t i1=0;i1<key_count;i1++) {
		if(key_times[held_keys[i1]])
		if(--key_times[held_keys[i1]]==0) {
			if(preset_mode == 1) {
				if(held_keys[i1] % 16 == 0) {
					preset = held_keys[i1] / 16;

					// WRITE PRESET

					switch (ansible_mode) {
					case mGridMP:
						flashc_memset8((void*)&(f.mp_state.preset), preset, 1, true);
						flashc_memset8((void*)&(f.mp_state.sound), sound, 1, true);
						flashc_memset8((void*)&(f.mp_state.voice_mode), voice_mode, 1, true);
						flashc_memcpy((void *)&f.mp_state.m[preset], &m, sizeof(m), true);

						flashc_memcpy((void *)&f.scale, &scale_data, sizeof(scale_data), true);

						preset_mode = false;
						grid_refresh = &refresh_mp;
						break;
						
					case mGridKria:
						flashc_memset8((void*)&(f.kria_state.preset), preset, 1, true);
						flashc_memset8((void*)&(f.kria_state.cue_div), cue_div, 1, true);
						flashc_memset8((void*)&(f.kria_state.cue_steps), cue_steps, 1, true);
						flashc_memset8((void*)&(f.kria_state.meta), meta, 1, true);
						flashc_memcpy((void *)&f.kria_state.k[preset], &k, sizeof(k), true);

						flashc_memcpy((void *)&f.scale, &scale_data, sizeof(scale_data), true);

						preset_mode = false;
						grid_refresh = &refresh_kria;
						break;

					case mGridES:
						flashc_memset8((void*)&(f.es_state.preset), preset, 1, true);
						flashc_memcpy((void *)&f.es_state.e[preset], &e, sizeof(e), true);
						preset_mode = false;
						grid_refresh = &refresh_es;
						break;
						
					default:
						break;
					}
					
					flashc_memset32((void*)&(f.kria_state.clock_period), clock_period, 4, true);
					monomeFrameDirty++;
				}
			}
			else if(ansible_mode == mGridKria) {
				if(k_mode == mPattern) {
					if(held_keys[i1] < 16) {
						memcpy((void *)&k.p[held_keys[i1]], &k.p[k.pattern], sizeof(k.p[k.pattern]));
						k.pattern = held_keys[i1];
						// pos_reset = true;
					}
				}
			}

			// print_dbg("\rlong press: ");
			// print_dbg_ulong(held_keys[i1]);
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
// KRIA

u8 track;

u8 loop_count;
u8 loop_first;
s8 loop_last;
u8 loop_edit;

bool note_sync;
uint8_t loop_sync;

u8 pos[4][KRIA_NUM_PARAMS];
u8 pos_mul[4][KRIA_NUM_PARAMS];
bool pos_reset;
u8 tr[4];
u8 note[4];
u8 oct[4];
u16 dur[4];

bool cue;
u8 cue_sub_count;
u8 cue_count;
u8 cue_pat_next;

uint8_t meta_pos;
uint8_t meta_count;
uint8_t meta_next;
uint8_t meta_edit;

static void kria_off0(void* o);
static void kria_off1(void* o);
static void kria_off2(void* o);
static void kria_off3(void* o);

static void kria_blink_off0(void* o);
static void kria_blink_off1(void* o);
static void kria_blink_off2(void* o);
static void kria_blink_off3(void* o);

bool kria_next_step(uint8_t t, uint8_t p);
static void adjust_loop_start(u8 t, u8 x, u8 m);
static void adjust_loop_end(u8 t, u8 x, u8 m);
static void adjust_loop_len(u8 t, u8 x, u8 m);
static void update_loop_start(u8 t, u8 x, u8 m);
static void update_loop_end(u8 t, u8 x, u8 m);
static void jump_pos(u8 t, u8 x, u8 m);

static void update_meta_start(u8 x);
static void update_meta_end(u8 x);

void change_pattern(uint8_t x);


void default_kria() {
	uint8_t i1;

	flashc_memset32((void*)&(f.kria_state.clock_period), 60, 4, true);
	flashc_memset8((void*)&(f.kria_state.preset), 0, 1, true);
	flashc_memset8((void*)&(f.kria_state.note_sync), true, 1, true);
	flashc_memset8((void*)&(f.kria_state.loop_sync), 2, 1, true);
	flashc_memset8((void*)&(f.kria_state.cue_div), 0, 1, true);
	flashc_memset8((void*)&(f.kria_state.cue_steps), 3, 1, true);
	flashc_memset8((void*)&(f.kria_state.meta), 0, 1, true);

	for(i1=0;i1<8;i1++)
		k.glyph[i1] = 0;

	memset(k.p[0].t[0].tr, 0, 16);
	memset(k.p[0].t[0].oct, 0, 16);
	memset(k.p[0].t[0].note, 0, 16);
	memset(k.p[0].t[0].dur, 0, 16);
	memset(k.p[0].t[0].p, 3, 16 * KRIA_NUM_PARAMS);
	// memset(k.p[0].t[0].ptr, 3, 16);
	// memset(k.p[0].t[0].poct, 3, 16);
	// memset(k.p[0].t[0].pnote, 3, 16);
	// memset(k.p[0].t[0].pdur, 3, 16);
	k.p[0].t[0].dur_mul = 4;
	memset(k.p[0].t[0].lstart, 0, KRIA_NUM_PARAMS);
	memset(k.p[0].t[0].lend, 5, KRIA_NUM_PARAMS);
	memset(k.p[0].t[0].llen, 6, KRIA_NUM_PARAMS);
	memset(k.p[0].t[0].lswap, 0, KRIA_NUM_PARAMS);
	memset(k.p[0].t[0].tmul, 1, KRIA_NUM_PARAMS);

	k.p[0].t[1] = k.p[0].t[0];
	k.p[0].t[2] = k.p[0].t[0];
	k.p[0].t[3] = k.p[0].t[0];
	k.p[0].scale = 0;

	for(i1=1;i1<KRIA_NUM_PATTERNS;i1++)
		k.p[i1] = k.p[0];

	k.pattern = 0;

	k.meta_start = 0;
	k.meta_end = 3;
	k.meta_len = 4;
	k.meta_lswap = 0;

	memset(k.meta_pat, 0, 64);
	memset(k.meta_steps, 7, 64);

	for(i1=0;i1<GRID_PRESETS;i1++)
		flashc_memcpy((void *)&f.kria_state.k[i1], &k, sizeof(k), true);
}

void init_kria() {
	track = 0;
	k_mode = mTr;
	k_mod_mode = modNone;

	note_sync = f.kria_state.note_sync;
	loop_sync = f.kria_state.loop_sync;
	cue_div = f.kria_state.cue_div;
	cue_steps = f.kria_state.cue_steps;

	preset = f.kria_state.preset;

	k.pattern = f.kria_state.k[preset].pattern;

	k = f.kria_state.k[preset];

	clock_mul = 1;

	clock_period = f.kria_state.clock_period;
	time_rough = (clock_period - 20) / 16;
	time_fine = (clock_period - 20) % 16;

	for ( int i=0; i<4; i++ ) {
		last_ticks[i] = get_ticks();
	}
}

void resume_kria() {
	grid_refresh = &refresh_kria;
	view_clock = false;
	view_config = false;

	preset = f.kria_state.preset;

	calc_scale(k.p[k.pattern].scale);

	// re-check clock jack
	clock_external = !gpio_get_pin_value(B10);

	if(clock_external)
		clock = &clock_null;
	else
		clock = &clock_kria;

	for ( int i=0; i<4; i++ ) {
		last_ticks[i] = get_ticks();
	}

	dac_set_slew(0,0);
	dac_set_slew(1,0);
	dac_set_slew(2,0);
	dac_set_slew(3,0);

	clr_tr(TR1);
	clr_tr(TR2);
	clr_tr(TR3);
	clr_tr(TR4);

	monomeFrameDirty++;
}

bool kria_next_step(uint8_t t, uint8_t p) {
	pos_mul[t][p]++;

	if(pos_mul[t][p] >= k.p[k.pattern].t[t].tmul[p]) {
		if(pos[t][p] == k.p[k.pattern].t[t].lend[p])
			pos[t][p] = k.p[k.pattern].t[t].lstart[p];
		else {
			pos[t][p]++;
			if(pos[t][p] > 15)
				pos[t][p] = 0;
		}
		pos_mul[t][p] = 0;
		switch(k.p[k.pattern].t[t].p[p][pos[t][p]]) {
		case 0:
			return false;
		case 1:
			// ~25%
			return (rnd() & 0xff) > 192;
		case 2:
			// ~50%
			return (rnd() & 0xff) > 128;
		case 3:
			return true;
		default:
			return true;
		}
	}
	else
		return false;
}

void clock_kria(uint8_t phase) {
	if(phase) {
		clock_count++;
		cue_sub_count++;

		if(cue_sub_count >= cue_div + 1) {
			cue_sub_count = 0;
			cue_count++;
			if(cue_count >= cue_steps + 1) {
				cue_count = 0;

				if(meta) {
					meta_count++;
					if(meta_count > k.meta_steps[meta_pos]) {
						if(meta_next)
							meta_pos = meta_next - 1;
						else if(meta_pos == k.meta_end)
							meta_pos = k.meta_start;
						else
							meta_pos++;

						change_pattern(k.meta_pat[meta_pos]);
						meta_next = 0;
						meta_count = 0;
					}
				}
				else if(cue_pat_next) {
					change_pattern(cue_pat_next - 1);
					cue_pat_next = 0;
				}
			}
		}

		if(pos_reset) {
			clock_count = 0;
			for(int i1=0;i1<4;i1++)
			for(int i2=0;i2<4;i2++) {
				pos[i1][i2] = k.p[k.pattern].t[i1].lend[i2];
				pos_mul[i1][i2] = k.p[k.pattern].t[i1].tmul[i2];
			}
			cue_count = 0;
			cue_sub_count = 0;
			pos_reset = false;
		}


		for ( uint8_t i=0; i<4; i++ )
		{
			if ( !kria_tt_clocked[i] )
				clock_kria_track( i );
		}

		monomeFrameDirty++;

		// may need forced DAC update here
		dac_timer_update();
	}
}

void clock_kria_track( uint8_t trackNum )
{
	u64 current_tick = get_ticks();
	clock_deltas[trackNum] = (u32)(current_tick-last_ticks[trackNum]);
	last_ticks[trackNum] = current_tick;

	if(kria_next_step(trackNum, mDur)) {
		f32 clock_scale = (clock_deltas[trackNum] * k.p[k.pattern].t[trackNum].tmul[mTr]) / (f32)384.0;
		f32 uncscaled = (k.p[k.pattern].t[trackNum].dur[pos[trackNum][mDur]]+1) * (k.p[k.pattern].t[trackNum].dur_mul<<2);
		dur[trackNum] = (u16)(uncscaled * clock_scale);
	}

	if(kria_next_step(trackNum, mOct)) {
		oct[trackNum] = k.p[k.pattern].t[trackNum].oct[pos[trackNum][mOct]];
	}

	if(kria_next_step(trackNum, mNote)) {
		note[trackNum] = k.p[k.pattern].t[trackNum].note[pos[trackNum][mNote]];
	}

	if(kria_next_step(trackNum, mTr)) {
		if(k.p[k.pattern].t[trackNum].tr[pos[trackNum][mTr]]) {

			if ( !kria_mutes[trackNum] ) {
				dac_set_value(trackNum, ET[cur_scale[note[trackNum]] + (oct[trackNum] * 12)] << 2);
				gpio_set_gpio_pin(TR1 + trackNum);
			}

			switch(trackNum) {
				case 0:
					timer_remove( &blinkTimer[0] );
					timer_add( &blinkTimer[0], max(dur[0],31), &kria_blink_off0, NULL );
					if ( !kria_mutes[trackNum] ) {
						timer_remove( &auxTimer[0]);
						timer_add(&auxTimer[0], dur[0], &kria_off0, NULL);
					}
					break;
				case 1:
					timer_remove( &blinkTimer[1] );
					timer_add( &blinkTimer[1], max(dur[1],31), &kria_blink_off1, NULL );
					if ( !kria_mutes[trackNum] ) {
						timer_remove( &auxTimer[1]);
						timer_add(&auxTimer[1], dur[1], &kria_off1, NULL);
					}
					break;
				case 2:
					timer_remove( &blinkTimer[2] );
					timer_add( &blinkTimer[2], max(dur[2],31), &kria_blink_off2, NULL );
					if ( !kria_mutes[trackNum] ) {
						timer_remove( &auxTimer[2]);
						timer_add(&auxTimer[2], dur[2], &kria_off2, NULL);
					}
					break;
				case 3:
					timer_remove( &blinkTimer[3] );
					timer_add( &blinkTimer[3], max(dur[3],31), &kria_blink_off3, NULL );
					if ( !kria_mutes[trackNum] ) {
						timer_remove( &auxTimer[3]);
						timer_add(&auxTimer[3], dur[3], &kria_off3, NULL);
					}
					break;
				default: break;
			}

			tr[trackNum] = 1;
			kria_blinks[trackNum] = 1;
		}
	}

}

static void kria_off0(void* o) {
	timer_remove( &auxTimer[0]);
	clr_tr(TR1);
	tr[0] = 0;
}

static void kria_off1(void* o) {
	timer_remove( &auxTimer[1]);
	clr_tr(TR2);
	tr[1] = 0;
}

static void kria_off2(void* o) {
	timer_remove( &auxTimer[2]);
	clr_tr(TR3);
	tr[2] = 0;
}

static void kria_off3(void* o) {
	timer_remove( &auxTimer[3]);
	clr_tr(TR4);
	tr[3] = 0;
}

static void kria_blink_off0(void* o) {
	timer_remove( &blinkTimer[0] );
	kria_blinks[0] = 0;
	monomeFrameDirty++;
}

static void kria_blink_off1(void* o) {
	timer_remove( &blinkTimer[1] );
	kria_blinks[1] = 0;
	monomeFrameDirty++;
}

static void kria_blink_off2(void* o) {
	timer_remove( &blinkTimer[2] );
	kria_blinks[2] = 0;
	monomeFrameDirty++;
}

static void kria_blink_off3(void* o) {
	timer_remove( &blinkTimer[3] );
	kria_blinks[3] = 0;
	monomeFrameDirty++;
}

void change_pattern(uint8_t x) {
	k.pattern = x;
	pos_reset = true;
	calc_scale(k.p[k.pattern].scale);
}

void ii_kria(uint8_t *d, uint8_t l) {
	// print_dbg("\r\nii/kria (");
	// print_dbg_ulong(l);
	// print_dbg(") ");
	// for(int i=0;i<l;i++) {
	// 	print_dbg_ulong(d[i]);
	// 	print_dbg(" ");
	// }

	int n;

	if(l) {
		switch(d[0]) {
		case II_KR_PRESET:
			if(d[1] > -1 && d[1] < 8) {
				preset = d[1];
				flashc_memset8((void*)&(f.kria_state.preset), preset, 1, true);
				init_kria();
				resume_kria();
			}
			break;
		case II_KR_PRESET + II_GET:
			ii_tx_queue(preset);
			break;
		case II_KR_PATTERN:
			if(d[1] > -1 && d[1] < 16) {
				k.pattern = d[1];
				pos_reset = true;
				calc_scale(k.p[k.pattern].scale);
			}
			break;
		case II_KR_PATTERN + II_GET:
			ii_tx_queue(k.pattern);
			break;
		case II_KR_SCALE:
			if(d[1] > -1 && d[1] < 16) {
				k.p[k.pattern].scale = d[1];
				calc_scale(k.p[k.pattern].scale);
			}
			break;
		case II_KR_SCALE + II_GET:
			ii_tx_queue(k.p[k.pattern].scale);
			break;
		case II_KR_PERIOD:
			n = (d[1] << 8) + d[2];
			if(n > 19) {
				clock_period = n;
				time_rough = (clock_period - 20) / 16;
				time_fine = (clock_period - 20) % 16;
				clock_set(clock_period);
			}
			break;
		case II_KR_PERIOD + II_GET:
			ii_tx_queue(clock_period >> 8);
			ii_tx_queue(clock_period & 0xff);
			break;
		case II_KR_RESET:
			switch(loop_sync) {
			case 2:
				for(int i1=0;i1<4;i1++)
				for(int i2=0;i2<4;i2++) {
					pos[i1][i2] = k.p[k.pattern].t[i1].lend[i2];
					pos_mul[i1][i2] = k.p[k.pattern].t[i1].tmul[i2];
				}
				break;
			case 1:
				if(d[1] == 0) {
					for(int i1=0;i1<4;i1++)
					for(int i2=0;i2<4;i2++) {
						pos[i1][i2] = k.p[k.pattern].t[i1].lend[i2];
						pos_mul[i1][i2] = k.p[k.pattern].t[i1].tmul[i2];
					}
				}
				else if(d[1] < 5) {
					for(int i1=0;i1<4;i1++) {
						pos[d[1]-1][i1] = k.p[k.pattern].t[d[1]-1].lend[i1];
						pos_mul[d[1]-1][i1] = k.p[k.pattern].t[d[1]-1].tmul[i1];
					}
				}
				break;
			case 0:
				if(d[1] == 0 && d[2] == 0) {
					for(int i1=0;i1<4;i1++)
					for(int i2=0;i2<4;i2++) {
						pos[i1][i2] = k.p[k.pattern].t[i1].lend[i2];
						pos_mul[i1][i2] = k.p[k.pattern].t[i1].tmul[i2];
					}
				}
				else if(d[1] == 0 && d[2] < 5) {
					for(int i1=0;i1<4;i1++) {
						pos[i1][d[2]-1] = k.p[k.pattern].t[i1].lend[d[2]-1];
						pos_mul[i1][d[2]-1] = k.p[k.pattern].t[i1].tmul[d[2]-1];
					}
				}
				else if(d[2] == 0 && d[1] < 5) {
					for(int i1=0;i1<4;i1++) {
						pos[d[1]-1][i1] = k.p[k.pattern].t[d[1]-1].lend[i1];
						pos_mul[d[1]-1][i1] = k.p[k.pattern].t[d[1]-1].tmul[i1];
					}
				}
				else if(d[1] < 5 && d[2] < 5) {
					pos[d[1]-1][d[2]-1] = k.p[k.pattern].t[d[1]-1].lend[d[2]-1];
					pos_mul[d[1]-1][d[2]-1] = k.p[k.pattern].t[d[1]-1].tmul[d[2]-1];
				}
				break;
			default:
				break;
			}
			break;
		case II_KR_LOOP_ST:
			if(d[3] < 16)
			switch(loop_sync) {
			case 2:
				for(int i1=0;i1<4;i1++)
					for(int i2=0;i2<4;i2++)
						adjust_loop_start(i1, d[3], i2);
				break;
			case 1:
				if(d[1] == 0) {
					for(int i1=0;i1<4;i1++)
						for(int i2=0;i2<4;i2++)
							adjust_loop_start(i1, d[3], i2);
				}
				else if(d[1] < 5) {
					for(int i1=0;i1<4;i1++)
						adjust_loop_start(d[1]-1, d[3], i1);
				}
				break;
			case 0:
				if(d[1] == 0 && d[2] == 0) {
					for(int i1=0;i1<4;i1++)
						for(int i2=0;i2<4;i2++)
							adjust_loop_start(i1, d[3], i2);
				}
				else if(d[1] == 0 && d[2] < 5) {
					for(int i1=0;i1<4;i1++)
						adjust_loop_start(i1, d[3], d[2]-1);
				}
				else if(d[2] == 0 && d[1] < 5) {
					for(int i1=0;i1<4;i1++)
						adjust_loop_start(d[1]-1, d[3], i1);
				}
				else if(d[1] < 5 && d[2] < 5) {
					adjust_loop_start(d[1]-1, d[3], d[2]-1);
				}
				break;
			default:
				break;
			}
			break;
		case II_KR_LOOP_ST + II_GET:
			if(d[1]==0 && d[2] == 0) {
				int n = 0;
				for(int i1=0;i1<4;i1++)
					for(int i2=0;i2<4;i2++)
						n += k.p[k.pattern].t[i1].lstart[i2];
				ii_tx_queue(n>>4);
			}
			else if(d[1] == 0) {
				int n = 0;
				for(int i1=0;i1<4;i1++)
					n += k.p[k.pattern].t[i1].lstart[d[2]-1];
				ii_tx_queue(n>>2);
			}
			else if(d[2] == 0) {
				int n = 0;
				for(int i1=0;i1<4;i1++)
					n += k.p[k.pattern].t[d[1]-1].lstart[i1];
				ii_tx_queue(n>>2);
			}
			else {
				ii_tx_queue(k.p[k.pattern].t[d[1]-1].lstart[d[2]-1]);
			}
			break;
		case II_KR_LOOP_LEN:
			if(d[3] < 17 && d[3] > 0)
			switch(loop_sync) {
			case 2:
				for(int i1=0;i1<4;i1++)
					for(int i2=0;i2<4;i2++)
						adjust_loop_len(i1, d[3], i2);
				break;
			case 1:
				if(d[1] == 0) {
					for(int i1=0;i1<4;i1++)
						for(int i2=0;i2<4;i2++)
							adjust_loop_len(i1, d[3], i2);
				}
				else if(d[1] < 5) {
					for(int i1=0;i1<4;i1++)
						adjust_loop_len(d[1]-1, d[3], i1);
				}
				break;
			case 0:
				if(d[1] == 0 && d[2] == 0) {
					for(int i1=0;i1<4;i1++)
						for(int i2=0;i2<4;i2++)
							adjust_loop_len(i1, d[3], i2);
				}
				else if(d[1] == 0 && d[2] < 5) {
					for(int i1=0;i1<4;i1++)
						adjust_loop_len(i1, d[3], d[2]-1);
				}
				else if(d[2] == 0 && d[1] < 5) {
					for(int i1=0;i1<4;i1++)
						adjust_loop_len(d[1]-1, d[3], i1);
				}
				else if(d[1] < 5 && d[2] < 5) {
					adjust_loop_len(d[1]-1, d[3], d[2]-1);
				}
				break;
			default:
				break;
			}
			break;
		case II_KR_LOOP_LEN + II_GET:
			if(d[1]==0 && d[2] == 0) {
				int n = 0;
				for(int i1=0;i1<4;i1++)
					for(int i2=0;i2<4;i2++)
						n += k.p[k.pattern].t[i1].llen[i2];
				ii_tx_queue(n>>4);
			}
			else if(d[1] == 0) {
				int n = 0;
				for(int i1=0;i1<4;i1++)
					n += k.p[k.pattern].t[i1].llen[d[2]-1];
				ii_tx_queue(n>>2);
			}
			else if(d[2] == 0) {
				int n = 0;
				for(int i1=0;i1<4;i1++)
					n += k.p[k.pattern].t[d[1]-1].llen[i1];
				ii_tx_queue(n>>2);
			}
			else {
				ii_tx_queue(k.p[k.pattern].t[d[1]-1].llen[d[2]-1]);
			}
			break;
		case II_KR_POS:
			if(d[3] < 17)
			switch(loop_sync) {
			case 2:
				for(int i1=0;i1<4;i1++)
					for(int i2=0;i2<4;i2++)
						jump_pos(i1, d[3], i2);
				break;
			case 1:
				if(d[1] == 0) {
					for(int i1=0;i1<4;i1++)
						for(int i2=0;i2<4;i2++)
							jump_pos(i1, d[3], i2);
				}
				else if(d[1] < 5) {
					for(int i1=0;i1<4;i1++)
						jump_pos(d[1]-1, d[3], i1);
				}
				break;
			case 0:
				if(d[1] == 0 && d[2] == 0) {
					for(int i1=0;i1<4;i1++)
						for(int i2=0;i2<4;i2++)
							jump_pos(i1, d[3], i2);
				}
				else if(d[1] == 0 && d[2] < 5) {
					for(int i1=0;i1<4;i1++)
						jump_pos(i1, d[3], d[2]-1);
				}
				else if(d[2] == 0 && d[1] < 5) {
					for(int i1=0;i1<4;i1++)
						jump_pos(d[1]-1, d[3], i1);
				}
				else if(d[1] < 5 && d[2] < 5) {
					jump_pos(d[1]-1, d[3], d[2]-1);
				}
				break;
			default:
				break;
			}
			break;
		case II_KR_POS + II_GET:
			if(d[1]==0 && d[2] == 0) {
				int n = 0;
				for(int i1=0;i1<4;i1++)
					for(int i2=0;i2<4;i2++)
						n += pos[i1][i2];
				ii_tx_queue(n>>4);
			}
			else if(d[1] == 0) {
				int n = 0;
				for(int i1=0;i1<4;i1++)
					n += pos[i1][d[2]-1];
				ii_tx_queue(n>>2);
			}
			else if(d[2] == 0) {
				int n = 0;
				for(int i1=0;i1<4;i1++)
					n += pos[d[1]-1][i1];
				ii_tx_queue(n>>2);
			}
			else {
				ii_tx_queue(pos[d[1]-1][d[2]-1]);
			}
			break;
		case II_KR_CV + II_GET:
			if (d[1] > 3) {
				ii_tx_queue(0);
				ii_tx_queue(0);
				break;
			}
			ii_tx_queue(dac_get_value(d[1]) >> 8);
			ii_tx_queue(dac_get_value(d[1]) & 0xff);
			break;
		case II_KR_MUTE:
			if ( d[1] == 0 ) {
				for ( int i=0; i<4; i++ )
					kria_mutes[i] = max( min( d[2], 1 ), 0 );
				monomeFrameDirty++;
			}
			else if ( d[1] < 5 && d[1] > 0 ) {
				kria_mutes[d[1]-1] = max( min( d[2], 1 ), 0 );
				monomeFrameDirty++;
			}
			break;
		case II_KR_MUTE + II_GET:
			if ( d[1] > 0 && d[1] < 5 ) {
				ii_tx_queue( kria_mutes[d[1]-1] );
			}
			break;
		case II_KR_TMUTE:
			if ( d[1] == 0 ) {
				for ( int i=0; i<4; i++ ) {
					kria_mutes[i] = !kria_mutes[i];
				}
				monomeFrameDirty++;
			}
			else if ( d[1] < 5 && d[1] > 0 ) {
				kria_mutes[d[1]-1] = !kria_mutes[d[1]-1];
				monomeFrameDirty++;
			}
			break;
		case II_KR_CLK:
			if ( d[1] == 0 ) {
				for ( int i=0; i<4; i++ ) {
					if ( kria_tt_clocked[i] )
						clock_kria_track( i );
				}
			}
			else if ( d[1] < 5 && d[1] > 0  ) {
				if ( kria_tt_clocked[d[1]-1] )
					clock_kria_track( d[1]-1 );
			}
		default:
			break;
		}
	}
}

void handler_KriaGridKey(s32 data) {
	u8 x, y, z, index, i1, found;

	monome_grid_key_parse_event_data(data, &x, &y, &z);
	// print_dbg("\r\n monome event; x: ");
	// print_dbg_hex(x);
	// print_dbg("; y: 0x");
	// print_dbg_hex(y);
	// print_dbg("; z: 0x");
	// print_dbg_hex(z);

	//// TRACK LONG PRESSES
	index = y*16 + x;
	if(z) {
		held_keys[key_count] = index;
		key_count++;
		key_times[index] = GRID_KEY_HOLD_TIME;		//// THRESHOLD key hold time
	} else {
		found = 0; // "found"
		for(i1 = 0; i1<key_count; i1++) {
			if(held_keys[i1] == index)
				found++;
			if(found)
				held_keys[i1] = held_keys[i1+1];
		}
		key_count--;

		// FAST PRESS
		if(key_times[index] > 0) {
			// PRESET MODE FAST PRESS DETECT
			if(preset_mode == 1) {
				if(x == 0) {
					if(y != preset) {
						preset = y;

						for(i1=0;i1<8;i1++)
							k.glyph[i1] = f.kria_state.k[preset].glyph[i1];

						// print_dbg("\r\npreset select:");
						// print_dbg_ulong(preset);
					}
 					else if(y == preset) {
 						// flash read
						flashc_memset8((void*)&(f.kria_state.preset), preset, 1, true);
						init_kria();
						resume_kria();

						preset_mode = false;
						grid_refresh = &refresh_kria;

						// print_dbg("\r\npreset RECALL:");
						// print_dbg_ulong(preset);
					}
				}
			}
			else if(k_mode == mPattern) {
				if(y == 0) {
					if(!meta) {
						if(cue) {
							cue_pat_next = x+1;
						}
						else {
							change_pattern(x);
						}
					}
					else {
						k.meta_pat[meta_edit] = x;
					}
				}
			}
		}
	}

	// PRESET SCREEN
	if(preset_mode) {
		// glyph magic
		if(z && x > 7) {
			k.glyph[y] ^= 1<<(x-8);

			monomeFrameDirty++;
		}
	}
	else if(view_clock) {
		if(z) {
			if(clock_external) {
				if(y==1) {
					clock_mul = x + 1;
					monomeFrameDirty++;
				}
			}
			else {
				if(y==1)
					time_rough = x;
				else if(y==2)
					time_fine = x;
				else if(y==4) {
					int i = 0;

					switch(x) {
					case 6:
						i = -4;
						break;
					case 7:
						i = -1;
						break;
					case 8:
						i = 1;
						break;
					case 9:
						i = 4;
						break;
					default:
						break;
					}

					i += clock_period;
					if(i < 20)
						i = 20;
					if(clock_period > 265)
						clock_period = 265;
					clock_period = i;

					time_rough = (clock_period - 20) / 16;
					time_fine = (clock_period - 20) % 16;
				}

				clock_period = 20 + (time_rough * 16) + time_fine;

				clock_set(clock_period);

				// print_dbg("\r\nperiod: ");
				// print_dbg_ulong(clock_period);

				monomeFrameDirty++;
			}
		}


		// time_rough = (clock_period - 20) / 16;
		// time_fine = (clock_period - 20) % 16;
	}
	else if(view_config) {
		if(z) {
			if(x<8) {
				note_sync ^= 1;
				if(loop_sync == 0)
					loop_sync = 1;
				flashc_memset8((void*)&(f.kria_state.note_sync), note_sync, 1, true);
				flashc_memset8((void*)&(f.kria_state.loop_sync), loop_sync, 1, true);
			}
			else if(y == 3) {
				if(loop_sync == 1) {
					loop_sync = 0;
					note_sync = 0;
				}
				else loop_sync = 1;

				flashc_memset8((void*)&(f.kria_state.note_sync), note_sync, 1, true);
				flashc_memset8((void*)&(f.kria_state.loop_sync), loop_sync, 1, true);
			}
			else if(y == 5) {
				if(loop_sync == 2) {
					loop_sync = 0;
					note_sync = 0;
				}
				else loop_sync = 2;

				flashc_memset8((void*)&(f.kria_state.note_sync), note_sync, 1, true);
				flashc_memset8((void*)&(f.kria_state.loop_sync), loop_sync, 1, true);
			}
			monomeFrameDirty++;
		}
	}
	// NORMAL
	else {
		// bottom row
		if(y == 7) {
			if(z) {
				switch(x) {
				case 0:
				case 1:
				case 2:
				case 3:
					if ( k_mod_mode == modLoop )
						kria_mutes[x] = !kria_mutes[x];
					else
						track = x;
					break;
				case 5:
					k_mode = mTr; break;
				case 6:
					k_mode = mNote; break;
				case 7:
					k_mode = mOct; break;
				case 8:
					k_mode = mDur; break;
				case 10:
					k_mod_mode = modLoop;
					loop_count = 0;
					break;
				case 11:
					k_mod_mode = modTime; break;
				case 12:
					k_mod_mode = modProb; break;
				case 14:
					k_mode = mScale; break;
				case 15:
					k_mode = mPattern;
					cue = true;
					break;
				default: break;
				}
			}
			else {
				switch(x) {
				case 10:
				case 11:
				case 12:
					k_mod_mode = modNone;
					break;
				case 15:
					cue = false;
					break;
				default: break;
				}
			}

			monomeFrameDirty++;
		}
		else {
			switch(k_mode) {
			case mTr:
				switch(k_mod_mode) {
				case modNone:
					if(z) {
						k.p[k.pattern].t[y].tr[x] ^= 1;
						monomeFrameDirty++;
					}
					break;
				case modLoop:
					if(z && y<4) {
						if(loop_count == 0) {
							loop_edit = y;
							loop_first = x;
							loop_last = -1;
						}
						else {
							loop_last = x;
							update_loop_start(loop_edit, loop_first, mTr);
							update_loop_end(loop_edit, loop_last, mTr);
						}

						loop_count++;
					}
					else if(loop_edit == y) {
						loop_count--;

						if(loop_count == 0) {
							if(loop_last == -1) {
								if(loop_first == k.p[k.pattern].t[loop_edit].lstart[mTr]) {
									update_loop_start(loop_edit, loop_first, mTr);
									update_loop_end(loop_edit, loop_first, mTr);
								}
								else
									update_loop_start(loop_edit, loop_first, mTr);
							}
							monomeFrameDirty++;
						}
					}
					break;
				case modTime:
					if(z) {
						k.p[k.pattern].t[track].tmul[mTr] = x + 1;
						monomeFrameDirty++;
					}
					break;
				case modProb:
					if(z && y > 1 && y < 6) {
						k.p[k.pattern].t[track].p[mTr][x] = 5 - y;
						monomeFrameDirty++;
					}
					break;
				default: break;
				}

				break;
			case mNote:
				switch(k_mod_mode) {
				case modNone:
					if(z) {
						if(note_sync) {
							if(k.p[k.pattern].t[track].tr[x] && k.p[k.pattern].t[track].note[x] == 6-y)
								k.p[k.pattern].t[track].tr[x] = 0;
							else {
								k.p[k.pattern].t[track].tr[x] = 1;
								k.p[k.pattern].t[track].note[x] = 6-y;
							}
						}
						else
							k.p[k.pattern].t[track].note[x] = 6-y;
						monomeFrameDirty++;
					}
					break;
				case modLoop:
					if(z) {
						if(loop_count == 0) {
							loop_first = x;
							loop_last = -1;
						}
						else {
							loop_last = x;
							update_loop_start(track, loop_first, mNote);
							update_loop_end(track, loop_last, mNote);
						}

						loop_count++;
					}
					else {
						loop_count--;

						if(loop_count == 0) {
							if(loop_last == -1) {
								if(loop_first == k.p[k.pattern].t[track].lstart[mNote]) {
									update_loop_start(track, loop_first, mNote);
									update_loop_end(track, loop_first, mNote);
								}
								else
									update_loop_start(track, loop_first, mNote);
							}
							monomeFrameDirty++;
						}
					}
					break;
				case modTime:
					if(z) {
						k.p[k.pattern].t[track].tmul[mNote] = x + 1;
						monomeFrameDirty++;
					}
					break;
				case modProb:
					if(z && y > 1 && y < 6) {
						k.p[k.pattern].t[track].p[mNote][x] = 5 - y;
						monomeFrameDirty++;
					}
					break;
				default: break;
				}
				break;
			case mOct:
				switch(k_mod_mode) {
				case modNone:
					if(z) {
						if(y>2)
							k.p[k.pattern].t[track].oct[x] = 6-y;
						monomeFrameDirty++;
					}
					break;
				case modLoop:
					if(z) {
						if(loop_count == 0) {
							loop_first = x;
							loop_last = -1;
						}
						else {
							loop_last = x;
							update_loop_start(track, loop_first, mOct);
							update_loop_end(track, loop_last, mOct);
						}

						loop_count++;
					}
					else {
						loop_count--;

						if(loop_count == 0) {
							if(loop_last == -1) {
								if(loop_first == k.p[k.pattern].t[track].lstart[mOct]) {
									update_loop_start(track, loop_first, mOct);
									update_loop_end(track, loop_first, mOct);
								}
								else
									update_loop_start(track, loop_first, mOct);
							}
							monomeFrameDirty++;
						}
					}
					break;
				case modTime:
					if(z) {
						k.p[k.pattern].t[track].tmul[mOct] = x + 1;
						monomeFrameDirty++;
					}
					break;
				case modProb:
					if(z && y > 1 && y < 6) {
						k.p[k.pattern].t[track].p[mOct][x] = 5 - y;
						monomeFrameDirty++;
					}
					break;
				default: break;
				}
				break;
			case mDur:
				switch(k_mod_mode) {
				case modNone:
					if(z) {
						if(y==0)
							k.p[k.pattern].t[track].dur_mul = x+1;
						else
							k.p[k.pattern].t[track].dur[x] = y-1;
						monomeFrameDirty++;
					}
					break;
				case modLoop:
					if(y>0) {
						if(z) {
							if(loop_count == 0) {
								loop_first = x;
								loop_last = -1;
							}
							else {
								loop_last = x;
								update_loop_start(track, loop_first, mDur);
								update_loop_end(track, loop_last, mDur);
							}

							loop_count++;
						}
						else {
							loop_count--;

							if(loop_count == 0) {
								if(loop_last == -1) {
									if(loop_first == k.p[k.pattern].t[track].lstart[mDur]) {
										update_loop_start(track, loop_first, mDur);
										update_loop_end(track, loop_first, mDur);
									}
									else
										update_loop_start(track, loop_first, mDur);
								}
								monomeFrameDirty++;
							}
						}
					}
					break;
				case modTime:
					if(z) {
						k.p[k.pattern].t[track].tmul[mDur] = x + 1;
						monomeFrameDirty++;
					}
					break;
				case modProb:
					if(z && y > 1 && y < 6) {
						k.p[k.pattern].t[track].p[mDur][x] = 5 - y;
						monomeFrameDirty++;
					}
					break;
				default: break;
				}
				break;
			case mScale:
				if(z) {
					// tt clocking stuff added here
					if ( y == 0 && x < 4 )
					{
						kria_tt_clocked[x] = !kria_tt_clocked[x];
					}
					else if(x < 8) {
						if(y > 4)
							k.p[k.pattern].scale = (y - 5) * 8 + x;
					}
					else {
						scale_data[k.p[k.pattern].scale][6-y] = x-8;
					}

					calc_scale(k.p[k.pattern].scale);

					monomeFrameDirty++;
				}
				break;
			case mPattern:
				if(y > 1 && y < 6) {
					if(z && cue) {
						meta_next = (y-2) * 16 + x + 1;
						if(meta_next - 1 > k.meta_end) {
							meta_next = 0;
						}
					}
					else if(k_mod_mode == modLoop) {
						if(z) {
							if(loop_count == 0) {
								loop_first = (y-2) * 16 + x;
								loop_last = -1;
							}
							else {
								loop_last = (y-2) * 16 + x;
								update_meta_start(loop_first);
								update_meta_end(loop_last);
							}
							loop_count++;
						}
						else {
							loop_count--;
							if(loop_count == 0) {
								if(loop_last == -1) {
									if(loop_first == k.meta_start) {
										update_meta_start(loop_first);
										update_meta_end(loop_first);
									}
									else
										update_meta_start(loop_first);
								}
								monomeFrameDirty++;
							}
						}
					}
					else if(z) {
						meta_edit = (y-2) * 16 + x;
					}
				}
				else if(z && y == 6) {
					if(cue) {
						meta ^= 1;
					}
					else {
						k.meta_steps[meta_edit] = x;
					}
				}
				else if(z && y == 1) {
					switch(k_mod_mode) {
					case modTime:
						cue_div = x;
						break;
					default:
						cue_steps = x;
						break;
					}
				}
				monomeFrameDirty++;
				break;

			default: break;
			}
		}

	}
}

static void update_meta_start(u8 x) {
	s16 temp;

	temp = meta_pos - k.meta_start + x;
	if(temp < 0) temp += 64;
	else if(temp > 63) temp -= 64;
	meta_next = temp + 1;

	k.meta_start = x;
	temp = x + k.meta_len-1;
	if(temp > 63) {
		k.meta_end = temp - 64;
		k.meta_lswap = 1;
	}
	else {
		k.meta_end = temp;
		k.meta_lswap = 0;
	}
}

static void update_meta_end(u8 x) {
	s16 temp;

	k.meta_end = x;
	temp = k.meta_end - k.meta_start;
	if(temp < 0) {
		k.meta_len = temp + 65;
		k.meta_lswap = 1;
	}
	else {
		k.meta_len = temp+1;
		k.meta_lswap = 0;
	}

	temp = meta_pos;
	if(k.meta_lswap) {
		if(temp < k.meta_start && temp > k.meta_end)
			meta_next = k.meta_start + 1;
	}
	else {
		if(temp < k.meta_start || temp > k.meta_end)
			meta_next = k.meta_start + 1;
	}
}

static void adjust_loop_start(u8 t, u8 x, u8 m) {
	s8 temp;

	temp = pos[t][m] - k.p[k.pattern].t[t].lstart[m] + x;
	if(temp < 0) temp += 16;
	else if(temp > 15) temp -= 16;
	pos[t][m] = temp;

	k.p[k.pattern].t[t].lstart[m] = x;
	temp = x + k.p[k.pattern].t[t].llen[m]-1;
	if(temp > 15) {
		k.p[k.pattern].t[t].lend[m] = temp - 16;
		k.p[k.pattern].t[t].lswap[m] = 1;
	}
	else {
		k.p[k.pattern].t[t].lend[m] = temp;
		k.p[k.pattern].t[t].lswap[m] = 0;
	}
}

static void adjust_loop_end(u8 t, u8 x, u8 m) {
	s8 temp;

	k.p[k.pattern].t[t].lend[m] = x;
	temp = k.p[k.pattern].t[t].lend[m] - k.p[k.pattern].t[t].lstart[m];
	if(temp < 0) {
		k.p[k.pattern].t[t].llen[m] = temp + 17;
		k.p[k.pattern].t[t].lswap[m] = 1;
	}
	else {
		k.p[k.pattern].t[t].llen[m] = temp+1;
		k.p[k.pattern].t[t].lswap[m] = 0;
	}

	temp = pos[t][m];
	if(k.p[k.pattern].t[t].lswap[m]) {
		if(temp < k.p[k.pattern].t[t].lstart[m] && temp > k.p[k.pattern].t[t].lend[m])
			pos[t][m] = k.p[k.pattern].t[t].lstart[m];
	}
	else {
		if(temp < k.p[k.pattern].t[t].lstart[m] || temp > k.p[k.pattern].t[t].lend[m])
			pos[t][m] = k.p[k.pattern].t[t].lstart[m];
	}
}

static void adjust_loop_len(u8 t, u8 x, u8 m) {
	adjust_loop_end(t, (x - 1 + k.p[k.pattern].t[t].lstart[m]) & 0xf, m);
}

static void update_loop_start(u8 t, u8 x, u8 m) {
	int i1, i2;
	switch(loop_sync) {
		case 1:
			for(i1=0;i1<KRIA_NUM_PARAMS;i1++)
				adjust_loop_start(t,x,i1);
			break;
		case 2:
			for(i2=0;i2<4;i2++)
			for(i1=0;i1<KRIA_NUM_PARAMS;i1++)
				adjust_loop_start(i2,x,i1);
			break;
		case 0:
			adjust_loop_start(t,x,m);
			break;
		default:
			break;
	}
}

static void update_loop_end(u8 t, u8 x, u8 m) {
	int i1, i2;
	switch(loop_sync) {
		case 1:
			for(i1=0;i1<KRIA_NUM_PARAMS;i1++)
				adjust_loop_end(t,x,i1);
			break;
		case 2:
			for(i2=0;i2<4;i2++)
			for(i1=0;i1<KRIA_NUM_PARAMS;i1++)
				adjust_loop_end(i2,x,i1);
			break;
		case 0:
			adjust_loop_end(t,x,m);
			break;
		default:
			break;
	}
}

static void jump_pos(u8 t, u8 x, u8 m) {
	pos[t][m] = (x + 15) & 0xf;
}

void handler_KriaRefresh(s32 data) {
	if(monomeFrameDirty) {
		grid_refresh();

		monome_set_quadrant_flag(0);
		monome_set_quadrant_flag(1);
		(*monome_refresh)();
	}
}

void handler_KriaKey(s32 data) {
	// print_dbg("\r\n> kria key");
	// print_dbg_ulong(data);

	switch(data) {
	case 0:
		grid_refresh = &refresh_kria;
		view_clock = false;
		break;
	case 1:
		grid_refresh = &refresh_clock;
		// print_dbg("\r\ntime: ");
		// print_dbg_ulong(time_fine);
		// print_dbg(" ");
		// print_dbg_ulong(time_rough);
		view_clock = true;
		view_config = false;
		break;
	case 2:
		grid_refresh = &refresh_kria;
		view_config = false;
		break;
	case 3:
		grid_refresh = &refresh_kria_config;
		view_config = true;
		view_clock = false;
		break;
	default:
		break;
	}

	monomeFrameDirty++;
}

void handler_KriaTr(s32 data) {
	// print_dbg("\r\n> kria tr ");
	// print_dbg_ulong(data);

	switch(data) {
	case 0:
		if(clock_mul == 1)
			clock_kria(0);
		break;
	case 1:
		if(clock_mul == 1)
			clock_kria(1);
		else {
			ext_clock_count++;
			if(ext_clock_count >= clock_mul - 1) {
				ext_clock_count = 0;
				ext_clock_phase ^= 1;
				clock_kria(ext_clock_phase);
			}
		}
		break;
	case 3:
		pos_reset = true;
		break;
	default:
		break;
	}

	monomeFrameDirty++;
}

void handler_KriaTrNormal(s32 data) {
	// print_dbg("\r\n> kria tr normal ");
	// print_dbg_ulong(data);

	clock_external = data;

	if(clock_external)
		clock = &clock_null;
	else
		clock = &clock_kria;

	monomeFrameDirty++;
}

void refresh_kria(void) {
	u8 i1,i2;

	memset(monomeLedBuffer,0,128);

	// bottom strip
	memset(monomeLedBuffer + R7 + 5, L0, 4);
	monomeLedBuffer[R7 + 10] = L0;
	monomeLedBuffer[R7 + 11] = L0;
	monomeLedBuffer[R7 + 12] = L0;
	monomeLedBuffer[R7 + 14] = L0;
	monomeLedBuffer[R7 + 15] = L0;

	for ( i1=0; i1<4; i1++ )
	{
		if ( kria_mutes[i1] )
			monomeLedBuffer[R7+i1] = (track == i1) ? L1 : 2;
		else
			monomeLedBuffer[R7+i1] = (track == i1) ? L2 : L0;
		
		// when a blink is happening, the LED is 2 brighter (but not when muted)
		if ( !kria_mutes[i1] )
			monomeLedBuffer[R7+i1] += kria_blinks[i1]*2;
	}



	switch(k_mode) {
	case mTr:
		i1 = 5; break;
	case mNote:
		i1 = 6; break;
	case mOct:
		i1 = 7; break;
	case mDur:
		i1 = 8; break;
	case mScale:
		i1 = 14; break;
	case mPattern:
		i1 = 15; break;
	default:
		i1 = 0; break;
	}

	monomeLedBuffer[R7 + i1] = L2;

	if(k_mod_mode == modLoop)
		monomeLedBuffer[R7 + 10] = L1;
	else if(k_mod_mode == modTime)
		monomeLedBuffer[R7 + 11] = L1;
	else if(k_mod_mode == modProb)
		monomeLedBuffer[R7 + 12] = L1;



	// modes

	switch(k_mode) {
	case mTr:
		switch(k_mod_mode) {
		case modTime:
			memset(monomeLedBuffer + R1, 3, 16);
			monomeLedBuffer[R1 + k.p[k.pattern].t[track].tmul[mTr] - 1] = L1;
			break;
		case modProb:
			memset(monomeLedBuffer + R5, 3, 16);
			for(i1=0;i1<16;i1++)
				if(k.p[k.pattern].t[track].p[mTr][i1])
					monomeLedBuffer[(5 - k.p[k.pattern].t[track].p[mTr][i1]) * 16 + i1] = 6;
			break;
		default:
			// steps
			for(i2=0;i2<4;i2++) {
				for(i1=0;i1<16;i1++) {
					if(k.p[k.pattern].t[i2].tr[i1])
						monomeLedBuffer[i2*16 + i1] = 3;
				}
				// playhead
				// if(tr[i2])
				monomeLedBuffer[i2*16 + pos[i2][mTr]] += 4;
			}

			// loop highlight
			for(i1=0;i1<4;i1++) {
				if(k.p[k.pattern].t[i1].lswap[mTr]) {
					for(i2=0;i2<k.p[k.pattern].t[i1].llen[mTr];i2++)
						monomeLedBuffer[i1*16 + (i2+k.p[k.pattern].t[i1].lstart[mTr])%16] += 2 + (k_mod_mode == modLoop);
				}
				else {
					for(i2=k.p[k.pattern].t[i1].lstart[mTr];i2<=k.p[k.pattern].t[i1].lend[mTr];i2++)
						monomeLedBuffer[i1*16 + i2] += 2 + (k_mod_mode == modLoop);
				}
			}
			break;
		}
		break;
	case mNote:
		switch(k_mod_mode) {
		case modTime:
			memset(monomeLedBuffer + R1, 3, 16);
			monomeLedBuffer[R1 + k.p[k.pattern].t[track].tmul[mNote] - 1] = L1;
			break;
		case modProb:
			memset(monomeLedBuffer + R5, 3, 16);
			for(i1=0;i1<16;i1++)
				if(k.p[k.pattern].t[track].p[mNote][i1])
					monomeLedBuffer[(5 - k.p[k.pattern].t[track].p[mNote][i1]) * 16 + i1] = 6;
			break;
		default:
			if(note_sync) {
				for(i1=0;i1<16;i1++)
					monomeLedBuffer[i1 + (6 - k.p[k.pattern].t[track].note[i1] ) * 16] =
						k.p[k.pattern].t[track].tr[i1] * 3;
			}
			else {
				for(i1=0;i1<16;i1++)
					monomeLedBuffer[i1 + (6 - k.p[k.pattern].t[track].note[i1] ) * 16] = 3;
			}

			monomeLedBuffer[pos[track][mNote] + (6-k.p[k.pattern].t[track].note[pos[track][mNote]])*16] += 4;

			if(k.p[k.pattern].t[track].lswap[mNote]) {
				for(i1=0;i1<k.p[k.pattern].t[track].llen[mNote];i1++)
					monomeLedBuffer[((i1+k.p[k.pattern].t[track].lstart[mNote])%16)+
						(6-k.p[k.pattern].t[track].note[i1])*16] += 3 + (k_mod_mode == modLoop)*2;
					// monomeLedBuffer[i1*16 + (i2+k.p[k.pattern].t[i1].lstart[mTr])%16] += 2 + (k_mod_mode == modLoop);
			}
			else {
				for(i1=k.p[k.pattern].t[track].lstart[mNote];i1<=k.p[k.pattern].t[track].lend[mNote];i1++)
					monomeLedBuffer[i1+(6-k.p[k.pattern].t[track].note[i1])*16] += 3 + (k_mod_mode == modLoop)*2;
			}
			break;
		}
		break;
	case mOct:
		switch(k_mod_mode) {
		case modTime:
			memset(monomeLedBuffer + R1, 3, 16);
			monomeLedBuffer[R1 + k.p[k.pattern].t[track].tmul[mOct] - 1] = L1;
			break;
		case modProb:
			memset(monomeLedBuffer + R5, 3, 16);
			for(i1=0;i1<16;i1++)
				if(k.p[k.pattern].t[track].p[mOct][i1])
					monomeLedBuffer[(5 - k.p[k.pattern].t[track].p[mOct][i1]) * 16 + i1] = 6;
			break;
		default:
			for(i1=0;i1<16;i1++) {
					for(i2=0;i2<=k.p[k.pattern].t[track].oct[i1];i2++)
						monomeLedBuffer[R6-16*i2+i1] = L0;

					if(i1 == pos[track][mOct])
						monomeLedBuffer[R6 - k.p[k.pattern].t[track].oct[i1]*16 + i1] += 4;
				}

			if(k.p[k.pattern].t[track].lswap[mOct]) {
				for(i1=0;i1<16;i1++)
					if((i1 < k.p[k.pattern].t[track].lstart[mOct]) && (i1 > k.p[k.pattern].t[track].lend[mOct]))
						for(i2=0;i2<=k.p[k.pattern].t[track].oct[i1];i2++)
							monomeLedBuffer[R6-16*i2+i1] -= 2;
			}
			else {
				for(i1=0;i1<16;i1++)
					if((i1 < k.p[k.pattern].t[track].lstart[mOct]) || (i1 > k.p[k.pattern].t[track].lend[mOct]))
						for(i2=0;i2<=k.p[k.pattern].t[track].oct[i1];i2++)
							monomeLedBuffer[R6-16*i2+i1] -= 2;
			}
			break;
		}
		break;
	case mDur:
		switch(k_mod_mode) {
		case modTime:
			memset(monomeLedBuffer + R1, 3, 16);
			monomeLedBuffer[R1 + k.p[k.pattern].t[track].tmul[mDur] - 1] = L1;
			break;
		case modProb:
			memset(monomeLedBuffer + R5, 3, 16);
			for(i1=0;i1<16;i1++)
				if(k.p[k.pattern].t[track].p[mDur][i1])
					monomeLedBuffer[(5 - k.p[k.pattern].t[track].p[mDur][i1]) * 16 + i1] = 6;
			break;
		default:
			monomeLedBuffer[k.p[k.pattern].t[track].dur_mul - 1] = L1;

			for(i1=0;i1<16;i1++) {
				for(i2=0;i2<=k.p[k.pattern].t[track].dur[i1];i2++)
					monomeLedBuffer[R1+16*i2+i1] = L0;

				if(i1 == pos[track][mDur])
					monomeLedBuffer[R1+i1+16*k.p[k.pattern].t[track].dur[i1]] += 4;
			}

			if(k.p[k.pattern].t[track].lswap[mDur]) {
				for(i1=0;i1<16;i1++)
					if((i1 < k.p[k.pattern].t[track].lstart[mDur]) && (i1 > k.p[k.pattern].t[track].lend[mDur]))
						for(i2=0;i2<=k.p[k.pattern].t[track].dur[i1];i2++)
							monomeLedBuffer[R1+16*i2+i1] -= 2;
			}
			else {
				for(i1=0;i1<16;i1++)
					if((i1 < k.p[k.pattern].t[track].lstart[mDur]) || (i1 > k.p[k.pattern].t[track].lend[mDur]))
						for(i2=0;i2<=k.p[k.pattern].t[track].dur[i1];i2++)
							monomeLedBuffer[R1+16*i2+i1] -= 2;
			}
			break;
		}
		break;
	case mScale:
		// shoehorning my track clocking feature here 
		for ( uint8_t i=0; i<4; i++ )
		{
			// if teletype clocking is enabled, its brighter
			monomeLedBuffer[i] = kria_tt_clocked[i] ? L1 : L0;
		}

		// vertical bar dividing the left and right half
		for(i1=0;i1<7;i1++)
			monomeLedBuffer[8+16*i1] = L0;
		// the two rows of scale selecting buttons 
		for(i1=0;i1<8;i1++) {
			monomeLedBuffer[R5 + i1] = 2;
			monomeLedBuffer[R6 + i1] = 2;
		}
		// highlight the selected scale
		monomeLedBuffer[R5 + (k.p[k.pattern].scale >> 3) * 16 + (k.p[k.pattern].scale & 0x7)] = L1;

		// the intervals of the selected scale
		for(i1=0;i1<7;i1++)
			monomeLedBuffer[scale_data[k.p[k.pattern].scale][i1] + 8 + (6-i1)*16] = L1;

		// if an active step of a track is playing a note, it brightness is incremented by one
		for(i1=0;i1<4;i1++) {
			if(k.p[k.pattern].t[i1].tr[pos[i1][mTr]])
				monomeLedBuffer[scale_data[k.p[k.pattern].scale][note[i1]] + 8 + (6-note[i1])*16]++;
		}
		break;
	case mPattern:
		if(!meta) {
			memset(monomeLedBuffer, 3, 16);
			monomeLedBuffer[k.pattern] = L1;
		}
		else {
			// bar
			memset(monomeLedBuffer + 96, 3, k.meta_steps[meta_pos]+1);
			monomeLedBuffer[96 + meta_count] = L1;
			monomeLedBuffer[96 + k.meta_steps[meta_edit]] = L2;
			// top
			monomeLedBuffer[k.pattern] = L0;
			monomeLedBuffer[k.meta_pat[meta_edit]] = L1;
			// meta data
			if(!k.meta_lswap)
				memset(monomeLedBuffer + 32 + k.meta_start, 3, k.meta_len);
			else {
				memset(monomeLedBuffer + 32, 3, k.meta_end);
				memset(monomeLedBuffer + 32 + k.meta_start, 3, 64 - k.meta_start);
			}
			monomeLedBuffer[32 + meta_pos] = L1;
			monomeLedBuffer[32 + meta_edit] = L2;
			if(meta_next) {
				monomeLedBuffer[32 + meta_next - 1] = L2;
				monomeLedBuffer[k.meta_pat[meta_next] - 1] = L2;
			}
		}
		if(cue_pat_next) {
			monomeLedBuffer[cue_pat_next-1] = L2;
		}
		switch(k_mod_mode) {
			case modTime:
			monomeLedBuffer[16 + cue_count] = L0;
			monomeLedBuffer[16 + cue_div] = L1;
			break;
		default:
			monomeLedBuffer[16 + cue_steps] = L0;
			monomeLedBuffer[16 + cue_count] = L1;
			break;
		}
	default: break;
	}
}

void refresh_kria_config(void) {
	// clear grid
	memset(monomeLedBuffer,0,128);

	uint8_t i = note_sync * 4 + 3;

	monomeLedBuffer[R2 + 2] = i;
	monomeLedBuffer[R2 + 3] = i;
	monomeLedBuffer[R2 + 4] = i;
	monomeLedBuffer[R2 + 5] = i;
	monomeLedBuffer[R3 + 2] = i;
	monomeLedBuffer[R3 + 5] = i;
	monomeLedBuffer[R4 + 2] = i;
	monomeLedBuffer[R4 + 5] = i;
	monomeLedBuffer[R5 + 2] = i;
	monomeLedBuffer[R5 + 3] = i;
	monomeLedBuffer[R5 + 4] = i;
	monomeLedBuffer[R5 + 5] = i;

	i = (loop_sync == 1) * 4 + 3;

	monomeLedBuffer[R3 + 10] = i;

	i = (loop_sync == 2) * 4 + 3;

	monomeLedBuffer[R5 + 10] = i;
	monomeLedBuffer[R5 + 11] = i;
	monomeLedBuffer[R5 + 12] = i;
	monomeLedBuffer[R5 + 13] = i;
}

////////////////////////////////////////////////////////////////////////////////
// MP

#define MP_1V 0
#define MP_2V 1
#define MP_4V 2
#define MP_8T 3


u8 edit_row;
u8 mode = 0;
u8 prev_mode = 0;
s8 kcount = 0;
s8 scount[8];
u8 state[8];
u8 pstate[8];
u8 clear[8];
s8 position[8];		// current position in cycle
u8 tick[8]; 		// position in speed countdown
u8 pushed[8];		// manual key reset
u8 reset[8];

s8 note_now[4];
u16 note_age[4];


const u8 sign[8][8] = {{0,0,0,0,0,0,0,0},       // o
       {0,24,24,126,126,24,24,0},     			// +
       {0,0,0,126,126,0,0,0},       			// -
       {0,96,96,126,126,96,96,0},     			// >
       {0,6,6,126,126,6,6,0},       			// <
       {0,102,102,24,24,102,102,0},   			// * rnd
       {0,120,120,102,102,30,30,0},   			// <> up/down
       {0,126,126,102,102,126,126,0}};  		// [] sync

uint8_t get_note_slot(uint8_t v);
void mp_note_on(uint8_t n);
void mp_note_off(uint8_t n);


void default_mp() {
	uint8_t i1, i2;

	flashc_memset8((void*)&(f.mp_state.preset), 0, 1, true);
	flashc_memset8((void*)&(f.mp_state.sound), 0, 1, true);
	flashc_memset8((void*)&(f.mp_state.voice_mode), 0, 1, true);

	for(i1=0;i1<8;i1++) {
		m.count[i1] = 7+i1;
		m.speed[i1] = 0;
		m.min[i1] = 7+i1;
		m.max[i1] = 7+i1;
		m.trigger[i1] = (1<<i1);
		m.toggle[i1] = 0;
		m.rules[i1] = 1;
		m.rule_dests[i1] = i1;
		m.sync[i1] = (1<<i1);
		m.rule_dest_targets[i1] = 3;
		m.smin[i1] = 0;
		m.smax[i1] = 0;
	}

	for(i1=0;i1<8;i1++)
		m.glyph[i1] = 0;

	for(i1=0;i1<GRID_PRESETS;i1++)
		flashc_memcpy((void *)&f.mp_state.m[i1], &m, sizeof(m), true);

	// default scales
	for(i1=0;i1<7;i1++) {
		flashc_memset8((void*)&(f.scale[i1][0]), 0, 1, true);
		for(i2=0;i2<7;i2++)
			flashc_memset8((void*)&(f.scale[i1][i2+1]), SCALE_INT[i1][i2], 1, true);
	}
	for(i1=7;i1<16;i1++) {
		flashc_memset8((void*)&(f.scale[i1][0]), 0, 1, true);
		for(i2=0;i2<7;i2++)
			flashc_memset8((void*)&(f.scale[i1][i2+1]), 1, 1, true);
	}
}

void init_mp() {
	sound = f.mp_state.sound;
	voice_mode = f.mp_state.voice_mode;

	preset = f.mp_state.preset;

	for(uint8_t i1=0;i1<8;i1++) {
		m = f.mp_state.m[preset];

		// m.count[i1] = f.mp_state.m[preset].count[i1];
		// m.speed[i1] = f.mp_state.m[preset].speed[i1];
		// m.min[i1] = f.mp_state.m[preset].min[i1];
		// m.max[i1] = f.mp_state.m[preset].max[i1];
		// m.trigger[i1] = f.mp_state.m[preset].trigger[i1];
		// m.toggle[i1] = f.mp_state.m[preset].toggle[i1];
		// m.rules[i1] = f.mp_state.m[preset].rules[i1];
		// m.rule_dests[i1] = f.mp_state.m[preset].rule_dests[i1];
		// m.sync[i1] = f.mp_state.m[preset].sync[i1];
		// m.rule_dest_targets[i1] = f.mp_state.m[preset].rule_dest_targets[i1];
		// m.smin[i1] = f.mp_state.m[preset].smin[i1];
		// m.smax[i1] = f.mp_state.m[preset].smax[i1];

		position[i1] = f.mp_state.m[preset].count[i1];
		tick[i1] = 0;
		pushed[i1] = 0;
		scount[i1] = 0;
		clear[i1] = 0;
		state[i1] = 0;
	}

	// for(uint8_t i1=0;i1<8;i1++)
		// m.glyph[i1] = f.mp_state.m[preset].glyph[i1];

	m.scale = f.mp_state.m[preset].scale;

	memcpy(scale_data, f.scale, sizeof(scale_data));

	calc_scale(m.scale);

	note_now[0] = -1;
	note_now[1] = -1;
	note_now[2] = -1;
	note_now[3] = -1;
}

void resume_mp() {
	grid_refresh = &refresh_mp;
	view_clock = false;
	view_config = false;
	preset_mode = false;

	preset = f.mp_state.preset;

	// re-check clock jack
	clock_external = !gpio_get_pin_value(B10);

	if(clock_external)
		clock = &clock_null;
	else
		clock = &clock_mp;

	dac_set_slew(0,0);
	dac_set_slew(1,0);
	dac_set_slew(2,0);
	dac_set_slew(3,0);

	clr_tr(TR1);
	clr_tr(TR2);
	clr_tr(TR3);
	clr_tr(TR4);

	monomeFrameDirty++;
}

void clock_mp(uint8_t phase) {
	static u8 i;

	if(phase) {
		clock_count++;

		memcpy(pstate, state, 8);
		// gpio_set_gpio_pin(B10);

		for(i=0;i<8;i++) {
			if(pushed[i]) {
				for(int n=0;n<8;n++) {
					if(m.sync[i] & (1<<n)) {
						reset[n] = 1;
					}

					if(m.trigger[i] & (1<<n)) {
						state[n] = 1;
						clear[n] = 1;
					}
					else if(m.toggle[i] & (1<<n)) {
						state[n] ^= 1;
					}
				}

				pushed[i] = 0;
			}

			if(tick[i] == 0) {
				tick[i] = m.speed[i];
				if(position[i] == 0) {
					// RULES
				    if(m.rules[i] == 1) {     // inc
				    	if(m.rule_dest_targets[i] & 1) {
					    	m.count[m.rule_dests[i]]++;
					    	if(m.count[m.rule_dests[i]] > m.max[m.rule_dests[i]]) {
					    		m.count[m.rule_dests[i]] = m.min[m.rule_dests[i]];
					    	}
					    }
					    if(m.rule_dest_targets[i] & 2) {
					    	m.speed[m.rule_dests[i]]++;
					    	if(m.speed[m.rule_dests[i]] > m.smax[m.rule_dests[i]]) {
					    		m.speed[m.rule_dests[i]] = m.smin[m.rule_dests[i]];
					    	}
					    }
				    }
				    else if(m.rules[i] == 2) {  // dec
				    	if(m.rule_dest_targets[i] & 1) {
				    		m.count[m.rule_dests[i]]--;
					    	if(m.count[m.rule_dests[i]] < m.min[m.rule_dests[i]]) {
					    		m.count[m.rule_dests[i]] = m.max[m.rule_dests[i]];
					    	}
					    }
					    if(m.rule_dest_targets[i] & 2) {
					    	m.speed[m.rule_dests[i]]--;
					    	if(m.speed[m.rule_dests[i]] < m.smin[m.rule_dests[i]]) {
					    		m.speed[m.rule_dests[i]] = m.smax[m.rule_dests[i]];
					    	}
					    }
				    }
				    else if(m.rules[i] == 3) {  // max
				    	if(m.rule_dest_targets[i] & 1)
				    		m.count[m.rule_dests[i]] = m.max[m.rule_dests[i]];
				    	if(m.rule_dest_targets[i] & 2)
				    		m.speed[m.rule_dests[i]] = m.smax[m.rule_dests[i]];
				    }
				    else if(m.rules[i] == 4) {  // min
				    	if(m.rule_dest_targets[i] & 1)
					    	m.count[m.rule_dests[i]] = m.min[m.rule_dests[i]];
					    if(m.rule_dest_targets[i] & 2)
					    	m.speed[m.rule_dests[i]] = m.smin[m.rule_dests[i]];
				    }
				    else if(m.rules[i] == 5) {  // rnd
				    	if(m.rule_dest_targets[i] & 1)
				    		m.count[m.rule_dests[i]] =
				    			(rnd() % (m.max[m.rule_dests[i]] - m.min[m.rule_dests[i]] + 1)) + m.min[m.rule_dests[i]];
				    	if(m.rule_dest_targets[i] & 2)
				    		m.speed[m.rule_dests[i]] =
				    			(rnd() % (m.smax[m.rule_dests[i]] - m.smin[m.rule_dests[i]] + 1)) + m.smin[m.rule_dests[i]];

				      // print_dbg("\r\n RANDOM: ");
				      // print_dbg_hex(m.count[m.rule_dests[i]]);
				      // print_dbg_hex(rnd() % 11);
				    }
				    else if(m.rules[i] == 6) {  // pole
				    	if(m.rule_dest_targets[i] & 1) {
					    	if(abs(m.count[m.rule_dests[i]] - m.min[m.rule_dests[i]]) <
					    		abs(m.count[m.rule_dests[i]] - m.max[m.rule_dests[i]]) ) {
					    		m.count[m.rule_dests[i]] = m.max[m.rule_dests[i]];
					    	}
					    	else {
					    		m.count[m.rule_dests[i]] = m.min[m.rule_dests[i]];
					    	}
					    }
					    if(m.rule_dest_targets[i] & 2) {
					    	if(abs(m.speed[m.rule_dests[i]] - m.smin[m.rule_dests[i]]) <
					    		abs(m.speed[m.rule_dests[i]] - m.smax[m.rule_dests[i]]) ) {
					    		m.speed[m.rule_dests[i]] = m.smax[m.rule_dests[i]];
					    	}
					    	else {
					    		m.speed[m.rule_dests[i]] = m.smin[m.rule_dests[i]];
					    	}
					    }
				    }
				    else if(m.rules[i] == 7) {  // stop
				    	if(m.rule_dest_targets[i] & 1)
				    		position[m.rule_dests[i]] = -1;
				    }

					position[i]--;

					for(int n=0;n<8;n++) {
						if(m.sync[i] & (1<<n)) {
							reset[n] = 1;
							// position[n] = m.count[n];
							// tick[n] = m.speed[n];
						}

						if(m.trigger[i] & (1<<n)) {
							state[n] = 1;
							clear[n] = 1;
						}
						else if(m.toggle[i] & (1<<n)) {
							state[n] ^= 1;
						}
					}
				}
				else if(position[i] > 0) position[i]--;
			}
			else tick[i]--;
		}

		for(i=0;i<8;i++) {
			if(reset[i]) {
				position[i] = m.count[i];
				tick[i] = m.speed[i];
				reset[i] = 0;
			}
		}

		for(i=0;i<8;i++)
			if(state[i] && !pstate[i])
				mp_note_on(i);
				// gpio_set_gpio_pin(outs[i]);
			else if(!state[i] && pstate[i])
				mp_note_off(i);
				// gpio_clr_gpio_pin(outs[i]);

		monomeFrameDirty++;
	}
	else {
		// gpio_clr_gpio_pin(B10);

		for(i=0;i<8;i++) {
			if(clear[i]) {
				mp_note_off(i);
				// gpio_clr_gpio_pin(outs[i]);
				state[i] = 0;
			}
			clear[i] = 0;
		}
 	}
}

uint8_t get_note_slot(uint8_t v) {
	int8_t w = -1;

	for(int i1=0;i1<v;i1++)
		note_age[i1]++;

	// find empty
	for(int i1=0;i1<v;i1++)
		if(note_now[i1] == -1) {
			w = i1;
			break;
		}

	if(w == -1) {
		w = 0;
		for(int i1=1;i1<v;i1++)
			if(note_age[w] < note_age[i1])
				w = i1;
	}

	note_age[w] = 1;

	return w;
}

void mp_note_on(uint8_t n) {
	uint8_t w;
	// print_dbg("\r\nmp note on: ");
	// print_dbg_ulong(n);
	switch(voice_mode) {
	case MP_8T:
			if(n < 4)
				set_tr(TR1 + n);
			else
				dac_set_value(n-4, DAC_10V);
		break;
	case MP_1V:
		note_now[0] = n;
		dac_set_value(0, ET[cur_scale[7-n]] << 2);
		set_tr(TR1);
		break;
	case MP_2V:
		w = get_note_slot(2);
		note_now[w] = n;
		dac_set_value(w, ET[cur_scale[7-n]] << 2);
		set_tr(TR1 + w);
		break;
	case MP_4V:
		w = get_note_slot(4);
		note_now[w] = n;
		dac_set_value(w, ET[cur_scale[7-n]] << 2);
		set_tr(TR1 + w);
		break;
	default:
		break;
	}
}

void mp_note_off(uint8_t n) {
	// print_dbg("\r\nmp note off: ");
	// print_dbg_ulong(n);
	switch(voice_mode) {
	case MP_8T:
			if(n < 4)
				clr_tr(TR1 + n);
			else
				dac_set_value(n-4, 0);
		break;
	case MP_1V:
		if(note_now[0] == n) {
			note_now[0] = -1;
			clr_tr(TR1);
		}
		break;
	case MP_2V:
		for(int i1=0;i1<2;i1++) {
			if(note_now[i1] == n) {
				note_now[i1] = -1;
				clr_tr(TR1 + i1);
			}
		}
		break;
	case MP_4V:
		for(int i1=0;i1<4;i1++) {
			if(note_now[i1] == n) {
				note_now[i1] = -1;
				clr_tr(TR1 + i1);
			}
		}
		break;
	default:
		break;
	}
}

void ii_mp(uint8_t *d, uint8_t l) {
	// print_dbg("\r\nii/mp (");
	// print_dbg_ulong(l);
	// print_dbg(") ");
	// for(int i=0;i<l;i++) {
	// 	print_dbg_ulong(d[i]);
	// 	print_dbg(" ");
	// }

	int n;

	if(l) {
		switch(d[0]) {
		case II_MP_PRESET:
			if(d[1] > -1 && d[1] < 8) {
				preset = d[1];
				flashc_memset8((void*)&(f.mp_state.preset), preset, 1, true);
				init_mp();
			}
			break;
		case II_MP_PRESET + II_GET:
			ii_tx_queue(preset);
			break;
		case II_MP_SCALE:
			if(d[1] > -1 && d[1] < 16) {
				m.scale = d[1];
				calc_scale(m.scale);
			}
			break;
		case II_MP_SCALE + II_GET:
			ii_tx_queue(m.scale);
			break;
		case II_MP_PERIOD:
			n = (d[1] << 8) + d[2];
			if(n > 19) {
				clock_period = n;
				time_rough = (clock_period - 20) / 16;
				time_fine = (clock_period - 20) % 16;
				clock_set(clock_period);
			}
			break;
		case II_MP_PERIOD + II_GET:
			ii_tx_queue(clock_period >> 8);
			ii_tx_queue(clock_period & 0xff);
			break;
		case II_MP_RESET:
			if(d[1] == 0) {
				for(int n=0;n<8;n++) {
					position[n] = m.count[n];
					tick[n] = m.speed[n];
				}
			}
			else if(d[1] < 9) {
				position[d[1]-1] = m.count[d[1]-1];
				tick[d[1]-1] = m.speed[d[1]-1];
			}
			break;
		case II_MP_STOP:
			if(d[1] == 0) {
				for(int n=0;n<8;n++)
					position[n] =  -1;
			}
			else if(d[1] < 9) {
				position[d[1]-1] = -1;
			}
			break;
		case II_MP_CV + II_GET:
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

void handler_MPGridKey(s32 data) {
 	u8 x, y, z, index, i1, found;
	monome_grid_key_parse_event_data(data, &x, &y, &z);
	// print_dbg("\r\n monome event; x: ");
	// print_dbg_hex(x);
	// print_dbg("; y: 0x");
	// print_dbg_hex(y);
	// print_dbg("; z: 0x");
	// print_dbg_hex(z);

	//// TRACK LONG PRESSES
	index = y*16 + x;
	if(z) {
		held_keys[key_count] = index;
		key_count++;
		key_times[index] = 10;		//// THRESHOLD key hold time
	} else {
		found = 0; // "found"
		for(i1 = 0; i1<key_count; i1++) {
			if(held_keys[i1] == index)
				found++;
			if(found)
				held_keys[i1] = held_keys[i1+1];
		}
		key_count--;

		// FAST PRESS
		if(key_times[index] > 0) {
			if(preset_mode) {
				if(x == 0) {
					if(y != preset) {
						preset = y;

						for(i1=0;i1<8;i1++)
							m.glyph[i1] = f.mp_state.m[preset].glyph[i1];

						// print_dbg("\r\npreset select:");
						// print_dbg_ulong(preset);
					}
 					else if(y == preset) {
 						// flash read
						flashc_memset8((void*)&(f.mp_state.preset), preset, 1, true);
						init_mp();

						preset_mode = false;
						grid_refresh = &refresh_mp;

						// print_dbg("\r\npreset RECALL:");
						// print_dbg_ulong(preset);
					}
				}

				monomeFrameDirty++;
			}
			// print_dbg("\r\nfast press: ");
			// print_dbg_ulong(index);
			// print_dbg(": ");
			// print_dbg_ulong(key_times[index]);
		}
	}

	// PRESET SCREEN
	if(preset_mode) {
		// draw glyph
		if(z && x>7)
			m.glyph[y] ^= 1<<(x-8);

		monomeFrameDirty++;
	}
	else if(view_clock) {
		if(z) {
			if(clock_external) {
				if(y==1) {
					clock_mul = x + 1;
					monomeFrameDirty++;
				}
			}
			else {
				if(y==1)
					time_rough = x;
				else if(y==2)
					time_fine = x;
				else if(y==4) {
					int i = 0;

					switch(x) {
					case 6:
						i = -4;
						break;
					case 7:
						i = -1;
						break;
					case 8:
						i = 1;
						break;
					case 9:
						i = 4;
						break;
					default:
						break;
					}

					i += clock_period;
					if(i < 20)
						i = 20;
					if(clock_period > 265)
						clock_period = 265;
					clock_period = i;

					time_rough = (clock_period - 20) / 16;
					time_fine = (clock_period - 20) % 16;
				}

				clock_period = 20 + (time_rough * 16) + time_fine;

				clock_set(clock_period);

				// print_dbg("\r\nperiod: ");
				// print_dbg_ulong(clock_period);

				monomeFrameDirty++;
			}
		}


		// time_rough = (clock_period - 20) / 16;
		// time_fine = (clock_period - 20) % 16;

	}
	else if(view_config) {
		if(z) {
			if(y < 6 && x < 8) {
				switch(x) {
				case 0:
				case 1:
				case 2:
				case 3:
					if(voice_mode == MP_8T)
						sound ^= 1;
					voice_mode = MP_8T;
					break;
				case 4:
					if(voice_mode == MP_4V)
						sound ^= 1;
					voice_mode = MP_4V;
					break;
				case 5:
					if(voice_mode == MP_2V)
						sound ^= 1;
					voice_mode = MP_2V;
					break;
				case 6:
				case 7:
					if(voice_mode == MP_1V)
						sound ^= 1;
					voice_mode = MP_1V;
					break;
				default:
					break;
				}
			}
			else if(voice_mode != MP_8T) {
				if(x < 8) {
					m.scale = (y - 6) * 8 + x;
				}
				else {
					scale_data[m.scale][7-y] = x-8;
				}

				calc_scale(m.scale);
			}

			monomeFrameDirty++;
		}
	}
	// NORMAL
	else {
		prev_mode = mode;

		// mode check
		if(x == 0) {
			kcount += (z<<1)-1;

			if(kcount < 0)
				kcount = 0;

			// print_dbg("\r\nkey count: ");
			// print_dbg_ulong(kcount);

			if(kcount == 1 && z == 1)
				mode = 1;
			else if(kcount == 0) {
				mode = 0;
				scount[y] = 0;
			}

			if(z == 1 && mode == 1) {
				edit_row = y;
			}
		}
		else if(x == 1 && mode != 0) {
			if(mode == 1 && z == 1) {
				mode = 2;
				edit_row = y;
			}
			else if(mode == 2 && z == 0)
				mode = 1;
		}
		// set position / minmax / stop
		else if(mode == 0) {
			scount[y] += (z<<1)-1;
			if(scount[y]<0) scount[y] = 0;		// in case of grid glitch?

			if(z == 1 && scount[y] == 1) {
				position[y] = x;
				m.count[y] = x;
				m.min[y] = x;
				m.max[y] = x;
				tick[y] = m.speed[y];

				if(sound) {
					pushed[y] = 1;
				}
			}
			else if(z == 1 && scount[y] == 2) {
				if(x < m.count[y]) {
					m.min[y] = x;
					m.max[y] = m.count[y];
				}
				else {
					m.max[y] = x;
					m.min[y] = m.count[y];
				}
			}
		}
		// set speeds and trig/tog
		else if(mode == 1) {
			scount[y] += (z<<1)-1;
			if(scount[y]<0) scount[y] = 0;

			if(z==1) {
				if(x > 7) {
					if(scount[y] == 1) {
						m.smin[y] = x-8;
						m.smax[y] = x-8;
						m.speed[y] = x-8;
						tick[y] = m.speed[y];
					}
					else if(scount[y] == 2) {
						if(x-8 < m.smin[y]) {
							m.smax[y] = m.smin[y];
							m.smin[y] = x-8;
						}
						else
							m.smax[y] = x-8;
					}
				}
				else if(x == 5) {
					m.toggle[edit_row] ^= 1<<y;
					m.trigger[edit_row] &= ~(1<<y);
				}
				else if(x == 6) {
					m.trigger[edit_row] ^= 1<<y;
					m.toggle[edit_row] &= ~(1<<y);
				}
				else if(x == 4) {
					sound ^= 1;
				}
				else if(x == 2) {
					if(position[y] == -1) {
						position[y] = m.count[y];
					}
					else {
						position[y] = -1;
					}
				}
				else if(x == 3) {
					m.sync[edit_row] ^= (1<<y);
				}
			}
		}
		else if(mode == 2 && z == 1) {
			if(x > 3 && x < 7) {
				m.rule_dests[edit_row] = y;
				m.rule_dest_targets[edit_row] = x-3;
			  // post("\nrule_dests", edit_row, ":", rule_dests[edit_row]);
			}
			else if(x > 6) {
				m.rules[edit_row] = y;
			  // post("\nrules", edit_row, ":", rules[edit_row]);
			}
		}

		monomeFrameDirty++;
	}
}

void handler_MPRefresh(s32 data) {
	if(monomeFrameDirty) {
		grid_refresh();

		monome_set_quadrant_flag(0);
		monome_set_quadrant_flag(1);
		(*monome_refresh)();
	}
}

void handler_MPKey(s32 data) {
	// print_dbg("\r\n> MP key ");
	// print_dbg_ulong(data);

	switch(data) {
	case 0:
		grid_refresh = &refresh_mp;
		view_clock = false;
		break;
	case 1:
		grid_refresh = &refresh_clock;
		// print_dbg("\r\ntime: ");
		// print_dbg_ulong(time_fine);
		// print_dbg(" ");
		// print_dbg_ulong(time_rough);
		view_clock = true;
		view_config = false;
		break;
	case 2:
		grid_refresh = &refresh_mp;
		view_config = false;
		break;
	case 3:
		grid_refresh = &refresh_mp_config;
		view_config = true;
		view_clock = false;
		break;
	default:
		break;
	}

	monomeFrameDirty++;
}

void handler_MPTr(s32 data) {
	// print_dbg("\r\n> MP tr ");
	// print_dbg_ulong(data);

	switch(data) {
	case 0:
		if(clock_mul == 1)
			clock_mp(0);
		break;
	case 1:
		if(clock_mul == 1)
			clock_mp(1);
		else {
			ext_clock_count++;
			if(ext_clock_count >= clock_mul - 1) {
				ext_clock_count = 0;
				ext_clock_phase ^= 1;
				clock_mp(ext_clock_phase);
			}
		}
		break;
	case 3:
		// right jack upwards: RESET
		for(int n=0;n<8;n++) {
			position[n] = m.count[n];
			tick[n] = m.speed[n];
		}
		break;
	default:
		break;
	}

	monomeFrameDirty++;
}

void handler_MPTrNormal(s32 data) {
	// print_dbg("\r\n> MP tr normal ");
	// print_dbg_ulong(data);

	clock_external = data;

	if(clock_external)
		clock = &clock_null;
	else
		clock = &clock_mp;

	monomeFrameDirty++;
}

void refresh_clock(void) {
	// clear grid
	memset(monomeLedBuffer,0,128);

	monomeLedBuffer[clock_count & 0xf] = L0;

	if(clock_external) {
		memset(monomeLedBuffer + R1,3,16);
		monomeLedBuffer[R1 + clock_mul - 1] = L2;
	}
	else {
		monomeLedBuffer[R1 + time_rough] = L2;
		monomeLedBuffer[R2 + time_fine] = L1;

		monomeLedBuffer[R4+6] = 7;
		monomeLedBuffer[R4+7] = 3;
		monomeLedBuffer[R4+8] = 3;
		monomeLedBuffer[R4+9] = 7;

	}
}

void refresh_mp_config(void) {
	u8 i1;//, i2, i3;
	u8 c;

	// clear grid
	memset(monomeLedBuffer,0,128);

	// voice mode + sound
	c = L0;
	if(voice_mode == MP_8T)
		c = L1 + (4 * sound);

	monomeLedBuffer[R1 + 1] = c;
	monomeLedBuffer[R1 + 2] = c;
	monomeLedBuffer[R2 + 1] = c;
	monomeLedBuffer[R2 + 2] = c;
	monomeLedBuffer[R3 + 1] = c;
	monomeLedBuffer[R3 + 2] = c;
	monomeLedBuffer[R4 + 1] = c;
	monomeLedBuffer[R4 + 2] = c;

	c = L0;
	if(voice_mode == MP_4V)
		c = L1 + (4 * sound);

	monomeLedBuffer[R1 + 4] = c;
	monomeLedBuffer[R2 + 4] = c;
	monomeLedBuffer[R3 + 4] = c;
	monomeLedBuffer[R4 + 4] = c;

	c = L0;
	if(voice_mode == MP_2V)
		c = L1 + (4 * sound);

	monomeLedBuffer[R1 + 5] = c;
	monomeLedBuffer[R2 + 5] = c;

	c = L0;
	if(voice_mode == MP_1V)
		c = L1 + (4 * sound);

	monomeLedBuffer[R1 + 6] = c;

	// scale
	if(voice_mode != MP_8T) {
		for(i1=0;i1<8;i1++) {
			monomeLedBuffer[8+16*i1] = L0;
			monomeLedBuffer[R6 + i1] = 2;
			monomeLedBuffer[R7 + i1] = 2;
		}
		monomeLedBuffer[R6 + (m.scale >> 3) * 16 + (m.scale & 0x7)] = L2;

		for(i1=0;i1<8;i1++)
			monomeLedBuffer[scale_data[m.scale][i1] + 8 + (7-i1)*16] = L1;
	}
}

void refresh_mp(void) {
	u8 i1, i2, i3;

	// clear grid
	memset(monomeLedBuffer,0,128);

	// SHOW POSITIONS
	if(mode == 0) {
		for(i1=0;i1<8;i1++) {
			for(i2=m.min[i1];i2<=m.max[i1];i2++)
				monomeLedBuffer[i1*16 + i2] = L0;
			monomeLedBuffer[i1*16 + m.count[i1]] = L1;
			if(position[i1] >= 0) {
				monomeLedBuffer[i1*16 + position[i1]] = L2;
			}
		}
	}
	// SHOW SPEED
	else if(mode == 1) {
		for(i1=0;i1<8;i1++) {
			if(position[i1] >= 0)
				monomeLedBuffer[i1*16 + position[i1]] = L0;

			if(position[i1] != -1)
				monomeLedBuffer[i1*16 + 2] = 2;

			for(i2=m.smin[i1];i2<=m.smax[i1];i2++)
				monomeLedBuffer[i1*16 + i2+8] = L0;

			monomeLedBuffer[i1*16 + m.speed[i1]+8] = L1;

			if(sound)
				monomeLedBuffer[i1*16 + 4] = 2;

			if(m.toggle[edit_row] & (1 << i1))
				monomeLedBuffer[i1*16 + 5] = L2;
			else
				monomeLedBuffer[i1*16 + 5] = L0;

			if(m.trigger[edit_row] & (1 << i1))
				monomeLedBuffer[i1*16 + 6] = L2;
			else
				monomeLedBuffer[i1*16 + 6] = L0;

			if(m.sync[edit_row] & (1<<i1))
				monomeLedBuffer[i1*16 + 3] = L1;
			else
				monomeLedBuffer[i1*16 + 3] = L0;
		}

		monomeLedBuffer[edit_row * 16] = L2;
	}
	// SHOW RULES
	else if(mode == 2) {
		for(i1=0;i1<8;i1++)
			if(position[i1] >= 0)
				monomeLedBuffer[i1*16 + position[i1]] = L0;

		monomeLedBuffer[edit_row * 16] = L1;
		monomeLedBuffer[edit_row * 16 + 1] = L1;

		if(m.rule_dest_targets[edit_row] == 1) {
			monomeLedBuffer[m.rule_dests[edit_row] * 16 + 4] = L2;
			monomeLedBuffer[m.rule_dests[edit_row] * 16 + 5] = L0;
			monomeLedBuffer[m.rule_dests[edit_row] * 16 + 6] = L0;
		}
		else if (m.rule_dest_targets[edit_row] == 2) {
			monomeLedBuffer[m.rule_dests[edit_row] * 16 + 4] = L0;
			monomeLedBuffer[m.rule_dests[edit_row] * 16 + 5] = L2;
			monomeLedBuffer[m.rule_dests[edit_row] * 16 + 6] = L0;
		}
		else {
			monomeLedBuffer[m.rule_dests[edit_row] * 16 + 4] = L2;
			monomeLedBuffer[m.rule_dests[edit_row] * 16 + 5] = L2;
			monomeLedBuffer[m.rule_dests[edit_row] * 16 + 6] = L0;
		}

		for(i1=8;i1<16;i1++)
			monomeLedBuffer[m.rules[edit_row] * 16 + i1] = L0;


		for(i1=0;i1<8;i1++) {
			i3 = sign[m.rules[edit_row]][i1];
			for(i2=0;i2<8;i2++) {
				if((i3 & (1<<i2)) != 0)
					monomeLedBuffer[i1*16 + 8 + i2] = L2;
			}
		}
	}
}


void calc_scale(uint8_t s) {
	cur_scale[0] = scale_data[s][0];

	for(u8 i1=1;i1<8;i1++) {
		cur_scale[i1] = cur_scale[i1-1] + scale_data[s][i1];
		// print_dbg("\r\n ");
		// print_dbg_ulong(cur_scale[i1]);

	}
}


////////////////////////////////////////////////////////////////////////////////
// ES

es_mode_t es_mode;
es_view_t es_view;
u8 es_runes, es_edge, es_voices;

u32 es_tick;
u16 es_pos;
u8 es_blinker;
es_note_t es_notes[4];
u32 es_p_start, es_p_total;
u8 es_ignore_arm_release;

softTimer_t es_blinker_timer = { .next = NULL, .prev = NULL };
softTimer_t es_play_timer = { .next = NULL, .prev = NULL };
softTimer_t es_play_pos_timer = { .next = NULL, .prev = NULL };

static void es_blinker_callback(void* o) {
    if (ansible_mode != mGridES) {
        timer_remove(&es_blinker_timer);
        return;
    }
    
    es_blinker = !es_blinker;
    if (es_mode == es_recording) monomeFrameDirty++;
}

static void es_note_off_i(u8 i) {
    if (!es_notes[i].active) return;
    es_notes[i].active = 0;
    timer_remove(&auxTimer[i]);
    clr_tr(TR1 + i);
}

static void es_note_off(s8 x, s8 y) {
    for (u8 i = 0; i < 4; i++)
        if (es_notes[i].x == x && es_notes[i].y == y) {
            es_note_off_i(i);
            break;
        }
}

static void es_note_off_callback(void* o) {
    u8 i = (intptr_t)o;
    timer_remove(&auxTimer[i]);
    es_note_off_i(i);
    monomeFrameDirty++;
}

static void es_kill_all_notes(void) {
    for (u8 i = 0; i < 4; i++) es_note_off_i(i);
    monomeFrameDirty++;
}

static void es_note_on(s8 x, s8 y, u8 from_pattern, u16 timer, u8 voices) {
    u8 note = 255;
    for (u8 i = 0; i < 4; i++)
        if ((voices & (1 << i)) && (!es_notes[i].active || (es_notes[i].x == x && es_notes[i].y == y))) {
            note = i;
            break;
        }

    if (note == 255) {
        u32 earliest = 0xffffffff;
        for (u8 i = 0; i < 4; i++)
            if ((voices & (1 << i)) && es_notes[i].start < earliest) {
                earliest = es_notes[i].start;
                note = i;
            }
    }
    
    if (note == 255) return;
    
    es_note_off_i(note);

    es_notes[note].active = 1;
    es_notes[note].x = x;
    es_notes[note].y = y;
    es_notes[note].start = get_ticks();
    es_notes[note].from_pattern = from_pattern;

    s16 note_index = x + (7 - y) * 5 - 1;
    if (note_index < 0)
        note_index = 0;
    else if (note_index > 119)
        note_index = 119;
    dac_set_value(note, ET[note_index] << 2);
    set_tr(TR1 + note);
    
    if (timer) timer_add(&auxTimer[note], timer, &es_note_off_callback, (void *)(intptr_t)note);
}

/*
static void es_update_pitches(void) {
    u8 first_note_x = e.p[e.p_select].e[0].index & 15;
    u8 first_note_y = e.p[e.p_select].e[0].index >> 4;
    
    s16 x, y, note_index;
    for (u8 i = 0; i < 4; i++) {
        es_notes[i].x = x = es_notes[i].x + e.p[e.p_select].root_x - first_note_x;
        es_notes[i].y = y = es_notes[i].y + e.p[e.p_select].root_y - first_note_y;
        note_index = x + (7 - y) * 5 - 1;
        if (note_index < 0)
            note_index = 0;
        else if (note_index > 119)
            note_index = 119;
        dac_set_value(i, ET[note_index] << 2);
    }
}
*/

static void es_complete_recording(void) {
    if (!e.p[e.p_select].length) return;
    
    e.p[e.p_select].e[e.p[e.p_select].length - 1].interval = get_ticks() - es_tick;
    for (u16 i = 0; i < e.p[e.p_select].length; i++) {
        if (e.p[e.p_select].e[i].interval > ES_CHORD_THRESHOLD) {
            e.p[e.p_select].interval_ind = i;
            break;
        }
    }
}

static void es_record_pattern_note(u8 x, u8 y, u8 on) {
    u16 l = e.p[e.p_select].length;
    if (l >= ES_EVENTS_PER_PATTERN) {
        es_complete_recording(); // will update interval for the last event
        return;
    }
    
    if (!l) {
        e.p[e.p_select].root_x = x;
        e.p[e.p_select].root_y = y;
    } 
    
    if (l) e.p[e.p_select].e[l - 1].interval = get_ticks() - es_tick;
    es_tick = get_ticks();
    
    e.p[e.p_select].e[l].index = x + (y << 4);
    e.p[e.p_select].e[l].on = on;
    e.p[e.p_select].length++;
}

static void es_play_pattern_note(void) {
    u16 i = e.p[e.p_select].dir ? e.p[e.p_select].length - 1 : 0;
    u8 first_note_x = e.p[e.p_select].e[i].index & 15;
    u8 first_note_y = e.p[e.p_select].e[i].index >> 4;
    s16 x = (e.p[e.p_select].e[es_pos].index & 15) + e.p[e.p_select].root_x - first_note_x;
    s16 y = (e.p[e.p_select].e[es_pos].index >> 4) + e.p[e.p_select].root_y - first_note_y;
    
    if (e.p[e.p_select].e[es_pos].on)
        es_note_on(x, y, 1, 
            e.p[e.p_select].edge == ES_EDGE_FIXED ? e.p[e.p_select].edge_time : 0,
            e.p[e.p_select].voices);
    else if (e.p[e.p_select].edge == ES_EDGE_PATTERN)
        es_note_off(x, y);
    monomeFrameDirty++;
}

static void es_kill_pattern_notes(void) {
    for (u8 i = 0; i < 4; i++)
        if (es_notes[i].from_pattern) es_note_off_i(i);
    monomeFrameDirty++;
}

static void es_update_total_time(void) {
    u16 interval;
    es_p_total = 0;
    for (u16 i = 0; i < e.p[e.p_select].length; i++) {
        interval = e.p[e.p_select].e[i].interval;
        if (e.p[e.p_select].linearize) {
            if (interval < ES_CHORD_THRESHOLD)
                interval = 1;
            else
                interval = e.p[e.p_select].e[e.p[e.p_select].interval_ind].interval;
        }
        es_p_total += interval;
    }
}

static void es_play_pos_callback(void* o) {
    if (ansible_mode != mGridES) {
        timer_remove(&es_play_pos_timer);
        return;
    }
    
    if (es_mode == es_playing) monomeFrameDirty++;
}

static void es_play_callback(void* o) {
    timer_remove(&es_play_timer);
    if (ansible_mode != mGridES) {
        es_mode = es_stopped;
        return;
    }
    
    if (clock_external) return;
    
    if (++es_pos >= e.p[e.p_select].length) {
        es_pos = 0;
        es_p_start = get_ticks();
        if (!e.p[e.p_select].loop) {
            es_kill_pattern_notes();
            timer_remove(&es_play_pos_timer);
            es_mode = es_stopped;
            return;
        }
    }

    u16 interval = e.p[e.p_select].e[es_pos].interval;
    if (e.p[e.p_select].linearize) {
        if (interval < ES_CHORD_THRESHOLD)
            interval = 1;
        else
            interval = e.p[e.p_select].e[e.p[e.p_select].interval_ind].interval;
    }
    if (!interval) interval = 1;
    timer_add(&es_play_timer, interval, &es_play_callback, NULL);
    es_play_pattern_note();
}

static void es_stop_playback(void) {
    timer_remove(&es_play_timer);
    timer_remove(&es_play_pos_timer);
    es_mode = es_stopped;
    es_kill_pattern_notes();
}

static void es_start_playback(u8 pos) {
    if (es_mode == es_playing) es_stop_playback();
    else if (es_mode == es_recording) es_complete_recording();
    
    if (!e.p[e.p_select].length) {
        es_mode = es_stopped;
        monomeFrameDirty++;
        return;
    }
    
    if (es_mode == es_playing) es_kill_pattern_notes();
    
    if (pos) {
        u32 start = (es_p_total * pos) >> 4;
        u32 tick = 0;
        for (es_pos = 0; es_pos < e.p[e.p_select].length; es_pos++) {
            if (tick > start) break;
            tick += e.p[e.p_select].e[es_pos].interval;
        }
        es_p_start = get_ticks() - tick;
    } else es_pos = 0;
    
    es_p_start = get_ticks();
    es_update_total_time();
    
    es_mode = es_playing;
    if (clock_external) return;
    
    u16 interval = e.p[e.p_select].e[0].interval;
    if (e.p[e.p_select].linearize) {
        if (interval < ES_CHORD_THRESHOLD)
            interval = 1;
        else
            interval = e.p[e.p_select].e[e.p[e.p_select].interval_ind].interval;
    }
    if (!interval) interval = 1;
    timer_add(&es_play_pos_timer, 25, &es_play_pos_callback, NULL);
    timer_add(&es_play_timer, interval, &es_play_callback, NULL );
    es_play_pattern_note();
}

static void es_start_recording(void) {
    e.p[e.p_select].length = 0;
    e.p[e.p_select].start = 0;
    e.p[e.p_select].end = 15;
    es_mode = es_recording;
    monomeFrameDirty++;
}

static u8 is_arm_pressed(void) {
    u8 found = 0;
    for (u8 i = 0; i < key_count; i++) {
        if (held_keys[i] == 32) found = 1;
        break;
    }
    return found;
}

/*
static s8 top_row_pressed(void) {
    s8 found = -1;
    for (u8 i = 0; i < key_count; i++) {
        if (held_keys[i] < 16) found = held_keys[i];
        break;
    }
    return found;
}
*/

static void es_prev_pattern(void) {
    if (!e.p_select) return;
    e.p_select--;
    es_start_playback(0);
}

static void es_next_pattern(void) {
    if (e.p_select >= 15) return;
    e.p_select++;
    es_start_playback(0);
}

static void es_double_speed(void) {
    for (u16 i = 0; i < e.p[e.p_select].length; i++)
        e.p[e.p_select].e[i].interval = max(1, e.p[e.p_select].e[i].interval >> 1);
    es_update_total_time();
}

static void es_half_speed(void) {
    for (u16 i = 0; i < e.p[e.p_select].length; i++)
        e.p[e.p_select].e[i].interval <<= 1;
    es_update_total_time();
}

static void es_reverse(void) {
    u16 l = e.p[e.p_select].length;
    if (!l) return;

    es_event_t te[ES_EVENTS_PER_PATTERN];
    
    for (u16 i = 0; i < l; i++) {
        te[i] = e.p[e.p_select].e[i];
        te[i].on = !te[i].on;
    }
    
    for (u16 i = 0; i < l; i++) 
        e.p[e.p_select].e[i] = te[l - i - 1];
    for (u16 i = 0; i < l - 1; i++) 
        e.p[e.p_select].e[i].interval = te[l - i - 2].interval;
}

// init functions

void default_es(void) {
    uint8_t i;
    flashc_memset8((void*)&(f.es_state.preset), 0, 1, true);
    for (i = 0; i < 8; i++) {
        e.arp = 0;
        e.p_select = 0;
        e.voices = 0b1111;
        e.octave = 0;
        for (u8 j = 0; j < 16; j++) {
            e.p[i].interval_ind = 0;
            e.p[i].length = 0;
            e.p[i].loop = 0;
            e.p[i].edge = ES_EDGE_PATTERN;
            e.p[i].edge_time = 16;
            e.p[i].voices = 0b1111;
            e.p[i].dir = 0;
            e.p[i].linearize = 0;
            e.p[i].start = 0;
            e.p[i].end = 15;
        }
        e.glyph[i] = 0;
    }
    for (i = 0; i < GRID_PRESETS; i++)
        flashc_memcpy((void *)&f.es_state.e[i], &e, sizeof(e), true);
}

void init_es(void) {
    preset = f.es_state.preset;
    e = f.es_state.e[preset];
    es_mode = es_stopped;
    es_view = es_main;
}

void resume_es(void) {
    es_mode = es_stopped;
    es_view = es_main;
    
    preset_mode = false;
    grid_refresh = &refresh_es;

    // re-check clock jack
    clock_external = !gpio_get_pin_value(B10);
    clock = &clock_null;

    for (u8 i = 0; i < 4; i++) {
        dac_set_slew(i, 0);
        clr_tr(TR1 + i);
        es_notes[i].active = 0;
    }

    timer_remove(&es_blinker_timer);
    timer_add(&es_blinker_timer, 288, &es_blinker_callback, NULL);
    timer_remove(&es_play_timer);
    timer_remove(&es_play_pos_timer);
    
    monomeFrameDirty++;
}

static void es_load_preset(void) {
    flashc_memset8((void*)&(f.es_state.preset), preset, 1, true);
    init_es();
    resume_es();
}

// handlers

void handler_ESRefresh(s32 data) {
    if(monomeFrameDirty) {
        grid_refresh();
        monome_set_quadrant_flag(0);
        monome_set_quadrant_flag(1);
        (*monome_refresh)();
    }
}

void handler_ESKey(s32 data) {
    switch(data) {
    case 0: // button 1 released
        break;
    case 1: // button 1 pressed
        es_prev_pattern();
        break;
    case 2: // button 2 released
        es_next_pattern();
        break;
    case 3: // button 2 released
        break;
    default:
        break;
    }
}

void handler_ESTr(s32 data) {
    switch(data) {
    case 0: // input 1 low
        break;
    case 1: // input 1 high
        if (es_mode != es_playing) break;
        while (true) {
            es_play_pattern_note();
            if (e.p[e.p_select].e[es_pos].interval > ES_CHORD_THRESHOLD) break;
            if (++es_pos >= e.p[e.p_select].length) {
                es_pos--;
                break;
            }
        }
        if (++es_pos >= e.p[e.p_select].length) {
            es_pos = 0;
            if (!e.p[e.p_select].loop) {
                es_kill_pattern_notes();
                es_mode = es_stopped;
            }
        }
        break;
    case 2: // input 2 low
        break;
    case 3: // input 2 high
        if (es_mode != es_armed && es_mode != es_recording) es_start_playback(0);
        break;
    default:
        break;
    }
}

void handler_ESTrNormal(s32 data) {
    clock_external = data;
    if (es_mode != es_playing) return;

    es_kill_pattern_notes();
    if (clock_external) {
        timer_remove(&es_play_timer);
        timer_remove(&es_play_pos_timer);
    } else {
        timer_add(&es_play_pos_timer, 25, &es_play_pos_callback, NULL);
        es_play_callback(NULL);
    }
}

void handler_ESGridKey(s32 data) {
    u8 x, y, z;
    monome_grid_key_parse_event_data(data, &x, &y, &z);
    u8 index = (y << 4) + x;

    // track held keys and long presses
    if (z) {
        held_keys[key_count] = index;
        if (key_count < MAX_HELD_KEYS) key_count++;
        key_times[index] = 10;
    } else {
        u8 found = 0;
        for (u8 i = 0; i < key_count; i++) {
            if (held_keys[i] == index) found++;
            if (found) held_keys[i] = held_keys[i + 1];
        }
        if (found) key_count--;
    }

    // preset screen
    if (preset_mode) {
        if (!z && x == 0) {
            if (y != preset) {
                preset = y;
                for (u8 i = 0; i < GRID_PRESETS; i++)
                e.glyph[i] = f.es_state.e[preset].glyph[i];
            } else {
                // flash read
                es_load_preset();
            }
        } else if (z && x > 7) {
            e.glyph[y] ^= 1 << (x - 8);
        }

        monomeFrameDirty++;
        return;
    }

    if (x == 0) {
        if (z && y == 0) { // start/stop
            if (es_view == es_patterns_held) {
                es_view = es_patterns;
            } else if (es_mode == es_stopped || es_mode == es_armed) {
                es_start_playback(0);
                if (is_arm_pressed()) es_ignore_arm_release = 1;
            } else if (es_mode == es_recording) {
                e.p[e.p_select].loop = 1;
                es_start_playback(0);
            } else if (es_mode == es_playing) {
                if (is_arm_pressed()) {
                    es_start_playback(0);
                    es_ignore_arm_release = 1;
                } else
                    es_stop_playback();
            }
        } else if (y == 1) { // p_select
            if (z && es_mode == es_recording) {
                es_complete_recording();
                es_mode = es_stopped;
            }
            if (z && es_view == es_patterns)
                es_view = es_main;
            else if (z && es_view == es_main)
                es_view = es_patterns_held;
            else if (!z && es_view == es_patterns_held)
                es_view = es_main;
        } else if (y == 2) { // arm
            es_view = es_main;
            if (z) {
                if (es_mode == es_armed) {
                    es_mode = es_stopped;
                    es_ignore_arm_release = 1;
                } else if (es_mode == es_recording) {
                    es_complete_recording();
                    es_mode = es_stopped;
                    es_ignore_arm_release = 1;
                }
            } else {
                if (es_ignore_arm_release) {
                    es_ignore_arm_release = 0;
                    return;
                }
                if (es_mode == es_stopped) {
                    es_mode = es_armed;
                } else if (es_mode == es_playing) {
                    es_stop_playback();
                    es_mode = es_armed;
                }
            }
        } else if (z && y == 3) { // loop
            e.p[e.p_select].loop = !e.p[e.p_select].loop;
        } else if (z && y == 4) { // arp
            e.arp = !e.arp;
        } else if (y == 5) { // edge mode
            es_edge = z;
        } else if (y == 6) { // runes
            es_runes = z;
        } else if (y == 7) { // voices
            es_voices = z;
        }
        
        if (!es_edge) {
            monomeFrameDirty++;
            return;
        }
    }

    if (es_runes) {
        if (!z) return;
        
        if (x > 1 && x < 5 && y > 1 && y < 5) {
            e.p[e.p_select].linearize = !e.p[e.p_select].linearize;
            es_update_total_time();
        } else if (x > 5 && x < 8 && y > 1 && y < 5) {
            e.p[e.p_select].dir = 1;
            es_reverse();
            if (es_mode == es_playing) es_kill_pattern_notes();
        } else if (x > 8 && x < 11 && y > 1 && y < 5) {
            e.p[e.p_select].dir = 0;
            es_reverse();
            if (es_mode == es_playing) es_kill_pattern_notes();
        } else if (x > 11 && x < 15 && y > 0 && y < 3)
            es_double_speed();
        else if (x > 11 && x < 15 && y > 3 && y < 6)
            es_half_speed();
        
        monomeFrameDirty++;
        return;
    } 
    
    if (es_edge) {
        if (!z) return;
        
        if (y == 7) {
            e.p[e.p_select].edge = ES_EDGE_FIXED;
            e.p[e.p_select].edge_time = (x + 1) << 4;
            es_kill_all_notes();
        } else {
            if (x) {
                if (x < 6) {
                    e.p[e.p_select].edge = ES_EDGE_PATTERN;
                    es_kill_all_notes();
                } else if (x < 11) {
                    e.p[e.p_select].edge = ES_EDGE_FIXED;
                    es_kill_all_notes();
                } else {
                    e.p[e.p_select].edge = ES_EDGE_DRONE;
                }
            }
        }
        
        monomeFrameDirty++;
        return;
    }
    
    if (es_voices) {
        if (!z) return;
        
        u8 voice = 1 << (y - 2);
        if (x == 3 && y > 1 && y < 6) {
            if (e.voices && voice) es_note_off_i(y - 2);
            e.voices ^= voice;
        } else if (x == 2 && y > 1 && y < 6) {
            if (e.p[e.p_select].voices && voice) es_note_off_i(y - 2);
            e.p[e.p_select].voices ^= voice;
        } else if (y == 7 && x == 2 && e.octave) {
            e.octave--;
        } else if (y == 7 && x == 3 && e.octave < 5) {
            e.octave++;
        }
        monomeFrameDirty++;
        return;
    }
    
    if (es_view == es_patterns_held || es_view == es_patterns) {
        if (!z || x < 2 || x > 5 || y < 2 || y > 5) return;
        e.p_select = (x - 2) + ((y - 2) << 2);
        if (es_view == es_patterns) es_start_playback(0);
        monomeFrameDirty++;
        return;
    }
    
    /*
    if (y == 0 && es_mode == es_playing) {
        if (!z) return;
        s8 start = top_row_pressed();
        if (start == -1 || start == x) {
            es_start_playback(x);
        } else {
            e.p[e.p_select].start = min(x, start);
            e.p[e.p_select].end = max(x, start);
        }
        monomeFrameDirty++;
        return;
    }
    */
    
    if (x == 0) return;

    if (es_mode == es_armed) es_start_recording(); // will change es_mode to es_recording
    if (es_mode == es_recording) es_record_pattern_note(x, y, z);
    
    if (e.arp && es_mode != es_recording) {
        e.p[e.p_select].root_x = x;
        e.p[e.p_select].root_y = y;
        es_start_playback(0);
        // es_update_pitches();
    } else {
        if (e.p[e.p_select].edge == ES_EDGE_DRONE) {
            if (z) {
                u8 found = 0;
                for (u8 i = 0; i < 4; i++)
                    if (x == es_notes[i].x && y == es_notes[i].y && es_notes[i].active) {
                        es_note_off(x, y);
                        found = 1;
                    }
                if (!found) es_note_on(x, y, 0, 0, e.voices);
            }
        } else {
            if (z)
                es_note_on(x, y, 0, 0, es_mode == es_recording ? e.p[e.p_select].voices : e.voices);
            else
                es_note_off(x, y);
        }
    }

    monomeFrameDirty++;
}

void refresh_es(void) {
    memset(monomeLedBuffer, 0, MONOME_MAX_LED_BYTES);
    
    for (u8 i = 0; i < 8; i++)
        monomeLedBuffer[i << 4] = 2;
    
    if (es_mode == es_playing)
        monomeLedBuffer[0] = 15;
    else if (e.p[e.p_select].length)
        monomeLedBuffer[0] = 8;
    
    if (es_view == es_patterns) monomeLedBuffer[16] = 15;
    
    if (es_mode == es_recording)
        monomeLedBuffer[32] = 11 + (es_blinker ? 0 : 4);
    else if (es_mode == es_armed)
        monomeLedBuffer[32] = 7;
    
	if (e.p[e.p_select].loop) monomeLedBuffer[48] = 11;
    if (e.arp) monomeLedBuffer[64] = 11;
    
    if (es_mode == es_playing) {
        //for (u8 i = e.p[e.p_select].start; i <= e.p[e.p_select].end; i++) 
        //    monomeLedBuffer[i] = 4;
        u8 pos;
        if (clock_external)
            pos = e.p[e.p_select].length ? (es_pos << 4) / (e.p[e.p_select].length - 1) : 0;
        else
            pos = ((get_ticks() - es_p_start) << 4) / es_p_total;
        if (e.p[e.p_select].dir) pos = 15 - pos;
        for (u8 i = 1; i < 16; i++)
            if (i <= pos) monomeLedBuffer[i] = 11;
    }

    u8 l;
    if (es_runes) {
        l = e.p[e.p_select].linearize ? 15 : 7;
        
        // linearize
        monomeLedBuffer[34] = l;
        monomeLedBuffer[36] = l;
        monomeLedBuffer[66] = l;
        monomeLedBuffer[68] = l;
        
        l = e.p[e.p_select].dir ? 15 : 7;
        // reverse
        monomeLedBuffer[39] = l;
        monomeLedBuffer[54] = l;
        monomeLedBuffer[71] = l;
        
        l = e.p[e.p_select].dir ? 7 : 15;
        // forward
        monomeLedBuffer[41] = l;
        monomeLedBuffer[58] = l;
        monomeLedBuffer[73] = l;
        
        l = 8;
        // double speed
        monomeLedBuffer[29] = l;
        monomeLedBuffer[44] = l;
        monomeLedBuffer[46] = l;
        
        // half speed
        monomeLedBuffer[76] = l;
        monomeLedBuffer[78] = l;
        monomeLedBuffer[93] = l;

        return;
    }

    if (es_edge) {
        l = e.p[e.p_select].edge == ES_EDGE_PATTERN ? 15 : 7;
        monomeLedBuffer[34] = l;
        monomeLedBuffer[35] = l;
        monomeLedBuffer[36] = l;
        monomeLedBuffer[50] = l;
        monomeLedBuffer[52] = l;
        monomeLedBuffer[66] = l;
        monomeLedBuffer[68] = l;
        monomeLedBuffer[82] = l;
        monomeLedBuffer[84] = l;
        monomeLedBuffer[85] = l;

        l = e.p[e.p_select].edge == ES_EDGE_FIXED ? 15 : 7;
        monomeLedBuffer[39] = l;
        monomeLedBuffer[40] = l;
        monomeLedBuffer[41] = l;
        monomeLedBuffer[42] = l;
        monomeLedBuffer[55] = l;
        monomeLedBuffer[58] = l;
        monomeLedBuffer[71] = l;
        monomeLedBuffer[74] = l;
        monomeLedBuffer[87] = l;
        monomeLedBuffer[90] = l;
        
        l = e.p[e.p_select].edge == ES_EDGE_DRONE ? 15 : 7;
        monomeLedBuffer[44] = l;
        monomeLedBuffer[45] = l;
        monomeLedBuffer[46] = l;
        monomeLedBuffer[47] = l;
        
		if (e.p[e.p_select].edge == ES_EDGE_FIXED) {
			for (u8 i = 0; i < 16; i++)
				monomeLedBuffer[112 + i] = 4;
            u8 edge_index = 111 + (e.p[e.p_select].edge_time >> 4);
			if (edge_index <= 127) monomeLedBuffer[edge_index] = 11;
		}
        
        return;
    }

    if (es_voices) {
        for (u8 i = 0; i < 4; i++) {
            monomeLedBuffer[35 + (i << 4)] = e.voices & (1 << i) ? 15 : 4;
            monomeLedBuffer[34 + (i << 4)] = e.p[e.p_select].voices & (1 << i) ? 15 : 4;
        }
        
        monomeLedBuffer[e.octave ? 115 : 114] = 10 + e.octave;
        return;
    }
    
    if (es_view == es_main) {
        if (e.arp)
            monomeLedBuffer[e.p[e.p_select].root_x + (e.p[e.p_select].root_y << 4)] = 7;
        s16 index, x, y;
        for (u8 i = 0; i < 4; i++)
            if (es_notes[i].active) {
                x = es_notes[i].x;
                y = es_notes[i].y;
                while (x < 0) {
                    y++;
                    x += 5;
                }
                while (x > 15) {
                    y--;
                    x -= 5;
                }
                index = (y << 4) + x;
                if (index >= 0 && index <= MONOME_MAX_LED_BYTES && (index & 15) != 0)
                    monomeLedBuffer[index] = 15;
            }
    } else { // pattern view
        for (u8 i = 0; i < 16; i++)
            monomeLedBuffer[(i & 3) + 34 + ((i >> 2) << 4)] = e.p[i].length ? 7 : 4;
        monomeLedBuffer[(e.p_select & 3) + 34 + ((e.p_select >> 2) << 4)] = 15;
    }
}
