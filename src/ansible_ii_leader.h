#pragma once

#define I2C_FOLLOWER_COUNT 2

typedef void(*ii_init_cb)(uint8_t track, uint8_t on);
typedef void(*ii_tr_cb)(uint8_t track, uint8_t state);
typedef void(*ii_cv_cb)(uint8_t track, uint16_t dac_value);
typedef void(*ii_slew_cb)(uint8_t track, uint16_t slew);

typedef struct {
	bool active;
	uint8_t track_en;
	uint8_t oct;

	uint8_t addr;
	uint8_t tr_cmd;
	uint8_t cv_cmd;
	bool cv_extra;
	uint8_t cv_slew_cmd;
	uint8_t init_cmd;
	uint8_t vol_cmd;

	ii_init_cb init;
	ii_tr_cb tr;
	ii_cv_cb cv;
	ii_slew_cb slew;
} i2c_follower_t;

extern i2c_follower_t followers[I2C_FOLLOWER_COUNT];
