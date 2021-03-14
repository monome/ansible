/*

b00 led1
b01 led2

b02 tr1
b03 tr2
b04 tr3
b05 tr4

b06 k1
b07 k2

b08 in1
b09 in2
b10 in1-detect

nmi


usb flash

*/


#include <stdio.h>
#include <string.h> // memcpy

// asf
#include "delay.h"
#include "compiler.h"
#include "flashc.h"
#include "preprocessor.h"
#include "print_funcs.h"
#include "intc.h"
#include "pm.h"
#include "gpio.h"
#include "spi.h"
#include "sysclk.h"

// libavr32
#include "types.h"
#include "events.h"
#include "libfixmath/fix16.h"
#include "i2c.h"
#include "init_ansible.h"
#include "init_common.h"
#include "monome.h"
#include "midi.h"
#include "music.h"
#include "notes.h"
#include "timers.h"
#include "util.h"
#include "ftdi.h"
#include "ii.h"
#include "dac.h"


#include "conf_board.h"

// ansible
#include "main.h"
#include "ansible_grid.h"
#include "ansible_arc.h"
#include "ansible_midi.h"
#include "ansible_tt.h"
#include "ansible_usb_disk.h"
#include "ansible_ii_leader.h"


#define FIRSTRUN_KEY 0x22

uint8_t front_timer;

uint8_t preset_mode;

__attribute__((__section__(".flash_nvram")))
nvram_data_t f;

ansible_mode_t ansible_mode;

bool leader_mode = false;
uint16_t aux_param[2][4] = { { 0 } };

////////////////////////////////////////////////////////////////////////////////
// prototypes

// start/stop monome polling/refresh timers
extern void timers_set_monome(void);
extern void timers_unset_monome(void);

// check the event queue
static void check_events(void);

// handler protos
static void handler_KeyTimer(s32 data);
static void handler_Front(s32 data);
static void handler_FrontShort(s32 data);
static void handler_FrontLong(s32 data);
static void handler_MidiConnect(s32 data);
static void handler_MidiDisconnect(s32 data);

u8 flash_is_fresh(void);
void flash_write(void);
void flash_read(void);
void state_write(void);
void state_read(void);

void ii_ansible(uint8_t* d, uint8_t len);
static ansible_mode_t ii_ansible_mode_for_cmd(uint8_t cmd);
static uint8_t ii_ansible_cmd_for_mode(ansible_mode_t mode);
////////////////////////////////////////////////////////////////////////////////
// timers

static softTimer_t clockTimer = { .next = NULL, .prev = NULL };
static softTimer_t keyTimer = { .next = NULL, .prev = NULL };
static softTimer_t cvTimer = { .next = NULL, .prev = NULL };
static softTimer_t monomePollTimer = { .next = NULL, .prev = NULL };
static softTimer_t monomeRefreshTimer  = { .next = NULL, .prev = NULL };
static softTimer_t midiPollTimer = { .next = NULL, .prev = NULL };

softTimer_t auxTimer[4] = {
	{ .next = NULL, .prev = NULL },
	{ .next = NULL, .prev = NULL },
	{ .next = NULL, .prev = NULL },
	{ .next = NULL, .prev = NULL }
};

uint16_t tuning_table[4][120];

ansible_output_t outputs[4];

static uint8_t clock_phase;

void handler_None(s32 data) { ;; }

static void clockTimer_callback(void* o) {
	clock_phase++;
	if(clock_phase > 1)
		clock_phase = 0;
	clock(clock_phase);
}

static void keyTimer_callback(void* o) {
	static event_t e;
	e.type = kEventKeyTimer;
	e.data = 0;
	event_post(&e);
}

static void cvTimer_callback(void* o) {
	dac_timer_update();
}

static void monome_poll_timer_callback(void* obj) {
	ftdi_read();
}

static void monome_refresh_timer_callback(void* obj) {
	if(monomeFrameDirty > 0) {
		static event_t e;
		e.type = kEventMonomeRefresh;
		event_post(&e);
	}
}

static void midi_poll_timer_callback(void* obj) {
	midi_read();
}

