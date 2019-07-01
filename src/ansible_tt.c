#include "print_funcs.h"
#include "flashc.h"

#include "monome.h"
#include "i2c.h"
#include "init_common.h"
#include "dac.h"
#include "ii.h"

#include "main.h"
#include "ansible_tt.h"

uint8_t tr_pol[4];
uint16_t tr_time[4];

bool input[4];

void set_mode_tt(void) {
	print_dbg("\r\n> mode tt");
	app_event_handlers[kEventKey] = &handler_TTKey;
	app_event_handlers[kEventTr] = &handler_TTTr;
	app_event_handlers[kEventTrNormal] = &handler_TTTrNormal;
	clock = &clock_tt;
	clock_set(f.tt_state.clock_period);
	if (!leader_mode) init_i2c_slave(f.state.i2c_addr);
	process_ii = &ii_tt;
	update_leds(0);

	clr_tr(TR1);
	clr_tr(TR2);
	clr_tr(TR3);
	clr_tr(TR4);

	dac_set_slew(0,0);
	dac_set_slew(1,0);
	dac_set_slew(2,0);
	dac_set_slew(3,0);
}

void default_tt() {
	flashc_memset32((void*)&(f.tt_state.clock_period), 500, 4, true);
}

void init_tt() {
	for(int i1=0;i1<4;i1++) {
		tr_pol[i1] = 1;
		tr_time[i1] = 100;
	}
}

void clock_tt(uint8_t phase) {
	// static uint16_t d[4];
	// static uint16_t cv;
	// cv += 0xff;
	// cv &= 4095;

	// d[2] = cv;
	// d[3] = 4095 - cv;

	// update_dacs(d);

	// if(phase)
		// set_tr(TR4);
	// else
		// clr_tr(TR4);
	;;
}


void tr_pulse0(void* o) {
	if(tr_pol[0]) clr_tr(TR1);
	else set_tr(TR1);
	timer_remove( &auxTimer[0]);
}

void tr_pulse1(void* o) {
	if(tr_pol[1]) clr_tr(TR2);
	else set_tr(TR2);
	timer_remove( &auxTimer[1]);
}

void tr_pulse2(void* o) {
	if(tr_pol[2]) clr_tr(TR3);
	else set_tr(TR3);
	timer_remove( &auxTimer[2]);
}

void tr_pulse3(void* o) {
	if(tr_pol[3]) clr_tr(TR4);
	else set_tr(TR4);
	timer_remove( &auxTimer[3]);
}


void ii_tt(uint8_t *d, uint8_t l) {
	// print_dbg("\r\ni2c:");
	// print_dbg_ulong(l);
	// print_dbg(" ");
	// for(int i=0;i<l;i++) {
	// 	print_dbg_ulong(d[i]);
	// 	print_dbg(" ");
	// }

	if(l) {
		switch(d[0]) {
		case II_ANSIBLE_TR:
			if(d[2])
				set_tr(TR1 + d[1]);
			else
				clr_tr(TR1 + d[1]);
			break;
		case II_ANSIBLE_TR + II_GET:
			ii_tx_queue(get_tr(TR1 + d[1]));
			break;
		case II_ANSIBLE_TR_TOG:
			if(get_tr(TR1 + d[1]))
				clr_tr(TR1 + d[1]);
			else
				set_tr(TR1 + d[1]);
			break;
		case II_ANSIBLE_TR_TIME:
			tr_time[d[1]] = (d[2] << 8) + d[3];
			break;
		case II_ANSIBLE_TR_TIME + II_GET:
			ii_tx_queue(tr_time[d[1]] >> 8);
			ii_tx_queue(tr_time[d[1]] & 0xff);
			break;
		case II_ANSIBLE_TR_POL:
			tr_pol[d[1]] = d[2];
			break;
		case II_ANSIBLE_TR_POL + II_GET:
			ii_tx_queue(tr_pol[d[1]]);
			break;
		case II_ANSIBLE_TR_PULSE:
			switch(d[1]) {
			case 0:
				if(tr_pol[0]) set_tr(TR1);
				else clr_tr(TR1);
				timer_add(&auxTimer[0], tr_time[0], &tr_pulse0, NULL );
				break;
			case 1:
				if(tr_pol[1]) set_tr(TR2);
				else clr_tr(TR2);
				timer_add(&auxTimer[1], tr_time[1], &tr_pulse1, NULL );
				break;
			case 2:
				if(tr_pol[2]) set_tr(TR3);
				else clr_tr(TR3);
				timer_add(&auxTimer[2], tr_time[2], &tr_pulse2, NULL );
				break;
			case 3:
				if(tr_pol[3]) set_tr(TR4);
				else clr_tr(TR4);
				timer_add(&auxTimer[3], tr_time[3], &tr_pulse3, NULL );
				break;
			default: break;
			}
			break;
		case II_ANSIBLE_CV:
			dac_set_value(d[1], (d[2] << 8) + d[3]);
			break;
		case II_ANSIBLE_CV + II_GET:
			ii_tx_queue(dac_get_value(d[1]) >> 8);
			ii_tx_queue(dac_get_value(d[1]) & 0xff);
			break;
		case II_ANSIBLE_CV_SLEW:
			dac_set_slew(d[1], (d[2] << 8) + d[3]);
			break;
		case II_ANSIBLE_CV_SLEW + II_GET:
			ii_tx_queue(dac_get_slew(d[1]) >> 8);
			ii_tx_queue(dac_get_slew(d[1]) & 0xff);
			break;
		case II_ANSIBLE_CV_OFF:
			dac_set_off(d[1], (d[2] << 8) + d[3]);
			break;
		case II_ANSIBLE_CV_OFF + II_GET:
			ii_tx_queue(dac_get_off(d[1]) >> 8);
			ii_tx_queue(dac_get_off(d[1]) & 0xff);
			break;
		case II_ANSIBLE_CV_SET:
			dac_set_value_noslew(d[1], (d[2] << 8) + d[3]);
			break;
		case II_ANSIBLE_INPUT + II_GET:
			ii_tx_queue(input[d[1]]);
			break;
		default:
			ii_ansible(d, l);
			break;
		}
	}
}


void handler_TTKey(s32 data) {
	// print_dbg("\r\n> TT key");
	// print_dbg_ulong(data);

	switch(data) {
	case 0:
		input[2] = 0;
		break;
	case 1:
		input[2] = 1;
		break;
	case 2:
		input[3] = 0;
		break;
	case 3:
		input[3] = 1;
		break;
	default:
		break;
	}

}

void handler_TTTr(s32 data) {
	// print_dbg("\r\n> TT tr");
	// print_dbg_ulong(data);

	switch(data) {
	case 0:
		input[0] = 0;
		break;
	case 1:
		input[0] = 1;
		break;
	case 2:
		input[1] = 0;
		break;
	case 3:
		input[1] = 1;
		break;
	default:
		break;
	}
}

void handler_TTTrNormal(s32 data) {
	// print_dbg("\r\n> TT tr normal ");
	// print_dbg_ulong(data);
}
