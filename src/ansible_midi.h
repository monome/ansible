#pragma once

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
} midi_standard_state_t;

typedef struct {
	uint32_t clock_period;
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
