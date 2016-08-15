// libavr32
#include "print_funcs.h"
#include "flashc.h"

#include "midi_common.h"
#include "notes.h"
#include "timers.h"

#include "monome.h"
#include "i2c.h"
#include "gpio.h"

// this
#include "main.h"
#include "ansible_midi.h"

#define MIDI_NOTE_MAX 120

#define MONO_PITCH_CV &(aout[0])
#define MONO_VELOCITY_CV &(aout[1])
#define MONO_PRESSURE_CV &(aout[2])
#define MONO_MOD_CV &(aout[3])
#define MONO_BEND_CV &(aout[3])



//------------------------------
//------ prototypes

static void write_midi_standard(void);

static void set_cv_pitch(uint16_t *cv, u8 num, s16 offset);
static void set_cv_velocity(uint16_t *cv, u8 vel);
static void set_cv_cc(uint16_t *cv, u8 value);

static void set_voice_allocation(voicing_mode v);

static void reset(void);

static void poly_note_on(u8 ch, u8 num, u8 vel);
static void poly_note_off(u8 ch, u8 num, u8 vel);
static void poly_pitch_bend(u8 ch, u16 bend);
static void poly_sustain(u8 ch, u8 val);
static void poly_control_change(u8 ch, u8 num, u8 val);
static void poly_panic(void);

static void mono_note_on(u8 ch, u8 num, u8 vel);
static void mono_note_off(u8 ch, u8 num, u8 vel);
static void mono_pitch_bend(u8 ch, u16 bend);
static void mono_sustain(u8 ch, u8 val);
static void mono_channel_pressure(u8 ch, u8 val);
static void mono_control_change(u8 ch, u8 num, u8 val);
static void mono_panic(void);

static void mono_rt_tick(void);
static void mono_rt_start(void);
static void mono_rt_stop(void);
static void mono_rt_continue(void);

static void multi_tr_set(u8 ch);
static void multi_tr_clr(u8 ch);

static void multi_note_on(u8 ch, u8 num, u8 vel);
static void multi_note_off(u8 ch, u8 num, u8 vel);
static void multi_pitch_bend(u8 ch, u16 bend);
static void multi_sustain(u8 ch, u8 val);
static void multi_control_change(u8 ch, u8 num, u8 val);

static void fixed_write_mapping(const fixed_mapping_t *dest, const fixed_mapping_t *src);
static void fixed_start_learning(void);
static bool fixed_finalize_learning(bool cancel);
static void fixed_note_on(u8 ch, u8 num, u8 vel);
static void fixed_note_off(u8 ch, u8 num, u8 vel);
static void fixed_control_change(u8 ch, u8 num, u8 val);


//-----------------------------
//----- globals

// step = 4096.0 / (10 octave * 12.0 semitones per octave)
// [int(n * step) for n in xrange(0,128)]
const u16 SEMI[128] = { 
	0, 34, 68, 102, 136, 170, 204, 238, 273, 307, 341, 375, 409, 443, 477, 512,
	546, 580, 614, 648, 682, 716, 750, 785, 819, 853, 887, 921, 955, 989, 1024,
	1058, 1092, 1126, 1160, 1194, 1228, 1262, 1297, 1331, 1365, 1399, 1433, 1467,
	1501, 1536, 1570, 1604, 1638, 1672, 1706, 1740, 1774, 1809, 1843, 1877, 1911,
	1945, 1979, 2013, 2048, 2082, 2116, 2150, 2184, 2218, 2252, 2286, 2321, 2355,
	2389, 2423, 2457, 2491, 2525, 2560, 2594, 2628, 2662, 2696, 2730, 2764, 2798,
	2833, 2867, 2901, 2935, 2969, 3003, 3037, 3072, 3106, 3140, 3174, 3208, 3242,
	3276, 3310, 3345, 3379, 3413, 3447, 3481, 3515, 3549, 3584, 3618, 3652, 3686,
	3720, 3754, 3788, 3822, 3857, 3891, 3925, 3959, 3993, 4027, 4061, 4096, 4130,
	4164, 4198, 4232, 4266, 4300, 4334
};

