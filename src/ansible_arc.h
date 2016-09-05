#pragma once

#include "main.h"

#define ARC_NUM_PRESETS 8

typedef struct {
	uint16_t pattern[4][16];
	uint8_t note[4][16];
	bool mode[4];
	uint8_t now;
	uint8_t start;
	int8_t len;
	uint8_t dir;
	uint8_t scale[4];
	uint8_t octave[4];
	uint16_t offset[4];
	uint16_t range[4];
	uint16_t slew[4];
} levels_data_t;

typedef struct {
	// uint32_t clock_period;
	uint8_t preset;
	levels_data_t l[ARC_NUM_PRESETS];
} levels_state_t;


typedef struct {
	uint16_t pos[4];
	int16_t speed[4];
	uint8_t mult[4];
	uint8_t mode;
	uint8_t shape;
	uint8_t friction;
	uint16_t force;
} cycles_data_t;

typedef struct {
	// uint32_t clock_period;
	uint8_t preset;
	cycles_data_t c[ARC_NUM_PRESETS];
} cycles_state_t;

void set_mode_arc(void);
void handler_ArcFrontShort(s32 data);
void handler_ArcFrontLong(s32 data);
void arc_keytimer(void);
void refresh_arc_preset(void);
void handler_ArcPresetEnc(s32 data);
void handler_ArcPresetKey(s32 data);

void default_levels(void);
void init_levels(void);
void resume_levels(void);
void clock_levels(uint8_t phase);
void ii_levels(uint8_t *d, uint8_t l);
void refresh_levels(void);
void refresh_levels_change(void);
void refresh_levels_config(void);
void handler_LevelsEnc(s32 data);
void handler_LevelsRefresh(s32 data);
void handler_LevelsKey(s32 data);
void handler_LevelsTr(s32 data);
void handler_LevelsTrNormal(s32 data);

void default_cycles(void);
void init_cycles(void);
void resume_cycles(void);
void clock_cycles(uint8_t phase);
void ii_cycles(uint8_t *d, uint8_t l);
void refresh_cycles(void);
void refresh_cycles_config(void);
void handler_CyclesEnc(s32 data);
void handler_CyclesRefresh(s32 data);
void handler_CyclesKey(s32 data);
void handler_CyclesTr(s32 data);
void handler_CyclesTrNormal(s32 data);