#include "string.h"

#include "print_funcs.h"
#include "flashc.h"
#include "gpio.h"

#include "monome.h"
#include "i2c.h"
#include "dac.h"
#include "util.h" // rnd
#include "music.h"
#include "libfixmath/fix16.h"
#include "init_common.h"
#include "ii.h"

#include "main.h"
#include "ansible_grid.h"

#include "init_ansible.h"


#define L2 12
#define L1 8
#define L0 4

#define GRID_KEY_HOLD_TIME 15
#define MAX_KEYS 256
#define MAX_HELD_KEYS 32

#define ES_CHORD_THRESHOLD 30

bool preset_mode;
uint8_t preset;
static void preset_mode_exit(void);

bool follower_select;
bool mod_follower;
uint8_t follower;
bool kriaAltModeBlink; // flag gets flipped for the blinking

u8 grid_varibrightness = 16;
u8 key_count = 0;
u8 held_keys[MAX_HELD_KEYS];
u8 key_times[MAX_KEYS];

bool clock_external;
bool view_clock;
bool view_config;
bool view_tuning;
bool mod_tuning;
uint32_t clock_period;
uint8_t clock_count;
uint8_t clock_mul;
uint8_t ext_clock_count;
uint8_t ext_clock_phase;

u64 last_ticks[4];
u32 clock_deltas[4];

uint8_t time_rough;
uint8_t time_fine;

uint8_t tuning_track;
uint8_t tuning_octave;
int16_t tuning_octave_offset[4];
bool tuning_note_on[4];

uint8_t cue_div;
uint8_t cue_steps;

uint8_t meta;

uint8_t scale_data[16][8];

u8 cur_scale[8];
int8_t scale_adj[8];
void calc_scale(uint8_t s);

void (*grid_refresh)(void);
void ii_grid(uint8_t* data, uint8_t len);

// KRIA
kria_data_t k;
kria_view_t k_views[2]; 
kria_sync_mode_t kria_sync_mode;
uint8_t tmul[4][KRIA_NUM_PARAMS];


u8 kria_track_indices[4] = {
	0, 1, 2, 3
};
bool kria_mutes[4];
bool kria_blinks[4];
softTimer_t blinkTimer[4] = {
	{ .next = NULL, .prev = NULL },
	{ .next = NULL, .prev = NULL },
	{ .next = NULL, .prev = NULL },
	{ .next = NULL, .prev = NULL }
};

softTimer_t repeatTimer[4] = {
	{ .next = NULL, .prev = NULL },
	{ .next = NULL, .prev = NULL },
	{ .next = NULL, .prev = NULL },
	{ .next = NULL, .prev = NULL }
};

static void grid_keytimer_kria(uint8_t held_key);
static void kria_set_note(uint8_t trackNum);
static void preset_mode_handle_key(uint8_t x, uint8_t y, uint8_t z, uint8_t* glyph);
static kria_modes_t ii_kr_mode_for_cmd(uint8_t cmd);
static uint8_t ii_kr_cmd_for_mode(kria_modes_t mode);

// MP

mp_data_t m;
u8 sound;
u8 voice_mode;

// ES

es_data_t e;