// step = 4096.0 / (10 octave * 12.0 semitones per octave)
// semi_per_octave = step * 12
// bend_step = semi_per_octave / 512.0
// [int(n * bend_step) for n in xrange(0,512)]
const u16 BEND1[512] = {
	0, 0, 1, 2, 3, 4, 4, 5, 6, 7, 8, 8, 9, 10, 11, 12, 12, 13, 14, 15, 16, 16, 17,
	18, 19, 20, 20, 21, 22, 23, 24, 24, 25, 26, 27, 28, 28, 29, 30, 31, 32, 32,
	33, 34, 35, 36, 36, 37, 38, 39, 40, 40, 41, 42, 43, 44, 44, 45, 46, 47, 48,
	48, 49, 50, 51, 52, 52, 53, 54, 55, 56, 56, 57, 58, 59, 60, 60, 61, 62, 63,
	64, 64, 65, 66, 67, 68, 68, 69, 70, 71, 72, 72, 73, 74, 75, 76, 76, 77, 78,
	79, 80, 80, 81, 82, 83, 84, 84, 85, 86, 87, 88, 88, 89, 90, 91, 92, 92, 93,
	94, 95, 96, 96, 97, 98, 99, 100, 100, 101, 102, 103, 104, 104, 105, 106, 107,
	108, 108, 109, 110, 111, 112, 112, 113, 114, 115, 116, 116, 117, 118, 119,
	120, 120, 121, 122, 123, 124, 124, 125, 126, 127, 128, 128, 129, 130, 131,
	132, 132, 133, 134, 135, 136, 136, 137, 138, 139, 140, 140, 141, 142, 143,
	144, 144, 145, 146, 147, 148, 148, 149, 150, 151, 152, 152, 153, 154, 155,
	156, 156, 157, 158, 159, 160, 160, 161, 162, 163, 164, 164, 165, 166, 167,
	168, 168, 169, 170, 171, 172, 172, 173, 174, 175, 176, 176, 177, 178, 179,
	180, 180, 181, 182, 183, 184, 184, 185, 186, 187, 188, 188, 189, 190, 191,
	192, 192, 193, 194, 195, 196, 196, 197, 198, 199, 200, 200, 201, 202, 203,
	204, 204, 205, 206, 207, 208, 208, 209, 210, 211, 212, 212, 213, 214, 215,
	216, 216, 217, 218, 219, 220, 220, 221, 222, 223, 224, 224, 225, 226, 227,
	228, 228, 229, 230, 231, 232, 232, 233, 234, 235, 236, 236, 237, 238, 239,
	240, 240, 241, 242, 243, 244, 244, 245, 246, 247, 248, 248, 249, 250, 251,
	252, 252, 253, 254, 255, 256, 256, 257, 258, 259, 260, 260, 261, 262, 263,
	264, 264, 265, 266, 267, 268, 268, 269, 270, 271, 272, 272, 273, 274, 275,
	276, 276, 277, 278, 279, 280, 280, 281, 282, 283, 284, 284, 285, 286, 287,
	288, 288, 289, 290, 291, 292, 292, 293, 294, 295, 296, 296, 297, 298, 299,
	300, 300, 301, 302, 303, 304, 304, 305, 306, 307, 308, 308, 309, 310, 311,
	312, 312, 313, 314, 315, 316, 316, 317, 318, 319, 320, 320, 321, 322, 323,
	324, 324, 325, 326, 327, 328, 328, 329, 330, 331, 332, 332, 333, 334, 335,
	336, 336, 337, 338, 339, 340, 340, 341, 342, 343, 344, 344, 345, 346, 347,
	348, 348, 349, 350, 351, 352, 352, 353, 354, 355, 356, 356, 357, 358, 359,
	360, 360, 361, 362, 363, 364, 364, 365, 366, 367, 368, 368, 369, 370, 371,
	372, 372, 373, 374, 375, 376, 376, 377, 378, 379, 380, 380, 381, 382, 383,
	384, 384, 385, 386, 387, 388, 388, 389, 390, 391, 392, 392, 393, 394, 395,
	396, 396, 397, 398, 399, 400, 400, 401, 402, 403, 404, 404, 405, 406, 407,
	408, 408
};

// copy of nvram state for editing
static midi_standard_state_t standard_state;
static midi_arp_state_t *arp_state = NULL;

static midi_behavior_t active_behavior = {
	.note_on = NULL,
	.note_off = NULL,
	.channel_pressure = NULL,
	.pitch_bend = NULL,
	.control_change = NULL
};

typedef struct {
	u8 learning : 1;
	u8 note_idx : 3;
	u8 cc_idx : 3;
} fixed_learning_state_t;

