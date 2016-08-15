/*

anti-aliased LED point

1024 points

div by 16 to find "center"
mod by 16 to find "offset"

center = full bright
+1 = offset
-1 = 16 - offset

(or figure out the correct way)

*/

#include "print_funcs.h"
#include "flashc.h"

#include "monome.h"
#include "i2c.h"

#include "main.h"
#include "ansible_arc.h"


void set_mode_arc(void) {
	switch(f.state.mode) {
	case mArcLevels:
		print_dbg("\r\n> mode arc levels");
		app_event_handlers[kEventKey] = &handler_LevelsKey;
		app_event_handlers[kEventTr] = &handler_LevelsTr;
		app_event_handlers[kEventTrNormal] = &handler_LevelsTrNormal;
		app_event_handlers[kEventMonomeRingEnc] = &handler_LevelsEnc;
		app_event_handlers[kEventMonomeRefresh] = &handler_LevelsRefresh;
		clock = &clock_levels;
		clock_set(f.levels_state.clock_period);
		process_ii = &ii_levels;
		resume_levels();
		update_leds(1);
		break;
	case mArcCycles:
		print_dbg("\r\n> mode arc cycles");
		app_event_handlers[kEventKey] = &handler_CyclesKey;
		app_event_handlers[kEventTr] = &handler_CyclesTr;
		app_event_handlers[kEventTrNormal] = &handler_CyclesTrNormal;
		app_event_handlers[kEventMonomeRingEnc] = &handler_CyclesEnc;
		app_event_handlers[kEventMonomeRefresh] = &handler_CyclesRefresh;
		clock = &clock_cycles;
		clock_set(f.cycles_state.clock_period);
		process_ii = &ii_cycles;
		update_leds(2);
		break;
	default:
		break;
	}
	
	if(connected == conARC) {
		app_event_handlers[kEventFrontShort] = &handler_ArcFrontShort;
		app_event_handlers[kEventFrontLong] = &handler_ArcFrontLong;
	}

	flashc_memset32((void*)&(f.state.none_mode), f.state.mode, 4, true);
	flashc_memset32((void*)&(f.state.arc_mode), f.state.mode, 4, true);
}


void handler_ArcFrontShort(s32 data) {
	print_dbg("\r\n> PRESET");
}

void handler_ArcFrontLong(s32 data) {
	if(f.state.mode == mArcLevels)
		set_mode(mArcCycles);
	else
		set_mode(mArcLevels);
}

////////////////////////////////////////////////////////////////////////////////

u16 point;
uint8_t mode;
int8_t pattern_read;
int8_t pattern_write;
int16_t enc_count[4];
levels_data_t l;

void (*arc_refresh)(void);

void default_levels() {
	uint8_t i1;

	for(i1=0;i1<16;i1++)
		flashc_memcpy((void *)&f.levels_state.l.pattern[i1], &l.pattern, sizeof(l.pattern), true);

	flashc_memset32((void*)&(f.levels_state.clock_period), 100, 4, true);
	flashc_memset8((void*)&(f.levels_state.l.edit), 0, 1, true);
	flashc_memset8((void*)&(f.levels_state.l.start), 0, 1, true);
	flashc_memset8((void*)&(f.levels_state.l.len), 3, 1, true);
	flashc_memset8((void*)&(f.levels_state.l.dir), 0, 1, true);
}

void init_levels() {
	point = 0;
	mode = 0;

	// for(uint8_t i = 0;i<4;i++) {
	// 	pattern.value[i] = 0;
	// 	pattern.mode[i] = 0;
	// }
}

void resume_levels() {
	uint8_t i1;

	l.edit = f.levels_state.l.edit;
	l.start = f.levels_state.l.start;
	l.len = f.levels_state.l.len;
	l.dir = f.levels_state.l.dir;

	arc_refresh = &refresh_levels;

	for(i1=0;i1<4;i1++) {
		l.pattern[l.edit].value[i1] = f.levels_state.l.pattern[l.edit].value[i1];
		l.pattern[l.edit].mode[i1] = f.levels_state.l.pattern[l.edit].mode[i1];
	}

	mode = 0;
	monomeFrameDirty++;
}

