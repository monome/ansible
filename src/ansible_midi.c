// libavr32
#include "print_funcs.h"
#include "flashc.h"

#include "delay.h"
#include "midi_common.h"
#include "arp.h"
#include "notes.h"
#include "timers.h"
#include "util.h"

#include "monome.h"
#include "ii.h"
#include "i2c.h"
#include "gpio.h"
#include "dac.h"

#include "init_common.h"
#include "conf_tc_irq.h"
#include "interrupts.h"

// this
#include "main.h"
#include "ansible_midi.h"


#define MONO_PITCH_CV 0
#define MONO_VELOCITY_CV 1
#define MONO_PRESSURE_CV 2
#define MONO_MOD_CV 3
#define MONO_BEND_CV 3

// NB: default to a -2 octave shift for pitch to help people with
// small keyboards keep the osc pitch knob closer to mid range.
// -3277 == N -24 on tt
#define DEFAULT_PITCH_SHIFT -3277

//------------------------------
//------ types

typedef struct {
	u8 learning : 1;
	u8 note_idx : 3;
	u8 cc_idx : 3;
} fixed_learning_state_t;

typedef struct {
	u8 key1 : 1;
	u8 key1_consumed : 1;
	u8 key2 : 1;
	u8 key2_consumed : 1;
	u8 front : 1;
	u8 normaled : 1; // isn't this tracked elsewhere?
} key_state_t;

typedef enum {
	eClockInternal,
	eClockExternal,
	eClockMidi
} clock_source;

//------------------------------
//------ prototypes

static void write_midi_standard(void);
static void write_midi_arp(void);

static void midi_pitch(uint8_t n, uint16_t note, int16_t bend);
static uint16_t velocity_cv(u8 vel);
static uint16_t cc_cv(u8 value);

static void restore_midi_standard(void);

static void set_voice_allocation(voicing_mode v);
static void set_voice_slew(voicing_mode v, s16 slew);
static void set_voice_tune(voicing_mode v, s16 shift);

static void reset(void);

static void poly_note_on(u8 ch, u8 num, u8 vel);
static void poly_note_off(u8 ch, u8 num, u8 vel);
static void poly_pitch_bend(u8 ch, u16 bend);
static void poly_sustain(u8 ch, u8 val);
static void poly_control_change(u8 ch, u8 num, u8 val);
static void poly_panic(void);

static void mono_note_on(u8 ch, u8 num, u8 vel);
static void mono_note_off(u8 ch, u8 num, u8 vel);
static void mono_pitch_bend(u8 ch, u16 bend);
static void mono_sustain(u8 ch, u8 val);
static void mono_channel_pressure(u8 ch, u8 val);
static void mono_control_change(u8 ch, u8 num, u8 val);
static void mono_panic(void);

static void mono_rt_tick(void);
static void mono_rt_start(void);
static void mono_rt_stop(void);
static void mono_rt_continue(void);

static void multi_tr_set(u8 ch);
static void multi_tr_clr(u8 ch);

static void multi_note_on(u8 ch, u8 num, u8 vel);
static void multi_note_off(u8 ch, u8 num, u8 vel);
static void multi_pitch_bend(u8 ch, u16 bend);
static void multi_sustain(u8 ch, u8 val);
static void multi_control_change(u8 ch, u8 num, u8 val);

static void fixed_write_mapping(const fixed_mapping_t *dest, const fixed_mapping_t *src);
static void fixed_start_learning(void);
static bool fixed_finalize_learning(bool cancel);
static void fixed_note_on(u8 ch, u8 num, u8 vel);
static void fixed_note_off(u8 ch, u8 num, u8 vel);
static void fixed_control_change(u8 ch, u8 num, u8 val);

static void restore_midi_arp(void);

static void arp_state_set_hold(bool hold);

static void arp_rebuild(chord_t *c);
static void arp_reset(void);
static void arp_next_style(void);

static void arp_note_on(u8 ch, u8 num, u8 vel);
static void arp_note_off(u8 ch, u8 num, u8 vel);
static void arp_pitch_bend(u8 ch, u16 bend);
static void arp_control_change(u8 ch, u8 num, u8 val);
static void arp_rt_tick(void);
static void arp_rt_start(void);
static void arp_rt_stop(void);
static void arp_rt_continue(void);

static void player_note_on(u8 ch, u8 num, u8 vel);
static void player_note_off(u8 ch, u8 num, u8 vel);


//-----------------------------
//----- globals

// step = 16384.0 / (10 octave * 12.0 semitones per octave)
// [int(n * step) for n in xrange(0,128)]
const u16 SEMI14[128] = {
	0, 136, 273, 409, 546, 682, 819, 955, 1092, 1228, 1365, 1501, 1638,
	1774, 1911, 2048, 2184, 2321, 2457, 2594, 2730, 2867, 3003, 3140,
	3276, 3413, 3549, 3686, 3822, 3959, 4096, 4232, 4369, 4505, 4642,
	4778, 4915, 5051, 5188, 5324, 5461, 5597, 5734, 5870, 6007, 6144,
	6280, 6417, 6553, 6690, 6826, 6963, 7099, 7236, 7372, 7509, 7645,
	7782, 7918, 8055, 8192, 8328, 8465, 8601, 8738, 8874, 9011, 9147,
	9284, 9420, 9557, 9693, 9830, 9966, 10103, 10240, 10376, 10513, 10649,
	10786, 10922, 11059, 11195, 11332, 11468, 11605, 11741, 11878, 12014,
	12151, 12288, 12424, 12561, 12697, 12834, 12970, 13107, 13243, 13380,
	13516, 13653, 13789, 13926, 14062, 14199, 14336, 14472, 14609, 14745,
	14882, 15018, 15155, 15291, 15428, 15564, 15701, 15837, 15974, 16110,
	16247, 16384, 16520, 16657, 16793, 16930, 17066, 17203, 17339
};

