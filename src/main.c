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



change_mode(MODE)
	called on usb change, or front-push mode change
	sets all the handlers straight
	remembers what the last mode was (ie, for grid plug kria vs. mp)


generalized CV out system with ramping

*/






#include <stdio.h>

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

#include "conf_board.h"

// absible
#include "main.h"
#include "ansible_grid.h"
#include "ansible_arc.h"
#include "ansible_midi.h"
#include "ansible_tt.h"
	

#define FIRSTRUN_KEY 0x22

u16 cv;

typedef const struct {
	u8 fresh;
	u8 preset_select;
} nvram_data_t;

uint8_t front_timer;

uint8_t preset_mode;
uint8_t preset_select;


// NVRAM data structure located in the flash array.
__attribute__((__section__(".flash_nvram")))
static nvram_data_t flashy;



////////////////////////////////////////////////////////////////////////////////
// prototypes

static void refresh(void);
static void clock(u8 phase);

// start/stop monome polling/refresh timers
extern void timers_set_monome(void);
extern void timers_unset_monome(void);

// check the event queue
static void check_events(void);

// handler protos
static void handler_None(s32 data) { ;; }
static void handler_KeyTimer(s32 data);
static void handler_Front(s32 data);
static void handler_FrontShort(s32 data);
static void handler_FrontLong(s32 data);
static void handler_Tr(s32 data);
static void handler_TrNormal(s32 data);
static void handler_Key(s32 data);
static void handler_MidiConnect(s32 data);
static void handler_MidiDisconnect(s32 data);
static void handler_MidiPacket(s32 data);

static void ansible_process_ii(uint8_t i, int d);

u8 flash_is_fresh(void);
void flash_unfresh(void);
void flash_write(void);
void flash_read(void);

void set_mode(void);


////////////////////////////////////////////////////////////////////////////////
// timers

static softTimer_t clockTimer = { .next = NULL, .prev = NULL };
static softTimer_t keyTimer = { .next = NULL, .prev = NULL };
static softTimer_t monomePollTimer = { .next = NULL, .prev = NULL };
static softTimer_t monomeRefreshTimer  = { .next = NULL, .prev = NULL };
static softTimer_t midiPollTimer = { .next = NULL, .prev = NULL };
// static softTimer_t midiPollTimer = { .next = NULL, .prev = NULL };



static void clockTimer_callback(void* o) {  
	static uint8_t clock_phase;
	// static event_t e;
	// e.type = kEventTimer;
	// e.data = 0;
	// event_post(&e);
	if(clock_external == 0) {
		// print_dbg("\r\ntimer.");

		clock_phase++;
		if(clock_phase > 1) clock_phase = 0;
		(*clock_pulse)(clock_phase);
	}

	clock(0);
}

static void keyTimer_callback(void* o) {  
	static event_t e;
	e.type = kEventKeyTimer;
	e.data = 0;
	event_post(&e);
}

// monome polling callback
static void monome_poll_timer_callback(void* obj) {
	// asynchronous, non-blocking read
	// UHC callback spawns appropriate events
	ftdi_read();
}

// monome refresh callback
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

// monome: start polling
void timers_set_monome(void) {
	// print_dbg("\r\n setting monome timers");
	timer_add(&monomePollTimer, 20, &monome_poll_timer_callback, NULL );
	timer_add(&monomeRefreshTimer, 30, &monome_refresh_timer_callback, NULL );
}

// monome stop polling
void timers_unset_monome(void) {
	// print_dbg("\r\n unsetting monome timers");
	timer_remove( &monomePollTimer );
	timer_remove( &monomeRefreshTimer );

	app_event_handlers[kEventKey] = &handler_Key;
}