void clock_levels(uint8_t phase) {
	if(phase)
		set_tr(TR1);
	else
		clr_tr(TR1);
}

void ii_levels(uint8_t *d, uint8_t l) {
	;;
}

void handler_LevelsEnc(s32 data) { 
	uint8_t n;
	int8_t delta;
	int16_t i;

	monome_ring_enc_parse_event_data(data, &n, &delta);

	// print_dbg("\r\n levels enc \tn: "); 
	// print_dbg_ulong(n); 
	// print_dbg("\t delta: "); 
	// print_dbg_hex(delta);

	switch(mode) {
	case 0:
		i = l.pattern[l.edit].value[n] + delta;
		if(i < 0)
			i = 0;
		else if(i > 0x3ff)
			i = 0x3ff;
		l.pattern[l.edit].value[n] = i;
		break;
	case 1:
		i = (enc_count[n] + delta) & 0xff;
		if(i >> 4 != enc_count[n] >> 4) {
			switch(n) {
			case 0:
				pattern_read = i >> 4;
				pattern_write = -1;
				enc_count[1] = (l.edit << 4) + 8;
				break;
			case 1:
				pattern_write = i >> 4;
				pattern_read = -1;
				enc_count[0] = (l.edit << 4) + 8;
				break;
			case 2:
				l.start = i >> 4;
				break;
			case 3:
				if((i>>4) == 0) {
					if(l.dir && l.len == 2) {
						l.dir = 0;
					}
					else if(l.dir == 0 && l.len == 2)
						l.dir = 1;
				}

				if(l.dir) {
					l.len = ((16 - (i>>4)) & 0xf) + 1;
				}
				else {
					l.len = (i>>4) + 1;
				}
				
				// print_dbg("\r\nlen: ");
				// print_dbg_ulong(l.len);
				// print_dbg("\t");
				// print_dbg_ulong(l.dir);
				// print_dbg("\t");
				// print_dbg_ulong(i>>4);
				break;
			default:
				break;
			}
			monomeFrameDirty++;
		}
		// print_dbg("\r\ni: ");
		// print_dbg_ulong(i >> 4);
		enc_count[n] = i;
		break;
	case 2:
		break;
	default:
		break;
	}

	// print_dbg("\r\n accum: "); 
	// print_dbg_hex(i);

	monomeFrameDirty++;
}


void handler_LevelsRefresh(s32 data) {
	if(monomeFrameDirty) {
		arc_refresh();

		monome_set_quadrant_flag(0);
		monome_set_quadrant_flag(1);
		monome_set_quadrant_flag(2);
		monome_set_quadrant_flag(3);
		(*monome_refresh)();
	}
}

void refresh_levels() {
	uint16_t i1, i2;
	for(i1=0;i1<256;i1++) {
			monomeLedBuffer[i1] = 0;
		}

	for(i1=0;i1<4;i1++) {
		i2 = (l.pattern[l.edit].value[i1] * 3) >> 2;
		i2 = (i2 + 640) & 0x3ff;
		arc_draw_point(i1, i2);
	}
}

void refresh_levels_change() {
	uint16_t i1;
	for(i1=0;i1<256;i1++) {
			monomeLedBuffer[i1] = 0;
		}

	if(l.dir) {
		for(i1=0;i1<l.len * 4;i1++) {
			monomeLedBuffer[128 + ((((l.start - l.len + 1) * 4) + i1) & 63)] = 3;
			monomeLedBuffer[192 + ((((l.start - l.len + 1) * 4) + i1) & 63)] = 5;
		}
	}
	else {
		for(i1=0;i1<l.len * 4;i1++) {
			monomeLedBuffer[128 + (((l.start * 4) + i1) & 63)] = 3;
			monomeLedBuffer[192 + (((l.start * 4) + i1) & 63)] = 5;
		}
	}

	for(i1=0;i1<4;i1++) {
		// READ
		monomeLedBuffer[(l.edit * 4) + i1] = 5;
		if(pattern_read >= 0)
			monomeLedBuffer[(pattern_read * 4) + i1] = 15;

		// WRITE
		monomeLedBuffer[64 + (l.edit * 4) + i1] = 5;
		if(pattern_write >= 0)
			monomeLedBuffer[64 + (pattern_write * 4) + i1] = 15;

		// PATTERN START
		monomeLedBuffer[128 + (l.start * 4) + i1] = 15;
	}


}

