#include "print_funcs.h"

#include "monome.h"

#include "main.h"
#include "ansible_tt.h"


void set_mode_tt(void) {
	print_dbg("\r\n> mode tt");
	app_event_handlers[kEventKey] = &handler_TTKey;
}



void handler_TTKey(s32 data) { 
	print_dbg("\r\n> TT key");
	print_dbg_ulong(data);
}