// step = 16384.0 / (10 octave * 12.0 semitones per octave)
// semi_per_octave = step * 12
// bend_step = semi_per_octave / 512.0
// [int(n * bend_step) for n in xrange(0,512)]
const u16 BEND1_14[512] = {
	0, 3, 6, 9, 12, 16, 19, 22, 25, 28, 32, 35, 38, 41, 44, 48, 51, 54,
	57, 60, 64, 67, 70, 73, 76, 80, 83, 86, 89, 92, 96, 99, 102, 105,
	108, 112, 115, 118, 121, 124, 128, 131, 134, 137, 140, 144, 147,
	150, 153, 156, 160, 163, 166, 169, 172, 176, 179, 182, 185, 188,
	192, 195, 198, 201, 204, 208, 211, 214, 217, 220, 224, 227, 230,
	233, 236, 240, 243, 246, 249, 252, 256, 259, 262, 265, 268, 272,
	275, 278, 281, 284, 288, 291, 294, 297, 300, 304, 307, 310, 313,
	316, 320, 323, 326, 329, 332, 336, 339, 342, 345, 348, 352, 355,
	358, 361, 364, 368, 371, 374, 377, 380, 384, 387, 390, 393, 396,
	400, 403, 406, 409, 412, 416, 419, 422, 425, 428, 432, 435, 438,
	441, 444, 448, 451, 454, 457, 460, 464, 467, 470, 473, 476, 480,
	483, 486, 489, 492, 496, 499, 502, 505, 508, 512, 515, 518, 521,
	524, 528, 531, 534, 537, 540, 544, 547, 550, 553, 556, 560, 563,
	566, 569, 572, 576, 579, 582, 585, 588, 592, 595, 598, 601, 604,
	608, 611, 614, 617, 620, 624, 627, 630, 633, 636, 640, 643, 646,
	649, 652, 656, 659, 662, 665, 668, 672, 675, 678, 681, 684, 688,
	691, 694, 697, 700, 704, 707, 710, 713, 716, 720, 723, 726, 729,
	732, 736, 739, 742, 745, 748, 752, 755, 758, 761, 764, 768, 771,
	774, 777, 780, 784, 787, 790, 793, 796, 800, 803, 806, 809, 812,
	816, 819, 822, 825, 828, 832, 835, 838, 841, 844, 848, 851, 854,
	857, 860, 864, 867, 870, 873, 876, 880, 883, 886, 889, 892, 896,
	899, 902, 905, 908, 912, 915, 918, 921, 924, 928, 931, 934, 937,
	940, 944, 947, 950, 953, 956, 960, 963, 966, 969, 972, 976, 979,
	982, 985, 988, 992, 995, 998, 1001, 1004, 1008, 1011, 1014, 1017,
	1020, 1024, 1027, 1030, 1033, 1036, 1040, 1043, 1046, 1049, 1052,
	1056, 1059, 1062, 1065, 1068, 1072, 1075, 1078, 1081, 1084, 1088,
	1091, 1094, 1097, 1100, 1104, 1107, 1110, 1113, 1116, 1120, 1123,
	1126, 1129, 1132, 1136, 1139, 1142, 1145, 1148, 1152, 1155, 1158,
	1161, 1164, 1168, 1171, 1174, 1177, 1180, 1184, 1187, 1190, 1193,
	1196, 1200, 1203, 1206, 1209, 1212, 1216, 1219, 1222, 1225, 1228,
	1232, 1235, 1238, 1241, 1244, 1248, 1251, 1254, 1257, 1260, 1264,
	1267, 1270, 1273, 1276, 1280, 1283, 1286, 1289, 1292, 1296, 1299,
	1302, 1305, 1308, 1312, 1315, 1318, 1321, 1324, 1328, 1331, 1334,
	1337, 1340, 1344, 1347, 1350, 1353, 1356, 1360, 1363, 1366, 1369,
	1372, 1376, 1379, 1382, 1385, 1388, 1392, 1395, 1398, 1401, 1404,
	1408, 1411, 1414, 1417, 1420, 1424, 1427, 1430, 1433, 1436, 1440,
	1443, 1446, 1449, 1452, 1456, 1459, 1462, 1465, 1468, 1472, 1475,
	1478, 1481, 1484, 1488, 1491, 1494, 1497, 1500, 1504, 1507, 1510,
	1513, 1516, 1520, 1523, 1526, 1529, 1532, 1536, 1539, 1542, 1545,
	1548, 1552, 1555, 1558, 1561, 1564, 1568, 1571, 1574, 1577, 1580,
	1584, 1587, 1590, 1593, 1596, 1600, 1603, 1606, 1609, 1612, 1616,
	1619, 1622, 1625, 1628, 1632, 1635
};

// copy of nvram state for editing
static midi_standard_state_t standard_state;
static midi_arp_state_t arp_state;

static midi_behavior_t active_behavior = {0};
static midi_behavior_t player_behavior = {0};

// standard mode working state
static fixed_learning_state_t fixed_learn;
static note_pool_t notes[4];
static voice_flags_t flags[4];
static voice_state_t voice_state;

// arp mode working state
static chord_t chord;
static u8 chord_held_notes;
static bool chord_should_reset;
static arp_seq_t sequences[2];
static arp_seq_t *active_seq;
static arp_seq_t *next_seq;
static arp_player_t player[4];

// shared state
static s16 pitch_offset[4];
static s16 pitch_shift[4];
static midi_clock_t midi_clock;
static key_state_t key_state;
static clock_source sync_source;

void set_mode_midi(void) {
	switch(ansible_mode) {
	case mMidiStandard:
		print_dbg("\r\n> mode midi standard");
		standard_state = f.midi_standard_state;
		app_event_handlers[kEventKey] = &handler_StandardKey;
		app_event_handlers[kEventTr] = &handler_StandardTr;
		app_event_handlers[kEventTrNormal] = &handler_StandardTrNormal;
		app_event_handlers[kEventMidiPacket] = &handler_StandardMidiPacket;
		restore_midi_standard();
		if (!leader_mode) init_i2c_slave(II_MID_ADDR);
		process_ii = &ii_midi_standard;
		update_leds(1);
		break;
	case mMidiArp:
		print_dbg("\r\n> mode midi arp");
		arp_state = f.midi_arp_state;
		app_event_handlers[kEventKey] = &handler_ArpKey;
		app_event_handlers[kEventTr] = &handler_ArpTr;
		app_event_handlers[kEventTrNormal] = &handler_ArpTrNormal;
		app_event_handlers[kEventMidiPacket] = &handler_ArpMidiPacket;
		restore_midi_arp();
		clock = &clock_midi_arp;
		clock_set(arp_state.clock_period);
		if (!leader_mode) init_i2c_slave(II_ARP_ADDR);
		process_ii = &ii_midi_arp;
		update_leds(2);
		break;
	default:
		break;
	}

	if(connected == conMIDI) {
		app_event_handlers[kEventFrontShort] = &handler_MidiFrontShort;
		app_event_handlers[kEventFrontLong] = &handler_MidiFrontLong;
	}

	sync_source = eClockInternal;

	midi_clock_init(&midi_clock);
	midi_clock_set_div(&midi_clock, 4); // 16th notes (24 ppq / 4 == 6 pp 16th)

	key_state.key1 = key_state.key2 = key_state.front = 0;
	key_state.key1_consumed = key_state.key2_consumed = 0;
	key_state.normaled = !gpio_get_pin_value(B10);

	for (u8 i = 0; i < 4; i++) {
		pitch_offset[i] = 0;
		dac_set_value_noslew(i, 0);
	}
	dac_update_now();
}


void handler_MidiFrontShort(s32 data) {
	if (key_state.key2) {
		if (ansible_mode == mMidiStandard) {
			// save voice mode configuration to flash
			update_leds(0);
			write_midi_standard();
			print_dbg("\r\n standard: wrote midi config");
			update_leds(1);
		}
		else {
			// mMidiArp
			update_leds(0);
			write_midi_arp();
			print_dbg("\r\n arp: wrote config");
			update_leds(2);

		}
		key_state.key2_consumed = 1; // hide the release
	}
	else if (key_state.key1 && ansible_mode == mMidiStandard &&
					 standard_state.voicing == eVoiceFixed) {
		key_state.key1_consumed = 1;
		fixed_start_learning();
	}
}

void handler_MidiFrontLong(s32 data) {
	if (key_state.key2) {
		// panic sequence to reset standard/arp mode to defaults
		if (ansible_mode == mMidiStandard) {
			default_midi_standard();
			update_leds(0);
			delay_ms(50);
			update_leds(1);
			delay_ms(50);
			update_leds(0);
			delay_ms(50);
			set_mode(mMidiStandard);
			print_dbg("\r\n standard: wrote default config");
		}
		else {
			default_midi_arp();
			update_leds(0);
			delay_ms(50);
			update_leds(2);
			delay_ms(50);
			update_leds(0);
			delay_ms(50);
			set_mode(mMidiArp);
			print_dbg("\r\n arp: wrote default config");
		}
		key_state.key2_consumed = 1;
	}
	else {
		// normal mode switch
		if (ansible_mode == mMidiStandard)
			set_mode(mMidiArp);
		else
			set_mode(mMidiStandard);
	}
}


