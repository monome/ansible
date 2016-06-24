#pragma once

#include "main.h"


typedef struct {
	uint32_t clock_period;
	uint8_t pattern;
} kria_state_t;

typedef struct {
	uint8_t gate[16];
} kria_data_t;


typedef struct {
	uint32_t clock_period;
} mp_state_t;



void set_mode_grid(void);

void handler_GridFrontShort(s32 data);
void handler_GridFrontLong(s32 data);

void default_kria(void);
void clock_kria(uint8_t phase);
void ii_kria(uint8_t i, int d);
void handler_KriaGridKey(s32 data);
void handler_KriaRefresh(s32 data);
void handler_KriaKey(s32 data);
void handler_KriaTr(s32 data);
void handler_KriaTrNormal(s32 data);

void default_mp(void);
void clock_mp(uint8_t phase);
void ii_mp(uint8_t i, int d);
void handler_MPGridKey(s32 data);
void handler_MPRefresh(s32 data);
void handler_MPKey(s32 data);
void handler_MPTr(s32 data);
void handler_MPTrNormal(s32 data);