void set_mode_grid() {
	grid_varibrightness = (f.state.grid_varibrightness == 1) ? 1 :
			      (f.state.grid_varibrightness == 4) ? 4 :
			      16;


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
		if (!leader_mode) init_i2c_slave(II_KR_ADDR);
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
		if (!leader_mode) init_i2c_slave(II_MP_ADDR);
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
		if (!leader_mode) init_i2c_slave(ES);
		process_ii = &ii_es;
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

static void preset_mode_exit(void) {
	 // print_dbg("\r\n> PRESET EXIT");
	preset_mode = false;
	flashc_memcpy((void*)f.state.followers, followers, sizeof(followers), true);
}

void handler_GridFrontShort(s32 data) {
	if(preset_mode) {
		preset_mode_exit();

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
		monomeFrameDirty++;
	}
	else {
		// print_dbg("\r\n> PRESET ENTER");
		preset_mode = true;
		grid_refresh = &refresh_preset;
		view_config = false;
		view_clock = false;
		monomeFrameDirty++;
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

	memset(monomeLedBuffer, 0, MONOME_MAX_LED_BYTES);

	if (!follower_select) {
		monomeLedBuffer[preset * 16] = 11;
	}

	if (follower_select) {
		for (uint8_t i = 0; i < 4; i++) {
			monomeLedBuffer[R7 + i] = (followers[follower].track_en & (1 << i)) ? L1 : L0;
		}
	}
	for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
		if (follower_select) {
			monomeLedBuffer[5 + (2 + i)*16] = i == follower ? L1 : L0;
		}
		else {
			monomeLedBuffer[5 + (2 + i)*16] = followers[i].active ? L1 : L0;
		}
	}
	monomeLedBuffer[5 + R7] = mod_follower ? L1 : L0;

	if (follower_select) {
		memset(monomeLedBuffer, L0, 7);
		monomeLedBuffer[followers[follower].oct + 3] = L1;

		if (followers[follower].mode_ct > 1) {
			memset(monomeLedBuffer + 13, L0, followers[follower].mode_ct);
			monomeLedBuffer[13 + followers[follower].active_mode] = L1;
		}
	}
	else {
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
	}

	monome_set_quadrant_flag(0);
	monome_set_quadrant_flag(1);
	if (monome_size_y() == 16) {
		monome_set_quadrant_flag(2);
		monome_set_quadrant_flag(3);
	}
}

void refresh_grid_tuning(void) {
	memset(monomeLedBuffer, 0, MONOME_MAX_LED_BYTES);

	for (uint8_t i = 0; i < 4; i++) {
		monomeLedBuffer[i*16] = tuning_note_on[i] ? L1 : L0;

		memset(monomeLedBuffer + i*16 + 2, L0, 12);
		monomeLedBuffer[(int)i*16 + 2 + tuning_octave_offset[i]] += 4;
		if (i == tuning_track) {
			monomeLedBuffer[i*16 + 2 + tuning_octave_offset[tuning_track]] += 4;
		}
	}

	monomeLedBuffer[R4] = mod_tuning ? L1 : L0;

	memset(monomeLedBuffer + R5, L0, 10);
	monomeLedBuffer[R5 + tuning_octave] = L1;
	monomeLedBuffer[R5 + 11] = L1; // reload / longpress to restore factory default
	monomeLedBuffer[R5 + 13] = L1; // save offsets key
	monomeLedBuffer[R5 + 14] = L1; // save interpolated key
	monomeLedBuffer[R5 + 15] = L1; // save as-is key

	// lit key indicating position
	uint8_t tuning_slot = (int)(tuning_octave * 12) + tuning_octave_offset[tuning_track];
	uint8_t dac_step = tuning_table[tuning_track][tuning_slot] >> 6;
	memset(monomeLedBuffer + R6, 3, dac_step / 16);
	monomeLedBuffer[R6 + dac_step / 16] = dac_step % 16;

	// tuning steps either direction
	for (uint8_t i = 0; i < 8; i++) {
		if ((DAC_10V) - tuning_table[tuning_track][tuning_slot] > (1 << i)) {
			monomeLedBuffer[R7 + 8 + i] = 2*i + 1;
		}
		if (tuning_table[tuning_track][tuning_slot] > (1 << i)) {
			monomeLedBuffer[R7 + 7 - i] = 2*i + 1;
		}
	}
}

static void restore_grid_tuning(void) {
	tuning_octave = 0;
	tuning_track = 0;
	for (uint8_t i = 0; i < 4; i++) {
		tuning_note_on[i] = true;
		tuning_octave_offset[i] = 0;
		dac_set_value_noslew(
			i,
			tuning_table[i][
		        	(int)(tuning_octave * 12) +
				tuning_octave_offset[i]
			]);
		set_tr(TR1 + i);
	}
}

void grid_keytimer(void) {
	for(uint8_t i1=0;i1<key_count;i1++) {
		if(key_times[held_keys[i1]])
		if(--key_times[held_keys[i1]]==0) {
			uint8_t x = held_keys[i1] % 16;
			uint8_t y = held_keys[i1] / 16;
			if(preset_mode == 1) {
				if(x == 0 && !follower_select) {

					preset = y;

					// WRITE PRESET

					switch (ansible_mode) {
					case mGridMP:
						flashc_memset8((void*)&(f.mp_state.preset), preset, 1, true);
						flashc_memset8((void*)&(f.mp_state.sound), sound, 1, true);
						flashc_memset8((void*)&(f.mp_state.voice_mode), voice_mode, 1, true);
						flashc_memcpy((void *)&f.mp_state.m[preset], &m, sizeof(m), true);

						flashc_memcpy((void *)&f.scale, &scale_data, sizeof(scale_data), true);

						preset_mode_exit();
						grid_refresh = &refresh_mp;
						break;

					case mGridKria:
						flashc_memset8((void*)&(f.kria_state.preset), preset, 1, true);
						flashc_memset8((void*)&(f.kria_state.cue_div), cue_div, 1, true);
						flashc_memset8((void*)&(f.kria_state.cue_steps), cue_steps, 1, true);
						flashc_memset8((void*)&(f.kria_state.meta), meta, 1, true);
						flashc_memcpy((void *)&f.kria_state.k[preset], &k, sizeof(k), true);

						flashc_memcpy((void *)&f.scale, &scale_data, sizeof(scale_data), true);

						preset_mode_exit();
						grid_refresh = &refresh_kria;
						break;

					case mGridES:
						flashc_memset8((void*)&(f.es_state.preset), preset, 1, true);
						flashc_memcpy((void *)&f.es_state.e[preset], &e, sizeof(e), true);
						flashc_memcpy((void *)&f.scale, &scale_data, sizeof(scale_data), true);

						preset_mode_exit();
						grid_refresh = &refresh_es;
						break;

					default:
						break;
					}

					flashc_memset32((void*)&(f.kria_state.clock_period), clock_period, 4, true);
					flashc_memset32((void*)&(f.kria_state.sync_mode), kria_sync_mode, sizeof(kria_sync_mode), true);

					monomeFrameDirty++;
				}
			}
			else if(ansible_mode == mGridKria) {
				grid_keytimer_kria(held_keys[i1]);
			}

			if (view_tuning) {
				if (y == 5) {
					if (x == 11) {
						// reload factory default, don't immediately save it
						for (uint8_t i = 0; i < 4; i++) {
							for (uint8_t j = 0; j < 120; j ++) {
								tuning_table[i][j] = ET[j] << 2;
							}
						}
						restore_grid_tuning();
					}
					if (x == 13) {
						// apply fixed offset and save
						fit_tuning(0);
						flashc_memcpy((void*)f.tuning_table, tuning_table, sizeof(tuning_table), true);
						restore_grid_tuning();
					}
					if (x == 14) {
						// interpolate octaves and save
						fit_tuning(1);
						flashc_memcpy((void*)f.tuning_table, tuning_table, sizeof(tuning_table), true);
						restore_grid_tuning();
					}
					if (x == 15) {
						// save all tuning entries as-is
						flashc_memcpy((void *)f.tuning_table, tuning_table, sizeof(tuning_table), true);
						restore_grid_tuning();
					}
				}
			}

			// print_dbg("\rlong press: ");
			// print_dbg_ulong(held_keys[i1]);
		}
	}
}

void ii_grid(uint8_t* d, uint8_t len) {
	// print_dbg("\r\nii/grid (");
	// print_dbg_ulong(len);
	// print_dbg(") ");
	// for(int i=0;i<len;i++) {
	// 	print_dbg_ulong(d[i]);
	// 	print_dbg(" ");
	// }

	if (len < 1) {
		return;
	}

	switch (d[0]) {
	case II_GRID_KEY:
		if ( !preset_mode
		  && len >= 4
		  && d[1] < 16
		  && d[2] <  8
		  && d[3] < 16 ) {
			event_t e;
			uint8_t* data = (uint8_t*)(&(e.data));
			e.type = kEventMonomeGridKey;
			data[0] = d[1];
			data[1] = d[2];
			data[2] = d[3];
			event_post(&e);
		}
		break;
	case II_GRID_KEY + II_GET: {
		uint8_t z = 0;
		if ( len >= 2
		  && d[1] < 16
		  && d[2] < 8 ) {
			for (uint8_t i = 0; i < key_count; i++) {
				if (held_keys[i] % 16 == d[1]
				  && held_keys[i] / 16 == d[2]) {
					z = 1;
					break;
				}
			}
		}
		ii_tx_queue(z);
		break;
	}
	case II_GRID_LED + II_GET: {
		uint8_t led = 0;
		if ( len >= 3
		  && d[1] < 16
		  && d[2] < 8 ) {
			led = monomeLedBuffer[d[2] * 16 + d[1]];
		}
		ii_tx_queue(led);
		break;
	}
	default:
		break;
	}
}

////////////////////////////////////////////////////////////////////////////////
// KRIA

u8 track;

u8 loop_count;
u8 loop_first;
s8 loop_last;
u8 loop_edit;
u8 vrange_last;

bool note_sync;
uint8_t loop_sync;
bool note_div_sync;
uint8_t div_sync;

u8 pos[4][KRIA_NUM_PARAMS];
u8 pos_mul[4][KRIA_NUM_PARAMS];
bool pos_reset;
u8 tr[4];
u8 note[4];
u8 oct[4];
u16 dur[4];
u8 rpt[4]; // holds repeat count for step
u8 rptBits[4]; // holds repeat toggles for step
u8 alt_note[4];
u8 glide[4];
u8 activeRpt[4]; // needed for cases when triggers have a longer clock division than repeats (for viz)
u16 rptTicks[4]; // holds the length of a repeat
u8 repeats[4]; // gets decremented each repeat


bool cue;
u8 cue_sub_count;
u8 cue_count;
u8 cue_pat_next;

uint8_t meta_pos;
uint8_t meta_count;
uint8_t meta_next;
uint8_t meta_edit;

static void kria_off(void* o);
static void kria_blink_off(void* o);
static void kria_rpt_off(void* o);
static void kria_alt_mode_blink(void* o);
static void kria_set_alt_blink_timer(kria_view_t* views);
softTimer_t altBlinkTimer = { .next = NULL, .prev = NULL };
bool k_mode_is_alt = false;

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
	flashc_memset32((void*)&(f.kria_state.sync_mode), krSyncNone, sizeof(kria_sync_mode_t), true);
	flashc_memset8((void*)&(f.kria_state.preset), 0, 1, true);
	flashc_memset8((void*)&(f.kria_state.note_sync), true, 1, true);
	flashc_memset8((void*)&(f.kria_state.loop_sync), 2, 1, true);
	flashc_memset8((void*)&(f.kria_state.note_div_sync), false, 1, true);
	flashc_memset8((void*)&(f.kria_state.div_sync), 0, 1, true);
	flashc_memset8((void*)&(f.kria_state.cue_div), 0, 1, true);
	flashc_memset8((void*)&(f.kria_state.cue_steps), 3, 1, true);
	flashc_memset8((void*)&(f.kria_state.meta), 0, 1, true);

	for(i1=0;i1<8;i1++)
		k.glyph[i1] = 0;

	memset(k.p[0].t[0].tr, 0, 16);
	memset(k.p[0].t[0].oct, 0, 16);
	memset(k.p[0].t[0].note, 0, 16);
	memset(k.p[0].t[0].dur, 0, 16);
	memset(k.p[0].t[0].rpt, 1, 16);
	memset(k.p[0].t[0].rptBits, 1, 16);
	memset(k.p[0].t[0].p, 3, 16 * KRIA_NUM_PARAMS);
	k.p[0].t[0].octshift = 0;
	k.p[0].t[0].dur_mul = 4;
	k.p[0].t[0].direction = krDirForward;
	k.p[0].t[0].tt_clocked = false;
	k.p[0].t[0].trigger_clocked = false;
	memset(k.p[0].t[0].advancing, 1, KRIA_NUM_PARAMS);
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

	k_views[0].track = 0;
	k_views[0].mode = mTr;
	k_views[0].mod_mode = modNone;
	k_views[0].mode_is_alt = false;
	k_views[0].buffer = monomeLedBuffer;

	k_views[1].track = 0;
	k_views[1].mode = mNote;
	k_views[1].mod_mode = modNone;
	k_views[1].mode_is_alt = false;
	k_views[1].buffer = monomeLedBuffer + 128;

	note_sync = f.kria_state.note_sync;
	loop_sync = f.kria_state.loop_sync;
	note_div_sync = f.kria_state.note_div_sync;
	div_sync = f.kria_state.div_sync;
	cue_div = f.kria_state.cue_div;
	cue_steps = f.kria_state.cue_steps;

	preset = f.kria_state.preset;

	k.pattern = f.kria_state.k[preset].pattern;

	k = f.kria_state.k[preset];

	clock_mul = 1;

	clock_period = f.kria_state.clock_period;
	kria_sync_mode = f.kria_state.sync_mode;
	time_rough = (clock_period - 20) / 16;
	time_fine = (clock_period - 20) % 16;

	for ( int i=0; i<4; i++ ) {
		last_ticks[i] = get_ticks();
		for (int j=0;j<KRIA_NUM_PARAMS;j++) {
			tmul[i][j] = f.kria_state.k[preset].p[k.pattern].t[i].tmul[j];
		}
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

	reset_outputs();

	monomeFrameDirty++;
}

void grid_keytimer_kria(uint8_t held_key) {
	u8 k_mode = k_views[held_key < 128 ? 0 : 1].mode;
	held_key = held_key % 128;
	switch (k_mode) {
	case mPattern:
		if(held_key < 16) {
			memcpy((void *)&k.p[held_key], &k.p[k.pattern], sizeof(k.p[k.pattern]));
			k.pattern = held_key;
			// pos_reset = true;
		}
		break;
	case mRpt:
		if(held_key < 16) {
			k.p[k.pattern].t[track].rpt[held_key] -= 1;
			k.p[k.pattern].t[track].rptBits[held_key] = ~(0xff << k.p[k.pattern].t[track].rpt[held_key]);
		}
		else if(held_key >= R6 && held_key < R7) {
			k.p[k.pattern].t[track].rpt[held_key - R6] = 1;
			k.p[k.pattern].t[track].rptBits[held_key - R6] = 1;
		}
		break;
	default:
		break;
	}
}

bool kria_next_step(uint8_t t, uint8_t p) {
	pos_mul[t][p]++;

	bool latch_input = false;
	if (kria_sync_mode == krSyncNone) {
		latch_input = true;
	}
	else {
		switch (k.p[k.pattern].t[t].direction) {
		case krDirForward:
			if (pos[t][p] == k.p[k.pattern].t[t].lstart[p]) {
				latch_input = true;
			}
			break;
		case krDirReverse:
			if (pos[t][p] == k.p[k.pattern].t[t].lend[p]) {
				latch_input = true;
			}
			break;
		case krDirTriangle:
			if (pos[t][p] == k.p[k.pattern].t[t].lstart[p] || pos[t][p] == k.p[k.pattern].t[t].lend[p]) {
				latch_input = true;
			}
			break;
		default:
			latch_input = true;
			break;
		}
	}

	if (kria_sync_mode & krSyncTimeDiv) {
		if (latch_input) {
			tmul[t][p] = k.p[k.pattern].t[t].tmul[p];
		}
	}
	else {
		tmul[t][p] = k.p[k.pattern].t[t].tmul[p];
	}

	if(pos_mul[t][p] >= tmul[t][p]) {
		pos_mul[t][p] = 0;

		switch (k.p[k.pattern].t[t].direction) {
			default:
			case krDirForward: forward:
				if(pos[t][p] == k.p[k.pattern].t[t].lend[p]) {
					pos[t][p] = k.p[k.pattern].t[t].lstart[p];
				}
				else {
					pos[t][p]++;
					if(pos[t][p] > 15) {
						pos[t][p] = 0;
					}
				}
				break;
			case krDirReverse: reverse:
				if(pos[t][p] == k.p[k.pattern].t[t].lstart[p]) {
					pos[t][p] = k.p[k.pattern].t[t].lend[p];
				}
				else {
					pos[t][p]--;
					if(pos[t][p] > 15) {
						pos[t][p] = 15;
					}
				}
				break;
			case krDirTriangle:
				if (pos[t][p] == k.p[k.pattern].t[t].lend[p]) {
					k.p[k.pattern].t[t].advancing[p] = false;
				}
				if (pos[t][p] == k.p[k.pattern].t[t].lstart[p]) {
					k.p[k.pattern].t[t].advancing[p] = true;
				}
				if (k.p[k.pattern].t[t].advancing[p]) {
					goto forward;
				}
				else {
					goto reverse;
				}
				break;
			case krDirDrunk:
				if (rnd() % 2) {
					goto forward;
				} else {
					goto reverse;
				}
				break;
			case krDirRandom: {
				uint8_t lstart = k.p[k.pattern].t[t].lstart[p];
				uint8_t lend = k.p[k.pattern].t[t].lend[p];
				uint8_t llen = k.p[k.pattern].t[t].llen[p];

				if (lend >= lstart) {
					pos[t][p] = lstart + rnd() % (lend - lstart + 1);
				}
				else {
					pos[t][p] = (lstart + rnd() % (llen + 1)) % 16;
				}
				break;
			}
		}

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
			u64 current_tick = get_ticks();
			for(int i1=0;i1<KRIA_NUM_TRACKS;i1++)
			for(int i2=0;i2<KRIA_NUM_PARAMS;i2++) {
				pos[i1][i2] = k.p[k.pattern].t[i1].lend[i2];
				pos_mul[i1][i2] = k.p[k.pattern].t[i1].tmul[i2];
				last_ticks[i1] = current_tick - clock_deltas[i1];
			}
			cue_count = 0;
			cue_sub_count = 0;
			pos_reset = false;
		}

		for ( uint8_t i=0; i<KRIA_NUM_TRACKS; i++ )
		{
			if ( !k.p[k.pattern].t[i].tt_clocked )
				clock_kria_track( i );
		}

		monomeFrameDirty++;

		// may need forced DAC update here
		dac_timer_update();
	}
}

static inline int sum_clip(int l, int r, int clip) {
	return min(clip, max(0, l + r));
}

void clock_kria_note(kria_track* track, uint8_t trackNum) {
	if(kria_next_step(trackNum, mDur)) {
		f32 clock_scale = (clock_deltas[trackNum] * track->tmul[mTr]) / (f32)384.0;
		f32 unscaled = (track->dur[pos[trackNum][mDur]]+1) * (track->dur_mul<<2);
		dur[trackNum] = (u16)(unscaled * clock_scale);
		aux_param[0][trackNum] = (int)unscaled;
	}
	if(kria_next_step(trackNum, mOct)) {
		oct[trackNum] = sum_clip(track->octshift, track->oct[pos[trackNum][mOct]], 5);
	}
	if(kria_next_step(trackNum, mNote)) {
		note[trackNum] = track->note[pos[trackNum][mNote]];
	}
	if(kria_next_step(trackNum, mAltNote)) {
		alt_note[trackNum] = track->alt_note[pos[trackNum][mAltNote]];
	}
	if(kria_next_step(trackNum, mGlide)) {
		glide[trackNum] = track->glide[pos[trackNum][mGlide]];
	}
}

static void kria_set_note(uint8_t trackNum) {
	u8 noteInScale = (note[trackNum] + alt_note[trackNum]) % 7; // combine both note params
	u8 octaveBump = (note[trackNum] + alt_note[trackNum]) / 7; // if it wrapped around the octave, bump it
	set_cv_note(
		trackNum,
		(int)cur_scale[noteInScale] +
		scale_adj[noteInScale] +
		(int)((oct[trackNum]+octaveBump) * 12));
}