void timers_set_monome(void) {
	timer_add(&monomePollTimer, 20, &monome_poll_timer_callback, NULL );
	timer_add(&monomeRefreshTimer, 30, &monome_refresh_timer_callback, NULL );
}

void timers_unset_monome(void) {
	timer_remove( &monomePollTimer );
	timer_remove( &monomeRefreshTimer );
}

void set_mode(ansible_mode_t m) {
	ansible_mode = m;
	// flashc_memset32((void*)&(f.state.mode), m, 4, true);
	// print_dbg("\r\nset mode ");
	// print_dbg_ulong(f.state.mode);

	timer_remove(&auxTimer[0]);
	timer_remove(&auxTimer[1]);
	timer_remove(&auxTimer[2]);
	timer_remove(&auxTimer[3]);

	switch (m) {
	case mGridKria:
	case mGridMP:
	case mGridES:
		set_mode_grid();
		break;
	case mArcLevels:
	case mArcCycles:
		set_mode_arc();
		break;
	case mMidiStandard:
	case mMidiArp:
		set_mode_midi();
		break;
	case mTT:
		set_mode_tt();
		break;
	case mUsbDisk:
		set_mode_usb_disk();
		break;
	default:
		break;
	}
}

////////////////////////////////////////////////////////////////////////////////
// event handlers

static void handler_FtdiConnect(s32 data) {
	ftdi_setup();
}

static void handler_FtdiDisconnect(s32 data) {
	timers_unset_monome();
	app_event_handlers[ kEventFrontShort ]	= &handler_FrontShort;
	app_event_handlers[ kEventFrontLong ]	= &handler_FrontLong;
	connected = conNONE;
	// set_mode(f.state.mode);
}

static void handler_MonomeConnect(s32 data) {
	print_dbg("\r\n> connect: monome ");

	switch (monome_device()) {
	case eDeviceGrid:
		print_dbg("GRID");
		connected = conGRID;
		if(ansible_mode != f.state.grid_mode)
			set_mode(f.state.grid_mode);
		monomeFrameDirty++;
		app_event_handlers[kEventFrontShort] = &handler_GridFrontShort;
		app_event_handlers[kEventFrontLong] = &handler_GridFrontLong;
		break;
	case eDeviceArc:
		print_dbg("ARC");
		connected = conARC;
		if(ansible_mode != f.state.arc_mode)
			set_mode(f.state.arc_mode);
		monomeFrameDirty++;
		app_event_handlers[kEventFrontShort] = &handler_ArcFrontShort;
		app_event_handlers[kEventFrontLong] = &handler_ArcFrontLong;
		break;
	default:
		break;
	}
	timers_set_monome();
}

static void handler_MonomePoll(s32 data) {
	monome_read_serial();
}

static void handler_MidiConnect(s32 data) {
	print_dbg("\r\n> midi connect");
	timer_add(&midiPollTimer, 8, &midi_poll_timer_callback, NULL);
	connected = conMIDI;
	flashc_memset32((void*)&(f.state.none_mode), mTT, 4, true);
	set_mode(f.state.midi_mode);
}

static void handler_MidiDisconnect(s32 data) {
	print_dbg("\r\n> midi disconnect");
	timer_remove(&midiPollTimer);
	app_event_handlers[ kEventFrontShort ]	= &handler_FrontShort;
	app_event_handlers[ kEventFrontLong ]	= &handler_FrontLong;
	connected = conNONE;
	set_mode(mTT);
}

static volatile bool front_held = false;

static void handler_MscConnect(s32 data) {
	print_dbg("\r\n> usb disk connect");
	if (front_held) {
		usb_disk_select_app(ansible_mode);
	}
	set_mode(mUsbDisk);
}

static void handler_MscDisconnect(s32 data) {
	print_dbg("\r\n> usb disk disconnect");
	usb_disk_exit();
	update_leds(0);
	app_event_handlers[kEventFront]	= &handler_Front;
	usb_disk_skip_apps(false);
}

