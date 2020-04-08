#pragma once

#define I2C_FOLLOWER_COUNT 3

struct i2c_follower_t;

typedef void(*ii_u8_cb)(struct i2c_follower_t* follower, uint8_t track, uint8_t param);
typedef void(*ii_s8_cb)(struct i2c_follower_t* follower, uint8_t track, int8_t param);
typedef void(*ii_u16_cb)(struct i2c_follower_t* follower, uint8_t track, uint16_t param);

typedef struct i2c_follower_t {
  uint8_t addr;
	bool active;
	uint8_t track_en;
	int8_t oct;

	ii_u8_cb init;
	ii_u8_cb mode;
	ii_u8_cb tr;
        ii_u8_cb mute;

	ii_u16_cb cv;
	ii_u16_cb slew;
	ii_s8_cb octave;

	uint8_t mode_ct;
	uint8_t active_mode;
} i2c_follower_t;

extern i2c_follower_t followers[I2C_FOLLOWER_COUNT];

void follower_change_mode(i2c_follower_t* follower, uint8_t param);
void follower_change_octave(i2c_follower_t* follower, int8_t param);
