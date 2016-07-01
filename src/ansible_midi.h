#pragma once

typedef struct {
	uint32_t clock_period;
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

void default_midi_arp(void);
void clock_midi_arp(uint8_t phase);
void ii_midi_arp(uint8_t *d, uint8_t l);
void handler_ArpKey(s32 data);
void handler_ArpTr(s32 data);
void handler_ArpTrNormal(s32 data);