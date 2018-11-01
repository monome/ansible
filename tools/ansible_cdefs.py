CDEFS = {
    '1.6.1': r'''
typedef enum {
	conNONE,
	conARC,
	conGRID,
	conMIDI,
	conFLASH
} connected_t;

typedef enum {
	mArcLevels,
	mArcCycles,
	mGridKria,
	mGridMP,
	mMidiStandard,
	mMidiArp,
	mTT
} ansible_mode_t;

typedef struct {
	connected_t connected;
	ansible_mode_t arc_mode;
	ansible_mode_t grid_mode;
	ansible_mode_t midi_mode;
	ansible_mode_t none_mode;
	uint8_t i2c_addr;
} ansible_state_t;



typedef struct {
	uint8_t tr[16];
	uint8_t oct[16];
	uint8_t note[16];
	uint8_t dur[16];
	uint8_t rpt[16];
	uint8_t alt_note[16];
	uint8_t glide[16];

	uint8_t p[7][16];

	// uint8_t ptr[16];
	// uint8_t poct[16];
	// uint8_t pnote[16];
	// uint8_t pdur[16];

	uint8_t dur_mul;

	uint8_t lstart[7];
	uint8_t lend[7];
	uint8_t llen[7];
	uint8_t lswap[7];
	uint8_t tmul[7];
} kria_track;

typedef struct {
	kria_track t[4];
	uint8_t scale;
} kria_pattern;

typedef struct {
	kria_pattern p[16];
	uint8_t pattern;
	uint8_t meta_pat[64];
	uint8_t meta_steps[64];
	uint8_t meta_start;
	uint8_t meta_end;
	uint8_t meta_len;
	uint8_t meta_lswap;
	uint8_t glyph[8];
} kria_data_t;

typedef struct {
	uint32_t clock_period;
	uint8_t preset;
	bool note_sync;
	uint8_t loop_sync;
	uint8_t cue_div;
	uint8_t cue_steps;
	uint8_t meta;
	kria_data_t k[8];
} kria_state_t;



typedef struct {
	// int8_t position[8];		// current position in cycle
	// uint8_t tick[8]; 		// position in speed countdown
	// uint8_t pushed[8];		// manual key reset

	uint8_t count[8];		// length of cycle
	int8_t speed[8];		// speed of cycle
	uint8_t min[8];
	uint8_t max[8];
	uint8_t trigger[8];
	uint8_t toggle[8];
	uint8_t rules[8];
	uint8_t rule_dests[8];
	uint8_t sync[8]; 		// if true, reset dest rule to count
	uint8_t rule_dest_targets[8];
	uint8_t smin[8];
	uint8_t smax[8];

	uint8_t scale;
	uint8_t glyph[8];
} mp_data_t;

typedef struct {
	uint8_t preset;
	uint8_t sound;
	uint8_t voice_mode;
	mp_data_t m[8];
} mp_state_t;



typedef struct {
	uint16_t pattern[4][16];
	uint8_t note[4][16];
	bool mode[4];
	bool all[4];
	uint8_t now;
	uint8_t start;
	int8_t len;
	uint8_t dir;
	uint8_t scale[4];
	uint8_t octave[4];
	uint16_t offset[4];
	uint16_t range[4];
	uint16_t slew[4];
} levels_data_t;

typedef struct {
	// uint32_t clock_period;
	uint8_t preset;
	levels_data_t l[8];
} levels_state_t;



typedef struct {
	uint16_t pos[4];
	int16_t speed[4];
	int8_t mult[4];
	uint8_t range[4];
	uint8_t mode;
	uint8_t shape;
	uint8_t friction;
	uint16_t force;
	uint8_t div[4];
} cycles_data_t;

typedef struct {
	// uint32_t clock_period;
	uint8_t preset;
	cycles_data_t c[8];
} cycles_state_t;



typedef enum {
	eVoicePoly = 0,
	eVoiceMono,
	eVoiceMulti,
	eVoiceFixed,
	
	eVoiceMAX
} voicing_mode;

// note, cc mappings for fixed voice mode
typedef struct {
	uint8_t notes[4];
	uint8_t cc[4];
} fixed_mapping_t;

// standard mode values saved to nvram
typedef struct {
	uint32_t clock_period;
	uint8_t voicing;
	fixed_mapping_t fixed;
	int16_t shift;   // tuning/dac offset
	int16_t slew;    // pitch cv slew (ms)
} midi_standard_state_t;

typedef struct {
	uint8_t fill;
	uint8_t division;
	int8_t rotation;
	uint8_t gate;
	uint8_t steps;
	uint8_t offset;

	int16_t slew;
	int16_t shift;
} midi_arp_player_state_t;

// arp mode value saved to nvram
typedef struct {
	uint32_t clock_period;
	uint8_t style;    // NB: not using arp_style as type because enums have vairable size
	bool hold;   // if true new notes add to chord if at least one note in chord is still held
	midi_arp_player_state_t p[4];
} midi_arp_state_t;



typedef struct {
	uint32_t clock_period;
	uint16_t tr_time[4];
	uint16_t cv_slew[4];
} tt_state_t;



typedef const struct {
	uint8_t fresh;
	ansible_state_t state;
	kria_state_t kria_state;
	mp_state_t mp_state;
	levels_state_t levels_state;
	cycles_state_t cycles_state;
	midi_standard_state_t midi_standard_state;
	midi_arp_state_t midi_arp_state;
	tt_state_t tt_state;
	uint8_t scale[16][8];
} nvram_data_t;
    ''',
}