void refresh_levels_config() {
	uint16_t i1;
	for(i1=0;i1<256;i1++) {
			monomeLedBuffer[i1] = 0;
		}

	for(i1=0;i1<4;i1++) {
		monomeLedBuffer[i1*64 + 32] = 15;
	}
}


void handler_LevelsKey(s32 data) { 
	// print_dbg("\r\n> levels key ");
	// print_dbg_ulong(data);

	switch(data) {
	// key 1 UP
	case 0:
		mode = 0;
		arc_refresh = &refresh_levels;
		break;
	// key 1 DOWN
	case 1:
		mode = 1;
		pattern_read = 0;
		pattern_write = 0;
		arc_refresh = &refresh_levels_change;
		enc_count[0] = (l.edit << 4) + 8;
		enc_count[1] = (l.edit << 4) + 8;
		enc_count[2] = (l.start << 4) + 8;
		enc_count[3] = ((l.len - 1) << 4) + 8;
		break;
	// key 2 UP
	case 2:
		mode = 0;
		arc_refresh = &refresh_levels;
		break;
	// key 2 DOWN
	case 3:
		mode = 2;
		arc_refresh = &refresh_levels_config;
		break;
	default:
		break;
	}
	monomeFrameDirty++;
}

void handler_LevelsTr(s32 data) { 
	print_dbg("\r\n> levels tr ");
	print_dbg_ulong(data);
}

void handler_LevelsTrNormal(s32 data) { 
	print_dbg("\r\n> levels tr normal ");
	print_dbg_ulong(data);
}


////////////////////////////////////////////////////////////////////////////////

void default_cycles() {
	flashc_memset32((void*)&(f.cycles_state.clock_period), 50, 4, true);
}

void clock_cycles(uint8_t phase) {
	if(phase)
		set_tr(TR1);
	else
		clr_tr(TR1);
}

void ii_cycles(uint8_t *d, uint8_t l) {
	;;
}

void handler_CyclesEnc(s32 data) { 
	uint8_t n;
	int8_t delta;

	monome_ring_enc_parse_event_data(data, &n, &delta);

	print_dbg("\r\n cycles enc \tn: "); 
	print_dbg_ulong(n); 
	print_dbg("\t delta: "); 
	print_dbg_hex(delta);
}


void handler_CyclesRefresh(s32 data) { 
	if(monomeFrameDirty) {
		////
		for(uint8_t i1=0;i1<16;i1++) {
			monomeLedBuffer[i1] = 0;
			monomeLedBuffer[16+i1] = 0;
			monomeLedBuffer[32+i1] = 4;
			monomeLedBuffer[48+i1] = 0;
		}

		monomeLedBuffer[0] = 15;

		////
		monome_set_quadrant_flag(0);
		monome_set_quadrant_flag(1);
		(*monome_refresh)();
	}
}

void handler_CyclesKey(s32 data) { 
	print_dbg("\r\n> cycles key ");
	print_dbg_ulong(data);
}

void handler_CyclesTr(s32 data) { 
	print_dbg("\r\n> cycles tr ");
	print_dbg_ulong(data);
}

void handler_CyclesTrNormal(s32 data) { 
	print_dbg("\r\n> cycles tr normal ");
	print_dbg_ulong(data);
}







void arc_draw_point(uint8_t n, uint16_t p) {
	int c;

	c = p / 16;

	monomeLedBuffer[(n * 64) + c] = 15;
	monomeLedBuffer[(n * 64) + ((c + 1) % 64)] = p % 16;
	monomeLedBuffer[(n * 64) + ((c + 63) % 64)] = 15 - (p % 16);
}