void set_mode(void) {
	switch (ansible_state.mode) {
	case mGridKria:
	case mGridMP:
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

static void handler_FtdiConnect(s32 data) { ftdi_setup(); }
static void handler_FtdiDisconnect(s32 data) { 
	timers_unset_monome();
	app_event_handlers[ kEventFrontShort ]	= &handler_FrontShort;
	app_event_handlers[ kEventFrontLong ]	= &handler_FrontLong;
	ansible_state.connected = conNONE;
	ansible_state.none_mode = ansible_state.mode;
	set_mode();
}

static void handler_MonomeConnect(s32 data) {
	print_dbg("\r\n> connect: monome ");

	switch (monome_device()) {
	case eDeviceGrid:
		print_dbg("GRID");
		ansible_state.connected = conGRID;
		ansible_state.mode = ansible_state.grid_mode;
		set_mode();
		break;
	case eDeviceArc:
		print_dbg("ARC");
		ansible_state.connected = conARC;
		ansible_state.mode = ansible_state.arc_mode;
		set_mode();
		break;
	default:
		break;
	}
	timers_set_monome();
}

static void handler_MonomePoll(s32 data) { 
	monome_read_serial(); 
}

static void handler_MonomeRefresh(s32 data) {
	if(monomeFrameDirty) {
		refresh();
		(*monome_refresh)();
	}
}

static void handler_MidiConnect(s32 data) {
	print_dbg("\r\n> midi connect");
	timer_add(&midiPollTimer, 13, &midi_poll_timer_callback, NULL);
	ansible_state.connected = conMIDI;
	ansible_state.mode = ansible_state.midi_mode;
	set_mode();
}

static void handler_MidiDisconnect(s32 data) {
	print_dbg("\r\n> midi disconnect");
	timer_remove(&midiPollTimer);
	ansible_state.connected = conNONE;
	ansible_state.mode = mTT;
	ansible_state.none_mode = ansible_state.mode;
	set_mode();
}

static void handler_MidiPacket(s32 data) {
	print_dbg("\r\n> midi packet");
}

static void handler_Front(s32 data) {
	// print_dbg("\r\n+ front ");
	// print_dbg_ulong(data);

	if(data == 1) {
		front_timer = 15;
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

	monomeFrameDirty++;
}

static void handler_FrontShort(s32 data) {
	if(ansible_state.mode == mTT)
		ansible_state.mode = ansible_state.none_mode;
	else
		ansible_state.mode = mTT;

	set_mode();
}

static void handler_FrontLong(s32 data) {
	print_dbg("\r\n+ front long");
	uint8_t address;
 	address = 100 + (!gpio_get_pin_value(B06) * 2) + (!gpio_get_pin_value(B06) * 4);
 	print_dbg("\r\n+ i2c address: ");
 	print_dbg_ulong(address);
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
}

static void handler_Tr(s32 data) {
	print_dbg("\r\n+ tr ");
	print_dbg_ulong(data);
}

static void handler_TrNormal(s32 data) {
	print_dbg("\r\n+ jack ");
	print_dbg_ulong(data);
}

static void handler_Key(s32 data) {
	print_dbg("\r\n+ key ");
	print_dbg_ulong(data);
}



// assign event handlers
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
	app_event_handlers[ kEventMonomeRefresh ]	= &handler_MonomeRefresh ;
	app_event_handlers[ kEventMonomeGridKey ]	= &handler_None ;
	app_event_handlers[ kEventMonomeRingEnc ]	= &handler_None ;
	app_event_handlers[ kEventTr ]	= &handler_Tr ;
	app_event_handlers[ kEventTrNormal ] = &handler_TrNormal ;
	app_event_handlers[ kEventKey ]	= &handler_Key ;
	app_event_handlers[ kEventMidiConnect ]	    = &handler_MidiConnect ;
	app_event_handlers[ kEventMidiDisconnect ]  = &handler_MidiDisconnect ;
	app_event_handlers[ kEventMidiPacket ]      = &handler_MidiPacket ;
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
  return (flashy.fresh != FIRSTRUN_KEY);
}

void flash_unfresh(void) {
  flashc_memset8((void*)&(flashy.fresh), FIRSTRUN_KEY, 4, true);
}

void flash_write(void) {
	print_dbg("\r\n> write preset ");
	print_dbg_ulong(preset_select);
	flashc_memset8((void*)&(flashy.preset_select), preset_select, 4, true);
}

void flash_read(void) {
	print_dbg("\r\n> read preset ");
	print_dbg_ulong(preset_select);

	// ...
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// application clock code

void clock(u8 phase) {
	cv += 0xff;
	cv &= 4095;

	spi_selectChip(SPI,DAC_SPI);
	spi_write(SPI,0x31);
	spi_write(SPI,cv>>4); // 2
	spi_write(SPI,cv<<4);
	spi_write(SPI,0x31);
	spi_write(SPI,cv>>4); // 0
	spi_write(SPI,cv<<4);
	spi_unselectChip(SPI,DAC_SPI);
	
	spi_selectChip(SPI,DAC_SPI);
	spi_write(SPI,0x38);
	spi_write(SPI,cv>>4); // 3
	spi_write(SPI,cv<<4);
	spi_write(SPI,0x38);
	spi_write(SPI,cv>>4); // 1
	spi_write(SPI,cv<<4);
	spi_unselectChip(SPI,DAC_SPI);

	if(cv & 0x0010) {
		gpio_set_gpio_pin(B00);
		gpio_set_gpio_pin(B01);
		gpio_set_gpio_pin(B02);
		gpio_set_gpio_pin(B03);
		gpio_set_gpio_pin(B04);
		gpio_set_gpio_pin(B05);
	}
	else {
		gpio_clr_gpio_pin(B00);
		gpio_clr_gpio_pin(B01);
		gpio_clr_gpio_pin(B02);
		gpio_clr_gpio_pin(B03);
		gpio_clr_gpio_pin(B04);
		gpio_clr_gpio_pin(B05);
	}
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// application grid code

// static void handler_MonomeGridKey(s32 data) { 
// 	u8 x, y, z;
// 	monome_grid_key_parse_event_data(data, &x, &y, &z);

// 	print_dbg("\r\n monome event; x: "); 
// 	print_dbg_hex(x); 
// 	print_dbg("; y: 0x"); 
// 	print_dbg_hex(y); 
// 	print_dbg("; z: 0x"); 
// 	print_dbg_hex(z);

// 	monomeFrameDirty++;
// }

////////////////////////////////////////////////////////////////////////////////
// application grid redraw
static void refresh() {
	u8 i1;

	// clear top, cut, pattern, prob
	for(i1=0;i1<16;i1++) {
		monomeLedBuffer[i1] = 0;
		monomeLedBuffer[16+i1] = 0;
		monomeLedBuffer[32+i1] = 4;
		monomeLedBuffer[48+i1] = 0;
	}

	monomeLedBuffer[0] = 15;

	monome_set_quadrant_flag(0);
	monome_set_quadrant_flag(1);
}



////////////////////////////////////////////////////////////////////////////////
// ii
static void ansible_process_ii(uint8_t i, int d) {
	print_dbg("\r\n ii");
}





////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// main

int main(void)
{
	sysclk_init();

	init_dbg_rs232(FMCK_HZ);

	init_gpio();
	assign_main_event_handlers();
	init_events();
	init_tc();
	init_spi();
	// init_adc();

	irq_initialize_vectors();
	register_interrupts();
	cpu_irq_enable();

	init_usb_host();
	init_monome();

	init_i2c_slave(0x80);


	print_dbg("\r\n\n// ansible //////////////////////////////// ");
	print_dbg_ulong(sizeof(flashy));

	if(flash_is_fresh()) {
		print_dbg("\r\nfirst run.");
		flash_unfresh();
		preset_select = 0;
		flash_write();
	}
	else {
		// load from flash at startup
		preset_select = flashy.preset_select;
		flash_read();
	}

	// ALL THESE NORMALLY GET RESTORED FROM FLASH:
	ansible_state.connected = conNONE;
	ansible_state.grid_mode = mGridKria;
	ansible_state.arc_mode = mArcLevels;
	ansible_state.midi_mode = mMidiStandard;
	ansible_state.none_mode = mTT;
	ansible_state.mode = mTT;
	set_mode();

	process_ii = &ansible_process_ii;

	clock_pulse = &clock;
	clock_external = !gpio_get_pin_value(B09);

	timer_add(&clockTimer,40,&clockTimer_callback, NULL);
	timer_add(&keyTimer,50,&keyTimer_callback, NULL);

	// setup daisy chain for two dacs
	spi_selectChip(SPI,DAC_SPI);
	spi_write(SPI,0x80);
	spi_write(SPI,0xff);
	spi_write(SPI,0xff);
	spi_unselectChip(SPI,DAC_SPI);

	while (true) {
		check_events();
	}
}