void clock_kria_track( uint8_t trackNum ) {
	u64 current_tick = get_ticks();
	clock_deltas[trackNum] = (u32)(current_tick-last_ticks[trackNum]);
	last_ticks[trackNum] = current_tick;

	kria_track* track = &k.p[k.pattern].t[trackNum];
	u8* trackIndex = &kria_track_indices[trackNum];

	bool trNextStep = kria_next_step(trackNum, mTr);
	bool isTrigger = track->tr[pos[trackNum][mTr]];

	// if the track isn't in trigger_step mode, or if there is a trigger
	// THEN we clock the other parameters
	if (!k.p[k.pattern].t[trackNum].trigger_clocked) {
		clock_kria_note(track, trackNum);
	}
	if(kria_next_step(trackNum, mRpt)) {
		rpt[trackNum] = track->rpt[pos[trackNum][mRpt]];
		rptBits[trackNum] = track->rptBits[pos[trackNum][mRpt]];
	}

	if(trNextStep && isTrigger) {
		if( !kria_mutes[trackNum] ) {

			set_cv_slew( trackNum, glide[trackNum] * 20 );

			activeRpt[trackNum] = rpt[trackNum];

			repeats[trackNum] = rpt[trackNum] - 1;
			timer_remove( &repeatTimer[trackNum] );
			if ( repeats[trackNum] > 0 ) {
				rptTicks[trackNum] = (clock_deltas[trackNum] * (u32)track->tmul[mTr] ) / rpt[trackNum];
				timer_add( &repeatTimer[trackNum], rptTicks[trackNum], &kria_rpt_off, trackIndex );
			}

			if ( rptBits[trackNum] & 1 ) {
				if (k.p[k.pattern].t[trackNum].trigger_clocked) {
					clock_kria_note(track, trackNum);
				}
				kria_set_note(trackNum);
				set_tr( TR1 + trackNum );

				timer_remove( &auxTimer[trackNum]);
				timer_add(&auxTimer[trackNum], ((u32)dur[trackNum]) / ((u32)rpt[trackNum]), &kria_off, trackIndex);
				timer_remove( &blinkTimer[trackNum] );
				timer_add( &blinkTimer[trackNum], max(dur[trackNum]/rpt[trackNum],31), &kria_blink_off, trackIndex );

				tr[trackNum] = 1;
				kria_blinks[trackNum] = 1;
			}
		}
	}
}

static void kria_off(void* o) {
	int index = *(u8*)o;
	timer_remove( &auxTimer[index] );
	clr_tr(TR1 + index);
	tr[index] = 0;
}

static void kria_blink_off(void* o) {
	int index = *(u8*)o;
	timer_remove( &blinkTimer[index] );
	kria_blinks[index] = 0;
	monomeFrameDirty++;
}

static void kria_rpt_off(void* o) {
	int index = *(u8*)o;
	uint8_t bit = activeRpt[index] - repeats[index];
	kria_track* track = &k.p[k.pattern].t[index];

	repeats[index]--;
	if ( repeats[index] <= 0 ) {
		timer_remove( &repeatTimer[index] );
	}

	if ( track->rptBits[pos[index][mRpt]] & (1 << bit) ) {
		if (track->trigger_clocked) {
			clock_kria_note(track, index);
		}
		kria_set_note(index);
		set_tr( TR1 + index );
		tr[index] = 1;
		kria_blinks[index] = 1;

		timer_remove( &auxTimer[index]);
		timer_add(&auxTimer[index], ((u32)dur[index]) / ((u32)rpt[index]), &kria_off, o);
		timer_remove( &blinkTimer[index] );
		timer_add( &blinkTimer[index], max(dur[index]/rpt[index],31), &kria_blink_off, o );
		monomeFrameDirty++;
	}
}

static void kria_alt_mode_blink(void* o) {
	kriaAltModeBlink = !kriaAltModeBlink;
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
	int track, param;

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
				change_pattern(d[1]);
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
			track = d[1];
			param = d[2];
			switch(loop_sync) {
			case 2:
				for ( int i=0; i<KRIA_NUM_TRACKS; i++ ) {
					for ( int j=0; j<KRIA_NUM_PARAMS; j++ ) {
						pos[i][j] = k.p[k.pattern].t[i].lend[j];
						pos_mul[i][j] = k.p[k.pattern].t[i].tmul[j];
					}
				}
				break;
			case 1:
				if(track == 0) {
					for ( int i=0; i<KRIA_NUM_TRACKS; i++ ) {
						for ( int j=0; j<KRIA_NUM_PARAMS; j++ ) {
							pos[i][j] = k.p[k.pattern].t[i].lend[j];
							pos_mul[i][j] = k.p[k.pattern].t[i].tmul[j];
						}
					}
				}
				else if( track <= KRIA_NUM_TRACKS ) {
					for( int i=0; i<KRIA_NUM_PARAMS; i++ ) {
						pos[track-1][i] = k.p[k.pattern].t[track-1].lend[i];
						pos_mul[track-1][i] = k.p[k.pattern].t[track-1].tmul[i];
					}
				}
				break;
			case 0:
				if( track == 0 && param == 0 ) {
					for ( int i=0; i<KRIA_NUM_TRACKS; i++ ) {
						for ( int j=0; j<KRIA_NUM_PARAMS; j++ ) {
							pos[i][j] = k.p[k.pattern].t[i].lend[j];
							pos_mul[i][j] = k.p[k.pattern].t[i].tmul[j];
						}
					}
				}
				else if( track == 0 && param <= KRIA_NUM_PARAMS ) {
					for ( int i=0; i<KRIA_NUM_TRACKS; i++ ) {
						pos[i][param-1] = k.p[k.pattern].t[i].lend[param-1];
						pos_mul[i][param-1] = k.p[k.pattern].t[i].tmul[param-1];
					}
				}
				else if( param == 0 && track <= KRIA_NUM_TRACKS ) {
					for ( int i=0; i<KRIA_NUM_PARAMS; i++ ) {
						pos[track-1][i] = k.p[k.pattern].t[track-1].lend[i];
						pos_mul[track-1][i] = k.p[k.pattern].t[track-1].tmul[i];
					}
				}
				else if(track <= KRIA_NUM_TRACKS && param <= KRIA_NUM_PARAMS) {
					pos[track-1][param-1] = k.p[k.pattern].t[track-1].lend[param-1];
					pos_mul[track-1][param-1] = k.p[k.pattern].t[track-1].tmul[param-1];
				}
				break;
			default:
				break;
			}
			break;
		case II_KR_LOOP_ST:
			if(d[3] < 16) {
				track = d[1];
				param = d[2];
				int loopStart = d[3];
				switch(loop_sync) {
				case 2:
					for(int i=0;i<4;i++)
						for(int j=0;j<4;j++)
							adjust_loop_start(i, loopStart, j);
					break;
				case 1:
					if(d[1] == 0) {
						for(int i=0;i<KRIA_NUM_TRACKS;i++)
							for(int j=0;j<KRIA_NUM_PARAMS;j++)
								adjust_loop_start(i, loopStart, j);
					}
					else if( track <= KRIA_NUM_TRACKS ) {
						for(int i=0;i<KRIA_NUM_PARAMS;i++)
							adjust_loop_start(track-1, loopStart, i);
					}
					break;
				case 0:
					if(track == 0 && param == 0) {
						for(int i=0;i<KRIA_NUM_TRACKS;i++)
							for(int j=0;j<KRIA_NUM_PARAMS;j++)
								adjust_loop_start(i, loopStart, j);
					}
					else if(track == 0 && param <= KRIA_NUM_PARAMS) {
						for(int i=0;i<KRIA_NUM_TRACKS;i++)
							adjust_loop_start(i, loopStart, param-1);
					}
					else if(param == 0 && track <= KRIA_NUM_TRACKS) {
						for(int i=0;i<KRIA_NUM_PARAMS;i++)
							adjust_loop_start(track-1, loopStart, i);
					}
					else if(track <= KRIA_NUM_TRACKS && param <= KRIA_NUM_PARAMS) {
						adjust_loop_start(track-1, loopStart, param-1);
					}
					break;
				default:
					break;
				}
			}
			break;
		case II_KR_LOOP_ST + II_GET:
			track = d[1];
			param = d[2];
			if( track == 0 && param == 0) {
				int n = 0;
				for(int i=0; i<KRIA_NUM_TRACKS; i++)
					for(int j=0;j<KRIA_NUM_PARAMS;j++)
						n += k.p[k.pattern].t[i].lstart[j];
				ii_tx_queue(n>>4);
			}
			else if(track == 0) {
				int n = 0;
				for(int i=0; i<KRIA_NUM_TRACKS; i++)
					n += k.p[k.pattern].t[i].lstart[param-1];
				ii_tx_queue(n>>2);
			}
			else if(param == 0) {
				int n = 0;
				for(int i=0;i<KRIA_NUM_PARAMS;i++)
					n += k.p[k.pattern].t[track-1].lstart[i];
				ii_tx_queue(n>>2);
			}
			else {
				ii_tx_queue(k.p[k.pattern].t[track-1].lstart[param-1]);
			}
			break;
		case II_KR_LOOP_LEN:
			if(d[3] < 17 && d[3] > 0) {

				track = d[1];
				param = d[2];
				int loopLength = d[3];

				switch(loop_sync) {
				case 2:
					for(int i=0;i<KRIA_NUM_TRACKS;i++)
						for(int j=0;j<KRIA_NUM_PARAMS;j++)
							adjust_loop_len(i, d[3], j);
					break;
				case 1:
					if(track == 0) {
						for(int i=0;i<KRIA_NUM_TRACKS;i++)
							for(int j=0;j<KRIA_NUM_PARAMS;j++)
								adjust_loop_len(i, loopLength, j);
					}
					else if(track <= KRIA_NUM_TRACKS) {
						for(int i=0;i<KRIA_NUM_PARAMS;i++)
							adjust_loop_len(d[1]-1, d[3], i);
					}
					break;
				case 0:
					if(track == 0 && param == 0) {
						for(int i=0;i<KRIA_NUM_TRACKS;i++)
							for(int j=0;j<KRIA_NUM_PARAMS;j++)
								adjust_loop_len(i, loopLength, j);
					}
					else if(track == 0 && param <= KRIA_NUM_PARAMS) {
						for(int i=0;i<KRIA_NUM_TRACKS;i++)
							adjust_loop_len(i, loopLength, param-1);
					}
					else if(param == 0 && track <= KRIA_NUM_TRACKS) {
						for(int i=0;i<KRIA_NUM_PARAMS;i++)
							adjust_loop_len(d[1]-1, loopLength, i);
					}
					else if(track <= KRIA_NUM_TRACKS && param < KRIA_NUM_PARAMS) {
						adjust_loop_len(track-1, loopLength, param-1);
					}
					break;
				default:
					break;
				}
			}
			break;
		case II_KR_LOOP_LEN + II_GET:
			track = d[1];
			param = d[2];
			if( track == 0 && param == 0) {
				int n = 0;
				for(int i=0;i<KRIA_NUM_TRACKS;i++)
					for(int j=0;j<KRIA_NUM_PARAMS;j++)
						n += k.p[k.pattern].t[i].llen[j];
				ii_tx_queue(n>>4);
			}
			else if(track == 0) {
				int n = 0;
				for(int i=0;i<KRIA_NUM_TRACKS;i++)
					n += k.p[k.pattern].t[i].llen[param-1];
				ii_tx_queue(n>>2);
			}
			else if(param == 0) {
				int n = 0;
				for(int i=0;i<KRIA_NUM_PARAMS;i++)
					n += k.p[k.pattern].t[track-1].llen[i];
				ii_tx_queue(n>>2);
			}
			else {
				ii_tx_queue(k.p[k.pattern].t[track-1].llen[param-1]);
			}
			break;
		case II_KR_POS:
			if(d[3] < 17) {

				track = d[1];
				param = d[2];
				int pos = d[3];

				switch(loop_sync) {
				case 2:
					for(int i=0;i<KRIA_NUM_TRACKS;i++)
						for(int j=0;j<KRIA_NUM_PARAMS;j++)
							jump_pos(i, pos, j);
					break;
				case 1:
					if(d[1] == 0) {
						for(int i=0;i<KRIA_NUM_TRACKS;i++)
							for(int j=0;j<KRIA_NUM_PARAMS;j++)
								jump_pos(i, pos, j);
					}
					else if(d[1] < 5) {
						for(int i=0;i<KRIA_NUM_PARAMS;i++)
							jump_pos(d[1]-1, pos, i);
					}
					break;
				case 0:
					if(track == 0 && param == 0) {
						for(int i=0;i<KRIA_NUM_TRACKS;i++)
							for(int j=0;j<KRIA_NUM_PARAMS;j++)
								jump_pos(i, pos, j);
					}
					else if(track == 0 && param <= KRIA_NUM_PARAMS) {
						for(int i=0;i<KRIA_NUM_TRACKS;i++)
							jump_pos(i, pos, param-1);
					}
					else if(param == 0 && track <= KRIA_NUM_TRACKS) {
						for(int i=0;i<KRIA_NUM_PARAMS;i++)
							jump_pos(track-1, pos, i);
					}
					else if(track <= KRIA_NUM_TRACKS && param <= KRIA_NUM_PARAMS) {
						jump_pos(track-1, pos, param-1);
					}
					break;
				default:
					break;
				}
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
				for ( int i=0; i<KRIA_NUM_TRACKS; i++ ) {
					if ( k.p[k.pattern].t[i].tt_clocked )
						clock_kria_track( i );
				}
			}
			else if ( d[1] <= KRIA_NUM_TRACKS && d[1] > 0  ) {
				if ( k.p[k.pattern].t[d[1]-1].tt_clocked )
					clock_kria_track( d[1]-1 );
			}
			break;
		case II_KR_PAGE:
			if ( l >= 2 ) {
				kria_modes_t next_mode = ii_kr_mode_for_cmd(d[1]);
				if (next_mode < 0) {
					break;
				}
				k_views[0].mode = next_mode;
				if (k_views[0].mode == mPattern) {
					cue = true;
				}
				kria_set_alt_blink_timer(k_views);
			}
			break;
		case II_KR_PAGE + II_GET:
			ii_tx_queue(ii_kr_cmd_for_mode(k_views[0].mode));
			break;
		case II_KR_CUE:
			if (!meta && l >= 2) {
				cue_pat_next = d[1] + 1;
				cue = true;
			}
			break;
		case II_KR_CUE + II_GET: {
			ii_tx_queue(cue_pat_next - 1);
			break;
		}
		case II_KR_DIR:
			if ( d[1] < 0
			  || d[1] > KRIA_NUM_TRACKS
			  || d[2] < 0
			  || d[2] > krDirRandom) break;
			if ( d[1] == 0 ) {
				for ( int i=0; i<KRIA_NUM_TRACKS; i++) {
					k.p[k.pattern].t[i].direction = d[2];
				}
			}
			else {
				k.p[k.pattern].t[d[1]-1].direction = d[2];
			}
			break;
		case II_KR_DIR + II_GET:
			if ( d[1] <= 0
			  || d[1] > KRIA_NUM_TRACKS) break;
			if (l >= 2) {
				ii_tx_queue(k.p[k.pattern].t[d[1] - 1].direction);
			}
			break;
		case II_KR_DURATION + II_GET:
			if ( d[1] < 0
			  || d[1] >= KRIA_NUM_TRACKS) {
				ii_tx_queue(0);
				ii_tx_queue(0);
				break;
			}
			ii_tx_queue(dur[d[1]] >> 8);
			ii_tx_queue(dur[d[1]] & 0xFF);
			break;
		default:
			ii_grid(d, l);
			ii_ansible(d, l);
			break;
		}
	}
}

