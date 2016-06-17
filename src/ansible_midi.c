#include "print_funcs.h"

#include "monome.h"

#include "main.h"
#include "ansible_midi.h"


void set_mode_midi(void) {
	switch(ansible_state.mode) {
	case mMidiStandard:
		print_dbg("\r\n> mode midi standard");
		app_event_handlers[kEventKey] = &handler_StandardKey;
		break;
	case mMidiArp:
		print_dbg("\r\n> mode midi arp");
		app_event_handlers[kEventKey] = &handler_ArpKey;
		break;
	default:
		break;
	}
	
	if(ansible_state.connected == conMIDI) {
		app_event_handlers[kEventFrontShort] = &handler_MidiFrontShort;
		app_event_handlers[kEventFrontLong] = &handler_MidiFrontLong;
	}
}



void handler_MidiFrontShort(s32 data) {
	;;
}

void handler_MidiFrontLong(s32 data) {
	if(ansible_state.mode == mMidiStandard)
		ansible_state.mode = mMidiArp;
	else
		ansible_state.mode = mMidiStandard;

	ansible_state.midi_mode = ansible_state.mode;

	set_mode_midi();
}



void handler_StandardKey(s32 data) { 
	print_dbg("\r\n> standard key");
	print_dbg_ulong(data);
}


void handler_ArpKey(s32 data) { 
	print_dbg("\r\n> arp key ");
	print_dbg_ulong(data);
}