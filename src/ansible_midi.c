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

static void multi_note_on(u8 ch, u8 num, u8 vel);
static void multi_note_off(u8 ch, u8 num, u8 vel);
static void multi_pitch_bend(u8 ch, u16 bend);
static void multi_sustain(u8 ch, u8 val);
static void multi_control_change(u8 ch, u8 num, u8 val);

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

static midi_standard_state_t standard_state;
static midi_arp_state_t *arp_state = NULL;

static midi_behavior_t active_behavior = {
	.note_on = NULL,
	.note_off = NULL,
	.channel_pressure = NULL,
	.pitch_bend = NULL,
	.control_change = NULL
};




static u8 sustain_active = 0; // TODO
static uint16_t aout[4];


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
		clock = &clock_midi_standard; //REMOVE?
		clock_set(f.midi_standard_state.clock_period);
		process_ii = &ii_midi_standard;
		notes_init();
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

	aout[0] = aout[1] = aout[2] = aout[3] = 0;
	update_dacs(aout);
}


void handler_MidiFrontShort(s32 data) {
	;;
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











////////////////////////////////////////////////////////////////////////////////
///// handlers (standard)

void default_midi_standard() {
	flashc_memset32((void*)&(f.midi_standard_state.clock_period), 100, 4, true);
	flashc_memset8((void*)&(f.midi_standard_state.voicing), eVoicePoly, 1, true);
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
	// print_dbg("\r\n> standard key");
	// print_dbg_ulong(data);

	if (data == 1) { // key 1 press
		// TODO: panic, all notes off
		print_dbg("\r\n standard: panic");
	}
	else if (data == 3) {   // key 2 press		
		standard_state.voicing++;
		if (standard_state.voicing >= eVoiceMAX) {
			standard_state.voicing = eVoicePoly;
		}

		switch (standard_state.voicing) {
		case eVoicePoly:
			active_behavior.note_on = &poly_note_on;
			active_behavior.note_off = &poly_note_off;
			active_behavior.channel_pressure = NULL;
			active_behavior.pitch_bend = &poly_pitch_bend;
			active_behavior.control_change = &poly_control_change;
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
		
		// start from a clean slate
		aout[0] = aout[1] = aout[2] = aout[3] = 0;
		update_dacs(aout);
		clr_tr(TR1);
		clr_tr(TR2);
		clr_tr(TR3);
		clr_tr(TR4);
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
	// print_dbg("\r\n> standard midi packet");
	midi_packet_parse(&active_behavior, (u32)data);
}

////////////////////////////////////////////////////////////////////////////////
///// poly behavior (standard)

static void poly_note_on(u8 ch, u8 num, u8 vel) {
}

static void poly_note_off(u8 ch, u8 num, u8 vel) {
}

static void poly_pitch_bend(u8 ch, u16 bend) {
}

static void poly_sustain(u8 ch, u8 val) {
}

static void poly_control_change(u8 ch, u8 num, u8 val) {
}


////////////////////////////////////////////////////////////////////////////////
///// mono behavior (standard)

static void mono_note_on(u8 ch, u8 num, u8 vel) {
	if (num > MIDI_NOTE_MAX)
		// drop notes outside CV range
		// FIXME: update for ansible
		return;

	// keep track of held notes for legato
	notes_hold(num, vel);
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

	if (sustain_active == 0) {
		notes_release(num);
		prior = notes_get(kNotePriorityLast);
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

static void multi_note_on(u8 ch, u8 num, u8 vel) {
	if (ch > 3 || num > MIDI_NOTE_MAX)
		return;

	// TODO: legato
	
	set_cv_cc(&(aout[ch]), num);
	update_dacs(aout);
	
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

static void multi_note_off(u8 ch, u8 num, u8 vel) {
	if (ch > 3 || num > MIDI_NOTE_MAX)
		return;

	// TODO: legato and sustain
	
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

static void multi_pitch_bend(u8 ch, u16 bend) {
	if (ch > 3)
		return;
}

static void multi_sustain(u8 ch, u8 val) {
	if (ch > 3)
		return;
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

static void fixed_note_on(u8 ch, u8 num, u8 vel) {
}

static void fixed_note_off(u8 ch, u8 num, u8 vel) {
}

static void fixed_control_change(u8 ch, u8 num, u8 val) {
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
