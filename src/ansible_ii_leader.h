#pragma once

#define I2C_FOLLOWER_COUNT 2
#define MAX_FOLLOWER_PARAMS 2

struct i2c_follower_t;

typedef void(*ii_init_cb)(struct i2c_follower_t* follower, uint8_t track, uint8_t on);
typedef void(*ii_tr_cb)(struct i2c_follower_t* follower, uint8_t track, uint8_t state);
typedef void(*ii_cv_cb)(struct i2c_follower_t* follower, uint8_t track, uint16_t dac_value);
typedef void(*ii_slew_cb)(struct i2c_follower_t* follower, uint8_t track, uint16_t slew);

typedef struct i2c_follower_t {
	bool active;
	uint8_t track_en;
	int8_t oct;

	ii_init_cb init;
	ii_tr_cb tr;
	ii_cv_cb cv;
	ii_slew_cb slew;

	uint8_t param_ct;
	uint8_t active_param[MAX_FOLLOWER_PARAMS];
	uint8_t mode_ct;
	uint8_t active_mode;
} i2c_follower_t;

extern i2c_follower_t followers[I2C_FOLLOWER_COUNT];
