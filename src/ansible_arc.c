#include "print_funcs.h"
#include "flashc.h"

#include "monome.h"
#include "i2c.h"

#include "main.h"
#include "ansible_arc.h"


void set_mode_arc(void) {
	switch(f.state.mode) {
	case mArcLevels:
		print_dbg("\r\n> mode arc levels");
		app_event_handlers[kEventKey] = &handler_LevelsKey;
		app_event_handlers[kEventTr] = &handler_LevelsTr;
		app_event_handlers[kEventTrNormal] = &handler_LevelsTrNormal;
		app_event_handlers[kEventMonomeRingEnc] = &handler_LevelsEnc;
		app_event_handlers[kEventMonomeRefresh] = &handler_LevelsRefresh;
		clock = &clock_levels;
		clock_set(f.levels_state.clock_period);
		process_ii = &ii_levels;
		update_leds(1);
		break;
	case mArcCycles:
		print_dbg("\r\n> mode arc cycles");
		app_event_handlers[kEventKey] = &handler_CyclesKey;
		app_event_handlers[kEventTr] = &handler_CyclesTr;
		app_event_handlers[kEventTrNormal] = &handler_CyclesTrNormal;
		app_event_handlers[kEventMonomeRingEnc] = &handler_CyclesEnc;
		app_event_handlers[kEventMonomeRefresh] = &handler_CyclesRefresh;
		clock = &clock_cycles;
		clock_set(f.cycles_state.clock_period);
		process_ii = &ii_cycles;
		update_leds(2);
		break;
	default:
		break;
	}
	
	if(connected == conARC) {
		app_event_handlers[kEventFrontShort] = &handler_ArcFrontShort;
		app_event_handlers[kEventFrontLong] = &handler_ArcFrontLong;
	}

	flashc_memset32((void*)&(f.state.none_mode), f.state.mode, 4, true);
	flashc_memset32((void*)&(f.state.arc_mode), f.state.mode, 4, true);
}


void handler_ArcFrontShort(s32 data) {
	print_dbg("\r\n> PRESET");
}

void handler_ArcFrontLong(s32 data) {
	if(f.state.mode == mArcLevels)
		set_mode(mArcCycles);
	else
		set_mode(mArcLevels);
}

////////////////////////////////////////////////////////////////////////////////


void default_levels() {
	flashc_memset32((void*)&(f.levels_state.clock_period), 100, 4, true);
}

void clock_levels(uint8_t phase) {
	if(phase)
		set_tr(TR1);
	else
		clr_tr(TR1);
}

void ii_levels(uint8_t *d, uint8_t l) {
	;;
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
	if(monomeFrameDirty) {
		////
		for(uint8_t i1=0;i1<16;i1++) {
			monomeLedBuffer[i1] = 0;
			monomeLedBuffer[16+i1] = 0;
			monomeLedBuffer[32+i1] = 4;
			monomeLedBuffer[48+i1] = 0;
		}

		monomeLedBuffer[0] = 15;

		////
		monome_set_quadrant_flag(0);
		monome_set_quadrant_flag(1);
		(*monome_refresh)();
	}
}

void handler_LevelsKey(s32 data) { 
	print_dbg("\r\n> levels key ");
	print_dbg_ulong(data);
}

void handler_LevelsTr(s32 data) { 
	print_dbg("\r\n> levels tr ");
	print_dbg_ulong(data);
}

void handler_LevelsTrNormal(s32 data) { 
	print_dbg("\r\n> levels tr normal ");
	print_dbg_ulong(data);
}


////////////////////////////////////////////////////////////////////////////////

void default_cycles() {
	flashc_memset32((void*)&(f.cycles_state.clock_period), 50, 4, true);
}

void clock_cycles(uint8_t phase) {
	if(phase)
		set_tr(TR1);
	else
		clr_tr(TR1);
}

void ii_cycles(uint8_t *d, uint8_t l) {
	;;
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
	if(monomeFrameDirty) {
		////
		for(uint8_t i1=0;i1<16;i1++) {
			monomeLedBuffer[i1] = 0;
			monomeLedBuffer[16+i1] = 0;
			monomeLedBuffer[32+i1] = 4;
			monomeLedBuffer[48+i1] = 0;
		}

		monomeLedBuffer[0] = 15;

		////
		monome_set_quadrant_flag(0);
		monome_set_quadrant_flag(1);
		(*monome_refresh)();
	}
}

void handler_CyclesKey(s32 data) { 
	print_dbg("\r\n> cycles key ");
	print_dbg_ulong(data);
}

void handler_CyclesTr(s32 data) { 
	print_dbg("\r\n> cycles tr ");
	print_dbg_ulong(data);
}

void handler_CyclesTrNormal(s32 data) { 
	print_dbg("\r\n> cycles tr normal ");
	print_dbg_ulong(data);
}