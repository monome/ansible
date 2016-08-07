// libavr32
#include "print_funcs.h"
#include "flashc.h"

#include "midi_common.h"
#include "notes.h"

#include "monome.h"
#include "i2c.h"

// this
#include "main.h"
#include "ansible_midi.h"

#define MIDI_NOTE_MAX 120

#define MONO_PITCH_CV &(aout[0])
#define MONO_VELOCITY_CV &(aout[1])
#define MONO_MOD_CV &(aout[2])
#define MONO_BEND_CV &(aout[3])


//------------------------------
//------ prototypes

static void set_cv_pitch(uint16_t *cv, u8 num);
static void set_cv_velocity(uint16_t *cv, u8 vel);
static void set_cv_cc(uint16_t *cv, u8 value);

static void set_voice_allocation(voicing_mode v);

static void poly_note_on(u8 ch, u8 num, u8 vel);
static void poly_note_off(u8 ch, u8 num, u8 vel);
static void poly_pitch_bend(u8 ch, u16 bend);
static void poly_sustain(u8 ch, u8 val);
static void poly_control_change(u8 ch, u8 num, u8 val);

static void mono_note_on(u8 ch, u8 num, u8 vel);
static void mono_note_off(u8 ch, u8 num, u8 vel);
static void mono_pitch_bend(u8 ch, u16 bend);
static void mono_sustain(u8 ch, u8 val);
static void mono_control_change(u8 ch, u8 num, u8 val);

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
static uint16_t aout[4];

// TODO: move to main.c?
typedef struct {
	u8 key1 : 1;
	u8 key2 : 1;
	u8 front : 1;
	u8 normaled : 1; // isn't this tracked elsewhere?
} key_state_t;

static key_state_t key_state;

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

	key_state.key1 = key_state.key2 = key_state.front = 0;
	
	aout[0] = aout[1] = aout[2] = aout[3] = 0;
	update_dacs(aout);
}