////////////////////////////////////////////////////////////////////////////////
///// common cv utilities

inline static void midi_pitch(uint8_t n, uint16_t note, int16_t bend) {
    set_cv_note(n, note, bend + pitch_shift[n]);
}

inline static uint16_t velocity_cv(u8 vel) {
	// 128 << 7 == 16384; 14-bit val, shift to 12-bit on dac update
	return vel << 7;
}

inline static uint16_t cc_cv(u8 value) {
	return value << 7;
}

static void reset(void) {
	// start from a clean slate
	for (int i = 0; i < 4; i++) {
		voice_flags_init(&(flags[i]));
		notes_init(&(notes[i]));
		pitch_offset[i] = 0;
		dac_set_value_noslew(i, 0);
	}
	dac_update_now();
	clr_tr(TR1);
	clr_tr(TR2);
	clr_tr(TR3);
	clr_tr(TR4);
}

static void set_voice_allocation(voicing_mode v) {
	reset();

	// config behavior, clock, flags, etc.
	switch (v) {
	case eVoicePoly:
		active_behavior.note_on = &poly_note_on;
		active_behavior.note_off = &poly_note_off;
		active_behavior.channel_pressure = NULL;
		active_behavior.pitch_bend = &poly_pitch_bend;
		active_behavior.control_change = &poly_control_change;
		active_behavior.clock_tick = NULL;
		active_behavior.seq_start = NULL;
		active_behavior.seq_stop = NULL;
		active_behavior.seq_continue = NULL;
		active_behavior.panic = &poly_panic;
		voice_slot_init(&voice_state, kVoiceAllocRotate, 4); // TODO: count configurable?
		clock = &clock_null;
		print_dbg("\r\n standard: voice poly");
		break;
	case eVoiceMono:
		active_behavior.note_on = &mono_note_on;
		active_behavior.note_off = &mono_note_off;
		active_behavior.channel_pressure = &mono_channel_pressure;
		active_behavior.pitch_bend = &mono_pitch_bend;
		active_behavior.control_change = &mono_control_change;
		active_behavior.clock_tick = &mono_rt_tick;
		active_behavior.seq_start = &mono_rt_start;
		active_behavior.seq_stop = &mono_rt_stop;
		active_behavior.seq_continue = &mono_rt_continue;
		active_behavior.panic = &mono_panic;
		clock = &clock_null;
		flags[0].legato = 1;
		mono_pitch_bend(0, MIDI_BEND_ZERO);
		print_dbg("\r\n standard: voice mono");
		break;
	case eVoiceMulti:
		active_behavior.note_on = &multi_note_on;
		active_behavior.note_off = &multi_note_off;
		active_behavior.channel_pressure = NULL;
		active_behavior.pitch_bend = &multi_pitch_bend;
		active_behavior.control_change = &multi_control_change;
		active_behavior.clock_tick = NULL;
		active_behavior.seq_start = NULL;
		active_behavior.seq_stop = NULL;
		active_behavior.seq_continue = NULL;
		active_behavior.panic = &reset;
		clock = &clock_null;
		flags[0].legato = flags[1].legato = flags[2].legato = flags[3].legato = 1;
		print_dbg("\r\n standard: voice multi");
		break;
	case eVoiceFixed:
		active_behavior.note_on = &fixed_note_on;
		active_behavior.note_off = &fixed_note_off;
		active_behavior.channel_pressure = NULL;
		active_behavior.pitch_bend = NULL;
		active_behavior.control_change = &fixed_control_change;
		active_behavior.clock_tick = NULL;
		active_behavior.seq_start = NULL;
		active_behavior.seq_stop = NULL;
		active_behavior.seq_continue = NULL;
		active_behavior.panic = &reset;
		clock = &clock_null;
		print_dbg("\r\n standard: voice fixed");
		break;
	default:
		print_dbg("\r\n standard: bad voice mode");
	}
}

static void set_voice_slew(voicing_mode v, s16 slew) {
	u8 i;

	switch (v) {
	case eVoicePoly:
	case eVoiceMulti:
		for (i = 0; i < 4; i++) {
			dac_set_slew(i, slew);
		}
		break;
	case eVoiceMono:
		dac_set_slew(0, slew);  // pitch
		dac_set_slew(1, 0);     // velocity
		dac_set_slew(2, 5);     // channel pressure
		dac_set_slew(3, 5);     // mod
		break;
	default:
		for (i = 0; i < 4; i++) {
			dac_set_slew(i, 0);
		}
		break;
	}
}

static void set_voice_tune(voicing_mode v, s16 shift) {
	u8 i;

	switch (v) {
	case eVoicePoly:
	case eVoiceMulti:
		for (i = 0; i < 4; i++) {
			pitch_shift[i] = shift;
		}
		break;
	case eVoiceMono:
		pitch_shift[0] = shift;  // pitch
		pitch_shift[1] = 0;      // velocity
		pitch_shift[2] = 0;      // channel pressure
		pitch_shift[3] = 0;      // mod
		break;
	default:
		for (i = 0; i < 4; i++) {
			pitch_shift[i] = 0;
		}
		break;
	}
}


////////////////////////////////////////////////////////////////////////////////
///// handlers (standard)
static void restore_midi_standard(void) {
	set_voice_allocation(standard_state.voicing);
	set_voice_slew(standard_state.voicing, standard_state.slew);
	set_voice_tune(standard_state.voicing, standard_state.shift);
	clock_set(standard_state.clock_period);
}

void default_midi_standard(void) {
	fixed_mapping_t m;

	flashc_memset32((void*)&(f.midi_standard_state.clock_period), 100, 4, true);
	flashc_memset8((void*)&(f.midi_standard_state.voicing), eVoicePoly, 1, true);

	m.notes[0] = 60; // C4
	m.notes[1] = 62; // D4
	m.notes[2] = 64; // E4
	m.notes[3] = 65; // F4

	m.cc[0] = 16;
	m.cc[1] = 17;
	m.cc[2] = 18;
	m.cc[3] = 19;

	fixed_write_mapping(&(f.midi_standard_state.fixed), &m);

	flashc_memset16((void*)&(f.midi_standard_state.shift), DEFAULT_PITCH_SHIFT, 2, true);
	flashc_memset16((void*)&(f.midi_standard_state.slew), 0, 2, true);
}

void write_midi_standard(void) {
	flashc_memset32((void*)&(f.midi_standard_state.clock_period),
									standard_state.clock_period, 4, true);
	flashc_memset8((void*)&(f.midi_standard_state.voicing),
								 standard_state.voicing, 1, true);

	fixed_write_mapping(&(f.midi_standard_state.fixed), &standard_state.fixed);

	flashc_memset16((void*)&(f.midi_standard_state.shift),
									standard_state.shift, 2, true);
	flashc_memset16((void*)&(f.midi_standard_state.slew),
									standard_state.slew, 2, true);

}

void clock_midi_standard(uint8_t phase) {
	if (phase)
		set_tr(TR4);
	else
		clr_tr(TR4);
}

