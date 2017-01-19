#pragma once

// NB: NOTE_POOL_SIZE and CHORD_MAX_NOTES (in libavr32) need to match
// in order for the as played arp logic to work correctly.
//
// the defines represent the maximum number of notes tracked in legato
// note handling and the maximum number of (input) notes to the arp
// logic respectively.

// libavr32
#include "arp.h"

// standard midi modes
typedef enum {
	eVoicePoly = 0,
	eVoiceMono,
	eVoiceMulti,
	eVoiceFixed,
	
	eVoiceMAX
} voicing_mode;

// note, cc mappings for fixed voice mode
typedef struct {
	u8 notes[4];
	u8 cc[4];
} fixed_mapping_t;

// standard mode values saved to nvram
typedef struct {
	uint32_t clock_period;
	u8 voicing;
	fixed_mapping_t fixed;
	s16 shift;   // tuning/dac offset
	s16 slew;    // pitch cv slew (ms)
} midi_standard_state_t;

typedef struct {
	u8 fill;
	u8 division;
	s8 rotation;
	u8 gate;
	u8 steps;
	u8 offset;

	s16 slew;
	s16 shift;
} midi_arp_player_state_t;

// arp mode value saved to nvram
typedef struct {
	uint32_t clock_period;
	u8 style;    // NB: not using arp_style as type because enums have vairable size
	bool hold;   // if true new notes add to chord if at least one note in chord is still held
	midi_arp_player_state_t p[4];
} midi_arp_state_t;

void set_mode_midi(void);

void handler_MidiFrontShort(s32 data);
void handler_MidiFrontLong(s32 data);

void default_midi_standard(void);
void clock_midi_standard(uint8_t phase);
void ii_midi_standard(uint8_t *d, uint8_t l);
void handler_StandardKey(s32 data);
void handler_StandardTr(s32 data);
void handler_StandardTrNormal(s32 data);
void handler_StandardMidiPacket(s32 data);

void default_midi_arp(void);
void clock_midi_arp(uint8_t phase);
void ii_midi_arp(uint8_t *d, uint8_t l);
void handler_ArpKey(s32 data);
void handler_ArpTr(s32 data);
void handler_ArpTrNormal(s32 data);
void handler_ArpMidiPacket(s32 data);