static kria_modes_t ii_kr_mode_for_cmd(uint8_t d) {
	switch (d) {
	case 0:  return mTr;
	case 1:  return mRpt;
	case 2:  return mNote;
	case 3:  return mAltNote;
	case 4:  return mOct;
	case 5:  return mGlide;
	case 6:  return mDur;
	case 7:  return -1;
	case 8:  return mScale;
	case 9:  return mPattern;
	default: return -1;
	}
}

static uint8_t ii_kr_cmd_for_mode(kria_modes_t mode) {
	switch (mode) {
	case mTr:      return 0;
	case mRpt:     return 1;
	case mNote:    return 2;
	case mAltNote: return 3;
	case mOct:     return 4;
	case mGlide:   return 5;
	case mDur:     return 6;
	case mScale:   return 8;
	case mPattern: return 9;
	default:       return -1;
	}
}

static void kria_set_alt_blink_timer(kria_view_t* views) {
	bool any_view_is_alt = false;
	for (int i = 0; i < 2; i++) {
		if ( views[i].mode == mRpt || views[i].mode == mAltNote || views[i].mode == mGlide ) {
			any_view_is_alt = true;
			views[i].mode_is_alt = true;
		}
	}
	timer_remove( &altBlinkTimer );	
	if (any_view_is_alt) {
		timer_add( &altBlinkTimer, 100, &kria_alt_mode_blink, NULL);
	}
}

static void kria_set_tmul(uint8_t track, kria_modes_t mode, uint8_t new_tmul) {
	switch (div_sync) {
	case 1:
		if (note_div_sync) {
			if (mode == mTr || mode == mNote) {
				k.p[k.pattern].t[track].tmul[mTr] = new_tmul;
				k.p[k.pattern].t[track].tmul[mNote] = new_tmul;
			} else {
				k.p[k.pattern].t[track].tmul[mRpt] = new_tmul;
				k.p[k.pattern].t[track].tmul[mAltNote] = new_tmul;
				k.p[k.pattern].t[track].tmul[mOct] = new_tmul;
				k.p[k.pattern].t[track].tmul[mGlide] = new_tmul;
				k.p[k.pattern].t[track].tmul[mDur] = new_tmul;
			}
		} else {
			k.p[k.pattern].t[track].tmul[mTr] = new_tmul;
			k.p[k.pattern].t[track].tmul[mNote] = new_tmul;
			k.p[k.pattern].t[track].tmul[mRpt] = new_tmul;
			k.p[k.pattern].t[track].tmul[mAltNote] = new_tmul;
			k.p[k.pattern].t[track].tmul[mOct] = new_tmul;
			k.p[k.pattern].t[track].tmul[mGlide] = new_tmul;
			k.p[k.pattern].t[track].tmul[mDur] = new_tmul;
		}
		break;
	case 2:
		for (uint8_t i = 0; i < 4; i++) {
			if (note_div_sync) {
				if (mode == mTr || mode == mNote) {
					k.p[k.pattern].t[i].tmul[mTr] = new_tmul;
					k.p[k.pattern].t[i].tmul[mNote] = new_tmul;
				} else {
					k.p[k.pattern].t[i].tmul[mRpt] = new_tmul;
					k.p[k.pattern].t[i].tmul[mAltNote] = new_tmul;
					k.p[k.pattern].t[i].tmul[mOct] = new_tmul;
					k.p[k.pattern].t[i].tmul[mGlide] = new_tmul;
					k.p[k.pattern].t[i].tmul[mDur] = new_tmul;
				}
			} else {
				k.p[k.pattern].t[i].tmul[mTr] = new_tmul;
				k.p[k.pattern].t[i].tmul[mNote] = new_tmul;
				k.p[k.pattern].t[i].tmul[mRpt] = new_tmul;
				k.p[k.pattern].t[i].tmul[mAltNote] = new_tmul;
				k.p[k.pattern].t[i].tmul[mOct] = new_tmul;
				k.p[k.pattern].t[i].tmul[mGlide] = new_tmul;
				k.p[k.pattern].t[i].tmul[mDur] = new_tmul;
			}
		}
		break;
	default:
		k.p[k.pattern].t[track].tmul[mode] = new_tmul;
		if (note_div_sync) {
			if (mode == mTr) {
				k.p[k.pattern].t[track].tmul[mNote] = new_tmul;
			}
			if (mode == mNote) {
				k.p[k.pattern].t[track].tmul[mTr] = new_tmul;
			}
		}
		break;
	}
}