static void handler_Front(s32 data) {
	// print_dbg("\r\n+ front ");
	// print_dbg_ulong(data);

	if(data == 1) {
		front_timer = KEY_HOLD_TIME;
		front_held = true;
	}
	else {
		front_held = false;
		if(front_timer) {
			static event_t e;
			e.type = kEventFrontShort;
			e.data = 0;
			event_post(&e);
		}
		front_timer = 0;
	}
}

static void handler_FrontShort(s32 data) {
	if(ansible_mode != mTT) {
		flashc_memset32((void*)&(f.state.none_mode), mTT, 4, true);
		set_mode(mTT);
	}
}

static void handler_FrontLong(s32 data) {
	print_dbg("\r\n+ front long");
	uint8_t addr = 0xA0 + (!gpio_get_pin_value(B07) * 2) + (!gpio_get_pin_value(B06) * 4);
	flashc_memset8((void*)&(f.state.i2c_addr), addr, 1, true);
	print_dbg("\r\n+ i2c address: ");
	print_dbg_hex(f.state.i2c_addr);
	// TEST
	if (!leader_mode) init_i2c_follower(f.state.i2c_addr);
}

static void handler_SaveFlash(s32 data) {
	flash_write();
}

static void handler_KeyTimer(s32 data) {
	static uint8_t key0_state;
	static uint8_t key1_state;
	static uint8_t keyfront_state;
	static uint8_t tr0normal_state;

	if(key0_state != !gpio_get_pin_value(B07)) {
		key0_state = !gpio_get_pin_value(B07);
		static event_t e;
		e.type = kEventKey;
	    e.data = key0_state;
		event_post(&e);
	}

	if(key1_state != !gpio_get_pin_value(B06)) {
		key1_state = !gpio_get_pin_value(B06);
		static event_t e;
		e.type = kEventKey;
	    e.data = key1_state + 2;
		event_post(&e);
	}

	if(keyfront_state != !gpio_get_pin_value(NMI)) {
		keyfront_state = !gpio_get_pin_value(NMI);
		static event_t e;
		e.type = kEventFront;
	    e.data = keyfront_state;
		event_post(&e);
	}

	if(tr0normal_state != !gpio_get_pin_value(B10)) {
		tr0normal_state = !gpio_get_pin_value(B10);
		static event_t e;
		e.type = kEventTrNormal;
	    e.data = tr0normal_state;
		event_post(&e);
	}

	if(front_timer) {
		if(front_timer == 1) {
			static event_t e;
			e.type = kEventFrontLong;
		    e.data = 0;
			event_post(&e);
		}
		front_timer--;
	}

	if(connected == conGRID)
		grid_keytimer();
	else if(connected == conARC)
		arc_keytimer();
}

// assign default event handlers
static inline void assign_main_event_handlers(void) {
	app_event_handlers[ kEventFront ]	= &handler_Front;
	app_event_handlers[ kEventFrontShort ]	= &handler_FrontShort;
	app_event_handlers[ kEventFrontLong ]	= &handler_FrontLong;
	app_event_handlers[ kEventKeyTimer ] = &handler_KeyTimer;
	app_event_handlers[ kEventSaveFlash ] = &handler_SaveFlash;
	app_event_handlers[ kEventFtdiConnect ]	= &handler_FtdiConnect ;
	app_event_handlers[ kEventFtdiDisconnect ]	= &handler_FtdiDisconnect ;
	app_event_handlers[ kEventMscConnect ]	= &handler_MscConnect ;
	app_event_handlers[ kEventMscDisconnect ]	= &handler_MscDisconnect ;
	app_event_handlers[ kEventMonomeConnect ]	= &handler_MonomeConnect ;
	app_event_handlers[ kEventMonomeDisconnect ]	= &handler_None ;
	app_event_handlers[ kEventMonomePoll ]	= &handler_MonomePoll ;
	app_event_handlers[ kEventMonomeRefresh ]	= &handler_None ;
	app_event_handlers[ kEventMonomeGridKey ]	= &handler_None ;
	app_event_handlers[ kEventMonomeRingEnc ]	= &handler_None ;
	app_event_handlers[ kEventTr ]	= &handler_None ;
	app_event_handlers[ kEventTrNormal ] = &handler_None ;
	app_event_handlers[ kEventKey ]	= &handler_None ;
	app_event_handlers[ kEventMidiConnect ]	    = &handler_MidiConnect ;
	app_event_handlers[ kEventMidiDisconnect ]  = &handler_MidiDisconnect ;
	app_event_handlers[ kEventMidiPacket ]      = &handler_None;
}