static fixed_learning_state_t fixed_learn;
static note_pool_t notes[4];
static voice_flags_t flags[4];
static voice_state_t voice_state;
static s16 pitch_offset[4];
static uint16_t aout[4];

// TODO: move to main.c?
typedef struct {
	u8 key1 : 1;
	u8 key2 : 1;
	u8 front : 1;
	u8 normaled : 1; // isn't this tracked elsewhere?
} key_state_t;

static key_state_t key_state;

static midi_clock_t midi_clock;

void set_mode_midi(void) {
	switch(f.state.mode) {
	case mMidiStandard:
		print_dbg("\r\n> mode midi standard");

		standard_state = f.midi_standard_state;
		//print_standard_state(&standard_state);
		
		app_event_handlers[kEventKey] = &handler_StandardKey;
		app_event_handlers[kEventTr] = &handler_StandardTr;
		app_event_handlers[kEventTrNormal] = &handler_StandardTrNormal;
		app_event_handlers[kEventMidiPacket] = &handler_StandardMidiPacket;

		set_voice_allocation(standard_state.voicing);
		clock_set(standard_state.clock_period);
		process_ii = &ii_midi_standard;
		update_leds(1);
		break;
	case mMidiArp:
		print_dbg("\r\n> mode midi arp");
		app_event_handlers[kEventKey] = &handler_ArpKey;
		app_event_handlers[kEventTr] = &handler_ArpTr;
		app_event_handlers[kEventTrNormal] = &handler_ArpTrNormal;
		app_event_handlers[kEventMidiPacket] = &handler_ArpMidiPacket;

		clock = &clock_midi_arp;
		clock_set(f.midi_arp_state.clock_period);
		process_ii = &ii_midi_arp;
		update_leds(2);
		break;
	default:
		break;
	}
	
	if(connected == conMIDI) {
		app_event_handlers[kEventFrontShort] = &handler_MidiFrontShort;
		app_event_handlers[kEventFrontLong] = &handler_MidiFrontLong;
	}

	midi_clock_init(&midi_clock);
	midi_clock_set_div(&midi_clock, 4); // 16th notes; TODO; make configurable?

	key_state.key1 = key_state.key2 = key_state.front = 0;
	key_state.normaled = !gpio_get_pin_value(B10);
	
	aout[0] = aout[1] = aout[2] = aout[3] = 0;
	update_dacs(aout);
}


void handler_MidiFrontShort(s32 data) {
	if (standard_state.voicing == eVoiceFixed && key_state.key2) {
		fixed_start_learning();
	}
	else {
		// save voice mode configuration to flash
		write_midi_standard();
		print_dbg("\r\n standard: wrote midi config");
	}
}

void handler_MidiFrontLong(s32 data) {
	if(f.state.mode == mMidiStandard)
		set_mode(mMidiArp);
	else
		set_mode(mMidiStandard);
}


////////////////////////////////////////////////////////////////////////////////
///// common cv utilities

static void set_cv_pitch(uint16_t *cv, u8 num, s16 offset) {
	*cv = SEMI[num] + offset;
}

static void set_cv_velocity(uint16_t *cv, u8 vel) {
	// for the moment, straight mapping
	*cv = vel << 5;
}

static inline void set_cv_cc(uint16_t *cv, u8 value) {
	// 128 << 5 == 4096; 12-bit dac
	*cv = value << 5;
}

static void reset(void) {
	// start from a clean slate
	for (int i = 0; i < 4; i++) {
		voice_flags_init(&(flags[i]));
		notes_init(&(notes[i]));
		pitch_offset[i] = 0;
		aout[i] = 0;
	}
	update_dacs(aout);
	clr_tr(TR1);
	clr_tr(TR2);
	clr_tr(TR3);
	clr_tr(TR4);
}