static void preset_mode_handle_key(u8 x, u8 y, u8 z, u8* glyph) {
	if (z) {
		if (x == 5 && y == 7) {
			if (follower_select) {
				follower_select = false;
			} else {
				mod_follower = true;
			}
		}
		if (follower_select) {
			if (y == 0) {
				if (x <= 6) {
					followers[follower].oct = x - 3;
					follower_change_octave(&followers[follower], followers[follower].oct);
				}
				if (followers[follower].mode_ct > 1
				 && x >= 13
				 && x <= (13 + followers[follower].mode_ct)) {
					follower_change_mode(&followers[follower], x - 13);
				}
			}
			if (y >= 2 && y <= 5) {
				if (x == 5) {
					follower = y - 2;
				}
			}
			if (y == 7) {
				if (x <= 3) {
					followers[follower].track_en ^= 1 << x;
				}
			}
		}
		else {
			if (x > 7) {
				glyph[y] ^= 1<<(x-8);
			}
			if (x == 5 && y >= 2 && y <= 5) {
				if (mod_follower) {
					follower = y - 2;
					follower_select = true;
				}
				else {
					toggle_follower(y - 2);
				}
			}
		}

		monomeFrameDirty++;
	}
	else {
		if (x == 5 && y == 7) {
			mod_follower = false;
			monomeFrameDirty++;
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
				if(x == 0 && !follower_select) {
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

						preset_mode_exit();
						grid_refresh = &refresh_kria;

						// print_dbg("\r\npreset RECALL:");
						// print_dbg_ulong(preset);
					}
				}
			}
			else if ((k_views[0].mode == mPattern && y == 0 ) || (k_views[1].mode == mPattern && y == 8)) {
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
			else if(view_tuning) {
				if (y == 5) {
					if (x == 11) {
						init_tuning();
						restore_grid_tuning();
					} else if (x == 13) {
						fit_tuning(0);
						restore_grid_tuning();
					} else if (x == 14) {
						fit_tuning(1);
						restore_grid_tuning();
					}
				}
			}
		}
	}

	// PRESET SCREEN
	if(preset_mode) {
		preset_mode_handle_key(x, y, z, k.glyph);
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
				else if(y>=4) {
					if(y==4 && x >= 6 && x <= 9) {
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
				}

				clock_period = 20 + (time_rough * 16) + time_fine;

				clock_set(clock_period);

				// print_dbg("\r\nperiod: ");
				// print_dbg_ulong(clock_period);
			}


			if (y >= 4 && x <= 3) {
				note_div_sync ^= 1;
				flashc_memset8((void*)&(f.kria_state.note_div_sync), note_div_sync, sizeof(note_div_sync), true);
			}
			if (x >= 7 && x <= 8 && y >= 6) {
				kria_sync_mode ^= 1;
				flashc_memset8((void*)&(f.kria_state.sync_mode), kria_sync_mode, sizeof(kria_sync_mode), true);
			}
			if (x >= 12) {
				if (y == 5) {
					if (div_sync == 1) {
						div_sync = 0;
					} else {
						div_sync = 1;
					}
					flashc_memset8((void*)&(f.kria_state.div_sync), div_sync, sizeof(div_sync), true);
				}
				if (y == 7) {
					if (div_sync == 2) {
						div_sync = 0;
					} else {
						div_sync = 2;
					}
					flashc_memset8((void*)&(f.kria_state.div_sync), div_sync, sizeof(div_sync), true);
				}
			}

			monomeFrameDirty++;
		}


		// time_rough = (clock_period - 20) / 16;
		// time_fine = (clock_period - 20) % 16;
	}
	else if(view_config) {
		if(z) {
			if(x<8 && y > 0 && y<7) {
				note_sync ^= 1;
				flashc_memset8((void*)&(f.kria_state.note_sync), note_sync, 1, true);
			}
			else if(y == 0 && x < 3) {
				grid_varibrightness = x == 0 ? 1 :
						      x == 1 ? 4 :
						      16;
				flashc_memset8((void*)&(f.state.grid_varibrightness), grid_varibrightness, 1, true);
			}
			else if(y == 3) {
				if(loop_sync == 1) {
					loop_sync = 0;
				}
				else loop_sync = 1;

				flashc_memset8((void*)&(f.kria_state.loop_sync), loop_sync, 1, true);
			}
			else if(y == 5) {
				if(loop_sync == 2) {
					loop_sync = 0;
				}
				else loop_sync = 2;

				flashc_memset8((void*)&(f.kria_state.loop_sync), loop_sync, 1, true);
			}
			else if (y == 7 && x == 14) {
				view_config = false;
				view_clock = false;
				view_tuning = true;

				grid_refresh = &refresh_grid_tuning;
				restore_grid_tuning();
			}
			monomeFrameDirty++;
		}
	}
	else if(view_tuning) {
		if(z) {
			if (y == 4 && x == 0) {
			   	mod_tuning = true;
		        }
			if (y <= 3) {
				if (x == 0) {
			  		if (tuning_note_on[y]) {
						tuning_note_on[y] = false;
						clr_tr(TR1 + y);
					}
					else {
						tuning_note_on[y] = true;
						set_tr(TR1 + y);
					}
				}
				else if (x >= 2 && x <= 13) {
					if (y == tuning_track
					    && (x - 2 == tuning_octave_offset[y])) {
			  			if (tuning_note_on[y]) {
							tuning_note_on[y] = false;
							clr_tr(TR1 + y);
						}
						else {
							tuning_note_on[y] = true;
							set_tr(TR1 + y);
						}
					}
					else {
						tuning_track = y;
						tuning_octave_offset[y] = (int)x - 2;
						dac_set_value_noslew(
							y,
							tuning_table[y][
							        (int)(tuning_octave * 12) +
								tuning_octave_offset[y]
							]);
					}
				}
			}
			else if (y == 5 && x <= 9) {
				tuning_octave = x;
				for (uint8_t i = 0; i < 4; i++) {
					dac_set_value_noslew(
						i,
						tuning_table[i][
						        (int)(tuning_octave * 12) +
							tuning_octave_offset[i]
						]);
				}
			}
			else if (y == 6) {
				int tuning_slot = (int)(tuning_octave * 12) + tuning_octave_offset[tuning_track];
				tuning_table[tuning_track][tuning_slot] = x * ((DAC_10V) / 16);
				dac_set_value_noslew(
					tuning_track,
					tuning_table[tuning_track][
					        (int)(tuning_octave * 12) +
						tuning_octave_offset[tuning_track]
					]);
			}
			else if (y == 7) {
				int tuning_slot = (int)(tuning_octave * 12) + tuning_octave_offset[tuning_track];
				if (x >= 8) {
					if (mod_tuning) {
						for (uint8_t t = 0; t < 4; t++) {
							for (uint8_t s = 0; s < 120; s++) {
								tuning_table[t][s] = sum_clip(
									tuning_table[t][s],
									1 << (x - 8),
									DAC_10V);
							}
						}
					}
					else {
						tuning_table[tuning_track][tuning_slot] = sum_clip(
							tuning_table[tuning_track][tuning_slot],
							1 << (x - 8),
							DAC_10V);
					}
				} else {
					if (mod_tuning) {
						for (uint8_t t = 0; t < 4; t++) {
							for (uint8_t s = 0; s < 120; s++) {
								tuning_table[t][s] = sum_clip(
									tuning_table[t][s],
									- (1 << (8 - x)),
									DAC_10V);
							}
						}
					}
					else {
						tuning_table[tuning_track][tuning_slot] = sum_clip(
							tuning_table[tuning_track][tuning_slot],
							- (1 << (8 - x)),
							DAC_10V);
					}
				}

				if (mod_tuning) {
					for (uint8_t t = 0; t < 4; t++) {
						dac_set_value_noslew(
							t,
							tuning_table[t][
							        (int)(tuning_octave * 12) +
								tuning_octave_offset[t]
							]);
					}
				}
				else {
					dac_set_value_noslew(
						tuning_track,
						tuning_table[tuning_track][
						        (int)(tuning_octave * 12) +
							tuning_octave_offset[tuning_track]
						]);
				}
			}
		}
		else {
			if (y == 4 && x == 0) {
			   	mod_tuning = false;
		        }
		}
	}
	// NORMAL
	else {

		kria_view_t* view = &k_views[0];
		if (y >= 8) {
			// lower half of a 256
			view = &k_views[1];
			y = y - 8;
		}

		// bottom row
		if(y == 7) {
			if(z) {
				switch(x) {
				case 0:
				case 1:
				case 2:
				case 3:
					if ( view->mod_mode == modLoop )
						kria_mutes[x] = !kria_mutes[x];
					else
						view->track = x;
					break;
				case 5:
					if ( view->mode == mTr )
						view->mode = mRpt;
					else
						view->mode = mTr;
					break;
				case 6:
					if ( view->mode == mNote )
						view->mode = mAltNote;
					else
						view->mode = mNote;
					break;
				case 7:
					if ( view->mode == mOct )
						view->mode = mGlide;
					else
						view->mode = mOct;
					break;
				case 8:
					view->mode = mDur; break;
				case 10:
					view->mod_mode = modLoop;
					loop_count = 0;
					break;
				case 11:
					view->mod_mode = modTime; break;
				case 12:
					view->mod_mode = modProb; break;
				case 14:
					view->mode = mScale; break;
				case 15:
					view->mode = mPattern;
					cue = true;
					break;
				default: break;
				}
				kria_set_alt_blink_timer(k_views);
			}
			else {
				switch(x) {
				case 10:
				case 11:
				case 12:
					view->mod_mode = modNone;
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

			const u8 track = view->track;
			const kria_modes_t k_mode = view->mode;
			const kria_mod_modes_t k_mod_mode = view->mod_mode;

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
							if (note_sync) {
								update_loop_start(loop_edit, loop_first, mNote);
								update_loop_end(loop_edit, loop_last, mNote);
							}
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
									if (note_sync) {
										update_loop_start(loop_edit, loop_first, mNote);
										update_loop_end(loop_edit, loop_first, mNote);
									}
								} else {
									update_loop_start(loop_edit, loop_first, mTr);
									if (note_sync) {
										update_loop_start(loop_edit, loop_first, mNote);
									}
								}
							}
							monomeFrameDirty++;
						}
					}
					break;
				case modTime:
					if(z) {
						kria_set_tmul(track, mTr, x + 1);
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
							if (note_sync) {
								update_loop_start(track, loop_first, mTr);
								update_loop_end(track, loop_last, mTr);
							}
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
									if (note_sync) {
										update_loop_start(track, loop_first, mTr);
										update_loop_end(track, loop_first, mTr);
									}
								} else {
									update_loop_start(track, loop_first, mNote);
									if (note_sync) {
										update_loop_start(track, loop_first, mTr);
									}
								}
							}
							monomeFrameDirty++;
						}
					}
					break;
				case modTime:
					if(z) {
						kria_set_tmul(track, mNote, x + 1);
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
						if(y==0) {
							if (x <= 5) {
								k.p[k.pattern].t[track].octshift = x;
							}
						}
						else {
							uint8_t abs_oct = 6 - y;
							k.p[k.pattern].t[track].oct[x] = (int)abs_oct - (int)k.p[k.pattern].t[track].octshift;
						}
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
						kria_set_tmul(track, mOct, x + 1);
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
						kria_set_tmul(track, mDur, x + 1);
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
			case mRpt:
				switch(k_mod_mode) {
				case modNone:
					if (z) {
						if ( y == 0 ) {
							if (k.p[k.pattern].t[track].rpt[x] < 5) {
								k.p[k.pattern].t[track].rpt[x]++;
							}
							else {
								k.p[k.pattern].t[track].rpt[x] = 5;
							}
						}
						if ( y > 0 && y < 6 ) {
					  		uint8_t rptBits = k.p[k.pattern].t[track].rptBits[x] ^ (1 << (5 - y));
							uint8_t rpt = 1;
							k.p[k.pattern].t[track].rptBits[x] = rptBits;
							while (rptBits >>= 1) rpt++;
							k.p[k.pattern].t[track].rpt[x] = rpt;

							monomeFrameDirty++;
						}
						if ( y == 6 ) {
							if (k.p[k.pattern].t[track].rpt[x] > 1) {
								k.p[k.pattern].t[track].rpt[x]--;
							}
							else {
								k.p[k.pattern].t[track].rpt[x] = 1;
							}
						}
					}
					break;
				case modLoop:
					if(z) {
						if(loop_count == 0) {
							loop_first = x;
							loop_last = -1;
							vrange_last = -1;
						}
						else {
							loop_last = x;
							if(loop_last == loop_first) {
								vrange_last = y;
								k.p[k.pattern].t[track].rpt[x] = 6 - y;
							}
							else {
								update_loop_start(track, loop_first, mRpt);
								update_loop_end(track, loop_last, mRpt);
							}
						}

						loop_count++;
					}
					else {
						loop_count--;

						if(loop_count == 0) {
							if(loop_last == -1) {
								if(loop_first == k.p[k.pattern].t[track].lstart[mRpt]) {
									if(vrange_last == -1 && x == loop_first) {
										k.p[k.pattern].t[track].rpt[x] = 6 - y;
									}
									else {
										update_loop_start(track, loop_first, mRpt);
										update_loop_end(track, loop_first, mRpt);
									}
								}
								else {
									update_loop_start(track, loop_first, mRpt);
								}
							}
							monomeFrameDirty++;
						}
					}
					break;
				case modTime:
					if (z) {
						kria_set_tmul(track, mRpt, x + 1);
						monomeFrameDirty++;
					}
					break;
				case modProb:
					if(z && y > 1 && y < 6) {
						k.p[k.pattern].t[track].p[mRpt][x] = 5 - y;
						monomeFrameDirty++;
					}
					break;
				default: break;
				}
				break;
			case mAltNote:
				switch(k_mod_mode) {
				case modNone:
					if(z) {
						k.p[k.pattern].t[track].alt_note[x] = 6-y;
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
							update_loop_start(track, loop_first, mAltNote);
							update_loop_end(track, loop_last, mAltNote);
						}

						loop_count++;
					}
					else {
						loop_count--;

						if(loop_count == 0) {
							if(loop_last == -1) {
								if(loop_first == k.p[k.pattern].t[track].lstart[mAltNote]) {
									update_loop_start(track, loop_first, mAltNote);
									update_loop_end(track, loop_first, mAltNote);
								}
								else
									update_loop_start(track, loop_first, mAltNote);
							}
							monomeFrameDirty++;
						}
					}
					break;
				case modTime:
					if(z) {
						kria_set_tmul(track, mAltNote, x + 1);
						monomeFrameDirty++;
					}
					break;
				case modProb:
					if(z && y > 1 && y < 6) {
						k.p[k.pattern].t[track].p[mAltNote][x] = 5 - y;
						monomeFrameDirty++;
					}
					break;
				default: break;
				}
				break;
			case mGlide:
				switch(k_mod_mode) {
				case modNone:
					if(z) {
						// if(y>2)
							k.p[k.pattern].t[track].glide[x] = 6-y;
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
							update_loop_start(track, loop_first, mGlide);
							update_loop_end(track, loop_last, mGlide);
						}

						loop_count++;
					}
					else {
						loop_count--;

						if(loop_count == 0) {
							if(loop_last == -1) {
								if(loop_first == k.p[k.pattern].t[track].lstart[mGlide]) {
									update_loop_start(track, loop_first, mGlide);
									update_loop_end(track, loop_first, mGlide);
								}
								else
									update_loop_start(track, loop_first, mGlide);
							}
							monomeFrameDirty++;
						}
					}
					break;
				case modTime:
					if(z) {
						kria_set_tmul(track, mGlide, x + 1);
						monomeFrameDirty++;
					}
					break;
				case modProb:
					if(z && y > 1 && y < 6) {
						k.p[k.pattern].t[track].p[mGlide][x] = 5 - y;
						monomeFrameDirty++;
					}
					break;
				default: break;
				}
				break;
			case mScale:
				if(z) {
					if ( y < 4 && x <= 7 ) {
						if (x == 0){
						        k.p[k.pattern].t[y].tt_clocked = !k.p[k.pattern].t[y].tt_clocked;
						}
						if (x == 1) {
							k.p[k.pattern].t[y].trigger_clocked = !k.p[k.pattern].t[y].trigger_clocked;
						}
						if (x >= 3 && x <= 7) {
							k.p[k.pattern].t[y].direction = x - 3;
						}
					}
					else if(x < 8) {
						if(y > 4) {
							k.p[k.pattern].scale = (y - 5) * 8 + x;
							for (uint8_t i = 0; i < 8; i++) {
								scale_adj[i] = 0;
							}
						}
					}
					else {
						uint8_t i;
						for (i = 0; i < key_count; i++) {
							if (held_keys[i] == 16*y + scale_data[k.p[k.pattern].scale][6 - y] + 8) {
								scale_adj[6 - y] = x - 8 - scale_data[k.p[k.pattern].scale][6 - y];
								break;
							}
						}
						if (i == key_count) {
							scale_data[k.p[k.pattern].scale][6-y] = x-8;
							scale_adj[6 - y] = 0;
						}
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
		if (monome_size_y() == 16) {
			monome_set_quadrant_flag(2);
			monome_set_quadrant_flag(3);
		}
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
		if (!view_tuning) {
			grid_refresh = &refresh_kria;
			view_config = false;
		}
		break;
	case 3:
		if (view_tuning) {
			view_tuning = false;
			view_config = false;
			view_clock = false;
			resume_kria();
			break;
		}
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
	refresh_kria_view(&k_views[0]);
	if (monome_size_y() == 16) {
		refresh_kria_view(&k_views[1]);
	}
}

void refresh_kria_view(kria_view_t* view)
{
	u8* monomeLedBuffer = view->buffer;
	const u8 track = view->track;
	const bool k_mode_is_alt = view->mode_is_alt;
	const kria_modes_t k_mode = view->mode;
	//const kria_mod_modes_t k_mod_mode = view->mod_mode;

	memset(monomeLedBuffer, 0, 128);

	// bottom strip
	memset(monomeLedBuffer + R7 + 5, L0, 4);
	monomeLedBuffer[R7 + 10] = L0;
	monomeLedBuffer[R7 + 11] = L0;
	monomeLedBuffer[R7 + 12] = L0;
	monomeLedBuffer[R7 + 14] = L0;
	monomeLedBuffer[R7 + 15] = L0;

	for ( uint8_t i=0; i<4; i++ )
	{
		if ( kria_mutes[i] )
			monomeLedBuffer[R7+i] = (track == i) ? L1 : 2;
		else
			monomeLedBuffer[R7+i] = (track == i) ? L2 : L0;

		// when a blink is happening, the LED is 2 brighter (but not when muted)
		if ( !kria_mutes[i] )
			monomeLedBuffer[R7+i] += kria_blinks[i]*2;
	}

	int activeModeIndex = 0;
	switch(k_mode) {
	case mTr:
	case mRpt:
		activeModeIndex = R7+5; break;
	case mNote:
	case mAltNote:
		activeModeIndex = R7+6; break;
	case mOct:
	case mGlide:
		activeModeIndex = R7+7; break;
	case mDur:
		activeModeIndex = R7+8; break;
	case mScale:
		activeModeIndex = R7+14; break;
	case mPattern:
		activeModeIndex = R7+15; break;
	default:
		activeModeIndex = R7+0; break;
	}

	monomeLedBuffer[activeModeIndex] = (k_mode_is_alt && kriaAltModeBlink) ? L1 : L2;


	if (refresh_kria_mod(view)) return;

	// modes
	switch(k_mode) {
	case mTr:
		refresh_kria_tr(view);
		break;
	case mNote:
	case mAltNote:
		refresh_kria_note(view);
		break;
	case mOct:
		refresh_kria_oct(view);
		break;
	case mDur:
		refresh_kria_dur(view);
		break;
	case mRpt:
		refresh_kria_rpt(view);
		break;
	case mGlide:
		refresh_kria_glide(view);
		break;
	case mScale:
		refresh_kria_scale(view);
		break;
	case mPattern:
		refresh_kria_pattern(view);
	default: break;
	}
}

bool refresh_kria_mod(kria_view_t* view)
{
	u8* monomeLedBuffer = view->buffer;
	const u8 track = view->track;
	//const bool k_mode_is_alt = view->mode_is_alt;
	const kria_modes_t k_mode = view->mode;
	const kria_mod_modes_t k_mod_mode = view->mod_mode;

	switch (k_mod_mode) {
	case modLoop:
		monomeLedBuffer[R7 + 10] = L1;
		return false;
	case modTime:
		monomeLedBuffer[R7 + 11] = L1;
		memset(monomeLedBuffer + R1, 3, 16);
		monomeLedBuffer[R1 + k.p[k.pattern].t[track].tmul[k_mode] - 1] = L1;
		return true;
	case modProb:
		monomeLedBuffer[R7 + 12] = L1;
		memset(monomeLedBuffer + R5, 3, 16);
		for(uint8_t i=0;i<16;i++) {
			if(k.p[k.pattern].t[track].p[k_mode][i]) {
				monomeLedBuffer[(5 - k.p[k.pattern].t[track].p[k_mode][i]) * 16 + i] = i == pos[track][k_mode] ? 10 : 6;
			}
		}
		return true;
	default:
		return false;
	}
}

void refresh_kria_tr(kria_view_t* view)
{
	u8* monomeLedBuffer = view->buffer;
	//const u8 track = view->track;
	//const bool k_mode_is_alt = view->mode_is_alt;
	//const kria_modes_t k_mode = view->mode;
	const kria_mod_modes_t k_mod_mode = view->mod_mode;

	// steps
	for(uint8_t i=0;i<4;i++) {
		for(uint8_t j=0;j<16;j++) {
			if(k.p[k.pattern].t[i].tr[j])
				monomeLedBuffer[i*16 + j] = 3;
		}
		// playhead
		// if(tr[i2])
		monomeLedBuffer[i*16 + pos[i][mTr]] += 4;
	}

	// loop highlight
	for(uint8_t i=0;i<4;i++) {
		if(k.p[k.pattern].t[i].lswap[mTr]) {
			for(uint8_t j=0;j<k.p[k.pattern].t[i].llen[mTr];j++)
				monomeLedBuffer[i*16 + (j+k.p[k.pattern].t[i].lstart[mTr])%16] += 2 + (k_mod_mode == modLoop);
		}
		else {
			for(uint8_t j=k.p[k.pattern].t[i].lstart[mTr];j<=k.p[k.pattern].t[i].lend[mTr];j++)
				monomeLedBuffer[i*16 + j] += 2 + (k_mod_mode == modLoop);
		}
	}
}

void refresh_kria_note(kria_view_t* view)
{
	u8* monomeLedBuffer = view->buffer;
	const u8 track = view->track;
	//const bool k_mode_is_alt = view->mode_is_alt;
	const kria_modes_t k_mode = view->mode;
	const kria_mod_modes_t k_mod_mode = view->mod_mode;

	const kria_modes_t noteMode = k_mode;
	
	u8 (*notesArray)[16] = k_mode == mAltNote ? &k.p[k.pattern].t[track].alt_note : &k.p[k.pattern].t[track].note;

	if(!(k_mode == mAltNote) && note_sync) {
		for(uint8_t i=0;i<16;i++)
			monomeLedBuffer[i + (6 - (*notesArray)[i] ) * 16] =
				k.p[k.pattern].t[track].tr[i] * 3;
	}
	else {
		for(uint8_t i=0;i<16;i++)
			monomeLedBuffer[i + (6 - (*notesArray)[i] ) * 16] = 3;
	}

	monomeLedBuffer[pos[track][noteMode] + (6-(*notesArray)[pos[track][noteMode]])*16] += 4;

	if(k.p[k.pattern].t[track].lswap[noteMode]) {
		for(uint8_t i=0;i<k.p[k.pattern].t[track].llen[noteMode];i++) {
			uint8_t x = (k.p[k.pattern].t[track].lstart[noteMode] + i) % 16;
			uint8_t y = 6 - (*notesArray)[x];
			monomeLedBuffer[16*y + x] += 3 + (k_mod_mode == modLoop)*2;
		}
	}
	else {
		for(uint8_t i=k.p[k.pattern].t[track].lstart[noteMode];i<=k.p[k.pattern].t[track].lend[noteMode];i++)
			monomeLedBuffer[i+(6-(*notesArray)[i])*16] += 3 + (k_mod_mode == modLoop)*2;
	}
}

void refresh_kria_oct(kria_view_t* view)
{
	u8* monomeLedBuffer = view->buffer;
	const u8 track = view->track;
	//const bool k_mode_is_alt = view->mode_is_alt;
	//const kria_modes_t k_mode = view->mode;
	const kria_mod_modes_t k_mod_mode = view->mod_mode;

	memset(monomeLedBuffer, 2, 6);
	monomeLedBuffer[R0+k.p[k.pattern].t[track].octshift] = L1;

	for(uint8_t i=0;i<16;i++) {
		const uint8_t octshift = k.p[k.pattern].t[track].octshift;
		const int8_t octsum = sum_clip(k.p[k.pattern].t[track].oct[i], (int)octshift, 5);

		for(uint8_t j=0;j<=5;j++) {
			if (octsum >= octshift) {
				if (j < octshift || j > octsum) continue;
			}
			else {
				if (j < octsum || j > octshift) continue;
			}
			monomeLedBuffer[R6-16*j+i] = grid_varibrightness < 16 ? L0 : 3;

			if(k.p[k.pattern].t[track].lswap[mOct]) {
				if((i < k.p[k.pattern].t[track].lstart[mOct]) && (i > k.p[k.pattern].t[track].lend[mOct])) {
					monomeLedBuffer[R6-16*j+i] -= 2;
				}
				else if ( k_mod_mode == modLoop ) {
					monomeLedBuffer[R6-16*j+i] += 1;
				}
			}
			else {
				if((i < k.p[k.pattern].t[track].lstart[mOct]) || (i > k.p[k.pattern].t[track].lend[mOct])) {
					monomeLedBuffer[R6-16*j+i] -= 2;
				}
				else if ( k_mod_mode == modLoop ) {
					monomeLedBuffer[R6-16*j+i] += 1;
				}
			}
		}

		if(i == pos[track][mOct]) {
			monomeLedBuffer[R6 - octsum*16 + i] += 4;
		}
	}
}

void refresh_kria_dur(kria_view_t* view)
{
	u8* monomeLedBuffer = view->buffer;
	const u8 track = view->track;
	//const bool k_mode_is_alt = view->mode_is_alt;
	//const kria_modes_t k_mode = view->mode;
	const kria_mod_modes_t k_mod_mode = view->mod_mode;

	monomeLedBuffer[k.p[k.pattern].t[track].dur_mul - 1] = L1;

	for(uint8_t i=0;i<16;i++) {
		for(uint8_t j=0;j<=k.p[k.pattern].t[track].dur[i];j++) {
			monomeLedBuffer[R1+16*j+i] = grid_varibrightness < 16 ? L0 : 3;
			if(k.p[k.pattern].t[track].lswap[mDur]) {
				if((i < k.p[k.pattern].t[track].lstart[mDur]) && (i > k.p[k.pattern].t[track].lend[mDur])) {
					monomeLedBuffer[R1+16*j+i] -= 2;
				}
				else if ( k_mod_mode == modLoop ) {
					monomeLedBuffer[R1+16*j+i] += 1;
				}
			}
			else {
				if((i < k.p[k.pattern].t[track].lstart[mDur]) || (i > k.p[k.pattern].t[track].lend[mDur])) {
					monomeLedBuffer[R1+16*j+i] -= 2;
				}
				else if ( k_mod_mode == modLoop ) {
					monomeLedBuffer[R1+16*j+i] += 1;
				}
			}
		}

		if(i == pos[track][mDur])
			monomeLedBuffer[R1+i+16*k.p[k.pattern].t[track].dur[i]] += 4;
	}
}

void refresh_kria_rpt(kria_view_t* view)
{
	u8* monomeLedBuffer = view->buffer;
	const u8 track = view->track;
	//const bool k_mode_is_alt = view->mode_is_alt;
	//const kria_modes_t k_mode = view->mode;
	const kria_mod_modes_t k_mod_mode = view->mod_mode;

	for ( uint8_t i=0; i<16; i++ ) {
		uint8_t rptBits = k.p[k.pattern].t[track].rptBits[i];
		for ( uint8_t j=0; j<5; j++) {
			uint8_t led = 16*(5-j) + i;
			monomeLedBuffer[led] = 0;
			if (rptBits & (1 << j)) {
				monomeLedBuffer[led] = L0;
			}
			if (j < k.p[k.pattern].t[track].rpt[i]) {
				monomeLedBuffer[led] += grid_varibrightness < 16 ? 4 : 2;

				if ( k.p[k.pattern].t[track].lswap[mRpt] ) {
					if ( (i < k.p[k.pattern].t[track].lstart[mRpt]) && (i > k.p[k.pattern].t[track].lend[mRpt]) ) {
						monomeLedBuffer[led] -= 2;
					}
					else if ( k_mod_mode == modLoop && j < k.p[k.pattern].t[track].rpt[i] ) {
						monomeLedBuffer[led] += 1;
					}
				}
				else {
					if ( (i < k.p[k.pattern].t[track].lstart[mRpt]) || (i > k.p[k.pattern].t[track].lend[mRpt]) ) {
						monomeLedBuffer[led] -= 2;
					}
					else if ( k_mod_mode == modLoop && j < k.p[k.pattern].t[track].rpt[i] ) {
						monomeLedBuffer[led] += 1;
					}
				}
			}
		}
		if ( i == pos[track][mRpt]) {
			uint8_t y = max(1, activeRpt[track] - repeats[track]);
			monomeLedBuffer[R6 - y*16 + i] += (rptBits & (1 << y)) ? 4 : 2;
		}
		monomeLedBuffer[i] = 2;
		monomeLedBuffer[R6+i] = 2;
	}
}

void refresh_kria_glide(kria_view_t* view)
{
	u8* monomeLedBuffer = view->buffer;
	const u8 track = view->track;
	//const bool k_mode_is_alt = view->mode_is_alt;
	//const kria_modes_t k_mode = view->mode;
	const kria_mod_modes_t k_mod_mode = view->mod_mode;

	for(uint8_t i=0;i<16;i++) {
		for(uint8_t j=0;j<=k.p[k.pattern].t[track].glide[i];j++){
			monomeLedBuffer[R6-16*j+i] = L1 - (k.p[k.pattern].t[track].glide[i]-j);
			if(k.p[k.pattern].t[track].lswap[mGlide]) {
				if((i < k.p[k.pattern].t[track].lstart[mGlide]) && (i > k.p[k.pattern].t[track].lend[mGlide])) {
					monomeLedBuffer[R6-16*j+i] -= 2;

				}
				else if ( k_mod_mode == modLoop ) {
					monomeLedBuffer[R6-16*j+i] += 1;
				}
			}
			else
			{
				if((i < k.p[k.pattern].t[track].lstart[mGlide]) || (i > k.p[k.pattern].t[track].lend[mGlide])) {
					monomeLedBuffer[R6-16*j+i] -= 2;
				}
				else if ( k_mod_mode == modLoop ) {
					monomeLedBuffer[R6-16*j+i] += 1;
				}
			}
			if(i == pos[track][mGlide]) {
				monomeLedBuffer[R6 - k.p[k.pattern].t[track].glide[i]*16 + i] += 4;
			}
		}
	}
}

void refresh_kria_scale(kria_view_t* view)
{
	u8* monomeLedBuffer = view->buffer;
	//const u8 track = view->track;
	//const bool k_mode_is_alt = view->mode_is_alt;
	//const kria_modes_t k_mode = view->mode;
	//const kria_mod_modes_t k_mod_mode = view->mod_mode;

	for ( uint8_t y=0; y<4; y++ ) {
		// highlight TT clock enables and trigger steps
		monomeLedBuffer[0+16*y] = k.p[k.pattern].t[y].tt_clocked ? L1 : L0;
		monomeLedBuffer[1+16*y] = k.p[k.pattern].t[y].trigger_clocked ? L1 : L0;

		// show selected direction
		for ( uint8_t x=3; x<=7; x++ ) {
			monomeLedBuffer[x+16*y] = (k.p[k.pattern].t[y].direction == (x - 3)) ? 4 : 2;
		}
	}

	// vertical bar dividing the left and right half
	for(uint8_t i=0;i<7;i++)
		monomeLedBuffer[8+16*i] = L0;
	// the two rows of scale selecting buttons
	for(uint8_t i=0;i<8;i++) {
		monomeLedBuffer[R5 + i] = 2;
		monomeLedBuffer[R6 + i] = 2;
	}
	// highlight the selected scale
	monomeLedBuffer[R5 + (k.p[k.pattern].scale >> 3) * 16 + (k.p[k.pattern].scale & 0x7)] = L1;

	// the intervals of the selected scale
	for(uint8_t i=0;i<7;i++) {
		uint8_t scale_pos = scale_data[k.p[k.pattern].scale][i] + 8 + (6-i)*16;
		monomeLedBuffer[(int)scale_pos + scale_adj[i]] = L0;
		monomeLedBuffer[scale_pos] = L1;
	}

	// if an active step of a track is playing a note, it brightness is incremented by one
	for(uint8_t i=0;i<4;i++) {
		if(k.p[k.pattern].t[i].tr[pos[i][mTr]])
			monomeLedBuffer[scale_data[k.p[k.pattern].scale][note[i]] + 8 + scale_adj[note[i]] + (6-note[i])*16]++;
	}
}

void refresh_kria_pattern(kria_view_t* view)
{
	u8* monomeLedBuffer = view->buffer;
	//const u8 track = view->track;
	//const bool k_mode_is_alt = view->mode_is_alt;
	//const kria_modes_t k_mode = view->mode;
	const kria_mod_modes_t k_mod_mode = view->mod_mode;

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
}

void refresh_kria_config(void)
{
	// clear grid
	memset(monomeLedBuffer, 0, MONOME_MAX_LED_BYTES);

	memset(monomeLedBuffer,4, 3);
	monomeLedBuffer[R0 + (grid_varibrightness == 1 ? 0 :
			      grid_varibrightness == 4 ? 1 :
			      2)] = 12;

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

	monomeLedBuffer[R7 + 14] = L0;
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

u16 mp_clock_count;	// how many note triggers per clock

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

	reset_outputs();

	monomeFrameDirty++;
}

void clock_mp(uint8_t phase) {
	static u8 i;

	mp_clock_count = 0;

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
		if(mp_clock_count < 1) {
			mp_clock_count++;
			note_now[0] = n;
			set_cv_note(0, (int)cur_scale[7-n] + scale_adj[7-n]);
			set_tr(TR1);
		}
		break;
	case MP_2V:
		if(mp_clock_count < 2) {
			mp_clock_count++;
			w = get_note_slot(2);
			note_now[w] = n;
			set_cv_note(w, (int)cur_scale[7-n] + scale_adj[7-n]);
			set_tr(TR1 + w);
		}
		break;
	case MP_4V:
		if(mp_clock_count < 4) {
			mp_clock_count++;
			w = get_note_slot(4);
			note_now[w] = n;
			set_cv_note(w, (int)cur_scale[7-n] + scale_adj[7-n]);
			set_tr(TR1 + w);
		}
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
			ii_grid(d, l);
			ii_ansible(d, l);
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
				if(x == 0 && !follower_select) {
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

						preset_mode_exit();
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
		preset_mode_handle_key(x, y, z, m.glyph);
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
		if (monome_size_y() == 16) {
			monome_set_quadrant_flag(2);
			monome_set_quadrant_flag(3);
		}
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
	memset(monomeLedBuffer, 0, MONOME_MAX_LED_BYTES);

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

	if (ansible_mode == mGridKria) {
		uint8_t i = note_div_sync * 4 + 3;
		monomeLedBuffer[R4 + 0] = i;
		monomeLedBuffer[R5 + 0] = i;
		monomeLedBuffer[R6 + 0] = i;
		monomeLedBuffer[R7 + 0] = i;
		monomeLedBuffer[R4 + 1] = i;
		monomeLedBuffer[R4 + 2] = i;
		monomeLedBuffer[R4 + 3] = i;
		monomeLedBuffer[R5 + 3] = i;
		monomeLedBuffer[R6 + 3] = i;
		monomeLedBuffer[R7 + 3] = i;
		monomeLedBuffer[R7 + 2] = i;
		monomeLedBuffer[R7 + 1] = i;

		i = kria_sync_mode & krSyncTimeDiv ? 7 : 3;
		monomeLedBuffer[R6 + 7] = i;
		monomeLedBuffer[R6 + 8] = i;
		monomeLedBuffer[R7 + 7] = i;
		monomeLedBuffer[R7 + 8] = i;

		i = (div_sync == 1) * 4 + 3;
		monomeLedBuffer[R5 + 12] = i;

		i = (div_sync == 2) * 4 + 3;
		monomeLedBuffer[R7 + 12] = i;
		monomeLedBuffer[R7 + 13] = i;
		monomeLedBuffer[R7 + 14] = i;
		monomeLedBuffer[R7 + 15] = i;
	}
}

void refresh_mp_config(void) {
	u8 i1;//, i2, i3;
	u8 c;

	// clear grid
	memset(monomeLedBuffer, 0, MONOME_MAX_LED_BYTES);

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
	memset(monomeLedBuffer, 0, MONOME_MAX_LED_BYTES);

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
    //if (!es_notes[i].active) return;
    es_notes[i].active = 0;
    timer_remove(&auxTimer[i]);
    clr_tr(TR1 + i);
}

static void es_note_off(s8 x, s8 y) {
    for (u8 i = 0; i < 4; i++)
        if (es_notes[i].x == x && es_notes[i].y == y) {
            es_note_off_i(i);
            //break;
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
    set_cv_note(note, note_index);
    dac_update_now();
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
        dac_set_value_noslew(i, tuning_table[i][note_index]);
        dac_update_now();
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
    if (x == 15 && y == 0) // rest
        e.p[e.p_select].e[l].on = on ? 3 : 2;
    else
        e.p[e.p_select].e[l].on = on ? 1 : 0;
    e.p[e.p_select].length++;
}

static void es_play_pattern_note(void) {
    u16 i = e.p[e.p_select].dir ? e.p[e.p_select].length - 1 : 0;
    u8 first_note_x = e.p[e.p_select].e[i].index & 15;
    u8 first_note_y = e.p[e.p_select].e[i].index >> 4;
    s16 x = (e.p[e.p_select].e[es_pos].index & 15) + e.p[e.p_select].root_x - first_note_x;
    s16 y = (e.p[e.p_select].e[es_pos].index >> 4) + e.p[e.p_select].root_y - first_note_y;

    if (e.p[e.p_select].e[es_pos].on == 1)
        es_note_on(x, y, 1,
            e.p[e.p_select].edge == ES_EDGE_FIXED ? e.p[e.p_select].edge_time : 0,
            e.p[e.p_select].voices);
    else if (e.p[e.p_select].e[es_pos].on == 0 && e.p[e.p_select].edge == ES_EDGE_PATTERN)
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

static u8 es_next_note(void) {
    if (++es_pos >= e.p[e.p_select].length) {
        es_pos = 0;
        es_p_start = get_ticks();
        if (!e.p[e.p_select].loop) {
            es_kill_pattern_notes();
            timer_remove(&es_play_pos_timer);
            es_mode = es_stopped;
            return 1;
        }
    }
    return 0;
}

static void es_play_callback(void* o) {
    timer_remove(&es_play_timer);
    if (ansible_mode != mGridES) {
        es_mode = es_stopped;
        return;
    }

    if (clock_external) return;

    if (es_next_note()) return;

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
    es_mode = es_playing;

    u32 interval;

    if (pos) {
        u32 start = (es_p_total * pos) >> 4;
        u32 tick = 0;
        for (es_pos = 0; es_pos < e.p[e.p_select].length; es_pos++) {
            interval = e.p[e.p_select].e[es_pos].interval;
            if (e.p[e.p_select].linearize) {
                if (interval < ES_CHORD_THRESHOLD)
                    interval = 1;
                else
                    interval = e.p[e.p_select].e[e.p[e.p_select].interval_ind].interval;
            }
            if (tick + interval > start) break;
            tick += interval;
        }
        if (es_pos >= e.p[e.p_select].length) {
            es_pos = e.p[e.p_select].length - 1;
            interval = 1;
        } else {
            interval = tick + interval - start;
            if (!interval) interval = 1;
        }

        if (clock_external) return;

        es_p_start = get_ticks() - start;
        timer_add(&es_play_pos_timer, 25, &es_play_pos_callback, NULL);
        timer_add(&es_play_timer, interval, &es_play_callback, NULL );
        return;
    }

    es_pos = 0;
    es_p_start = get_ticks();
    es_update_total_time();

    if (clock_external) return;

    interval = e.p[e.p_select].e[0].interval;
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
    e.p[e.p_select].dir = 0;
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
        if (held_keys[i] < 16) {
            found = held_keys[i];
            break;
        }
    }
    return found;
}
*/

static u8 rest_pressed(void) {
    for (u8 i = 0; i < key_count; i++) {
        if (held_keys[i] == 15) return 1;
    }
    return 0;
}

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
    for (u16 i = 0; i < e.p[e.p_select].length; i++) {
        if (e.p[e.p_select].e[i].interval > (ES_CHORD_THRESHOLD << 1))
            e.p[e.p_select].e[i].interval >>= 1;
        else if (e.p[e.p_select].e[i].interval > ES_CHORD_THRESHOLD)
            e.p[e.p_select].e[i].interval = ES_CHORD_THRESHOLD + 1;
    }
    es_update_total_time();
}

static void es_half_speed(void) {
    u16 interval;
    for (u16 i = 0; i < e.p[e.p_select].length; i++)
        if (e.p[e.p_select].e[i].interval > ES_CHORD_THRESHOLD) {
            interval = e.p[e.p_select].e[i].interval << 1;
            if (interval > e.p[e.p_select].e[i].interval) e.p[e.p_select].e[i].interval = interval;
        }
    es_update_total_time();
}

static void es_reverse(void) {
    u16 l = e.p[e.p_select].length;
    if (!l) return;

    es_event_t te[ES_EVENTS_PER_PATTERN];

    for (u16 i = 0; i < l; i++) {
        te[i] = e.p[e.p_select].e[i];
        if (te[i].on == 3)
            te[i].on = 2;
        else if (te[i].on == 2)
            te[i].on = 3;
        else if (te[i].on == 1)
            te[i].on = 0;
        else if (te[i].on == 0)
            te[i].on = 1;
    }

    for (u16 i = 0; i < l; i++)
        e.p[e.p_select].e[i] = te[l - i - 1];
    for (u16 i = 0; i < l - 1; i++)
        e.p[e.p_select].e[i].interval = te[l - i - 2].interval;

    e.p[e.p_select].e[l - 1].interval = te[l - 1].interval;

    if (e.p[e.p_select].dir) {
        for (s16 i = e.p[e.p_select].length - 1; i >= 0; i--)
            if ((e.p[e.p_select].e[i].on == 3 || e.p[e.p_select].e[i].on == 1)
                && e.p[e.p_select].e[i].interval > ES_CHORD_THRESHOLD) {
                e.p[e.p_select].interval_ind = i;
                break;
            }
    } else {
        for (u16 i = 0; i < e.p[e.p_select].length; i++)
            if ((e.p[e.p_select].e[i].on == 3 || e.p[e.p_select].e[i].on == 1)
                && e.p[e.p_select].e[i].interval > ES_CHORD_THRESHOLD) {
                e.p[e.p_select].interval_ind = i;
                break;
            }
    }
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
        for (u8 j = 0; j < 128; j++)
            e.keymap[i] = 0;
        e.scale = 16;
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

	memcpy(scale_data, f.scale, sizeof(scale_data));
	if (e.scale < 16) calc_scale(e.scale);
}

void resume_es(void) {
    es_mode = es_stopped;
    es_view = es_main;

    preset_mode = false;
    grid_refresh = &refresh_es;

    // re-check clock jack
    clock_external = !gpio_get_pin_value(B10);
    clock = &clock_null;

    reset_outputs();
    for (u8 i = 0; i < 4; i++) {
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
		if (monome_size_y() == 16) {
			monome_set_quadrant_flag(2);
			monome_set_quadrant_flag(3);
		}
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
    u8 i;
    switch(data) {
    case 0: // input 1 low
        break;
    case 1: // input 1 high
        if (es_mode != es_playing) break;
        i = e.p[e.p_select].length;
        while (i > 0) {
            i--;
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
        preset_mode_handle_key(x, y, z, e.glyph);
        if (z == 0) {
	    if (x == 0 && !follower_select) {
	        if (y != preset) {
		    preset = y;
		    for (u8 i = 0; i < GRID_PRESETS; i++)
		        e.glyph[i] = f.es_state.e[preset].glyph[i];
		} else {
		    // flash read
		    es_load_preset();
		}
	    }
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
            if (e.p[e.p_select].dir != 1) es_reverse();
            e.p[e.p_select].dir = 1;
            if (es_mode == es_playing) es_kill_pattern_notes();
        } else if (x > 8 && x < 11 && y > 1 && y < 5) {
            if (e.p[e.p_select].dir != 0) es_reverse();
            e.p[e.p_select].dir = 0;
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
            // if (e.voices && voice) es_note_off_i(y - 2);
            e.voices ^= voice;
        } else if (x == 2 && y > 1 && y < 6) {
            // if (e.p[e.p_select].voices && voice) es_note_off_i(y - 2);
            e.p[e.p_select].voices ^= voice;
        } else if (y == 7 && x == 2 && e.octave) {
            e.octave--;
        } else if (y == 7 && x == 3 && e.octave < 5) {
            e.octave++;
        }
        es_kill_all_notes();
        monomeFrameDirty++;
        return;
    }

    if (es_view == es_patterns_held || es_view == es_patterns) {
        if (!z) return;

        if (x > 7 && y > 2 && y < 5) {
            // scale selection
            u8 scale = x - 8 + ((y - 3) << 3);
            if (scale == e.scale)
                e.scale = 16;
            else {
                e.scale = scale;
                calc_scale(e.scale);
            }
            monomeFrameDirty++;
        }

        if (x < 2 || x > 5 || y < 2 || y > 5) return;
        e.p_select = (x - 2) + ((y - 2) << 2);
        if (es_view == es_patterns) es_start_playback(0);
        monomeFrameDirty++;
        return;
    }

    if (y == 0 && es_mode == es_playing) {
        if (!z) return;
        es_start_playback(e.p[e.p_select].dir ? 15 - x : x);
        /*
        s8 start = top_row_pressed();
        if (start == -1 || start == x) {
            es_start_playback(x);
        } else {
            e.p[e.p_select].start = min(x, start);
            e.p[e.p_select].end = max(x, start);
        }
        */
        monomeFrameDirty++;
        return;
    }

    if (x == 0) return;

    if (es_mode == es_armed) es_start_recording(); // will change es_mode to es_recording
    if (es_mode == es_recording) es_record_pattern_note(x, y, z);

    if (e.arp && es_mode != es_recording) {
        if (!z) return;
        e.p[e.p_select].root_x = x;
        e.p[e.p_select].root_y = y;
        es_start_playback(0);
        // es_update_pitches();
    } else if (es_mode == es_stopped && rest_pressed() && z) {
        // keymap
        e.keymap[(y << 4) + x] = (e.keymap[(y << 4) + x] + 1) % 3;
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
            if (z) {
                if (x != 15 || y != 0)
                    es_note_on(x, y, 0, 0, es_mode == es_recording ? e.p[e.p_select].voices : e.voices);
            } else es_note_off(x, y);
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
            if (i <= pos) monomeLedBuffer[i] = 8;
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

    s16 index, x, y;
    if (es_view == es_main) {
        if (e.scale == 16) {
            for (u8 i = 0; i < 128; i++)
                if (e.keymap[i]) monomeLedBuffer[i] = e.keymap[i] << 1;
        } else {
            u8 in_scale;
            for (x = 1; x < 16; x++)
                for (y = es_mode == es_playing ? 1 : 0; y < 8; y++) {
                    index = x + (7 - y) * 5 - 1;
                    in_scale = 0;
                    for (u8 sc = 0; sc < 8; sc++) {
                        for (u8 oct = 0; oct < 5; oct++) {
                            if (index == cur_scale[sc] + oct * 12) {
                                monomeLedBuffer[(y << 4) + x] = sc == 0 ? 4 : 2;
                                in_scale = 1;
                                break;
                            }
                        }
                        if (in_scale) break;
                    }
                }
        }

        if (e.arp)
            monomeLedBuffer[e.p[e.p_select].root_x + (e.p[e.p_select].root_y << 4)] = 7;
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

        // scale
        for (x = 8; x < 16; x++)
            for (y = 3; y < 5; y++)
                monomeLedBuffer[x + (y << 4)] = 4;

        if (e.scale != 16)
            monomeLedBuffer[(e.scale & 7) + 8 + ((3 + (e.scale >> 3)) << 4)] = 15;
    }
}

void ii_es(uint8_t *data, uint8_t l) {
    if (!l) return;

	s16 d = (data[1] << 8) | data[2];
    u8 value;

    switch(data[0]) {
        case ES_PRESET:
            if (d >= 0 && d < GRID_PRESETS) {
                preset = d;
                es_load_preset();
                monomeFrameDirty++;
            }
            break;

        case ES_PATTERN:
            if (d >= 0 && d < 16) {
                e.p_select = d;
                monomeFrameDirty++;
            }
            break;

        case ES_CLOCK:
            value = e.p[e.p_select].length;
            while (value > 0) {
                value--;
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

        case ES_RESET:
            value = d;
            if (value > 15) value = 15;
            es_start_playback(value);
            break;

        case ES_STOP:
            es_stop_playback();
            break;

        case ES_TRANS:
            if (d > 0) {
                for (u8 i = 0; i < d; i++) {
                    if (e.p[e.p_select].root_y == 1 && e.p[e.p_select].root_x == 15) break;
                    e.p[e.p_select].root_x++;
                    if (e.p[e.p_select].root_x == 16)
                    {
                        e.p[e.p_select].root_x = 11;
                        e.p[e.p_select].root_y--;
                    }
                }
            } else {
                for (u8 i = 0; i < abs(d); i++) {
                    if (e.p[e.p_select].root_y == 7 && e.p[e.p_select].root_x == 1) break;
                    e.p[e.p_select].root_x--;
                    if (e.p[e.p_select].root_x == 0)
                    {
                        e.p[e.p_select].root_x = 5;
                        e.p[e.p_select].root_y++;
                    }
                }
            }
            es_start_playback(0);
            monomeFrameDirty++;
            break;

        case ES_MAGIC:
            switch (d) {
                case 1:
                    es_half_speed();
                    break;

                case 2:
                    es_double_speed();
                    break;

                case 3:
                    e.p[e.p_select].linearize = 1;
                    es_update_total_time();
                    break;

                case 4:
                    e.p[e.p_select].linearize = 0;
                    es_update_total_time();
                    break;

                case 5:
                    if (e.p[e.p_select].dir) es_reverse();
                    e.p[e.p_select].dir = 0;
                    if (es_mode == es_playing) es_kill_pattern_notes();
                    break;

                case 6:
                    if (!e.p[e.p_select].dir) es_reverse();
                    e.p[e.p_select].dir = 1;
                    if (es_mode == es_playing) es_kill_pattern_notes();
                    break;
            }
            monomeFrameDirty++;
            break;

        case ES_MODE:
            if (d < 0 || d >= 16) {
                e.p[e.p_select].edge = ES_EDGE_PATTERN;
                es_kill_all_notes();
            } else if (d == 0) {
                e.p[e.p_select].edge = ES_EDGE_DRONE;
            } else {
                u8 fixed = d;
                if (fixed > 15) fixed = 15;
                e.p[e.p_select].edge = ES_EDGE_FIXED;
                e.p[e.p_select].edge_time = (fixed + 1) << 4;
                es_kill_all_notes();
            }
            monomeFrameDirty++;
            break;

        default:
	    ii_grid(data, l);
	    ii_ansible(data, l);
    }
}