void ii_midi_standard(uint8_t *d, uint8_t l) {
	s16 s;

	if (l) {
		switch (d[0]) {
		case II_MID_SLEW:
			s = sclip((int16_t)((d[1] << 8) + d[2]), 0, 20000);

			// print_dbg("\r\nmid ii slew: ");
			// print_dbg_ulong(s);

			standard_state.slew = s;
			set_voice_slew(standard_state.voicing, s);
			break;

		case II_MID_SHIFT:
			s = (int16_t)((d[1] << 8) + d[2]);

			// print_dbg("\r\nmid ii shift: ");
			// print_dbg_hex(s);

			standard_state.shift = s;
			set_voice_tune(standard_state.voicing, s);
			break;

		default:
			ii_ansible(d, l);
			break;
		}
	}
}

void handler_StandardKey(s32 data) {
	switch (data) {

	case 0: // key 1 release
		key_state.key1 = 0;
		if (key_state.key1_consumed) {
			key_state.key1_consumed = 0;
		}
		else {
			if (fixed_learn.learning == 1) {
				// cancel learning
				fixed_finalize_learning(true);
			}
			else if (active_behavior.panic) {
				// panic, all notes off
				(*active_behavior.panic)();
			}
		}
		break;

	case 1:
		key_state.key1 = 1;
		break;

	case 2: // key 2 release
		key_state.key2 = 0;
		if (key_state.key2_consumed) {
			key_state.key2_consumed = 0;
		}
		else {
			if (fixed_learn.learning == 1) {
				// in learning mode; do nothing
			}
			else {
				// switch voicing mode
				standard_state.voicing++;
				if (standard_state.voicing >= eVoiceMAX) {
					standard_state.voicing = eVoicePoly;
				}
				set_voice_allocation(standard_state.voicing);
				set_voice_slew(standard_state.voicing, standard_state.slew);
				set_voice_tune(standard_state.voicing, standard_state.shift);
			}
		}
		break;

	case 3:
		key_state.key2 = 1;
		break;
	}
}

void handler_StandardTr(s32 data) {
}

void handler_StandardTrNormal(s32 data) {
	key_state.normaled = data;
}

void handler_StandardMidiPacket(s32 data) {
	midi_packet_parse(&active_behavior, (u32)data);
}

////////////////////////////////////////////////////////////////////////////////
///// poly behavior (standard)

static void poly_note_on(u8 ch, u8 num, u8 vel) {
	u8 slot = voice_slot_next(&voice_state);

	if (voice_slot_active(&voice_state, slot)) {
		// steal slot
		multi_tr_clr(slot);
		// FIXME: here we clear tr but then immediately reset it for note
		// on so the envelop wont retrigger. need some way to queue or
		// pause long enough to have tr retrigger.
		voice_slot_release(&voice_state, slot);
	}

	voice_slot_activate(&voice_state, slot, num);
	multi_note_on(slot, num, vel);
}

static void poly_note_off(u8 ch, u8 num, u8 vel) {
	s8 slot = voice_slot_find(&voice_state, num);
	if (slot != -1) {
		if (!flags[slot].sustain) {
			multi_tr_clr(slot);
			voice_slot_release(&voice_state, slot);
		}
	}
}

static void poly_pitch_bend(u8 ch, u16 bend) {
	// MAINT: assuming bend is always 14 bit...
	if (bend == MIDI_BEND_ZERO) {
		pitch_offset[0] = 0;
	}
	else if (bend > MIDI_BEND_ZERO) {
		bend -= MIDI_BEND_ZERO;
		pitch_offset[0] = BEND1_14[bend >> 4];
	}
	else {
		bend = MIDI_BEND_ZERO - bend - 1;
		pitch_offset[0] = -BEND1_14[bend >> 4];
	}

	for (u8 i = 0; i < voice_state.count; i++) {
		if (voice_slot_active(&voice_state, i)) {
			midi_pitch(i, voice_slot_num(&voice_state, i), pitch_offset[0]);
		}
	}
	dac_update_now();
}

static void poly_sustain(u8 ch, u8 val) {
	for (u8 slot = 0; slot < MAX_VOICE_COUNT; slot++) {
		if (val < 64) {
			// release active voices
			if (voice_slot_active(&voice_state, slot)) {
				multi_tr_clr(slot);
				voice_slot_release(&voice_state, slot);
			}
			flags[slot].sustain = 0;
		}
		else {
			flags[slot].sustain = 1;
		}
	}
}

static void poly_control_change(u8 ch, u8 num, u8 val) {
	switch (num) {
		case 64:  // sustain pedal
			poly_sustain(ch, val);
			break;
	}
}

static void poly_panic(void) {
	voice_slot_init(&voice_state, voice_state.mode, voice_state.count);
	reset();
}


////////////////////////////////////////////////////////////////////////////////
///// mono behavior (standard)

static void mono_note_on(u8 ch, u8 num, u8 vel) {
	if (num > MIDI_NOTE_MAX)
		return;

	// keep track of held notes for legato and pitch bend
	notes_hold(&notes[0], num, vel);
	midi_pitch(MONO_PITCH_CV, num, pitch_offset[0]);
	dac_set_value_noslew(MONO_VELOCITY_CV, velocity_cv(vel));
	dac_update_now();
	set_tr(TR1);
}

static void mono_note_off(u8 ch, u8 num, u8 vel) {
	const held_note_t *prior;

	if (num > MIDI_NOTE_MAX)
		return;

	if (flags[0].sustain == 0) {
		notes_release(&notes[0], num);
		prior = notes_get(&notes[0], kNotePriorityLast);
		if (prior) {
			midi_pitch(MONO_PITCH_CV, prior->num, pitch_offset[0]);
			dac_set_value(MONO_VELOCITY_CV, velocity_cv(prior->vel));
			dac_update_now();
		}
		else {
			clr_tr(TR1);
		}
	}
}

static void mono_pitch_bend(u8 ch, u16 bend) {
	// MAINT: assuming bend is always 14 bit...
	if (bend == MIDI_BEND_ZERO) {
		pitch_offset[0] = 0;
	}
	else if (bend > MIDI_BEND_ZERO) {
		bend -= MIDI_BEND_ZERO;
		pitch_offset[0] = BEND1_14[bend >> 4];
	}
	else {
		bend = MIDI_BEND_ZERO - bend - 1;
		pitch_offset[0] = -BEND1_14[bend >> 4];
	}

	// re-set pitch to pick up changed offset
	const held_note_t *active = notes_get(&(notes[0]), kNotePriorityLast);
	if (active) {
		midi_pitch(MONO_PITCH_CV, active->num, pitch_offset[0]);
		dac_update_now();
	}
}

static void mono_sustain(u8 ch, u8 val) {
	if (val < 64) {
		clr_tr(TR2);
		flags[0].sustain = 0;
		// release any held notes
		notes_init(&notes[0]);
		clr_tr(TR1);
	}
	else {
		set_tr(TR2);
		flags[0].sustain = 1;
	}
}

static void mono_channel_pressure(u8 ch, u8 val) {
	dac_set_value_noslew(MONO_PRESSURE_CV, cc_cv(val));
	dac_update_now();
}

static void mono_generic(u8 ch, u8 val) {
	(val < 64) ? clr_tr(TR3) : set_tr(TR3);
}

