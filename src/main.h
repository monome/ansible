#pragma once

#include "timers.h"

#include "ansible_grid.h"
#include "ansible_arc.h"
#include "ansible_midi.h"
#include "ansible_tt.h"

#define TR1 B02
#define TR2 B03
#define TR3 B04
#define TR4 B05

#define KEY_HOLD_TIME 8

typedef enum {
	conNONE,
	conARC,
	conGRID,
	conMIDI,
	conFLASH
} connected_t;

connected_t connected;

typedef enum {
	mArcLevels,
	mArcCycles,
	mGridKria,
	mGridMP,
	mGridES,
	mMidiStandard,
	mMidiArp,
	mTT
} ansible_mode_t;

typedef struct {
	connected_t connected;
	ansible_mode_t arc_mode;
	ansible_mode_t grid_mode;
	ansible_mode_t midi_mode;
	ansible_mode_t none_mode;
	uint8_t i2c_addr;
} ansible_state_t;


// NVRAM data structure located in the flash array.
typedef const struct {
	uint8_t fresh;
	ansible_state_t state;
	kria_state_t kria_state;
	mp_state_t mp_state;
	es_state_t es_state;
	levels_state_t levels_state;
	cycles_state_t cycles_state;
	midi_standard_state_t midi_standard_state;
	midi_arp_state_t midi_arp_state;
	tt_state_t tt_state;
	uint8_t scale[16][8];
} nvram_data_t;

extern nvram_data_t f;
extern ansible_mode_t ansible_mode;

extern softTimer_t auxTimer[4];


void (*clock)(u8 phase);

extern void handler_None(s32 data);
extern void clock_null(u8 phase);
extern void ii_null(uint8_t *d, uint8_t l);

void set_mode(ansible_mode_t m);
void update_leds(uint8_t m);
void set_tr(uint8_t n);
void clr_tr(uint8_t n);
uint8_t get_tr(uint8_t n);
void clock_set(uint32_t n);
void clock_set_tr(uint32_t n, uint8_t phase);
