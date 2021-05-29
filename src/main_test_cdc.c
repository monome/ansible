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
#include "usb/cdc/cdc.h"

uint8_t front_timer;
#define KEY_HOLD_TIME 8

static void handler_KeyTest(s32 data) {
    if (data==0) {
	//...
	//	cdc_read_buf();
    }
}

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

////////////////////////////////////////////////////////////////////////////////
// timers

static softTimer_t keyTimer = { .next = NULL, .prev = NULL };
static softTimer_t cvTimer = { .next = NULL, .prev = NULL };
static softTimer_t monomePollTimer = { .next = NULL, .prev = NULL };
static softTimer_t monomeRefreshTimer  = { .next = NULL, .prev = NULL };
static softTimer_t midiPollTimer = { .next = NULL, .prev = NULL };

void handler_None(s32 data) { ;; }

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

////////////////////////////////////////////////////////////////////////////////
// event handlers

static void handler_FtdiConnect(s32 data) {
	ftdi_setup();
}

static void handler_FtdiDisconnect(s32 data) {
	timers_unset_monome();
}

static void handler_MonomeConnect(s32 data) {
	print_dbg("\r\n> connect: monome ");
	timers_set_monome();
}

static void handler_MonomePoll(s32 data) {
	monome_read_serial();
}

static void handler_MidiConnect(s32 data) {
	print_dbg("\r\n> midi connect");
	timer_add(&midiPollTimer, 8, &midi_poll_timer_callback, NULL);
}

static void handler_MidiDisconnect(s32 data) {
	print_dbg("\r\n> midi disconnect");
	timer_remove(&midiPollTimer);
}

static volatile bool front_held = false;

static void handler_MscConnect(s32 data) {
	print_dbg("\r\n> usb disk connect");
}

static void handler_MscDisconnect(s32 data) {
	print_dbg("\r\n> usb disk disconnect");
}

static void handler_Front(s32 data) {
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
	print_dbg("\r\n+ front short");
}

static void handler_FrontLong(s32 data) {
	print_dbg("\r\n+ front long");
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

}

// assign default event handlers
static inline void assign_main_event_handlers(void) {
	app_event_handlers[ kEventFront ]	= &handler_Front;
	app_event_handlers[ kEventFrontShort ]	= &handler_FrontShort;
	app_event_handlers[ kEventFrontLong ]	= &handler_FrontLong;
	app_event_handlers[ kEventKeyTimer ] = &handler_KeyTimer;
	app_event_handlers[ kEventSaveFlash ] = &handler_None;
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
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// main

int main(void)
{
	sysclk_init();

	init_dbg_rs232(FMCK_HZ);

	print_dbg("\r\n\n// ansible //////////////////////////////// ");
	
	init_gpio();

	gpio_set_gpio_pin(B00);
	gpio_set_gpio_pin(B01);
	
	dac_set_value(0, 0);
	dac_set_value(1, 0);
	dac_set_value(2, 0);
	dac_set_value(3, 0);

	assign_main_event_handlers();
	init_events();
	init_tc();
	init_spi();

	irq_initialize_vectors();
	register_interrupts();
	cpu_irq_enable();

	timer_add(&keyTimer,50,&keyTimer_callback, NULL);	
	timer_add(&cvTimer,DAC_RATE_CV,&cvTimer_callback, NULL);

	init_dacs();

	init_usb_host();
	init_monome();

	while (true) {
		check_events();
	}
}