// app event loop
void check_events(void) {
	static event_t e;
	if( event_next(&e) ) {
		(app_event_handlers)[e.type](e.data);
	}
}



////////////////////////////////////////////////////////////////////////////////
// flash

u8 flash_is_fresh(void) {
	return (f.fresh != FIRSTRUN_KEY);
}

void flash_unfresh(void) {
	flashc_memset8((void*)&(f.fresh), FIRSTRUN_KEY, 1, true);
}

void flash_write(void) {
	print_dbg("\r\n> write preset ");
	// print_dbg_ulong(preset_select);
	// flashc_memset8((void*)&(f.preset_select), preset_select, 4, true);

	// flashc_memcpy((void *)&(f.state), &ansible_state, sizeof(ansible_state), true);
}

void flash_read(void) {
	print_dbg("\r\n> read preset ");
	// print_dbg_ulong(preset_select);

	// preset_select = f.preset_select;

	// memcpy(&ansible_state, &f.state, sizeof(ansible_state));

	// ...
}

////////////////////////////////////////////////////////////////////////////////
// tuning

void default_tuning(void) {
	for (uint8_t i = 0; i < 4; i++) {
		for (uint8_t j = 0; j < 120; j++) {
			tuning_table[i][j] = ET[j];
		}
	}
	flashc_memcpy((void *)f.tuning_table, tuning_table, sizeof(tuning_table), true);
}

void init_tuning(void) {
	memcpy((void *)&tuning_table, &f.tuning_table, sizeof(tuning_table));
}

void fit_tuning(int mode) {
	switch (mode) {
		case 0: { // fixed offset per channel
			for (uint8_t i = 0; i < 4; i++) {
				uint16_t offset = tuning_table[i][0];
				for (uint8_t j = 0; j < 120; j++) {
					tuning_table[i][j] = ET[j] + offset;
				}
			}
			break;
		}
		case 1: { // linear fit between octaves
			for (uint8_t i = 0; i < 4; i++) {
				fix16_t step = 0;
				for (uint8_t j = 0; j < 10; j++) {
					fix16_t acc = fix16_from_int(tuning_table[i][j*12]);
					if (j < 9) {
						step = fix16_div(
							fix16_from_int(tuning_table[i][(j+1)*12] - tuning_table[i][j*12]),
							fix16_from_int(12));
					}
					for (uint8_t k = j*12; k < (j+1)*12; k++) {
						tuning_table[i][k] = fix16_to_int(acc);
						acc = fix16_add(acc, step);
					}
				}
			}
			break;
		}
		default: break;
	}
}

////////////////////////////////////////////////////////////////////////////////
// functions

void clock_null(u8 phase) { ;; }

void update_leds(uint8_t m) {
	if(m & 1)
		gpio_set_gpio_pin(B00);
	else
		gpio_clr_gpio_pin(B00);


	if(m & 2)
		gpio_set_gpio_pin(B01);
	else
		gpio_clr_gpio_pin(B01);
}

void set_tr(uint8_t n) {
	gpio_set_gpio_pin(n);
	uint8_t tr = n - TR1;
	outputs[tr].tr = true;
	for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
		bool play_follower = followers[i].active
				  && followers[i].track_en & (1 << tr);
		if (play_follower) {
			followers[i].ops->tr(&followers[i], tr, 1);
		}
	}
}

void clr_tr(uint8_t n) {
	gpio_clr_gpio_pin(n);
	uint8_t tr = n - TR1;
	outputs[tr].tr = false;
	for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
		bool play_follower = followers[i].active
				  && followers[i].track_en & (1 << tr);
		if (play_follower) {
			followers[i].ops->tr(&followers[i], tr, 0);
		}
	}
}

uint8_t get_tr(uint8_t n) {
	return gpio_get_pin_value(n);
}

