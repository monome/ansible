#include "print_funcs.h"
#include "flashc.h"

#include "monome.h"
#include "i2c.h"

#include "main.h"
#include "ansible_midi.h"


void set_mode_midi(void) {
	switch(f.state.mode) {
	case mMidiStandard:
		print_dbg("\r\n> mode midi standard");
		app_event_handlers[kEventKey] = &handler_StandardKey;
		app_event_handlers[kEventTr] = &handler_StandardTr;
		app_event_handlers[kEventTrNormal] = &handler_StandardTrNormal;
		clock = &clock_midi_standard;
		clock_set(f.midi_standard_state.clock_period);
		process_ii = &ii_midi_standard;
		update_leds(1);
		break;
	case mMidiArp:
		print_dbg("\r\n> mode midi arp");
		app_event_handlers[kEventKey] = &handler_ArpKey;
		app_event_handlers[kEventTr] = &handler_ArpTr;
		app_event_handlers[kEventTrNormal] = &handler_ArpTrNormal;
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

void default_midi_standard() {
	flashc_memset32((void*)&(f.midi_standard_state.clock_period), 100, 4, true);
}

void clock_midi_standard(uint8_t phase) {
	if(phase)
		set_tr(TR1);
	else
		clr_tr(TR1);
}

void ii_midi_standard(uint8_t *d, uint8_t l) {
	;;
}

void handler_StandardKey(s32 data) { 
	print_dbg("\r\n> standard key");
	print_dbg_ulong(data);
}

void handler_StandardTr(s32 data) { 
	print_dbg("\r\n> standard tr");
	print_dbg_ulong(data);
}

void handler_StandardTrNormal(s32 data) { 
	print_dbg("\r\n> standard tr normal ");
	print_dbg_ulong(data);
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