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
#define I2C_FOLLOWER_COUNT 4

// WARNING: order must match array order of
// connected_t_options[], ansible_mode_options[]
// in ansible_preset_docdef.c
typedef enum {
	conNONE,
	conARC,
	conGRID,
	conMIDI,
	conFLASH
} connected_t;

typedef enum {
	mArcLevels,
	mArcCycles,
	mGridKria,
	mGridMP,
	mGridES,
	mMidiStandard,
	mMidiArp,
	mTT,
	mUsbDisk,
} ansible_mode_t;
// END WARNING

connected_t connected;

typedef struct {
	bool active;
	uint8_t addr;
	uint8_t tr_cmd;
	uint8_t cv_cmd;
	uint8_t cv_slew_cmd;
	uint8_t init_cmd;
	uint8_t vol_cmd;
} i2c_follower_t;

typedef struct {
	connected_t connected;
	ansible_mode_t arc_mode;
	ansible_mode_t grid_mode;
	ansible_mode_t midi_mode;
	ansible_mode_t none_mode;
	uint8_t i2c_addr;
	uint8_t grid_varibrightness;
	i2c_follower_t followers[I2C_FOLLOWER_COUNT];
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
	uint16_t tuning_table[4][120];
} nvram_data_t;

extern nvram_data_t f;
extern ansible_mode_t ansible_mode;
extern i2c_follower_t followers[I2C_FOLLOWER_COUNT];

extern softTimer_t auxTimer[4];
extern uint16_t tuning_table[4][120];


void (*clock)(u8 phase);
void init_tuning(void);
void default_tuning(void);
void fit_tuning(void);

extern void handler_None(s32 data);
extern void clock_null(u8 phase);
extern void ii_null(uint8_t *d, uint8_t l);

void set_mode(ansible_mode_t m);
void update_leds(uint8_t m);
void set_tr(uint8_t n);
void clr_tr(uint8_t n);
void set_cv(uint8_t n, uint16_t cv);
void set_cv_slew(uint8_t n, uint16_t s);
void toggle_follower(uint8_t n);
uint8_t get_tr(uint8_t n);
void clock_set(uint32_t n);
void clock_set_tr(uint32_t n, uint8_t phase);

void ii_ansible(uint8_t* d, uint8_t len);
void load_flash_state(void);
void flash_unfresh(void);
