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


#define FIRSTRUN_KEY 0x22

uint8_t front_timer;

uint8_t preset_mode;

__attribute__((__section__(".flash_nvram")))
nvram_data_t f;

ansible_mode_t ansible_mode;
i2c_follower_t followers[I2C_FOLLOWER_COUNT] = {
	{
		.active = false,
		.track_en = 0xF,
		.oct = 0,
		.addr = JF_ADDR,
		.tr_cmd = JF_TR,
		.cv_cmd = JF_VOX,
		.cv_extra = true,
	},
	{
		.active = false,
		.track_en = 0xF,
		.oct = 5,
		.addr = TELEXO_0,
		.tr_cmd = 0x6D, // TO_ENV
		.cv_cmd = 0x40, // TO_OSC
		.cv_slew_cmd = 0x4F, // TO_OSC_SCLEW
		.init_cmd = 0x60, // TO_ENV_ACT
		.vol_cmd = 0x10, // TO_CV
	},
	{
		.active = false,
		.track_en = 0xF,
		.oct = 5,
		.addr = TELEXO_1,
		.tr_cmd = 0x6D, // TO_ENV
		.cv_cmd = 0x40, // TO_OSC
		.cv_slew_cmd = 0x4F, // TO_OSC_SCLEW
		.init_cmd = 0x60, // TO_ENV_ACT
		.vol_cmd = 0x10, // TO_CV
	},
	{
		.active = false,
		.track_en = 0xF,
		.oct = 0,
		.addr = ER301_1,
		.tr_cmd = 0x05, // TO_TR_PULSE -> SC.TR.P
		.cv_cmd = 0x10, // TO_CV -> SC.CV
		.cv_slew_cmd = 0x12, // TO_CV_SLEW -> SC.CV.SLEW
	},
};
bool leader_mode = false;
uint16_t cv_extra[4] = { 8192, 8192, 8192, 8192 };

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
	if (!leader_mode) init_i2c_slave(f.state.i2c_addr);
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
			tuning_table[i][j] = ET[j] << 2;
		}
	}
	flashc_memcpy((void *)f.tuning_table, tuning_table, sizeof(tuning_table), true);
}

void init_tuning(void) {
	memcpy((void *)&tuning_table, &f.tuning_table, sizeof(tuning_table));
}

void fit_tuning(void) {
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
	for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
		bool play_follower = followers[i].active
				  && followers[i].track_en & (1 << n);
		if (play_follower) {
			uint8_t d[] = {
				followers[i].tr_cmd,
				n - TR1,
				1,
			};
			i2c_master_tx(followers[i].addr, d, 3);
		}
	}
}

void clr_tr(uint8_t n) {
	gpio_clr_gpio_pin(n);
	for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
		bool play_follower = followers[i].active
				  && followers[i].track_en & (1 << n);
		if (play_follower) {
			uint8_t d[] = {
				followers[i].tr_cmd,
				n - TR1,
				0,
			};
			i2c_master_tx(followers[i].addr, d, 3);
		}
	}
}

uint8_t get_tr(uint8_t n) {
	return gpio_get_pin_value(n);
}

void set_cv(uint8_t n, uint16_t cv) {
	dac_set_value(n, cv);
	for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
		bool play_follower = followers[i].active
				  && followers[i].track_en & (1 << n);
		if (play_follower) {
			uint16_t cv_transposed = cv + tuning_table[i][12 * followers[i].oct];
			uint8_t d[] = {
				followers[i].cv_cmd,
				n,
				cv_transposed >> 8,
				cv_transposed & 0xFF,
				cv_extra[i] >> 8,
				cv_extra[i] & 0xFF,
			};
			i2c_master_tx(followers[i].addr, d, followers[i].cv_extra ? 6 : 4);
		}
	}
}

void set_cv_slew(uint8_t n, uint16_t s) {
	dac_set_slew(n, s);
	for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
		bool play_follower = followers[i].active
				  && followers[i].cv_slew_cmd > 0
				  && followers[i].track_en & (1 << n);
		if (play_follower) {
			uint8_t d[] = {
				followers[i].cv_slew_cmd,
				n,
				s >> 8,
				s & 0xFF,
			};
			i2c_master_tx(followers[i].addr, d, 4);
		}
	}
}

static void follower_on(uint8_t n) {
	if (followers[n].init_cmd > 0) {
		for (uint8_t i = 0; i < 4; i++) {
			uint8_t d[] = { followers[n].init_cmd, i, 1 };
			i2c_master_tx(followers[n].addr, d, 3);
		}
	}
	if (followers[n].vol_cmd > 0) {
		for (uint8_t i = 0; i < 4; i++) {
			uint8_t d[] = { followers[n].vol_cmd, i, 8192 >> 8, 8192 & 0xFF }; // 5V
			i2c_master_tx(followers[n].addr, d, 4);
		}
	}
}

static void follower_off(uint8_t n) {
	if (followers[n].init_cmd > 0) {
		for (uint8_t i = 0; i < 4; i++) {
			uint8_t d[] = { followers[n].init_cmd, i, 0 };
			i2c_master_tx(followers[n].addr, d, 3);
		}
	}
	if (followers[n].vol_cmd > 0) {
		for (uint8_t i = 0; i < 4; i++) {
			uint8_t d[] = { followers[n].vol_cmd, i, 0, 0 };
			i2c_master_tx(followers[n].addr, d, 3);
		}
	}
}

void toggle_follower(uint8_t n) {
	followers[n].active = !followers[n].active;
	print_dbg("\r\ntoggle follower ");
	print_dbg_ulong(n);
	if (followers[n].active) {
		for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
			if (i != n && followers[i].active) {
				print_dbg("\r\follower ");
				print_dbg_ulong(n);
				print_dbg(" also on, quit");
				follower_on(n);
				return;
			}
		}
		leader_mode = true;
		init_i2c_master();
		follower_on(n);
	}
	else {
		follower_off(n);
		for (uint8_t i = 0; i < I2C_FOLLOWER_COUNT; i++) {
			if (i != n && followers[i].active) {
				print_dbg("\r\follower ");
				print_dbg_ulong(n);
				print_dbg("still on, quit");
				return;
			}
		}
		leader_mode = false;
		switch (ansible_mode) {
		case mArcLevels:
			init_i2c_slave(II_LV_ADDR);
			break;
		case mArcCycles:
			init_i2c_slave(II_CY_ADDR);
			break;
		case mGridKria:
			init_i2c_slave(II_KR_ADDR);
			break;
		case mGridMP:
			init_i2c_slave(II_MP_ADDR);
			break;
		case mGridES:
			init_i2c_slave(ES);
			break;
		case mMidiStandard:
			init_i2c_slave(II_MID_ADDR);
			break;
		case mMidiArp:
			init_i2c_slave(II_ARP_ADDR);
			break;
		case mTT:
			init_i2c_slave(f.state.i2c_addr);
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
	if (!leader_mode) init_i2c_slave(f.state.i2c_addr);
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