static void set_voice_allocation(voicing_mode v) {
	reset();

	// config behavior, clock, flags, etc.
	switch (v) {
	case eVoicePoly:
		active_behavior.note_on = &poly_note_on;
		active_behavior.note_off = &poly_note_off;
		active_behavior.channel_pressure = NULL;
		active_behavior.pitch_bend = &poly_pitch_bend;
		active_behavior.control_change = &poly_control_change;
		active_behavior.clock_tick = NULL;
		active_behavior.seq_start = NULL;
		active_behavior.seq_stop = NULL;
		active_behavior.seq_continue = NULL;
		active_behavior.panic = &poly_panic;
		voice_slot_init(&voice_state, kVoiceAllocRotate, 4); // TODO: count configurable?
		clock = &clock_null;
		print_dbg("\r\n standard: voice poly");
		break;
	case eVoiceMono:
		active_behavior.note_on = &mono_note_on;
		active_behavior.note_off = &mono_note_off;
		active_behavior.channel_pressure = &mono_channel_pressure;
		active_behavior.pitch_bend = &mono_pitch_bend;
		active_behavior.control_change = &mono_control_change;
		active_behavior.clock_tick = &mono_rt_tick;
		active_behavior.seq_start = &mono_rt_start;
		active_behavior.seq_stop = &mono_rt_stop;
		active_behavior.seq_continue = &mono_rt_continue;
		active_behavior.panic = &mono_panic;
		clock = &clock_null;
		flags[0].legato = 1;
		mono_pitch_bend(0, MIDI_BEND_ZERO);
		print_dbg("\r\n standard: voice mono");
		break;
	case eVoiceMulti:
		active_behavior.note_on = &multi_note_on;
		active_behavior.note_off = &multi_note_off;
		active_behavior.channel_pressure = NULL;
		active_behavior.pitch_bend = &multi_pitch_bend;
		active_behavior.control_change = &multi_control_change;
		active_behavior.clock_tick = NULL;
		active_behavior.seq_start = NULL;
		active_behavior.seq_stop = NULL;
		active_behavior.seq_continue = NULL;
		active_behavior.panic = &reset;
		clock = &clock_null;
		flags[0].legato = flags[1].legato = flags[2].legato = flags[3].legato = 1;
		print_dbg("\r\n standard: voice multi");
		break;
	case eVoiceFixed:
		active_behavior.note_on = &fixed_note_on;
		active_behavior.note_off = &fixed_note_off;
		active_behavior.channel_pressure = NULL;
		active_behavior.pitch_bend = NULL;
		active_behavior.control_change = &fixed_control_change;
		active_behavior.clock_tick = NULL;
		active_behavior.seq_start = NULL;
		active_behavior.seq_stop = NULL;
		active_behavior.seq_continue = NULL;
		active_behavior.panic = &reset;
		clock = &clock_null;
		print_dbg("\r\n standard: voice fixed");
		break;
	default:
		print_dbg("\r\n standard: bad voice mode");
	}
}


////////////////////////////////////////////////////////////////////////////////
///// handlers (standard)

void default_midi_standard(void) {
	fixed_mapping_t m;

	flashc_memset32((void*)&(f.midi_standard_state.clock_period), 100, 4, true);
	flashc_memset8((void*)&(f.midi_standard_state.voicing), eVoicePoly, 1, true);

	m.notes[0] = 60; // C4
	m.notes[1] = 62; // D4
	m.notes[2] = 64; // E4
	m.notes[3] = 65; // F4

	m.cc[0] = 16;
	m.cc[1] = 17;
	m.cc[2] = 18;
	m.cc[3] = 19;

	fixed_write_mapping(&(f.midi_standard_state.fixed), &m);
}

void write_midi_standard(void) {
	flashc_memset32((void*)&(f.midi_standard_state.clock_period),
									standard_state.clock_period, 4, true);
	flashc_memset8((void*)&(f.midi_standard_state.voicing),
								 standard_state.voicing, 1, true);
	fixed_write_mapping(&(f.midi_standard_state.fixed), &standard_state.fixed);
}

void clock_midi_standard(uint8_t phase) {
	if(phase)
		set_tr(TR4);
	else
		clr_tr(TR4);
}

void ii_midi_standard(uint8_t *d, uint8_t l) {
	;;
}

void handler_StandardKey(s32 data) { 
	//print_dbg("\r\n> standard key");
	//print_dbg_ulong(data);

	// 0 == key 1 release
	// 1 == key 1 press
	// 2 == key 2 release
	// 3 == key 2 press

	switch (data) {

	case 0: // key 1 release
		key_state.key1 = 0;
		//print_dbg("\r\n standard key1: release");
		if (fixed_learn.learning == 1 ) {
			//print_dbg("; canceling fixed mapping learning");
			fixed_finalize_learning(true);
		}
		else {
			// panic, all notes off
			if (active_behavior.panic) (*active_behavior.panic)();
		}
		break;

	case 1:
		key_state.key1 = 1;
		break;

	case 2: // key 2 release
		key_state.key2 = 0;
		//print_dbg("\r\n standard key2: release");
		if (fixed_learn.learning == 1) {
			// in learning mode; do nothing
			//print_dbg("; noop, in learning mode");
		}
		else {
			// switch voicing mode
			standard_state.voicing++;
			if (standard_state.voicing >= eVoiceMAX) {
				standard_state.voicing = eVoicePoly;
			}
			set_voice_allocation(standard_state.voicing);
		}
		break;

	case 3:
		key_state.key2 = 1;
		break;
	}
}

