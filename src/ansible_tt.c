#include "print_funcs.h"
#include "flashc.h"

#include "monome.h"
#include "i2c.h"
#include "dac.h"

#include "main.h"
#include "ansible_tt.h"


void set_mode_tt(void) {
	print_dbg("\r\n> mode tt");
	app_event_handlers[kEventKey] = &handler_TTKey;
	app_event_handlers[kEventTr] = &handler_TTTr;
	app_event_handlers[kEventTrNormal] = &handler_TTTrNormal;
	clock = &clock_tt;
	clock_set(50);
	process_ii = &ii_tt;
	update_leds(0);

	dac_set_slew(0,500);
}

void default_tt() {
	flashc_memset32((void*)&(f.tt_state.clock_period), 200, 4, true);
}

void clock_tt(uint8_t phase) {
	// static uint16_t d[4];
	// static uint16_t cv;
	// cv += 0xff;
	// cv &= 4095;

	// d[2] = cv;
	// d[3] = 4095 - cv;

	// update_dacs(d);
}

void ii_tt(uint8_t *d, uint8_t l) {
	print_dbg("\r\nii/tele (");
	print_dbg_ulong(l);
	print_dbg(") ");
	for(int i=0;i<l;i++) {
		print_dbg_ulong(d[i]);
		print_dbg(" ");
	}

	if(l) {
		if(d[0] == 1) {
			if(d[2]) 
				set_tr(TR1 + d[1]);
			else
				clr_tr(TR1 + d[1]);
		}

		else if(d[0] == 128) {
			ii_tx_queue(get_tr(TR1 + d[1]));
		}
	}
}


void handler_TTKey(s32 data) { 
	print_dbg("\r\n> TT key");
	print_dbg_ulong(data);

	switch(data) {
	case 0:
		// dac_set_value(0,0);
		break;
	case 1:
		// dac_set_value(0,DAC_10V);
		dac_set_value(0, 1636);
		dac_set_value(1, 1636);
		dac_set_value(2, 1636);
		dac_set_value(3, 1636);
		break;
	case 2:
		// dac_set_value_noslew(0,0);
		break;
	case 3:
		dac_set_value(0, 0);
		dac_set_value(1, 0);
		dac_set_value(2, 0);
		dac_set_value(3, 0);
		// dac_set_value_noslew(0,DAC_10V);
		break;
	default:
		break;
	}
}

void handler_TTTr(s32 data) { 
	print_dbg("\r\n> TT tr");
	print_dbg_ulong(data);
}

void handler_TTTrNormal(s32 data) { 
	print_dbg("\r\n> TT tr normal ");
	print_dbg_ulong(data);
}