static void mono_control_change(u8 ch, u8 num, u8 val) {
		switch (num) {
		case 1:  // mod wheel
			dac_set_value_noslew(MONO_MOD_CV, cc_cv(val));
			dac_update_now();
			break;
		case 64:  // sustain pedal
			mono_sustain(ch, val);
			break;
		case 80:  // generic
			mono_generic(ch, val);
		default:
			break;
	}
}

static void mono_rt_tick(void) {
	static u32 previous = 0;
	u32 now, average;

	if (key_state.normaled) {
		// external sync on;
		return;
	}

	now = time_now();
	if (previous == 0) {
		previous = now;
	}
	// smooth it? given all the polling tempo changes jump around
	average = (now + previous) >> 1;
	previous = now;

	midi_clock_pulse(&midi_clock, average);
	time_clear();

	//print_dbg("\r\n mono_rt_tick: ");
	//print_dbg_ulong(midi_clock.trigger);
	//print_dbg(" p: ");
	//print_dbg_ulong(midi_clock.pulse_period);

	if (midi_clock.trigger) {
		set_tr(TR4);
	}
	else {
		clr_tr(TR4);
	}
}

static void mono_rt_start(void) {
	print_dbg("\r\n mono_rt_start()");
	midi_clock_start(&midi_clock);
}

static void mono_rt_stop(void) {
	print_dbg("\r\n mono_rt_stop()");
	midi_clock_stop(&midi_clock);
	clr_tr(TR4);
}

static void mono_rt_continue(void) {
	print_dbg("\r\n mono_rt_continue()");
	midi_clock_continue(&midi_clock);
}

static void mono_panic(void) {
	midi_clock_stop(&midi_clock);
	reset();
}


////////////////////////////////////////////////////////////////////////////////
///// multi behavior (standard)

inline static void multi_tr_set(u8 ch) {
	switch (ch) {
	case 0:
		set_tr(TR1);
		break;
	case 1:
		set_tr(TR2);
		break;
	case 2:
		set_tr(TR3);
		break;
	case 3:
		set_tr(TR4);
		break;
	}
}

inline static void multi_tr_clr(u8 ch) {
	switch (ch) {
	case 0:
		clr_tr(TR1);
		break;
	case 1:
		clr_tr(TR2);
		break;
	case 2:
		clr_tr(TR3);
		break;
	case 3:
		clr_tr(TR4);
		break;
	}
}

static void multi_note_on(u8 ch, u8 num, u8 vel) {
	if (ch > 3 || num > MIDI_NOTE_MAX)
		return;

	notes_hold(&notes[ch], num, vel);
	midi_pitch(ch, num, pitch_offset[ch]);
	dac_update_now();
	multi_tr_set(ch);
}

static void multi_note_off(u8 ch, u8 num, u8 vel) {
	const held_note_t *prior;

	if (ch > 3 || num > MIDI_NOTE_MAX)
		return;

	if (flags[ch].sustain == 0) {
		notes_release(&notes[ch], num);
		if (flags[ch].legato) {
			prior = notes_get(&notes[ch], kNotePriorityLast);
			if (prior) {
				midi_pitch(ch, prior->num, pitch_offset[ch]);
				dac_update_now();
			}
			else {
				multi_tr_clr(ch);
			}
		}
		else {
			// legato is off
			multi_tr_clr(ch);
		}
	}
}

static void multi_pitch_bend(u8 ch, u16 bend) {
	if (ch > 3)
		return;

	// MAINT: assuming bend is always 14 bit...
	if (bend == MIDI_BEND_ZERO) {
		pitch_offset[ch] = 0;
	}
	else if (bend > MIDI_BEND_ZERO) {
		bend -= MIDI_BEND_ZERO;
		pitch_offset[ch] = BEND1_14[bend >> 4];
	}
	else {
		bend = MIDI_BEND_ZERO - bend - 1;
		pitch_offset[ch] = -BEND1_14[bend >> 4];
	}

	// re-set pitch to pick up changed offset
	const held_note_t *active = notes_get(&(notes[ch]), kNotePriorityLast);
	if (active) {
		midi_pitch(ch, active->num, pitch_offset[ch]);
		dac_update_now();
	}
}

static void multi_sustain(u8 ch, u8 val) {
	if (ch > 3)
		return;

	if (val < 64) {
		multi_tr_clr(ch);
		flags[ch].sustain = 0;
		if (flags[ch].legato) {
			notes_init(&(notes[ch]));
		}
	}
	else {
		flags[ch].sustain = 1;
	}
}

static void multi_control_change(u8 ch, u8 num, u8 val) {
	if (ch > 3)
		return;

	switch (num) {
		case 64:  // sustain pedal
			multi_sustain(ch, val);
			break;
		default:
			break;
	}
}


////////////////////////////////////////////////////////////////////////////////
///// fixed behavior (standard)

static void fixed_write_mapping(const fixed_mapping_t *dest, const fixed_mapping_t *src) {
	for (u8 i = 0; i < 4; i++) {
		flashc_memset8((void*)&(dest->notes[i]), src->notes[i], 1, true);
		flashc_memset8((void*)&(dest->cc[i]), src->cc[i], 1, true);
	}
}

static void fixed_start_learning(void) {
	print_dbg("\r\n standard: start fixed mode learn");
	fixed_learn.learning = 1;
	fixed_learn.note_idx = 0;
	fixed_learn.cc_idx = 0;
	for (u8 i = 0; i < 4; i++) {
		multi_tr_clr(i);
		dac_set_value_noslew(i, 0);
	}
	dac_update_now();
}

static bool fixed_finalize_learning(bool cancel) {
	if (cancel || (fixed_learn.note_idx == 4 && fixed_learn.cc_idx == 4)) {
		print_dbg("\r\n standard: fixed learning complete");
		fixed_learn.learning = 0;
		// clear all tr and cv
		for (u8 i = 0; i < 4; i++) {
			multi_tr_clr(i);
			dac_set_value_noslew(i, 0);
			print_dbg("\r\n n: ");
			print_dbg_ulong(standard_state.fixed.notes[i]);
			print_dbg(" cc: ");
			print_dbg_ulong(standard_state.fixed.cc[i]);
		}
		dac_update_now();

		if (!cancel) {
			fixed_write_mapping(&(f.midi_standard_state.fixed), &(standard_state.fixed));
		}
		return true;
	}

	// learning not finalized
	return false;
}

static void fixed_note_on(u8 ch, u8 num, u8 vel) {
	static bool initial_set = false;

	if (fixed_learn.learning) {
		if (fixed_learn.note_idx < 4) {
			if (fixed_learn.note_idx == 0 && initial_set == false) {
				initial_set = true;
				standard_state.fixed.notes[fixed_learn.note_idx] = num;
				multi_tr_set(fixed_learn.note_idx); // indicate mapping
			}

			// different note; advance to next mapping
			if (num != standard_state.fixed.notes[fixed_learn.note_idx]) {
				fixed_learn.note_idx++;
				if (fixed_learn.note_idx < 4) {
					standard_state.fixed.notes[fixed_learn.note_idx] = num;
					multi_tr_set(fixed_learn.note_idx); // indicate mapping
					if (fixed_learn.note_idx == 3) {
						fixed_learn.note_idx++;
					}
				}
			}
		}

		if (fixed_finalize_learning(false)) {
			// prep for next learning pass
			initial_set = false;
		}
	}
	else {
		for (u8 i = 0; i < 4; i++) {
			if (standard_state.fixed.notes[i] == num) {
				multi_tr_set(i);
				// TODO: should these be gates or triggers?
				break;
			}
		}
	}
}

