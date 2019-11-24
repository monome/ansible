#include "dac.h"
#include "i2c.h"
#include "ii.h"
#include "ansible_ii_leader.h"
#include "main.h"

static void ii_init_jf(i2c_follower_t* follower, uint8_t track, uint8_t on) {
	uint8_t d[] = { JF_MODE, on };
	i2c_master_tx(JF_ADDR, d, 2);
}

static void ii_tr_jf(i2c_follower_t* follower, uint8_t track, uint8_t state) {
	uint8_t d[6] = { 0 };
	uint8_t l = 0;
	uint16_t dac_value = dac_get_value(track);
	if (state) {
		uint16_t vel = aux_param[0][track] * 140;
		switch (follower->active_mode) {
			case 0: { // polyphonically allocated
				d[0] = JF_NOTE;
				d[1] = dac_value >> 8;
				d[2] = dac_value & 0xFF;
				d[3] = vel >> 8;
				d[4] = vel & 0xFF;
				l = 5;
				break;
			}
			case 1: { // tracks to first 4 voices
				d[0] = JF_VOX;
		        	d[1] = track + 1;
				d[2] = dac_value >> 8;
				d[3] = dac_value & 0xFF;
				d[4] = vel >> 8;
				d[5] = vel & 0xFF;
				l = 6;
				break;
			}
			case 2: { // envelopes
				d[0] = JF_VTR;
		        	d[1] = track + 1;
				d[2] = vel >> 8;
				d[3] = vel & 0xFF;
				l = 4;
				break;
			}
			default: {
				return;
			}
		}
	}
	else if (follower->active_mode == 2) {
		d[0] = JF_TR;
	        d[1] = track + 1;
		d[2] = 0;
		l = 3;
	}
	i2c_master_tx(JF_ADDR, d, l);
}

static void ii_init_txo(i2c_follower_t* follower, uint8_t track, uint8_t on) {
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

static void ii_tr_txo(i2c_follower_t* follower, uint8_t track, uint8_t state) {
	uint8_t d[3] = { 0 };

	switch (follower->active_mode) {
		case 0: { // enveloped oscillator
			d[0] = 0x6D; // TO_ENV
			d[1] = track;
			d[2] = state;
			i2c_master_tx(TELEXO_0, d, 3);
			break;
		}
		case 1: { // gate/cv
			d[0] = 0x00; // TO_TR
			d[1] = track;
			d[2] = state;
			i2c_master_tx(TELEXO_0, d, 3);
			break;
		}
		default: return;
	}
}

static void ii_cv_txo(i2c_follower_t* follower, uint8_t track, uint16_t dac_value) {
	uint8_t d[4] = { 0 };

	switch (follower->active_mode) {
		case 0: { // enveloped oscillator
			d[0] = 0x40; // TO_OSC
			d[1] = track;
			d[2] = dac_value >> 8;
			d[3] = dac_value & 0xFF;
			i2c_master_tx(TELEXO_0, d, 4);
			break;
		}
		case 1: { // gate/cv
			d[0] = 0x10; // TO_CV
			d[1] = track;
			d[2] = dac_value >> 8;
			d[3] = dac_value & 0xFF;
			i2c_master_tx(TELEXO_0, d, 4);
			break;
		}
		default: return;
	}
}

static void ii_slew_txo(i2c_follower_t* follower, uint8_t track, uint16_t slew) {
	uint8_t d[4] = {
		0x4F,  // TO_OSC_SLEW
		track,
		slew >> 8,
		slew & 0xFF,
	};
	i2c_master_tx(TELEXO_0, d, 4);
}

static void ii_init_nop(i2c_follower_t* follower, uint8_t track) {
}

static void ii_tr_nop(i2c_follower_t* follower, uint8_t track, uint8_t state) {
}

static void ii_cv_nop(i2c_follower_t* follower, uint8_t track, uint16_t dac_value) {
}

static void ii_slew_nop(i2c_follower_t* follower, uint8_t track, uint16_t slew) {
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
		.param_ct = 2,
		.mode_ct = 3,
		.active_mode = 0,
	},
	{
		.active = false,
		.track_en = 0xF,
		.oct = 3,
		.init = ii_init_txo,
		.tr = ii_tr_txo,
		.cv = ii_cv_txo,
		.slew = ii_slew_txo,
		.param_ct = 3,
		.mode_ct = 2,
		.active_mode = 0,
	},
};
