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
#include "i2c.h"
#include "init_ansible.h"
#include "init_common.h"
#include "monome.h"
#include "midi.h"
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


#define FIRSTRUN_KEY 0x22

uint8_t front_timer;

uint8_t preset_mode;

__attribute__((__section__(".flash_nvram")))
nvram_data_t f;

ansible_mode_t ansible_mode;

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
void flash_unfresh(void);
void flash_write(void);
void flash_read(void);
void state_write(void);
void state_read(void);


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

static void handler_Front(s32 data) {
	// print_dbg("\r\n+ front ");
	// print_dbg_ulong(data);

	if(data == 1) {
		front_timer = KEY_HOLD_TIME;
	}
	else {
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
 	init_i2c_slave(f.state.i2c_addr);
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
}

void clr_tr(uint8_t n) {
	gpio_clr_gpio_pin(n);
}

uint8_t get_tr(uint8_t n) {
	return gpio_get_pin_value(n);
}

void clock_set(uint32_t n) {
	timer_set(&clockTimer, n);
}

void clock_set_tr(uint32_t n, uint8_t phase) {
	timer_set(&clockTimer, n);
	clock_phase = phase;
	timer_manual(&clockTimer);
}

void ii_null(uint8_t *d, uint8_t l) {
	print_dbg("\r\nii/null");
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

	init_levels();
	init_cycles();
	init_kria();
	init_mp();
	init_es();
	init_tt();

	print_dbg("\r\ni2c addr: ");
	print_dbg_hex(f.state.i2c_addr);
	init_i2c_slave(f.state.i2c_addr);
	process_ii = &ii_null;

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
