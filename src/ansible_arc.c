#include "print_funcs.h"
#include "monome.h"

#include "ansible_arc.h"
#include "main.h"


void set_mode_arc(void) {
	switch(ansible_state.mode) {
	case mArcLevels:
		print_dbg("\r\n> mode arc levels");
		app_event_handlers[kEventKey] = &handler_LevelsKey;
		app_event_handlers[kEventMonomeRingEnc] = &handler_LevelsEnc;
		app_event_handlers[kEventMonomeRefresh] = &handler_LevelsRefresh;
		break;
	case mArcCycles:
		print_dbg("\r\n> mode arc cycles");
		app_event_handlers[kEventKey] = &handler_CyclesKey;
		app_event_handlers[kEventMonomeRingEnc] = &handler_CyclesEnc;
		app_event_handlers[kEventMonomeRefresh] = &handler_CyclesRefresh;
		break;
	default:
		break;
	}
	
	if(ansible_state.connected == conARC) {
		app_event_handlers[kEventFrontShort] = &handler_ArcFrontShort;
		app_event_handlers[kEventFrontLong] = &handler_ArcFrontLong;
	}
}


void handler_ArcFrontShort(s32 data) {
	print_dbg("\r\n> PRESET");
}

void handler_ArcFrontLong(s32 data) {
	if(ansible_state.mode == mArcLevels)
		ansible_state.mode = mArcCycles;
	else
		ansible_state.mode = mArcLevels;

	ansible_state.arc_mode = ansible_state.mode;

	set_mode_arc();
}


void handler_LevelsEnc(s32 data) { 
	uint8_t n;
	int8_t delta;

	monome_ring_enc_parse_event_data(data, &n, &delta);

	print_dbg("\r\n levels enc \tn: "); 
	print_dbg_ulong(n); 
	print_dbg("\t delta: "); 
	print_dbg_hex(delta);
}


void handler_LevelsRefresh(s32 data) { 

}

void handler_LevelsKey(s32 data) { 
	print_dbg("\r\n> levels key ");
	print_dbg_ulong(data);
}


void handler_CyclesEnc(s32 data) { 
	uint8_t n;
	int8_t delta;

	monome_ring_enc_parse_event_data(data, &n, &delta);

	print_dbg("\r\n cycles enc \tn: "); 
	print_dbg_ulong(n); 
	print_dbg("\t delta: "); 
	print_dbg_hex(delta);
}


void handler_CyclesRefresh(s32 data) { 

}

void handler_CyclesKey(s32 data) { 
	print_dbg("\r\n> cycles key ");
	print_dbg_ulong(data);
}