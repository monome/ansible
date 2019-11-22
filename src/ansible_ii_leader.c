#include "dac.h"
#include "i2c.h"
#include "ii.h"
#include "ansible_ii_leader.h"

static void ii_init_jf(uint8_t track, uint8_t on) {
	uint8_t d[] = { JF_MODE, on };
	i2c_master_tx(JF_ADDR, d, 2);
}

static void ii_tr_jf(uint8_t track, uint8_t state) {
	uint8_t d[6] = { 0 };
	uint8_t l = 0;
	uint16_t dac_value = dac_get_value(track);
	if (state) {
		d[0] = JF_VOX;
	        d[1] = track + 1;
		d[2] = dac_value >> 8;
		d[3] = dac_value & 0xFF;
		d[4] = 16384 >> 8;
		d[5] = 16384 & 0xFF;
		l = 6;
	}
	else {
		d[0] = JF_TR;
	        d[1] = track + 1;
		d[2] = 0;
		l = 3;
	}
	i2c_master_tx(JF_ADDR, d, l);
}

static void ii_init_txo(uint8_t track, uint8_t on) {
	uint8_t d[4] = {
		0x60, // TO_ENV_ACT
		track,
		on,
	};
	i2c_master_tx(TELEXO_0, d, 3);

	if (on) {
		d[0] = 0x10; // TO_CV
		d[2] = 8192 >> 8;
		d[3] = 8192 & 0xFF;
		i2c_master_tx(TELEXO_0, d, 4);
	}
}

static void ii_tr_txo(uint8_t track, uint8_t state) {
	uint8_t d[3] = {
		0x6D, // TO_ENV
		track,
		state,
	};
	i2c_master_tx(TELEXO_0, d, 3);
}

static void ii_cv_txo(uint8_t track, uint16_t dac_value) {
	uint8_t d[4] = {
		0x40, // TO_OSC
		track,
		dac_value >> 8,
		dac_value & 0xFF,
	};
	i2c_master_tx(TELEXO_0, d, 4);
}

static void ii_slew_txo(uint8_t track, uint16_t slew) {
	uint8_t d[4] = {
		0x4F,  // TO_OSC_SLEW
		track,
		slew >> 8,
		slew & 0xFF,
	};
	i2c_master_tx(TELEXO_0, d, 4);
}

static void ii_init_nop(uint8_t track) {
}

static void ii_tr_nop(uint8_t track, uint8_t state) {
}

static void ii_cv_nop(uint8_t track, uint16_t dac_value) {
}

static void ii_slew_nop(uint8_t track, uint16_t slew) {
}

i2c_follower_t followers[I2C_FOLLOWER_COUNT] = {
	{
		.active = false,
		.track_en = 0xF,
		.oct = 0,
		.init = ii_init_jf,
		.tr = ii_tr_jf,
		.cv = ii_cv_nop,
		.slew = ii_slew_nop,
	},
	{
		.active = false,
		.track_en = 0xF,
		.oct = 0,
		.init = ii_init_txo,
		.tr = ii_tr_txo,
		.cv = ii_cv_txo,
		.slew = ii_slew_txo,
	},
};
