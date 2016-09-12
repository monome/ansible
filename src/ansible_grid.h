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

typedef struct {
	uint32_t clock_period;
	uint8_t pattern;
} kria_state_t;

typedef struct {
	uint8_t gate[16];
} kria_data_t;


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

	u8 glyph[8];

	u8 scale;

} mp_data_t;

typedef struct {
	uint32_t clock_period;
	uint8_t preset;
	uint8_t sound;
	uint8_t voice_mode;

	mp_data_t m[GRID_PRESETS];
} mp_state_t;


void set_mode_grid(void);

void handler_GridFrontShort(s32 data);
void handler_GridFrontLong(s32 data);
void refresh_preset(void);
void grid_keytimer(void);

void default_kria(void);
void init_kria(void);
void resume_kria(void);
void clock_kria(uint8_t phase);
void ii_kria(uint8_t *d, uint8_t l);
void handler_KriaGridKey(s32 data);
void handler_KriaRefresh(s32 data);
void handler_KriaKey(s32 data);
void handler_KriaTr(s32 data);
void handler_KriaTrNormal(s32 data);
void refresh_kria(void);

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