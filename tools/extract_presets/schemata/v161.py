from preset_schema import PresetSchema


class PresetSchema_v161(PresetSchema):
    def app_list(self):
        return [
            'kria',
            'mp',      
            'levels',
            'cycles',
            'midi_standard',
            'midi_arp',
            'tt',
        ]   

    def meta(self, nvram):
        return self.combine(
            self.scalar_settings(nvram.state, ['i2c_addr']),
            self.enum_settings(nvram.state, [
                ('connected', 'connected_t'),
                ('arc_mode', 'ansible_mode_t'),
                ('grid_mode', 'ansible_mode_t'),
                ('midi_mode', 'ansible_mode_t'),
                ('none_mode', 'ansible_mode_t'),
            ]),
        )

    def shared(self, nvram):
        return self.array_2d_settings(nvram, ['scale'])
    
    def cdef(self):
        return r'''
typedef uint8_t u8;
typedef uint16_t u16;
typedef int8_t s8;
typedef int16_t s16;

typedef enum {
	conNONE,
	conARC,
	conGRID,
	conMIDI,
	conFLASH
} connected_t;

connected_t connected;

typedef enum {
	mArcLevels,
	mArcCycles,
	mGridKria,
	mGridMP,
	mMidiStandard,
	mMidiArp,
	mTT
} ansible_mode_t;



#define GRID_PRESETS 8

#define KRIA_NUM_TRACKS 4
#define KRIA_NUM_PARAMS 7
#define KRIA_NUM_PATTERNS 16

typedef struct {
	u8 tr[16];
	u8 oct[16];
	u8 note[16];
	u8 dur[16];
	u8 rpt[16];
	u8 alt_note[16];
	u8 glide[16];

	u8 p[KRIA_NUM_PARAMS][16];

	// u8 ptr[16];
	// u8 poct[16];
	// u8 pnote[16];
	// u8 pdur[16];

	u8 dur_mul;

	u8 lstart[KRIA_NUM_PARAMS];
	u8 lend[KRIA_NUM_PARAMS];
	u8 llen[KRIA_NUM_PARAMS];
	u8 lswap[KRIA_NUM_PARAMS];
	u8 tmul[KRIA_NUM_PARAMS];
} kria_track;

typedef struct {
	kria_track t[4];
	u8 scale;
} kria_pattern;

typedef struct {
	kria_pattern p[KRIA_NUM_PATTERNS];
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
	kria_data_t k[GRID_PRESETS];
} kria_state_t;




typedef struct {
	// s8 position[8];		// current position in cycle
	// u8 tick[8]; 		// position in speed countdown
	// u8 pushed[8];		// manual key reset

	u8 count[8];		// length of cycle
	s8 speed[8];		// speed of cycle
	u8 min[8];
	u8 max[8];
	u8 trigger[8];
	u8 toggle[8];
	u8 rules[8];
	u8 rule_dests[8];
	u8 sync[8]; 		// if true, reset dest rule to count
	u8 rule_dest_targets[8];
	u8 smin[8];
	u8 smax[8];

	u8 scale;
	u8 glyph[8];
} mp_data_t;

typedef struct {
	uint8_t preset;
	uint8_t sound;
	uint8_t voice_mode;
	mp_data_t m[GRID_PRESETS];
} mp_state_t;


#define ARC_NUM_PRESETS 8

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
	levels_data_t l[ARC_NUM_PRESETS];
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
	cycles_data_t c[ARC_NUM_PRESETS];
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


typedef struct {
	uint32_t clock_period;
	uint16_t tr_time[4];
	uint16_t cv_slew[4];
} tt_state_t;


typedef struct {
	connected_t connected;
	ansible_mode_t arc_mode;
	ansible_mode_t grid_mode;
	ansible_mode_t midi_mode;
	ansible_mode_t none_mode;
	uint8_t i2c_addr;
} ansible_state_t;


// NVRAM data structure located in the flash array.
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
'''
    
    def extract_kria_state(self, state):
        return self.combine(
            self.array_settings(state, [
                (
                    'k',
                    lambda preset: self.combine(
                        self.array_settings(preset, [
                            (
                                'p',
                                lambda pattern: self.combine(
                                    self.array_settings(pattern, [
                                        (
                                            't',
                                            lambda track: self.combine(
                                                self.array_1d_settings(track, [
                                                    'tr',
                                                    'oct',
                                                    'note',
                                                    'dur',
                                                    'rpt',
                                                    'alt_note',
                                                    'glide',
                                                    'lstart',
                                                    'lend',
                                                    'llen',
                                                    'lswap',
                                                    'tmul',
                                                ]),
                                                self.array_2d_settings(track, ['p']),
                                                self.scalar_settings(track, ['dur_mul']),
                                            ),
                                        ),
                                    ]),
                                ),
                            ),
                        ]),
                        self.array_1d_settings(preset, [
                            'meta_pat',
                            'meta_steps',
                            'glyph',
                        ]),
                        self.scalar_settings(preset, [
                            'meta_start',
                            'meta_end',
                            'meta_len',
                            'meta_lswap',
                            'pattern',
                        ]),
                    ),
                ),
            ]),
            self.scalar_settings(state, [
                'clock_period',
                'preset',
                'note_sync',
                'loop_sync',
                'cue_div',
                'cue_steps',
                'meta',
            ]),
        )
                                   

    def extract_mp_state(self, state):
        return self.combine(
            self.scalar_settings(state, [
                'preset',
                'sound',
                'voice_mode',
            ]),
            self.array_settings(state, [
                (
                    'm',
                    lambda preset: self.combine(
                        self.array_1d_settings(preset, [
                            'count',
                            'speed',
                            'min',
                            'max',
                            'trigger',
                            'toggle',
                            'rules',
                            'rule_dests',
                            'sync',
                            'rule_dest_targets',
                            'smin',
                            'smax',
                            'glyph',
                        ]),
                        self.scalar_settings(preset, [
                            'scale',
                        ]),
                    ),
                )
            ]),
        )

    def extract_levels_state(self, state):
        return self.combine(
            self.scalar_settings(state, [
                'preset',
            ]),
            self.array_settings(state, [
                (
                    'l',
                    lambda preset: self.combine(
                        self.array_2d_settings(preset, [
                            'pattern',
                            'note',
                        ]),
                        self.array_1d_settings(preset, [
                            'mode',
                            'all',
                            'scale',
                            'octave',
                            'offset',
                            'range',
                            'slew',
                        ]),
                        self.scalar_settings(preset, [
                            'now',
                            'start',
                            'len',
                            'dir',
                        ]),
                    ),
                ),
            ]),
        )

    def extract_cycles_state(self, state):
        return self.combine(
            self.scalar_settings(state, [
                'preset',
            ]),
            self.array_settings(state, [
                (
                    'c',
                    lambda preset: self.combine(
                        self.array_1d_settings(preset, [
                            'pos',
                            'speed',
                            'mult',
                            'range',
                            'div',
                        ]),
                        self.scalar_settings(preset, [
                            'mode',
                            'shape',
                            'friction',
                            'force',
                        ]),
                    ),
                ),
            ]),
        )

    def extract_midi_standard_state(self, state):
        return self.combine(
            self.lambda_settings(state, [
                (
                    'fixed',
                    lambda f: self.array_1d_settings(f, ['notes', 'cc']),
                ),
            ]),
            self.scalar_settings(state, [
                'clock_period',
                'voicing',
                'shift',
                'slew',
            ]),
        )

    def extract_midi_arp_state(self, state):
        return self.combine(
            self.array_settings(state, [
                (
                    'p',
                    lambda player_state: self.scalar_settings(player_state, [
                        'fill',
                        'division',
                        'rotation',
                        'gate',
                        'steps',
                        'offset',
                        'slew',
                        'shift',
                    ]),
                ),
            ]),
            self.scalar_settings(state, [
                'clock_period',
                'style',
                'hold',
            ]),
        )
    
    def extract_tt_state(self, state):
        return self.combine(
            self.scalar_settings(state, ['clock_period']),
            self.array_1d_settings(state, ['tr_time', 'cv_slew']),
        )