void handler_StandardTr(s32 data) {
	print_dbg("\r\n> standard tr => ");
	print_dbg_ulong(data);
}

void handler_StandardTrNormal(s32 data) { 
	print_dbg("\r\n> standard tr normal => ");
	print_dbg_ulong(data);
	// FIXME: this will be wrong on power up if a cable is inserted
	key_state.normaled = data;
}

void handler_StandardMidiPacket(s32 data) {
	midi_packet_parse(&active_behavior, (u32)data);
}

////////////////////////////////////////////////////////////////////////////////
///// poly behavior (standard)

static void poly_note_on(u8 ch, u8 num, u8 vel) {
	u8 slot = voice_slot_next(&voice_state);

	if (voice_slot_active(&voice_state, slot)) {
		// steal slot
		// TODO: check for sustain
		multi_tr_clr(slot);
		// FIXME: here we clear tr but then immediately reset it for note
		// on so the envelop wont retrigger. need some way to queue or
		// pause long enough to have tr retrigger.
		voice_slot_release(&voice_state, slot);
	}

	voice_slot_activate(&voice_state, slot, num);
	multi_note_on(slot, num, vel);
}

static void poly_note_off(u8 ch, u8 num, u8 vel) {
	s8 slot = voice_slot_find(&voice_state, num);
	if (slot != -1) {
		// TODO: check for sustain
		multi_tr_clr(slot);
		voice_slot_release(&voice_state, slot);
	}
}

static void poly_pitch_bend(u8 ch, u16 bend) {
	// MAINT: assuming bend is always 14 bit...
	if (bend == MIDI_BEND_ZERO) {
		pitch_offset[0] = 0;
	}
	else if (bend > MIDI_BEND_ZERO) {
		bend -= MIDI_BEND_ZERO;
		pitch_offset[0] = BEND1[bend >> 4];
	}
	else {
		bend = MIDI_BEND_ZERO - bend - 1;
		pitch_offset[0] = -BEND1[bend >> 4];
	}

	for (u8 i = 0; i < voice_state.count; i++) {
		if (voice_state.voice[i].active) {
			set_cv_pitch(&(aout[i]), voice_state.voice[i].num, pitch_offset[0]);
		}
	}
	update_dacs(aout);
}

static void poly_sustain(u8 ch, u8 val) {
	u8 slot;

	for (slot = 0; slot < MAX_VOICE_COUNT; slot++) {
		ch = voice_state.voice[slot].num;
		if (val < 64) {
			// release active voices
			if (voice_state.voice[slot].active) {
				voice_state.voice[slot].active = 0;
				multi_tr_clr(ch);
			}
			flags[ch].sustain = 0;
		}
		else {
			flags[ch].sustain = 1;
		}
	}
}

static void poly_control_change(u8 ch, u8 num, u8 val) {
	switch (num) {
		case 64:  // sustain pedal
			poly_sustain(ch, val);
			break;
	}
}

static void poly_panic(void) {
	voice_slot_init(&voice_state, voice_state.mode, voice_state.count);
	reset();
}


////////////////////////////////////////////////////////////////////////////////
///// mono behavior (standard)

static void mono_note_on(u8 ch, u8 num, u8 vel) {
	if (num > MIDI_NOTE_MAX)
		// drop notes outside CV range
		// FIXME: update for ansible
		return;

	// keep track of held notes for legato and pitch bend
	notes_hold(&notes[0], num, vel);
	set_cv_pitch(MONO_PITCH_CV, num, pitch_offset[0]);
	set_cv_velocity(MONO_VELOCITY_CV, vel);
	update_dacs(aout);
	set_tr(TR1);
}