static void fixed_note_off(u8 ch, u8 num, u8 vel) {
	if (!fixed_learn.learning) {
		for (u8 i = 0; i < 4; i++) {
			if (standard_state.fixed.notes[i] == num) {
				multi_tr_clr(i);
				break;
			}
		}
	}
}

static void fixed_control_change(u8 ch, u8 num, u8 val) {
	static bool initial_set = false;

	if (fixed_learn.learning) {
		if (fixed_learn.cc_idx < 4) {
			// initial state; first num always sets first channel
			if (fixed_learn.cc_idx == 0 && initial_set == false) {
				initial_set = true;
				standard_state.fixed.cc[fixed_learn.cc_idx] = num;
			}

			// different cc; advance to next mapping
			if (num != standard_state.fixed.cc[fixed_learn.cc_idx]) {
				fixed_learn.cc_idx++;
				if (fixed_learn.cc_idx < 4) {
					standard_state.fixed.cc[fixed_learn.cc_idx] = num;
					if (fixed_learn.cc_idx == 3) {
						// immediately end learning instead of waiting around for
						// a different cc to be sent..
						fixed_learn.cc_idx++;
					}
				}
			}
			// update outputs to provide feedback
			if (fixed_learn.cc_idx < 4) {
				dac_set_value_noslew(fixed_learn.cc_idx, cc_cv(val));
				dac_update_now();
			}
		}
		if (fixed_finalize_learning(false)) {
			// prepare for next learning pass
			initial_set = false;
		}
	}
	else {
		for (u8 i = 0; i < 4; i++) {
			if (standard_state.fixed.cc[i] == num) {
				dac_set_value_noslew(i, cc_cv(val));
				dac_update_now();
				break;
			}
		}
	}
}



////////////////////////////////////////////////////////////////////////////////

void default_midi_arp() {
	flashc_memset32((void*)&(f.midi_arp_state.clock_period), 100, 4, true);
	flashc_memset8((void*)&(f.midi_arp_state.style), eStylePlayed, 1, true);
	flashc_memset8((void*)&(f.midi_arp_state.hold), 0, 1, true);

	for (u8 i = 0; i < 4; i++) {
		flashc_memset8((void*)&(f.midi_arp_state.p[i].fill), 1, 1, true);
		flashc_memset8((void*)&(f.midi_arp_state.p[i].division), i + 1, 1, true);
		flashc_memset8((void*)&(f.midi_arp_state.p[i].rotation), 0, 1, true);

		flashc_memset8((void*)&(f.midi_arp_state.p[i].gate), 0, 1, true);
		flashc_memset8((void*)&(f.midi_arp_state.p[i].steps), 0, 1, true);
		flashc_memset8((void*)&(f.midi_arp_state.p[i].offset), 12, 1, true);

		flashc_memset16((void*)&(f.midi_arp_state.p[i].slew), 0, 2, true);
		flashc_memset16((void*)&(f.midi_arp_state.p[i].shift), DEFAULT_PITCH_SHIFT, 2, true);
	}
}

void write_midi_arp(void) {
	arp_player_t *p;

	flashc_memset32((void*)&(f.midi_arp_state.clock_period),
									arp_state.clock_period, 4, true);
	flashc_memset8((void*)&(f.midi_arp_state.style),
								 arp_state.style, 1, true);
	flashc_memset8((void*)&(f.midi_arp_state.hold),
								 arp_state.hold, 1, true);

	for (u8 i = 0; i < 4; i++) {
		p = &(player[i]);
		flashc_memset8((void*)&(f.midi_arp_state.p[i].fill),
									 arp_player_get_fill(p), 1, true);
		flashc_memset8((void*)&(f.midi_arp_state.p[i].division),
									 arp_player_get_division(p), 1, true);
		flashc_memset8((void*)&(f.midi_arp_state.p[i].rotation),
									 arp_player_get_rotation(p), 1, true);

		flashc_memset8((void*)&(f.midi_arp_state.p[i].gate),
									 arp_player_get_gate_width(p), 1, true);
		flashc_memset8((void*)&(f.midi_arp_state.p[i].steps),
									 arp_player_get_steps(p), 1, true);
		flashc_memset8((void*)&(f.midi_arp_state.p[i].offset),
									 arp_player_get_offset(p), 1, true);

		flashc_memset16((void*)&(f.midi_arp_state.p[i].slew),
										dac_get_slew(i), 2, true);
		flashc_memset16((void*)&(f.midi_arp_state.p[i].shift),
										dac_get_off(i), 2, true);
	}
}

static void arp_next_style(void) {
	arp_state.style++;
	if (arp_state.style > eStyleRandom) {
		arp_state.style = eStylePlayed;
	}
	print_dbg("\r\n arp style: ");
	print_dbg_ulong(arp_state.style);
	arp_rebuild(&chord);
}

static bool arp_seq_switch_active(void) {
	// TODO: better abstract this and move to arp.c
	arp_seq_t *last_seq;
	bool switched = false;

	// disable interrupts
	u8 irq_flags = irqs_pause();

	if (next_seq->state == eSeqWaiting) {
		next_seq->state = eSeqPlaying;
		active_seq->state = eSeqFree;
		last_seq = active_seq;
		active_seq = next_seq;
		next_seq = last_seq;
		switched = true;
	}

	// enable interrupts
	irqs_resume(irq_flags);

	return switched;
}

static void arp_clock_pulse(uint8_t phase) {
	if (phase) {
		arp_seq_switch_active();
	}

	for (u8 i = 0; i < 4; i++) {
		arp_player_pulse(&(player[i]), active_seq, &player_behavior, phase);
	}

	// NB: forcing a dac update so that when there is no slewing cv
	// changes ~30us after the tr goes high. without this cv change is
	// delayed ~1ms or more based on the update timer freq. that said
	// doing this does result in cv arriving at the target before
	// slew_ms since the first slew step is executed here...
	//
	// calling the timer update function is safe since this function is
	// running at interrupt level as well.
	dac_timer_update();
}

void clock_midi_arp(uint8_t phase) {
	// internal clock timer callback, pulse clock if we are not being
	// driven externally
	if (sync_source == eClockInternal) {
		arp_clock_pulse(phase);
	}
}