void set_cv_note(uint8_t n, uint16_t note, int16_t bend) {
	outputs[n].semitones = note;
	outputs[n].bend = bend;
	outputs[n].dac_target = (int16_t)tuning_table[n][note] + bend;
	dac_set_value(n, outputs[n].dac_target);
	for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
		bool play_follower = followers[i].active
				  && followers[i].track_en & (1 << n);
		if (play_follower) {
			uint16_t cv_transposed = (int16_t)ET[note] + bend;
			followers[i].ops->cv(&followers[i], n, cv_transposed);
		}
	}
}

void set_cv_slew(uint8_t n, uint16_t s) {
	outputs[n].slew = s;
	dac_set_slew(n, outputs[n].slew);
	for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
		bool play_follower = followers[i].active
				  && followers[i].track_en & (1 << n);
		if (play_follower) {
			followers[i].ops->slew(&followers[i], n, s);
		}
	}
}

void reset_outputs(void) {
	for (uint8_t n = 0; n < 4; n++) {
		outputs[n].slew = 0;
		dac_set_slew(n, 0);
		outputs[n].tr = false;
		gpio_clr_gpio_pin(n + TR1);
		for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
			bool play_follower = followers[i].active
			  && followers[i].track_en & (1 << n);
			if (play_follower) {
				followers[i].ops->mute(&followers[n], 0, 0);
			}
		}
	}
}

static void follower_on(uint8_t n) {
	for (uint8_t i = 0; i < 4; i++) {
		followers[n].ops->init(&followers[n], i, 1);
		followers[n].ops->mode(&followers[n], i, followers[n].active_mode);
		followers[n].ops->octave(&followers[n], 0, followers[n].oct);
	}
}

static void follower_off(uint8_t n) {
	for (uint8_t i = 0; i < 4; i++) {
		followers[n].ops->init(&followers[n], i, 0);
	}
}

void toggle_follower(uint8_t n) {
	followers[n].active = !followers[n].active;
	if (followers[n].active) {
		for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
			if (i != n && followers[i].active) {
				follower_on(n);
				return;
			}
		}
		print_dbg("\r\n> enter i2c leader mode");
		leader_mode = true;
		init_i2c_leader();
		follower_on(n);
	}
	else {
		follower_off(n);
		for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
			if (i != n && followers[i].active) {
				return;
			}
		}
		print_dbg("\r\n> exit i2c leader mode");
		leader_mode = false;
		ii_follower_resume();
	}
}

void ii_follower_pause(void) {
	if (!leader_mode) {
		// 0x03 is a reserved address 'for future use' in the i2c spec
		// used to effectively stop listening for i2c
		init_i2c_follower(0x03);
	}
}

void ii_follower_resume(void) {
	if (!leader_mode) {
		switch (ansible_mode) {
		case mArcLevels:
			init_i2c_follower(II_LV_ADDR);
			break;
		case mArcCycles:
			init_i2c_follower(II_CY_ADDR);
			break;
		case mGridKria:
			init_i2c_follower(II_KR_ADDR);
			break;
		case mGridMP:
			init_i2c_follower(II_MP_ADDR);
			break;
		case mGridES:
			init_i2c_follower(ES);
			break;
		case mMidiStandard:
			init_i2c_follower(II_MID_ADDR);
			break;
		case mMidiArp:
			init_i2c_follower(II_ARP_ADDR);
			break;
		case mTT:
			init_i2c_follower(f.state.i2c_addr);
			break;
	        default:
			break;
		}
	}
}

void clock_set(uint32_t n) {
	timer_set(&clockTimer, n);
}

void clock_set_tr(uint32_t n, uint8_t phase) {
	timer_set(&clockTimer, n);
	clock_phase = phase;
	timer_manual(&clockTimer);
}

///////
// global ii handlers
void load_flash_state(void) {
	init_tuning();
	init_levels();
	init_cycles();
	init_kria();
	init_mp();
	init_es();
	init_tt();

	print_dbg("\r\ni2c addr: ");
	print_dbg_hex(f.state.i2c_addr);

	leader_mode = false;
	memcpy((void*)followers, f.state.followers, sizeof(followers));
	for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
		if (followers[i].active) {
			if (!leader_mode) {
				leader_mode = true;
				init_i2c_leader();
			}
			follower_on(i);
		}
	}
	if (!leader_mode) {
		init_i2c_follower(f.state.i2c_addr);
	}
}

