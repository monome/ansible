#pragma once

#define I2C_FOLLOWER_COUNT 3
#define MAX_FOLLOWER_PARAMS 2

struct i2c_follower_t;

typedef void(*ii_u8_cb)(struct i2c_follower_t* follower, uint8_t track, uint8_t param);
typedef void(*ii_u16_cb)(struct i2c_follower_t* follower, uint8_t track, uint16_t param);

typedef struct i2c_follower_t {
  uint8_t addr;
	bool active;
	uint8_t track_en;
	int8_t oct;

	ii_u8_cb init;
	ii_u8_cb mode;
	ii_u16_cb param;
	ii_u8_cb tr;
	ii_u16_cb cv;
	ii_u16_cb slew;

	uint8_t param_ct;
	uint8_t active_param[MAX_FOLLOWER_PARAMS];
	uint8_t mode_ct;
	uint8_t active_mode;
} i2c_follower_t;

extern i2c_follower_t followers[I2C_FOLLOWER_COUNT];