void ii_midi_arp(uint8_t *d, uint8_t l) {
	arp_player_t *p;
	u8 i, v, p1, p2;
	s16 s;

	if (l) {
		switch (d[0]) {
		case II_ARP_STYLE:
			p1 = uclip(d[1], eStylePlayed, eStyleRandom);

			// print_dbg("\r\narp ii style: ");
			// print_dbg_ulong(p1);

			arp_state.style = p1;
			arp_rebuild(&chord);
			break;

		case II_ARP_HOLD:
			// print_dbg("\r\narp ii hold: ");
			// print_dbg_ulong(d[1]);

			arp_state_set_hold(d[1] > 0);
			break;

		case II_ARP_RPT:
			v = uclip(d[1], 0, 4);
			p1 = uclip(d[2], 0, 8);
			s = sclip((int16_t)((d[3] << 8) + d[4]), -24, 24);

			// print_dbg("\r\narp ii rpt: ");
			// print_dbg_ulong(v);
			// print_dbg(" ");
			// print_dbg_ulong(p1);
			// print_dbg(" ");
			// print_dbg_hex(s);

			if (v == 0) {
				for (i = 0; i < 4; i++)
					arp_player_set_steps(&(player[i]), p1);
					arp_player_set_offset(&(player[i]), s);
			}
			else {
				arp_player_set_steps(&(player[v-1]), p1);
				arp_player_set_offset(&(player[v-1]), s);
			}
			break;

		case II_ARP_GATE:
			v = uclip(d[1], 0, 4);
			p1 = uclip(d[2], 0, 127);

			// print_dbg("\r\narp ii gate: ");
			// print_dbg_ulong(v);
			// print_dbg(" ");
			// print_dbg_ulong(p1);
			// FIXME: the gate width input range is 0-127, should tt range
			// be non-midi like say 0-100?

			if (v == 0) {
				for (i = 0; i < 4; i++)
					arp_player_set_gate_width(&(player[i]), p1);
			}
			else {
				arp_player_set_gate_width(&(player[v-1]), p1);
			}
			break;

		case II_ARP_DIV:
			v = uclip(d[1], 0, 4);
			p1 = uclip(d[2], 1, 32);  // NB: 32 is maximum for euclidean tables

			// print_dbg("\r\narp ii div: ");
			// print_dbg_ulong(v);
			// print_dbg(" ");
			// print_dbg_ulong(p1);

			if (v == 0) {
				for (i = 0; i < 4; i++)
					arp_player_set_division(&(player[i]), p1, &player_behavior);
			}
			else {
				arp_player_set_division(&(player[v-1]), p1, &player_behavior);
			}
			break;

		case II_ARP_FILL:
			v = uclip(d[1], 0, 4);
			p1 = uclip(d[2], 0, 32);  // NB: 32 is maximum for euclidean tables

			// print_dbg("\r\narp ii fill: ");
			// print_dbg_ulong(v);
			// print_dbg(" ");
			// print_dbg_ulong(p1);

			if (v == 0) {
				for (i = 0; i < 4; i++)
					arp_player_set_fill(&(player[i]), p1);
			}
			else {
				arp_player_set_fill(&(player[v-1]), p1);
			}
			break;

		case II_ARP_ROT:
			v = uclip(d[1], 0, 4);
			s = sclip((int16_t)((d[2] << 8) + d[3]), -32, 32);

			// print_dbg("\r\narp ii rot: ");
			// print_dbg_ulong(v);
			// print_dbg(" ");
			// print_dbg_hex(s);

			if (v == 0) {
				for (i = 0; i < 4; i++)
					arp_player_set_rotation(&(player[i]), s);
			}
			else {
				arp_player_set_rotation(&(player[v-1]), s);
			}
			break;

		case II_ARP_ER:
			v = uclip(d[1], 0, 4);
			p1 = uclip(d[2], 0, 32);  // NB: 32 is maximum for euclidean tables
			p2 = uclip(d[3], 1, 32);
			s = sclip((int16_t)((d[4] << 8) + d[5]), -32, 32);

			// print_dbg("\r\narp ii er: ");
			// print_dbg_ulong(v);
			// print_dbg(" ");
			// print_dbg_ulong(p1);
			// print_dbg(" ");
			// print_dbg_ulong(p2);
			// print_dbg(" ");
			// print_dbg_hex(s);

			if (v == 0) {
				for (i = 0; i < 4; i++) {
					p = &(player[i]);
					arp_player_set_division(p, p2, &player_behavior);
					arp_player_set_fill(p, p1);
					arp_player_set_rotation(p, s);
				}
			}
			else {
				p = &(player[v-1]);
				arp_player_set_division(p, p2, &player_behavior);
				arp_player_set_fill(p, p1);
				arp_player_set_rotation(p, s);
			}
			break;

		case II_ARP_SLEW:
			v = uclip(d[1], 0, 4);
			s = sclip((int16_t)((d[2] << 8) + d[3]), 0, 20000);

			// print_dbg("\r\narp ii slew: ");
			// print_dbg_ulong(v);
			// print_dbg(" ");
			// print_dbg_ulong(s);

			if (v == 0) {
				for (i = 0; i < 4; i++)
					dac_set_slew(i, s);
			}
			else {
				dac_set_slew(v-1, s);
			}
			break;

		case II_ARP_RESET:
			v = uclip(d[1], 0, 4);

			// print_dbg("\r\narp ii reset: ");
			// print_dbg_ulong(v);

			if (v == 0) {
				for (i = 0; i < 4; i++)
					arp_player_reset(&(player[i]), &player_behavior);
			}
			else {
				arp_player_reset(&(player[v-1]), &player_behavior);
			}
			break;

		case II_ARP_SHIFT:
			v = uclip(d[1], 0, 4);
			s = (int16_t)((d[2] << 8) + d[3]);

			// print_dbg("\r\narp ii shift: ");
			// print_dbg_ulong(v);
			// print_dbg(" ");
			// print_dbg_hex(s);

			if (v == 0) {
				for (i = 0; i < 4; i++)
					pitch_shift[i] = s;
			}
			else {
				pitch_shift[v-1] = s;
			}
			break;

		default:
			ii_ansible(d, l);
			break;
		}
	}
}

void handler_ArpKey(s32 data) {
	static bool tapped = false;
	u32 now;

	switch (data) {
	case 0:
		// key 1 release
		key_state.key1 = 0;
		break;
	case 1:
		// key 1 press: tap tempo / force internal clock or toggle hold
		key_state.key1 = 1;
		if (key_state.key2 == 1) {
			arp_state_set_hold(!arp_state.hold);
			key_state.key2 = 0; // goofy; use this to signal to case 2 that style should change
		}
		else {
			if (tapped) {
				if (sync_source == eClockMidi) {
					sync_source = eClockInternal;
				}
				tapped = false;
				now = time_now();
				now = uclip(now >> 1, 23, 1000); // range in ms
				print_dbg("\r\n arp tap: ");
				print_dbg_ulong(now);
				arp_state.clock_period = now;
				clock_set_tr(now, 0);
			}
			else {
				tapped = true;
				time_clear();
			}
		}
		break;
	case 2:
		// key 2 release
		if (key_state.key2_consumed) {
			key_state.key2_consumed = 0;
		}
		else {
			if (key_state.key1 == 0 && key_state.key2 == 1) {
				// arp mode swetch on release if not toggling hold mode
				arp_next_style();
			}
		}
		key_state.key2 = 0;
		break;
	case 3:
		// key 2 press
		key_state.key2 = 1;
		break;
	}
}

void handler_ArpTr(s32 data) {
	u32 now;

	switch (data) {
	case 0:
		// tr 1 lo
		arp_clock_pulse(0);
		break;
	case 1:
		// tr 1 hi; sync internal clock timer
		now = time_now();
		time_clear();
		now = now >> 1; // high/low phase
		// TODO: clip now to low/high bounds?
		clock_set_tr(now, 0);
		arp_clock_pulse(1);
		break;
	case 2:
		// tr 2 lo; nothing
		break;
	case 3:
		// tr 2 hi; reset
		for (u8 i = 0; i < 4; i++) {
			arp_player_reset(&player[i], &player_behavior);
		}
		break;
	}
}