void ii_ansible(uint8_t* d, uint8_t len) {
	// print_dbg("\r\nii/ansible (");
	// print_dbg_ulong(len);
	// print_dbg(") ");
	// for(int i=0;i<len;i++) {
	// 	print_dbg_ulong(d[i]);
	// 	print_dbg(" ");
	// }

	if (len < 1) {
		return;
	}

	switch (d[0]) {
	case II_ANSIBLE_APP:
		if ( len >= 2 ) {
			ansible_mode_t next_mode = ii_ansible_mode_for_cmd(d[1]);
			if (next_mode < 0) {
				break;
			}
			set_mode(next_mode);
		}
		break;
	case II_ANSIBLE_APP + II_GET: {
		uint8_t cmd = ii_ansible_cmd_for_mode(ansible_mode);
		ii_tx_queue(cmd);
		break;
	}
	default:
		break;
	}
}

static ansible_mode_t ii_ansible_mode_for_cmd(uint8_t cmd) {
	switch (cmd) {
	case 0:  return mArcLevels;
	case 1:  return mArcCycles;
        case 2:  return mGridKria;
	case 3:  return mGridMP;
	case 4:  return mGridES;
	case 5:  return mMidiStandard;
	case 6:  return mMidiArp;
	case 7:  return mTT;
	default: return -1;
	}
}

static uint8_t ii_ansible_cmd_for_mode(ansible_mode_t mode) {
	switch (mode) {
	case mArcLevels:    return 0;
	case mArcCycles:    return 1;
        case mGridKria:     return 2;
	case mGridMP:       return 3;
	case mGridES:       return 4;
	case mMidiStandard: return 5;
	case mMidiArp:      return 6;
	case mTT:           return 7;
	default:            return -1;
	}
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// main

int main(void)
{
	sysclk_init();

	init_dbg_rs232(FMCK_HZ);

	print_dbg("\r\n\n// ansible //////////////////////////////// ");
	print_dbg("\r\n== FLASH struct size: ");
	print_dbg_ulong(sizeof(f));

	if(flash_is_fresh()) {
		// store flash defaults
		print_dbg("\r\nfirst run.");
		flashc_memset32((void*)&(f.state.none_mode), mTT, 4, true);
		flashc_memset32((void*)&(f.state.grid_mode), mGridKria, 4, true);
		flashc_memset32((void*)&(f.state.arc_mode), mArcLevels, 4, true);
		flashc_memset32((void*)&(f.state.midi_mode), mMidiStandard, 4, true);
		flashc_memset8((void*)&(f.state.i2c_addr), 0xA0, 1, true);
		flashc_memset8((void*)&(f.state.grid_varibrightness), 16, 1, true);
		flashc_memcpy((void*)f.state.followers, followers, sizeof(followers), true);
		default_tuning();
		default_kria();
		default_mp();
		default_es();
		default_levels();
		default_cycles();
		default_midi_standard();
		default_midi_arp();
		default_tt();

		flash_unfresh();
	}

	init_gpio();
	assign_main_event_handlers();
	init_events();
	init_tc();
	init_spi();
	// init_adc();

	irq_initialize_vectors();
	register_interrupts();
	cpu_irq_enable();

	load_flash_state();
	process_ii = &ii_ansible;

	clr_tr(TR1);
	clr_tr(TR2);
	clr_tr(TR3);
	clr_tr(TR4);

	clock = &clock_null;

	timer_add(&clockTimer,1000,&clockTimer_callback, NULL);
	timer_add(&keyTimer,50,&keyTimer_callback, NULL);
	timer_add(&cvTimer,DAC_RATE_CV,&cvTimer_callback, NULL);

	init_dacs();

	connected = conNONE;
	set_mode(f.state.none_mode);

	init_usb_host();
	init_monome();

	while (true) {
		check_events();
	}
}
