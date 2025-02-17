#include "dac.h"
#include "i2c.h"
#include "ii.h"
#include "ansible_ii_leader.h"
#include "main.h"
#include "music.h"

static uint16_t aux_to_vel(uint16_t aux) {
	// map from 1-320 range of duration param to V 2 - V 5 for velocity control
	return aux * 41 + 3264;
}

static void ii_init_jf(i2c_follower_t* follower, uint8_t track, uint8_t state) {
	uint8_t d[4] = { 0 };

	if (!state)
	{
		// set velocity to max to restore normal functionality
		d[0] = JF_VTR;
		d[1] = 0;
		d[2] = 16384 >> 8;
		d[3] = 16834 & 0xFF;
		i2c_leader_tx(follower->addr, d, 3);

		// clear all triggers to avoid hanging notes in SUSTAIN
		d[0] = JF_TR;
		d[1] = 0;
		d[2] = 0;
		i2c_leader_tx(follower->addr, d, 3);
	}

	d[0] = JF_MODE;
	d[1] = state;
	i2c_leader_tx(follower->addr, d, 2);
}

static void ii_tr_jf(i2c_follower_t* follower, uint8_t track, uint8_t state) {
	uint8_t d[6] = { 0 };
	uint8_t l = 0;
	uint16_t pitch = ET[outputs[track].semitones];
	if (state) {
		// map from 1-320 range of duration param to V 2 - V 5 for velocity control
		uint16_t vel = aux_to_vel(aux_param[0][track]);
		switch (follower->active_mode) {
			case 0: { // polyphonically allocated
				d[0] = JF_NOTE;
				d[1] = pitch >> 8;
				d[2] = pitch & 0xFF;
				d[3] = vel >> 8;
				d[4] = vel & 0xFF;
				l = 5;
				break;
			}
			case 1: { // tracks to first 4 voices
				d[0] = JF_VOX;
				d[1] = track + 1;
				d[2] = pitch >> 8;
				d[3] = pitch & 0xFF;
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
	else {
		if (follower->active_mode == 0) {
			d[0] = JF_NOTE;
			d[1] = pitch >> 8;
			d[2] = pitch & 0xFF;
			d[3] = 0;
			d[4] = 0;
			l = 5;
		}
		else
		{
			d[0] = JF_TR;
			d[1] = track + 1;
			d[2] = 0;
			l = 3;
		}
	}
	if (l > 0) {
		i2c_leader_tx(follower->addr, d, l);
	}
}

static void ii_mute_jf(i2c_follower_t* follower, uint8_t track, uint8_t mode) {
	uint8_t d[3] = { 0 };

	// clear all triggers to avoid hanging notes in SUSTAIN
	d[0] = JF_TR;
	d[1] = 0;
	d[2] = 0;
	i2c_leader_tx(follower->addr, d, 3);
}

static void ii_mode_jf(i2c_follower_t* follower, uint8_t track, uint8_t mode) {
	uint8_t d[4] = { 0 };

	if (mode > follower->ops->mode_ct) return;
	follower->active_mode = mode;
	if (mode == 2) {
		d[0] = JF_MODE;
		d[1] = 0;
		i2c_leader_tx(follower->addr, d, 2);

		// clear all triggers to avoid hanging notes in SUSTAIN
		d[0] = JF_TR;
		d[1] = 0;
		d[2] = 0;
		d[3] = 0;
		i2c_leader_tx(follower->addr, d, 3);
	}
	else
	{
		d[0] = JF_MODE;
		d[1] = 1;
		i2c_leader_tx(follower->addr, d, 2);
	}
}

static void ii_octave_jf(i2c_follower_t* follower, uint8_t track, int8_t octave) {
	int16_t shift;
	if (octave > 0) {
		shift = ET[12*octave];
	}
	else if (octave < 0) {
		shift = -(ET[12*(-octave)]);
	}
	else {
		shift = 0;
	}

	uint8_t d[] = { JF_SHIFT, shift >> 8, shift & 0xFF };
	i2c_leader_tx(follower->addr, d, 3);
}

static void ii_init_txo(i2c_follower_t* follower, uint8_t track, uint8_t state) {
	uint8_t d[4] = { 0 };

	if (state == 0) {
		d[0] = 0x60; // TO_ENV_ACT
		d[1] = track;
		d[2] = 0;
		d[3] = 0;
		i2c_leader_tx(follower->addr, d, 4);

		d[0] = 0x40; // TO_OSC
		d[1] = track;
		d[2] = 0;
		d[3] = 0;
		i2c_leader_tx(follower->addr, d, 4);

		d[0] = 0x10; // TO_CV
		d[1] = track;
		d[2] = 0;
		d[3] = 0;
		i2c_leader_tx(follower->addr, d, 4);
	}
}

static void ii_mode_txo(i2c_follower_t* follower, uint8_t track, uint8_t mode) {
	uint8_t d[4] = { 0 };

	if (mode > follower->ops->mode_ct) return;
	follower->active_mode = mode;

	switch (mode) {
		case 0: { // enveloped oscillators
			d[0] = 0x60; // TO_ENV_ACT
			d[1] = track;
			d[2] = 0;
			d[3] = 1;
			i2c_leader_tx(follower->addr, d, 4);

			d[0] = 0x15; // TO_CV_OFF
			d[1] = track;
			d[2] = 0;
			d[3] = 0;
			i2c_leader_tx(follower->addr, d, 4);

			d[0] = 0x10; // TO_CV
			d[1] = track;
			d[2] = 8192 >> 8;
			d[3] = 8192 & 0xFF;
			i2c_leader_tx(follower->addr, d, 4);
			break;
		}
		case 1: { // gate/cv
			d[0] = 0x60; // TO_ENV_ACT
			d[1] = track;
			d[2] = 0;
			d[3] = 0;
			i2c_leader_tx(follower->addr, d, 4);

			d[0] = 0x40; // TO_OSC
			d[1] = track;
			d[2] = 0;
			d[3] = 0;
			i2c_leader_tx(follower->addr, d, 4);

			d[0] = 0x10; // TO_CV
			d[1] = track;
			d[2] = 0;
			d[3] = 0;
			i2c_leader_tx(follower->addr, d, 4);
			break;
		}
		default: return;
	}
}

static void ii_octave_txo(i2c_follower_t* follower, uint8_t track, int8_t octave) {
	int16_t shift;
	switch (follower->active_mode) {
		case 0: { // enveloped oscillator, pitch is calculated from oct
			break;
		}
		case 1: { // gate / cv
			if (octave > 0) {
				shift = ET[12*octave];
			}
			else if (octave < 0) {
				shift = -ET[12*(-octave)];
			}
			else {
				shift = 0;
			}
			uint8_t d[] = {
				0x15, // TO_CV_OFF
				0,
				shift >> 8,
				shift & 0xFF,
			};
			for (uint8_t i = 0; i < 4; i++) {
				d[1] = i;
				i2c_leader_tx(follower->addr, d, 4);
			}
			break;
		}
		default: return;
	}
}

static void ii_tr_txo(i2c_follower_t* follower, uint8_t track, uint8_t state) {
	uint8_t d[4] = { 0 };

	switch (follower->active_mode) {
		case 0: { // enveloped oscillator
			d[0] = 0x6D; // TO_ENV
			d[1] = track;
			d[2] = 0;
			d[3] = state;
			i2c_leader_tx(follower->addr, d, 4);
			break;
		}
		case 1: { // gate/cv
			d[0] = 0x00; // TO_TR
			d[1] = track;
			d[2] = 0;
			d[3] = state;
			i2c_leader_tx(follower->addr, d, 4);
			break;
		}
		default: return;
	}
}

static void ii_mute_txo(i2c_follower_t* follower, uint8_t track, uint8_t mode) {
	for (uint8_t i = 0; i < 4; i++) {
		ii_tr_txo(follower, i, 0);
	}
}

static void ii_cv_txo(i2c_follower_t* follower, uint8_t track, uint16_t dac_value) {
	uint8_t d[4] = { 0 };

	switch (follower->active_mode) {
		case 0: { // enveloped oscillator
			dac_value = (int)dac_value + (int)ET[12*(4+follower->oct)];
			d[0] = 0x40; // TO_OSC
			d[1] = track;
			d[2] = dac_value >> 8;
			d[3] = dac_value & 0xFF;
			i2c_leader_tx(follower->addr, d, 4);
			break;
		}
		case 1: { // gate/cv
			d[0] = 0x10; // TO_CV
			d[1] = track;
			d[2] = dac_value >> 8;
			d[3] = dac_value & 0xFF;
			i2c_leader_tx(follower->addr, d, 4);
			break;
		}
		default: return;
	}
}

static void ii_slew_txo(i2c_follower_t* follower, uint8_t track, uint16_t slew) {
	uint8_t d[4] = { 0 };

	switch (follower->active_mode) {
		case 0: { // oscillator
			d[0] = 0x4F;  // TO_OSC_SLEW
			d[1] = track;
			d[2] = slew >> 8;
			d[3] = slew & 0xFF;
			i2c_leader_tx(follower->addr, d, 4);
			break;
		}
		case 1: { // gate/cv
			d[0] = 0x12;  // TO_CV_SLEW
			d[1] = track;
			d[2] = slew >> 8;
			d[3] = slew & 0xFF;
			i2c_leader_tx(follower->addr, d, 4);
			break;
		}
		default: return;
	}
}

static void ii_mode_disting_ex(i2c_follower_t* follower, uint8_t track, uint8_t mode) {
	if (mode > follower->ops->mode_ct) return;
	follower->active_mode = mode;
}

static void ii_tr_disting_ex(i2c_follower_t* follower, uint8_t track, uint8_t state) {
	uint8_t d[4] = { 0 };
	u16 note = outputs[track].semitones + 12 * (4 + follower->oct);

	switch (follower->active_mode) {
		case 0: // SD Multisample / SD Triggers allocated voices
			if (note < 0) note = 0; else if (note > 127) note = 127;
			if (state) {
				d[0] = 0x56;
				d[1] = note;
				i2c_leader_tx(follower->addr, d, 2);
				d[0] = 0x55;
				d[2] = 0x40;
				d[3] = 0;
				i2c_leader_tx(follower->addr, d, 4);
			} else {
				d[0] = 0x56;
				d[1] = note;
				i2c_leader_tx(follower->addr, d, 2);
			}
			break;

		case 1: // SD Multisample / SD Triggers fixed voices
			if (state) {
				d[0] = 0x52;
				d[1] = track;
				d[2] = 0x40;
				d[3] = 0;
				i2c_leader_tx(follower->addr, d, 4);
			} else {
				d[0] = 0x53;
				d[1] = track;
				i2c_leader_tx(follower->addr, d, 2);
			}
			break;

		case 2: // MIDI channel 1
			if (note < 0 || note > 127) return;
			if (state) {
				d[0] = 0x4F;
				d[1] = 0x90;
				d[2] = note;
				d[3] = 80;
				i2c_leader_tx(follower->addr, d, 4);
			} else {
				d[0] = 0x4F;
				d[1] = 0x80;
				d[2] = note;
				d[3] = 0;
				i2c_leader_tx(follower->addr, d, 4);
			}
			break;
		case 3: // MIDI channels 1-4
			if (note < 0 || note > 127) return;
			if (state) {
				d[0] = 0x4F;
				d[1] = 0x90 + track;
				d[2] = note;
				d[3] = 80;
				i2c_leader_tx(follower->addr, d, 4);
			} else {
				d[0] = 0x4F;
				d[1] = 0x80 + track;
				d[2] = note;
				d[3] = 0;
				i2c_leader_tx(follower->addr, d, 4);
			}
			break;
		default: return;
	}
}

static void ii_mute_disting_ex(i2c_follower_t* follower, uint8_t track, uint8_t mode) {
	for (uint8_t i = 0; i < 4; i++) {
		ii_tr_disting_ex(follower, i, 0);
	}
    
    uint8_t d[1] = { 0x57 };
    i2c_leader_tx(follower->addr, d, 1);
}

static void ii_cv_disting_ex(i2c_follower_t* follower, uint8_t track, uint16_t dac_value) {
	uint8_t d[4] = { 0 };

    u16 note = outputs[track].semitones + 12 * (4 + follower->oct);
    u16 pitch = ET[note] - ET[36];
    
	d[2] = pitch >> 8;
	d[3] = pitch;

	if (follower->active_mode == 0) {
		if (note < 0) note = 0; else if (note > 127) note = 127;
		d[0] = 0x54;
		d[1] = note;
		i2c_leader_tx(follower->addr, d, 4);
	} else if (follower->active_mode == 1) {
		d[0] = 0x51;
		d[1] = track;
		i2c_leader_tx(follower->addr, d, 4);
	}
}

static void ii_init_wsyn(i2c_follower_t* follower, uint8_t track, uint8_t state) {
	uint8_t d[4] = { 0 };

	if (!state)
	{
		// clear all triggers to avoid hanging notes in SUSTAIN
		d[0] = WS_S_VEL;
		d[1] = 0;
		d[2] = 0;
		d[3] = 0;
		i2c_leader_tx(follower->addr, d, 4);
	}
	else
	{
		d[0] = WS_S_AR_MODE;
		d[1] = 0;
		i2c_leader_tx(follower->addr, d, 2);
	}
}

static void ii_tr_wsyn(i2c_follower_t* follower, uint8_t track, uint8_t state) {
	uint8_t d[6] = { 0 };
	uint8_t l = 0;
	uint16_t pitch = ET[outputs[track].semitones + (3 + follower->oct) * 12];
	if (state) {
		uint16_t vel = aux_to_vel(aux_param[0][track]);
		switch (follower->active_mode) {
			case 0: { // polyphonically allocated
				d[0] = WS_S_NOTE;
				d[1] = pitch >> 8;
				d[2] = pitch & 0xFF;
				d[3] = vel >> 8;
				d[4] = vel & 0xFF;
				l = 5;
				break;
			}
			case 1: { // tracks to first 4 voices
				d[0] = WS_S_VOX;
				d[1] = track + 1;
				d[2] = pitch >> 8;
				d[3] = pitch & 0xFF;
				d[4] = vel >> 8;
				d[5] = vel & 0xFF;
				l = 6;
				break;
			}
			default: {
				return;
			}
		}
	}
	else {
		switch (follower->active_mode) {
			case 0: {
				d[0] = WS_S_NOTE;
				d[1] = pitch >> 8;
				d[2] = pitch & 0xFF;
				d[3] = 0;
				d[4] = 0;
				l = 5;
				break;
			}
			case 1: {
				d[0] = WS_S_VOX;
				d[1] = track + 1;
				d[2] = pitch >> 8;
				d[3] = pitch & 0xFF;
				d[4] = 0;
				d[5] = 0;
				l = 6;
				break;
			}
		}
	}
	if (l > 0) {
		i2c_leader_tx(follower->addr, d, l);
	}
}

static void ii_mode_wsyn(i2c_follower_t* follower, uint8_t track, uint8_t mode) {
	if (mode > follower->ops->mode_ct) return;
	follower->active_mode = mode;
}

static void ii_mute_wsyn(i2c_follower_t* follower, uint8_t track, uint8_t mode) {
	uint8_t d[6] = { 0 };

	// clear all triggers to avoid hanging notes in SUSTAIN
	d[0] = WS_S_VOX;
	d[1] = 0;
	d[2] = 0;
	d[3] = 0;
	d[4] = 0;
	d[5] = 0;
	i2c_leader_tx(follower->addr, d, 6);
}

static void ii_cv_wsyn(i2c_follower_t* follower, uint8_t track, uint16_t dac_value) {
	uint8_t d[4] = { 0 };
	d[0] = WS_S_PITCH;
	d[1] = track;
	d[2] = dac_value >> 8;
	d[3] = dac_value & 0xFF;
	i2c_leader_tx(follower->addr, d, 4);
}

static void ii_mode_crow(i2c_follower_t* follower, uint8_t track, uint8_t mode) {
	if (mode > follower->ops->mode_ct) return;
	follower->active_mode = mode;
}

static void ii_tr_crow(i2c_follower_t* follower, uint8_t track, uint8_t state) {
	uint8_t d[7];
	uint16_t pitch = ET[outputs[track].semitones];
	uint16_t vel = state && aux_to_vel(aux_param[0][track]);

	d[0] = 6; // call3
	d[1] = 0;
	d[2] = track + 1;
	d[3] = pitch >> 8;
	d[4] = pitch & 0xFF;
	d[5] = vel >> 8;
	d[6] = vel & 0xFF;
	i2c_leader_tx(follower->addr, d, 7);
}

static void ii_u8_nop(i2c_follower_t* follower, uint8_t track, uint8_t state) {
}

static void ii_s8_nop(i2c_follower_t* follower, uint8_t track, int8_t state) {
}

static void ii_u16_nop(i2c_follower_t* follower, uint8_t track, uint16_t dac_value) {
}

i2c_follower_t followers[I2C_FOLLOWER_COUNT] = {
	{
		.addr = JF_ADDR,
		.active = false,
		.track_en = 0xF,
		.oct = 0,
		.active_mode = 0,

		.ops = &(i2c_ops_t){
			.init = ii_init_jf,
			.mode = ii_mode_jf,
			.tr = ii_tr_jf,
			.mute = ii_mute_jf,
			.cv = ii_u16_nop,
			.octave = ii_octave_jf,
			.slew = ii_u16_nop,
			.mode_ct = 3,
		},
	},
	{
		.addr = TELEXO_0,
		.active = false,
		.track_en = 0xF,
		.oct = 0,
		.active_mode = 0,

		.ops = &(i2c_ops_t){
			.init = ii_init_txo,
			.mode = ii_mode_txo,
			.tr = ii_tr_txo,
			.mute = ii_mute_txo,
			.cv = ii_cv_txo,
			.octave = ii_octave_txo,
			.slew = ii_slew_txo,
			.mode_ct = 2,
		},
	},
	{
		.addr = ER301_1,
		.active = false,
		.track_en = 0xF,
		.oct = 0,
		.active_mode = 1, // always gate/cv

		.ops = &(i2c_ops_t){
			.init = ii_u8_nop,
			.mode = ii_u8_nop,
			.tr = ii_tr_txo,
			.mute = ii_mute_txo,
			.cv = ii_cv_txo,
			.octave = ii_octave_txo,
			.slew = ii_slew_txo,
			.mode_ct = 1,
		},
	},
	{
		.addr = DISTING_EX_1,
		.active = false,
		.track_en = 0xF,
		.oct = 0,
		.active_mode = 0,

		.ops = &(i2c_ops_t){
			.init = ii_u8_nop,
			.mode = ii_mode_disting_ex,
			.tr = ii_tr_disting_ex,
			.mute = ii_mute_disting_ex,
			.cv = ii_cv_disting_ex,
			.octave = ii_s8_nop,
			.slew = ii_u16_nop,
			.mode_ct = 4,
		},
	},
	{
		.addr = WS_S_ADDR,
		.active = false,
		.track_en = 0xF,
		.oct = -2,
		.active_mode = 0,

		.ops = &(i2c_ops_t){
			.init = ii_init_wsyn,
			.mode = ii_mode_wsyn,
			.tr = ii_tr_wsyn,
			.mute = ii_mute_wsyn,
			.cv = ii_cv_wsyn,
			.octave = ii_s8_nop,
			.slew = ii_u16_nop,

			.mode_ct = 2,
		},
	},
	{
		.addr = CROW,
		.active = false,
		.track_en = 0xF,
		.oct = 0,
		.active_mode = 0,

		.ops = &(i2c_ops_t){
			.init = ii_u8_nop,
			.mode = ii_mode_crow,
			.tr = ii_tr_crow,
			.mute = ii_u8_nop,
			.cv = ii_u16_nop,
			.octave = ii_s8_nop,
			.slew = ii_u16_nop,

			.mode_ct = 1,
		},
	},
};

void follower_change_mode(i2c_follower_t* follower, uint8_t param) {
	for (int i = 0; i < 4; i++) {
		if (follower->track_en & (1 << i)) {
			follower->ops->mode(follower, i, param);
		}
	}
}

void follower_change_octave(i2c_follower_t* follower, int8_t param) {
	for (int i = 0; i < 4; i++) {
		if (follower->track_en & (1 << i)) {
			follower->ops->octave(follower, i, param);
		}
	}
}
