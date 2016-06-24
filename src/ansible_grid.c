#include "print_funcs.h"
#include "flashc.h"

#include "monome.h"
#include "i2c.h"

#include "main.h"
#include "ansible_grid.h"


void set_mode_grid() {
	switch(f.state.mode) {
	case mGridKria:
		print_dbg("\r\n> mode grid kria");
		app_event_handlers[kEventKey] = &handler_KriaKey;
		app_event_handlers[kEventTr] = &handler_KriaTr;
		app_event_handlers[kEventTrNormal] = &handler_KriaTrNormal;
		app_event_handlers[kEventMonomeGridKey] = &handler_KriaGridKey;
		app_event_handlers[kEventMonomeRefresh] = &handler_KriaRefresh;
		clock = &clock_kria;
		clock_set(f.kria_state.clock_period);
		process_ii = &ii_kria;
		update_leds(1);
		break;
	case mGridMP:
		print_dbg("\r\n> mode grid mp");
		app_event_handlers[kEventKey] = &handler_MPKey;
		app_event_handlers[kEventTr] = &handler_MPTr;
		app_event_handlers[kEventTrNormal] = &handler_MPTrNormal;
		app_event_handlers[kEventMonomeGridKey] = &handler_MPGridKey;
		app_event_handlers[kEventMonomeRefresh] = &handler_MPRefresh;
		clock = &clock_mp;
		process_ii = &ii_mp;
		update_leds(2);
		break;
	default:
		break;
	}
	
	if(connected == conGRID) {
		app_event_handlers[kEventFrontShort] = &handler_GridFrontShort;
		app_event_handlers[kEventFrontLong] = &handler_GridFrontLong;
	}

	flashc_memset32((void*)&(f.state.none_mode), f.state.mode, 4, true);
	flashc_memset32((void*)&(f.state.grid_mode), f.state.mode, 4, true);
}


void handler_GridFrontShort(s32 data) {
	print_dbg("\r\n> PRESET");
}

void handler_GridFrontLong(s32 data) {
	if(f.state.mode == mGridKria)
		set_mode(mGridMP);
	else
		set_mode(mGridKria);
}

////////////////////////////////////////////////////////////////////////////////

void default_kria() {
	flashc_memset32((void*)&(f.kria_state.clock_period), 100, 4, true);
}

void clock_kria(uint8_t phase) {
	if(phase)
		set_tr(TR1);
	else
		clr_tr(TR1);
}

void ii_kria(uint8_t i, int d) {
	;;
}


void handler_KriaGridKey(s32 data) { 
	u8 x, y, z;
	monome_grid_key_parse_event_data(data, &x, &y, &z);

	print_dbg("\r\n KRIA grid key \tx: "); 
	print_dbg_ulong(x); 
	print_dbg("\t y: "); 
	print_dbg_ulong(y); 
	print_dbg("\t z: "); 
	print_dbg_ulong(z);

	// monomeFrameDirty++;
}

void handler_KriaRefresh(s32 data) { 
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

void handler_KriaKey(s32 data) { 
	print_dbg("\r\n> kria key");
	print_dbg_ulong(data);
}

void handler_KriaTr(s32 data) { 
	print_dbg("\r\n> kria tr ");
	print_dbg_ulong(data);
}

void handler_KriaTrNormal(s32 data) { 
	print_dbg("\r\n> kria tr normal ");
	print_dbg_ulong(data);
}



////////////////////////////////////////////////////////////////////////////////

void clock_mp(uint8_t phase) {
	if(phase)
		set_tr(TR4);
	else
		clr_tr(TR4);
}

void ii_mp(uint8_t i, int d) {
	;;
}

void handler_MPGridKey(s32 data) { 
	u8 x, y, z;
	monome_grid_key_parse_event_data(data, &x, &y, &z);

	print_dbg("\r\n MP grid key");

	// monomeFrameDirty++;
}

void handler_MPRefresh(s32 data) { 
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

void handler_MPKey(s32 data) { 
	print_dbg("\r\n> MP key ");
	print_dbg_ulong(data);
}

void handler_MPTr(s32 data) { 
	print_dbg("\r\n> MP tr ");
	print_dbg_ulong(data);
}

void handler_MPTrNormal(s32 data) { 
	print_dbg("\r\n> MP tr normal ");
	print_dbg_ulong(data);
}