void handler_ArpTrNormal(s32 data) {
	print_dbg("\r\n> arp tr normal ");
	print_dbg_ulong(data);

	switch (data) {
	case 0:
		key_state.normaled = 0;
		sync_source = eClockInternal;
		break;
	case 1:
		key_state.normaled = 1;
		sync_source = eClockExternal;
		break;
	}
}

void handler_ArpMidiPacket(s32 data) {
	midi_packet_parse(&active_behavior, (u32)data);
}

void restore_midi_arp(void) {
	arp_player_t *p;

	chord_init(&chord);
	chord_held_notes = 0;
	chord_should_reset = false;

	// ping pong
	active_seq = &sequences[0];
	next_seq = &sequences[1];
	arp_seq_init(active_seq);
	arp_seq_init(next_seq);

	// ensure style matches stored config
	arp_seq_build(active_seq, arp_state.style, &chord, &(notes[0]));
	arp_seq_build(next_seq, arp_state.style, &chord, &(notes[0]));

	for (u8 i = 0; i < 4; i++) {
		p = &(player[i]);
		arp_player_init(p, i, arp_state.p[i].division);
		arp_player_set_gate_width(p, arp_state.p[i].gate);
		arp_player_set_steps(p, arp_state.p[i].steps);
		arp_player_set_offset(p, arp_state.p[i].offset);
		arp_player_set_fill(p, arp_state.p[i].fill);

		pitch_shift[i] = arp_state.p[i].shift;
		dac_set_slew(i, arp_state.p[i].slew);
	}

	active_behavior.note_on = &arp_note_on;
	active_behavior.note_off = &arp_note_off;
	active_behavior.channel_pressure = NULL;
	active_behavior.pitch_bend = &arp_pitch_bend;
	active_behavior.control_change = &arp_control_change;
	active_behavior.clock_tick = &arp_rt_tick;
	active_behavior.seq_start = &arp_rt_start;
	active_behavior.seq_stop = &arp_rt_stop;
	active_behavior.seq_continue = &arp_rt_continue;
	active_behavior.panic = NULL;

	player_behavior.note_on = &player_note_on;
	player_behavior.note_off = &player_note_off;
	player_behavior.channel_pressure = NULL;
	player_behavior.pitch_bend = NULL;
	player_behavior.control_change = NULL;
	player_behavior.clock_tick = NULL;
	player_behavior.seq_start = NULL;
	player_behavior.seq_stop = NULL;
	player_behavior.seq_continue = NULL;
	player_behavior.panic = NULL;
}

static void arp_state_set_hold(bool hold) {
	if (hold != arp_state.hold) {
		arp_state.hold = hold;
		print_dbg("\r\n arp hold: ");
		print_dbg_ulong(arp_state.hold);

		if (arp_state.hold) {
			// entering hold mode, preserve chord
			chord_held_notes = chord.note_count;
		}
		else {
			// existing hold mode, reset arp
			arp_reset();
		}
	}
}

static void arp_rebuild(chord_t *c) {
	arp_seq_state current_state = arp_seq_get_state(next_seq);

	if (current_state == eSeqFree || current_state == eSeqWaiting) {
		arp_seq_set_state(next_seq, eSeqBuilding); // TODO: check return
		arp_seq_build(next_seq, arp_state.style, &chord, &(notes[0]));
		arp_seq_set_state(next_seq, eSeqWaiting);
	}
	else {
		print_dbg("\r\n > arp: next seq busy, skipping build");
	}
}

static void arp_reset(void) {
	// used when exiting held mode
	chord_should_reset = false;
	chord_init(&chord);
	notes_init(&(notes[0]));
	arp_rebuild(&chord);
}

static void arp_note_on(u8 ch, u8 num, u8 vel) {
	if (arp_state.hold) {
		if (chord_should_reset) {
			//print_dbg("\r\n > arp: hold chord resetting");
			chord_should_reset = false;
			chord_init(&chord);
			notes_init(&(notes[0]));
		}
		chord_held_notes++;

		//print_dbg("\r\n > arp: chord held: ");
		//print_dbg_ulong(chord_held_notes);
	}
	chord_note_add(&chord, num, vel);
	notes_hold(&(notes[0]), num, vel);
	arp_rebuild(&chord);
}

static void arp_note_off(u8 ch, u8 num, u8 vel) {
	if (arp_state.hold && chord_contains(&chord, num)) {
		chord_held_notes--;
		if (chord_held_notes == 0) {
			//print_dbg("\r\n > arp chord should reset");
			chord_should_reset = true;
		}
	}
	else {
		chord_note_release(&chord, num);
		notes_release(&(notes[0]), num);
		arp_rebuild(&chord);
	}
}

static void arp_pitch_bend(u8 ch, u16 bend) {
}

static void arp_control_change(u8 ch, u8 num, u8 val) {
	u16 period, t;
	u8 i;

	switch (num) {
	case 16:
		// clock mod
		if (sync_source == eClockInternal) {
			// clock speed; 1000ms - 23ms (same range as ww)
			period = 25000 / ((val << 3) + 25);
			//print_dbg("\r\n arp clock ms: ");
			//print_dbg_ulong(period);
			clock_set(period);
		}
		break;
	case 17:
		// gate mod
		//print_dbg("\r\n arp gates: ");
		for (i = 0; i < 4; i++) {
			t = arp_player_set_gate_width(&player[i], val);
			//print_dbg_ulong(t);
			//print_dbg(" ");
		}
		break;
	case 18:
		// division mod
		//print_dbg("\r\n arp divs: 1 ");
		for (i = 1; i < 4; i++) {
			t = (i + 1) + ((val >> 5) * (i + 1));
			//print_dbg_ulong(t);
			//print_dbg(" ");
			arp_player_set_division(&player[i], t, &player_behavior);
		}
	}
}

static void arp_rt_tick(void) {
	u32 now;

	if (sync_source != eClockMidi)
		return;

	now = time_now();
	time_clear();

	midi_clock_pulse(&midi_clock, now);

	if (midi_clock.pulse_count == 0) {
		arp_clock_pulse(1);
	}
	else if (midi_clock.pulse_count == (midi_clock.pulse_div_count >> 1)) {
		arp_clock_pulse(0);
	}
}

static void arp_rt_start(void) {
	if (sync_source != eClockExternal) {
		sync_source = eClockMidi;
		midi_clock_start(&midi_clock);
	}
}

static void arp_rt_stop(void) {
	if (sync_source == eClockExternal) {
		midi_clock_stop(&midi_clock);
		for (u8 i = 0; i < 4; i++) {
			arp_player_reset(&player[i], &player_behavior);
		}
	}
}

static void arp_rt_continue(void) {
	// some devices appear to only send continue and stop messages, not
	// start so continue messages will switch the sync to midi as well
	if (sync_source != eClockExternal) {
		sync_source = eClockMidi;
		midi_clock_continue(&midi_clock);
	}
}

static void player_note_on(u8 ch, u8 num, u8 vel) {
	if (ch > 3 || num > MIDI_NOTE_MAX)
		return;

	/*
	if (ch == 0) {
		print_dbg("\r\n >> p note on: ");
		print_dbg_ulong(num);
	}
	*/

	midi_pitch(ch, SEMI14[num], 0);
	multi_tr_set(ch);
}

static void player_note_off(u8 ch, u8 num, u8 vel) {
	if (ch > 3 || num > MIDI_NOTE_MAX)
		return;

	multi_tr_clr(ch);
}
