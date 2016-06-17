#include "print_funcs.h"

#include "monome.h"

#include "main.h"
#include "ansible_grid.h"




// handle grid keys
// handle key and tr inputs
// clock
// refresh grid
// ii

// dynamically change redraw/key handlers per screen and sub-mode


void set_mode_grid(void) {
	switch(ansible_state.mode) {
	case mGridKria:
		print_dbg("\r\n> mode grid kria");
		app_event_handlers[kEventKey] = &handler_KriaKey;
		app_event_handlers[kEventMonomeGridKey] = &handler_KriaGridKey;
		app_event_handlers[kEventMonomeRefresh] = &handler_KriaRefresh;
		break;
	case mGridMP:
		print_dbg("\r\n> mode grid mp");
		app_event_handlers[kEventKey] = &handler_MPKey;
		app_event_handlers[kEventMonomeGridKey] = &handler_MPGridKey;
		app_event_handlers[kEventMonomeRefresh] = &handler_MPRefresh;
		break;
	default:
		break;
	}
	
	if(ansible_state.connected == conGRID) {
		app_event_handlers[kEventFrontShort] = &handler_GridFrontShort;
		app_event_handlers[kEventFrontLong] = &handler_GridFrontLong;
	}
}


void handler_GridFrontShort(s32 data) {
	print_dbg("\r\n> PRESET");
}

void handler_GridFrontLong(s32 data) {
	if(ansible_state.mode == mGridKria)
		ansible_state.mode = mGridMP;
	else
		ansible_state.mode = mGridKria;

	ansible_state.grid_mode = ansible_state.mode;

	set_mode_grid();
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

}

void handler_KriaKey(s32 data) { 
	print_dbg("\r\n> kria key");
	print_dbg_ulong(data);
}

void handler_MPGridKey(s32 data) { 
	u8 x, y, z;
	monome_grid_key_parse_event_data(data, &x, &y, &z);

	print_dbg("\r\n MP grid key");

	// monomeFrameDirty++;
}

void handler_MPRefresh(s32 data) { 

}

void handler_MPKey(s32 data) { 
	print_dbg("\r\n> MP key ");
	print_dbg_ulong(data);
}