static void mono_note_off(u8 ch, u8 num, u8 vel) {
	const held_note_t *prior;
	
	if (num > MIDI_NOTE_MAX)
		// drop notes outside CV range
		return;

	if (flags[0].sustain == 0) {
		notes_release(&notes[0], num);
		prior = notes_get(&notes[0], kNotePriorityLast);
		if (prior) {
			set_cv_pitch(MONO_PITCH_CV, prior->num, pitch_offset[0]);
			set_cv_velocity(MONO_VELOCITY_CV, prior->vel);
			update_dacs(aout);
		}
		else {
			clr_tr(TR1);
		}
	}
}

static void mono_pitch_bend(u8 ch, u16 bend) {
	// MAINT: assuming bend is always 14 bit...
	if (bend == MIDI_BEND_ZERO) {
		pitch_offset[0] = 0;
	}
	else if (bend > MIDI_BEND_ZERO) {
		bend -= MIDI_BEND_ZERO;
		pitch_offset[0] = BEND1[bend >> 4];
	}
	else {
		bend = MIDI_BEND_ZERO - bend - 1;
		pitch_offset[0] = -BEND1[bend >> 4];
	}

	// re-set pitch to pick up changed offset
	const held_note_t *active = notes_get(&(notes[0]), kNotePriorityLast);
	if (active) {
		set_cv_pitch(MONO_PITCH_CV, active->num, pitch_offset[0]);
		update_dacs(aout);
	}
}

static void mono_sustain(u8 ch, u8 val) {
	if (val < 64) {
		clr_tr(TR2);
		flags[0].sustain = 0;
		// release any held notes
		notes_init(&notes[0]);
		clr_tr(TR1);
	}
	else {
		set_tr(TR2);
		flags[0].sustain = 1;
	}
}

static void mono_channel_pressure(u8 ch, u8 val) {
	set_cv_cc(MONO_PRESSURE_CV, val);
	update_dacs(aout);
}

static void mono_generic(u8 ch, u8 val) {
	(val < 64) ? clr_tr(TR3) : set_tr(TR3);
}

static void mono_control_change(u8 ch, u8 num, u8 val) {
		switch (num) {
		case 1:  // mod wheel
			set_cv_cc(MONO_MOD_CV, val);
			update_dacs(aout);
			break;
		case 64:  // sustain pedal
			mono_sustain(ch, val);
			break;
		case 80:  // generic
			mono_generic(ch, val);
		default:
			break;
	}
}

static void mono_rt_tick(void) {
	static u32 previous = 0;
	u32 now, average;

	if (key_state.normaled) {
		// external sync on;
		return;
	}

	now = time_now();
	if (previous == 0) {
		previous = now;
	}
	// smooth it? given all the polling tempo changes jump around
	average = (now + previous) >> 1;
	previous = now;

	midi_clock_pulse(&midi_clock, average);
	time_clear();

	//print_dbg("\r\n mono_rt_tick: ");
	//print_dbg_ulong(midi_clock.trigger);
	//print_dbg(" p: ");
	//print_dbg_ulong(midi_clock.pulse_period);

	if (midi_clock.trigger) {
		set_tr(TR4);
	}
	else {
		clr_tr(TR4);
	}
}

static void mono_rt_start(void) {
	print_dbg("\r\n mono_rt_start()");
	midi_clock_start(&midi_clock);
}

static void mono_rt_stop(void) {
	print_dbg("\r\n mono_rt_stop()");
	midi_clock_stop(&midi_clock);
	clr_tr(TR4);
}

static void mono_rt_continue(void) {
	print_dbg("\r\n mono_rt_continue()");
	midi_clock_continue(&midi_clock);
}

static void mono_panic(void) {
	midi_clock_stop(&midi_clock);
	reset();
}


////////////////////////////////////////////////////////////////////////////////
///// multi behavior (standard)

inline static void multi_tr_set(u8 ch) {
	switch (ch) {
	case 0:
		set_tr(TR1);
		break;
	case 1:
		set_tr(TR2);
		break;
	case 2:
		set_tr(TR3);
		break;
	case 3:
		set_tr(TR4);
		break;
	}
}

inline static void multi_tr_clr(u8 ch) {
	switch (ch) {
	case 0:
		clr_tr(TR1);
		break;
	case 1:
		clr_tr(TR2);
		break;
	case 2:
		clr_tr(TR3);
		break;
	case 3:
		clr_tr(TR4);
		break;
	}
}

