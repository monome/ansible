#pragma once

#define R0 0
#define R1 16
#define R2 32
#define R3 48
#define R4 64
#define R5 80
#define R6 96
#define R7 112

#define GRID_PRESETS 8

#define KRIA_NUM_TRACKS 4
#define KRIA_NUM_PARAMS 7
#define KRIA_NUM_PATTERNS 16

typedef enum {
  krDirForward = 0,
  krDirReverse = 1,
  krDirTriangle = 2,
  krDirDrunk = 3,
  krDirRandom = 4,
} kria_direction;

#define ES_EVENTS_PER_PATTERN 128
#define ES_EDGE_PATTERN 0
#define ES_EDGE_FIXED 1
#define ES_EDGE_DRONE 2

typedef struct {
	u8 tr[16];
	s8 oct[16];
	u8 note[16];
	u8 dur[16];
	u8 rpt[16];
	u8 rptBits[16];
	u8 alt_note[16];
	u8 glide[16];

	u8 p[KRIA_NUM_PARAMS][16];

	// u8 ptr[16];
	// u8 poct[16];
	// u8 pnote[16];
	// u8 pdur[16];

	u8 dur_mul;
	kria_direction direction;
	u8 advancing[KRIA_NUM_PARAMS];
	u8 octshift;

	u8 lstart[KRIA_NUM_PARAMS];
	u8 lend[KRIA_NUM_PARAMS];
	u8 llen[KRIA_NUM_PARAMS];
	u8 lswap[KRIA_NUM_PARAMS];
	u8 tmul[KRIA_NUM_PARAMS];

	bool tt_clocked;
	bool trigger_clocked;
} kria_track;

typedef struct {
	kria_track t[4];
	u8 scale;
} kria_pattern;

typedef struct {
	kria_pattern p[KRIA_NUM_PATTERNS];
	uint8_t pattern;
	uint8_t meta_pat[64];
	uint8_t meta_steps[64];
	uint8_t meta_start;
	uint8_t meta_end;
	uint8_t meta_len;
	uint8_t meta_lswap;
	uint8_t glyph[8];
} kria_data_t;

extern kria_data_t k;

typedef enum {
	krSyncNone    = 0x00,
	krSyncTimeDiv = 0x01,
} kria_sync_mode_t;

typedef struct {
	uint32_t clock_period;
	kria_sync_mode_t sync_mode;
	uint8_t preset;
	bool note_sync;
	uint8_t loop_sync;
	bool note_div_sync;
	uint8_t div_sync;
	uint8_t cue_div;
	uint8_t cue_steps;
	uint8_t meta;
	kria_data_t k[GRID_PRESETS];
} kria_state_t;

typedef enum {
	mTr, mNote, mOct, mDur, mRpt, mAltNote, mGlide, mScale, mPattern
} kria_modes_t;

typedef enum {
	modNone, modLoop, modTime, modProb
} kria_mod_modes_t;

typedef struct {
	u8 track;
	kria_modes_t mode;
	kria_mod_modes_t mod_mode;
	bool mode_is_alt;
	u8* buffer;
	softTimer_t* altBlinkTimer;
} kria_view_t;


typedef struct {
	// s8 position[8];		// current position in cycle
	// u8 tick[8]; 		// position in speed countdown
	// u8 pushed[8];		// manual key reset

	u8 count[8];		// length of cycle
	s8 speed[8];		// speed of cycle
	u8 min[8];
	u8 max[8];
	u8 trigger[8];
	u8 toggle[8];
	u8 rules[8];
	u8 rule_dests[8];
	u8 sync[8]; 		// if true, reset dest rule to count
	u8 rule_dest_targets[8];
	u8 smin[8];
	u8 smax[8];

	u8 scale;
	u8 glyph[8];
} mp_data_t;

extern mp_data_t m;

typedef struct {
	uint8_t preset;
	uint8_t sound;
	uint8_t voice_mode;
	mp_data_t m[GRID_PRESETS];
} mp_state_t;




typedef enum {
	es_stopped,
	es_armed,
	es_recording,
	es_playing
} es_mode_t;

typedef enum {
	es_main,
	es_patterns_held,
	es_patterns
} es_view_t;

typedef struct {
	u8 active;
	s8 x;
	s8 y;
	u32 start;
	u8 from_pattern;
} es_note_t;

typedef struct {
	u8 on;
	u8 index;
	u16 interval;
} es_event_t;

typedef struct {
	es_event_t e[ES_EVENTS_PER_PATTERN];
	u16 interval_ind;
	u16 length;
	u8 loop;
	u8 root_x;
	u8 root_y;
	u8 edge;
	u16 edge_time;
	u8 voices;
	u8 dir;
	u8 linearize;
	u8 start;
	u8 end;
} es_pattern_t;

typedef struct {
	u8 arp;
	u8 p_select;
	u8 voices;
	u8 octave;
	u8 scale;
	u16 keymap[128];
	es_pattern_t p[16];
	u8 glyph[8];
} es_data_t;

extern es_data_t e;

typedef struct {
	u8 preset;
	es_data_t e[GRID_PRESETS];
} es_state_t;


void set_mode_grid(void);

void handler_GridFrontShort(s32 data);
void handler_GridFrontLong(s32 data);
void refresh_preset(void);
void refresh_grid_tuning(void);
void grid_keytimer(void);
void ii_grid(uint8_t* data, uint8_t len);

void default_kria(void);
void init_kria(void);
void resume_kria(void);
void clock_kria(uint8_t phase);
void clock_kria_track( uint8_t trackNum );
void clock_kria_note(kria_track* track, uint8_t trackNum);
void ii_kria(uint8_t *d, uint8_t l);
void handler_KriaGridKey(s32 data);
void handler_KriaRefresh(s32 data);
void handler_KriaKey(s32 data);
void handler_KriaTr(s32 data);
void handler_KriaTrNormal(s32 data);
void refresh_kria(void);
void refresh_kria_view(kria_view_t* view);
bool refresh_kria_mod(kria_view_t* view);
void refresh_kria_tr(kria_view_t* view);
void refresh_kria_note(kria_view_t* view);
void refresh_kria_oct(kria_view_t* view);
void refresh_kria_dur(kria_view_t* view);
void refresh_kria_rpt(kria_view_t* view);
void refresh_kria_glide(kria_view_t* view);
void refresh_kria_scale(kria_view_t* view);
void refresh_kria_pattern(kria_view_t* view);
void refresh_kria_config(void);

void default_mp(void);
void init_mp(void);
void resume_mp(void);
void clock_mp(uint8_t phase);
void ii_mp(uint8_t *d, uint8_t l);
void handler_MPGridKey(s32 data);
void handler_MPRefresh(s32 data);
void handler_MPKey(s32 data);
void handler_MPTr(s32 data);
void handler_MPTrNormal(s32 data);
void refresh_mp(void);
void refresh_mp_config(void);
void refresh_clock(void);

void default_es(void);
void init_es(void);
void resume_es(void);
void handler_ESGridKey(s32 data);
void handler_ESRefresh(s32 data);
void handler_ESKey(s32 data);
void handler_ESTr(s32 data);
void handler_ESTrNormal(s32 data);
void refresh_es(void);
void ii_es(uint8_t *d, uint8_t l);