void handler_MidiFrontShort(s32 data) {
	print_dbg("\r\n midi front short: ");
	print_dbg_ulong(data);
	if (standard_state.voicing == eVoiceFixed && key_state.key2) {
		fixed_start_learning();
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

static void set_cv_pitch(uint16_t *cv, u8 num) {
	*cv = SEMI[num];
}

static void set_cv_velocity(uint16_t *cv, u8 vel) {
	// for the moment, straight mapping
	*cv = vel << 5;
}

static inline void set_cv_cc(uint16_t *cv, u8 value) {
	// 128 << 5 == 4096; 12-bit dac
	*cv = value << 5;
}


static void set_voice_allocation(voicing_mode v) {
	// start from a clean slate
	for (int i = 0; i < 4; i++) {
		voice_flags_init(&(flags[i]));
		notes_init(&(notes[i]));
		aout[i] = false;
	}
	update_dacs(aout);
	clr_tr(TR1);
	clr_tr(TR2);
	clr_tr(TR3);
	clr_tr(TR4);

	// config behavior, clock, flags, etc.
	switch (v) {
	case eVoicePoly:
		active_behavior.note_on = &poly_note_on;
		active_behavior.note_off = &poly_note_off;
		active_behavior.channel_pressure = NULL;
		active_behavior.pitch_bend = &poly_pitch_bend;
		active_behavior.control_change = &poly_control_change;
		voice_slot_init(&voice_state, kVoiceAllocRotate, 4); // TODO: count configurable?
		clock = &clock_null;
		print_dbg("\r\n standard: voice poly");
		break;
	case eVoiceMono:
		active_behavior.note_on = &mono_note_on;
		active_behavior.note_off = &mono_note_off;
		active_behavior.channel_pressure = NULL;
		active_behavior.pitch_bend = &mono_pitch_bend;
		active_behavior.control_change = &mono_control_change;
		clock = &clock_midi_standard;
		flags[0].legato = 1;
		print_dbg("\r\n standard: voice mono");
		break;
	case eVoiceMulti:
		active_behavior.note_on = &multi_note_on;
		active_behavior.note_off = &multi_note_off;
		active_behavior.channel_pressure = NULL;
		active_behavior.pitch_bend = &multi_pitch_bend;
		active_behavior.control_change = &multi_control_change;
		clock = &clock_null;
		print_dbg("\r\n standard: voice multi");
		break;
	case eVoiceFixed:
		active_behavior.note_on = &fixed_note_on;
		active_behavior.note_off = &fixed_note_off;
		active_behavior.channel_pressure = NULL;
		active_behavior.pitch_bend = NULL;
		active_behavior.control_change = &fixed_control_change;
		clock = &clock_null;
		print_dbg("\r\n standard: voice fixed");
		break;
	default:
		print_dbg("\r\n standard: bad voice mode");
	}
}


////////////////////////////////////////////////////////////////////////////////
///// handlers (standard)

void default_midi_standard() {
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
			print_dbg("; canceling fixed mapping learning");
			fixed_finalize_learning(true);
		}
		else {
			// TODO: panic, all notes off
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
			print_dbg("; noop, in learning mode");
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
}

void handler_StandardMidiPacket(s32 data) {
	midi_packet_parse(&active_behavior, (u32)data);
}

////////////////////////////////////////////////////////////////////////////////
///// poly behavior (standard)

static void poly_note_on(u8 ch, u8 num, u8 vel) {
	u8 slot;
	// get next slot
	slot = voice_slot_next(&voice_state);
	//ch = voice_slot_num(&voice_state, slot);
	// if slot is active; steal slot, perform note off
	if (voice_slot_active(&voice_state, slot)) {
		// TODO: check for sustain
		multi_tr_clr(slot);
		// FIXME: here we clear tr but then immediately reset it for note
		// on so the envelop wont retrigger. need some way to queue or
		// pause long enough to have tr retrigger.
		voice_slot_release(&voice_state, slot);
	}
	// preform note on for the slot's channel
	multi_note_on(slot, num, vel);
	// mark slot as active
	voice_slot_activate(&voice_state, slot, num);
}

static void poly_note_off(u8 ch, u8 num, u8 vel) {
	s8 slot;

	// find slot allocated for note num (if any)
	slot = voice_slot_find(&voice_state, num);
	if (slot != -1) {
		// TODO: check for sustain
		multi_tr_clr(slot);
		voice_slot_release(&voice_state, slot);
	}
}

static void poly_pitch_bend(u8 ch, u16 bend) {
	// bend all active slots/voices
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


////////////////////////////////////////////////////////////////////////////////
///// mono behavior (standard)

static void mono_note_on(u8 ch, u8 num, u8 vel) {
	if (num > MIDI_NOTE_MAX)
		// drop notes outside CV range
		// FIXME: update for ansible
		return;

	// keep track of held notes for legato
	notes_hold(&notes[0], num, vel);
	set_cv_pitch(MONO_PITCH_CV, num);
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
			set_cv_pitch(MONO_PITCH_CV, prior->num);
			set_cv_velocity(MONO_VELOCITY_CV, prior->vel);
			update_dacs(aout);
		}
		else {
			clr_tr(TR1);
		}
	}
}

static void mono_pitch_bend(u8 ch, u16 bend) {
}

static void mono_sustain(u8 ch, u8 val) {
	if (val < 64) {
		notes_init(&notes[0]);
		clr_tr(TR1);
		flags[0].sustain = 0;
	}
	else {
		flags[0].sustain = 1;
	}
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
		default:
			break;
	}
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

	// TODO: legato

	set_cv_cc(&(aout[ch]), num);
	update_dacs(aout);
	multi_tr_set(ch);
}

static void multi_note_off(u8 ch, u8 num, u8 vel) {
	if (ch > 3 || num > MIDI_NOTE_MAX)
		return;

	// TODO: legato?

	if (flags[ch].sustain == 0) {
		multi_tr_clr(ch);
	}
}

static void multi_pitch_bend(u8 ch, u16 bend) {
	if (ch > 3)
		return;
}

static void multi_sustain(u8 ch, u8 val) {
	if (ch > 3)
		return;

	if (val < 64) {
		multi_tr_clr(ch);
		flags[ch].sustain = 0;
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