static void multi_note_on(u8 ch, u8 num, u8 vel) {
	if (ch > 3 || num > MIDI_NOTE_MAX)
		return;

	notes_hold(&notes[ch], num, vel);
	set_cv_pitch(&(aout[ch]), num, pitch_offset[ch]);
	update_dacs(aout);
	multi_tr_set(ch);
}

static void multi_note_off(u8 ch, u8 num, u8 vel) {
	const held_note_t *prior;

	if (ch > 3 || num > MIDI_NOTE_MAX)
		return;

	if (flags[ch].sustain == 0) {
		notes_release(&notes[ch], num);
		if (flags[ch].legato) {
			prior = notes_get(&notes[ch], kNotePriorityLast);
			if (prior) {
				set_cv_pitch(&(aout[ch]), prior->num, pitch_offset[ch]);
				update_dacs(aout);
			}
			else {
				multi_tr_clr(ch);
			}
		}
		else {
			// legato is off
			multi_tr_clr(ch);
		}
	}
}

static void multi_pitch_bend(u8 ch, u16 bend) {
	if (ch > 3)
		return;

	// MAINT: assuming bend is always 14 bit...
	if (bend == MIDI_BEND_ZERO) {
		pitch_offset[ch] = 0;
	}
	else if (bend > MIDI_BEND_ZERO) {
		bend -= MIDI_BEND_ZERO;
		pitch_offset[ch] = BEND1[bend >> 4];
	}
	else {
		bend = MIDI_BEND_ZERO - bend - 1;
		pitch_offset[ch] = -BEND1[bend >> 4];
	}

	// re-set pitch to pick up changed offset
	const held_note_t *active = notes_get(&(notes[ch]), kNotePriorityLast);
	if (active) {
		set_cv_pitch(&(aout[ch]), active->num, pitch_offset[ch]);
		update_dacs(aout);
	}
}

static void multi_sustain(u8 ch, u8 val) {
	if (ch > 3)
		return;

	if (val < 64) {
		multi_tr_clr(ch);
		flags[ch].sustain = 0;
		if (flags[ch].legato) {
			notes_init(&(notes[ch]));
		}
	}
	else {
		flags[ch].sustain = 1;
	}
}

static void multi_control_change(u8 ch, u8 num, u8 val) {
	if (ch > 3)
		return;

	switch (num) {
		case 64:  // sustain pedal
			multi_sustain(ch, val);
			break;
		default:
			break;
	}
}


////////////////////////////////////////////////////////////////////////////////
///// fixed behavior (standard)

static void fixed_write_mapping(const fixed_mapping_t *dest, const fixed_mapping_t *src) {
	for (u8 i = 0; i < 4; i++) {
		flashc_memset8((void*)&(dest->notes[i]), src->notes[i], 1, true);
		flashc_memset8((void*)&(dest->cc[i]), src->cc[i], 1, true);
	}
}

static void fixed_start_learning(void) {
	print_dbg("\r\n standard: start fixed mode learn");
	fixed_learn.learning = 1;
	fixed_learn.note_idx = 0;
	fixed_learn.cc_idx = 0;
	for (u8 i = 0; i < 4; i++) {
		multi_tr_clr(i);
		aout[i] = 0;
	}
	update_dacs(aout);
}

static bool fixed_finalize_learning(bool cancel) {
	if (cancel || (fixed_learn.note_idx == 4 && fixed_learn.cc_idx == 4)) {
		print_dbg("\r\n standard: fixed learning complete");
		fixed_learn.learning = 0;
		// clear all tr and cv
		for (u8 i = 0; i < 4; i++) {
			multi_tr_clr(i);
			aout[i] = 0;
			print_dbg("\r\n n: ");
			print_dbg_ulong(standard_state.fixed.notes[i]);
			print_dbg(" cc: ");
			print_dbg_ulong(standard_state.fixed.cc[i]);
		}
		update_dacs(aout);

		if (!cancel) {
			fixed_write_mapping(&(f.midi_standard_state.fixed), &(standard_state.fixed));
		}
		return true;
	}

	// learning not finalized
	return false;
}

