#pragma once

#define TR1 B02
#define TR2 B03
#define TR3 B04
#define TR4 B05

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
	mMidiStandard,
	mMidiArp,
	mTT
} ansible_mode_t;


typedef struct {
	connected_t connected;
	ansible_mode_t mode;
	ansible_mode_t arc_mode;
	ansible_mode_t grid_mode;
	ansible_mode_t midi_mode;
	ansible_mode_t none_mode;
	uint8_t i2c_addr;
} ansible_state_t;

typedef struct {
	uint32_t clock_period;
	uint8_t pattern;
} kria_state_t;

typedef struct {
	uint8_t gate[16];
} kria_data_t;


typedef struct {
	uint32_t clock;
} mp_state_t;


// NVRAM data structure located in the flash array.
typedef const struct {
	u8 fresh;
	u8 preset_select;
	ansible_state_t state;
	kria_state_t kria_state;
} nvram_data_t;

extern nvram_data_t f;


void (*clock)(u8 phase);


void set_mode(ansible_mode_t m);
void update_dacs(uint16_t *d);
void update_leds(uint8_t m);
void set_tr(uint8_t n);
void clr_tr(uint8_t n);
void clock_set(uint32_t);