static void fixed_note_on(u8 ch, u8 num, u8 vel) {
	static bool initial_set = false;

	if (fixed_learn.learning) {
		if (fixed_learn.note_idx < 4) {
			if (fixed_learn.note_idx == 0 && initial_set == false) {
				initial_set = true;
				standard_state.fixed.notes[fixed_learn.note_idx] = num;
				multi_tr_set(fixed_learn.note_idx); // indicate mapping
				//print_dbg("\n\r fln: ");
				//print_dbg_ulong(fixed_learn.note_idx);
			}

			// different note; advance to next mapping
			if (num != standard_state.fixed.notes[fixed_learn.note_idx]) {
				fixed_learn.note_idx++;
				if (fixed_learn.note_idx < 4) {
					standard_state.fixed.notes[fixed_learn.note_idx] = num;
					multi_tr_set(fixed_learn.note_idx); // indicate mapping
					//print_dbg("\n\r fln: ");
					//print_dbg_ulong(fixed_learn.note_idx);
					if (fixed_learn.note_idx == 3) {
						fixed_learn.note_idx++;
					}
				}
			}
		}

		if (fixed_finalize_learning(false)) {
			// prep for next learning pass
			initial_set = false;
		}
	}
	else {
		for (u8 i = 0; i < 4; i++) {
			if (standard_state.fixed.notes[i] == num) {
				multi_tr_set(i);
				// TODO: should these be gates or triggers?
				break;
			}
		}
	}
}

static void fixed_note_off(u8 ch, u8 num, u8 vel) {
	if (!fixed_learn.learning) {
		for (u8 i = 0; i < 4; i++) {
			if (standard_state.fixed.notes[i] == num) {
				multi_tr_clr(i);
				break;
			}
		}
	}
}

static void fixed_control_change(u8 ch, u8 num, u8 val) {
	static bool initial_set = false;
	
	if (fixed_learn.learning) {
		if (fixed_learn.cc_idx < 4) {
			// initial state; first num always sets first channel
			if (fixed_learn.cc_idx == 0 && initial_set == false) {
				initial_set = true;
				standard_state.fixed.cc[fixed_learn.cc_idx] = num;
				//print_dbg("\n\r cc learn: assigned cc: ");
				//print_dbg_ulong(num);
				//print_dbg(" for ch: ");
				//print_dbg_ulong(fixed_learn.cc_idx);
			}

			// different cc; advance to next mapping
			if (num != standard_state.fixed.cc[fixed_learn.cc_idx]) {
				fixed_learn.cc_idx++;
				if (fixed_learn.cc_idx < 4) {
					standard_state.fixed.cc[fixed_learn.cc_idx] = num;
					//print_dbg("\n\r cc learn: assigned cc: ");
					//print_dbg_ulong(num);
					//print_dbg(" for ch: ");
					//print_dbg_ulong(fixed_learn.cc_idx);
					if (fixed_learn.cc_idx == 3) {
						// immediately end learning instead of waiting around for
						// a different cc to be sent..
						fixed_learn.cc_idx++;
					}
				}
			}
			// update outputs to provide feedback
			if (fixed_learn.cc_idx < 4) {
				aout[fixed_learn.cc_idx] = val << 5;
				update_dacs(aout);
			}
			//print_dbg("\n\r flcc: ");
			//print_dbg_ulong(fixed_learn.cc_idx);
		}
		if (fixed_finalize_learning(false)) {
			// prepare for next learning pass
			initial_set = false;
		}
	}
	else {
		for (u8 i = 0; i < 4; i++) {
			if (standard_state.fixed.cc[i] == num) {
				aout[i] = val << 5;
				update_dacs(aout);
				break;
			}
		}
	}
}



////////////////////////////////////////////////////////////////////////////////

void default_midi_arp() {
	flashc_memset32((void*)&(f.midi_arp_state.clock_period), 100, 4, true);
}

void clock_midi_arp(uint8_t phase) {
	if(phase)
		set_tr(TR1);
	else
		clr_tr(TR1);
}

void ii_midi_arp(uint8_t *d, uint8_t l) {
	;;
}

void handler_ArpKey(s32 data) { 
	print_dbg("\r\n> arp key ");
	print_dbg_ulong(data);
}

void handler_ArpTr(s32 data) { 
	print_dbg("\r\n> arp tr ");
	print_dbg_ulong(data);
}

void handler_ArpTrNormal(s32 data) { 
	print_dbg("\r\n> arp tr normal ");
	print_dbg_ulong(data);
}

void handler_ArpMidiPacket(s32 data) {
	print_dbg("\r\n> arp midi packet");
	//midi_packet_parse(&midi_behavior_arp, (u32)data);
}
