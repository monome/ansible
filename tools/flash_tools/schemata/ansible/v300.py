from schemata.ansible.v161 import PresetSchema_v161


class PresetSchema_v300(PresetSchema_v161):
    def app_list(self):
        return [
            'kria',
            'mp',
            'es',
            'levels',
            'cycles',
            'midi_standard',
            'midi_arp',
            'tt',
        ]

    def cdef(self):
        return r'''
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
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
	mGridES,
	mMidiStandard,
	mMidiArp,
	mTT,
} ansible_mode_t;

typedef enum {
  krDirForward = 0,
  krDirReverse = 1,
  krDirTriangle = 2,
  krDirDrunk = 3,
  krDirRandom = 4,
} kria_direction;


#define GRID_PRESETS 8

#define KRIA_NUM_TRACKS 4
#define KRIA_NUM_PARAMS 7
#define KRIA_NUM_PATTERNS 16

#define ES_EVENTS_PER_PATTERN 128
#define ES_EDGE_PATTERN 0
#define ES_EDGE_FIXED 1
#define ES_EDGE_DRONE 2


typedef struct {
	u8 tr[16];
	s8 oct[16];
	u8 note[16];
	u8 dur[16];
	u8 rpt[16];
	u8 rptBits[16];
	u8 alt_note[16];
	u8 glide[16];

	u8 p[KRIA_NUM_PARAMS][16];

	// u8 ptr[16];
	// u8 poct[16];
	// u8 pnote[16];
	// u8 pdur[16];

	u8 dur_mul;
	kria_direction direction;  
	u8 advancing[KRIA_NUM_PARAMS];
	u8 octshift;

	u8 lstart[KRIA_NUM_PARAMS];
	u8 lend[KRIA_NUM_PARAMS];
	u8 llen[KRIA_NUM_PARAMS];
	u8 lswap[KRIA_NUM_PARAMS];
	u8 tmul[KRIA_NUM_PARAMS];

	bool tt_clocked;
	bool trigger_clocked;
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

typedef enum {
	krSyncNone    = 0x00,
	krSyncTimeDiv = 0x01,
} kria_sync_mode_t;

typedef struct {
	uint32_t clock_period;
	uint8_t sync_mode; // enum kria_sync_mode_t
	uint8_t preset;
	bool note_sync;
	uint8_t loop_sync;
	bool note_div_sync;
	uint8_t div_sync;
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


typedef enum {
	es_stopped,
	es_armed,
	es_recording,
	es_playing
} es_mode_t;

typedef enum {
	es_main,
	es_patterns_held,
	es_patterns
} es_view_t;

typedef struct {
	u8 active;
	s8 x;
	s8 y;
	u32 start;
	u8 from_pattern;
} es_note_t;

typedef struct {
	u8 on;
	u8 index;
	u16 interval;
} es_event_t;

typedef struct {
	es_event_t e[ES_EVENTS_PER_PATTERN];
	u16 interval_ind;
	u16 length;
	u8 loop;
	u8 root_x;
	u8 root_y;
	u8 edge;
	u16 edge_time;
	u8 voices;
	u8 dir;
	u8 linearize;
	u8 start;
	u8 end;
} es_pattern_t;

typedef struct {
	u8 arp;
	u8 p_select;
	u8 voices;
	u8 octave;
	u8 scale;
	u16 keymap[128];
	es_pattern_t p[16];
	u8 glyph[8];
} es_data_t;

typedef struct {
	u8 preset;
	es_data_t e[GRID_PRESETS];
} es_state_t;


# define ARC_NUM_PRESETS 8

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


#define I2C_FOLLOWER_COUNT 3

struct i2c_follower_t;

typedef void(*ii_u8_cb)(struct i2c_follower_t* follower, uint8_t track, uint8_t param);
typedef void(*ii_s8_cb)(struct i2c_follower_t* follower, uint8_t track, int8_t param);
typedef void(*ii_u16_cb)(struct i2c_follower_t* follower, uint8_t track, uint16_t param);

typedef struct i2c_follower_t {
    uint8_t addr;
    bool active;
    uint8_t track_en;
    int8_t oct;

    u32 init; // ii_u8_cb
    u32 mode; // ii_u8_cb
    u32 tr; // ii_u8_cb
    u32 mute; // ii_u8_cb

    u32 cv; // ii_u16_cb
    u32 slew; // ii_u16_cb
    u32 octave; // ii_s8_cb

    uint8_t mode_ct;
    uint8_t active_mode;
} i2c_follower_t;


typedef struct {
	connected_t connected;
	ansible_mode_t arc_mode;
	ansible_mode_t grid_mode;
	ansible_mode_t midi_mode;
	ansible_mode_t none_mode;
	u8 i2c_addr;
	u8 grid_varibrightness;
	i2c_follower_t followers[I2C_FOLLOWER_COUNT];
} ansible_state_t;


// NVRAM data structure located in the flash array.
typedef const struct {
	uint8_t fresh;
	ansible_state_t state;
	kria_state_t kria_state;
	mp_state_t mp_state;
	es_state_t es_state;
	levels_state_t levels_state;
	cycles_state_t cycles_state;
	midi_standard_state_t midi_standard_state;
	midi_arp_state_t midi_arp_state;
	tt_state_t tt_state;
	uint8_t scale[16][8];
	uint16_t tuning_table[4][120];
} nvram_data_t;
'''

    def meta(self, nvram):
        return self.combine(
            self.scalar_settings(nvram.state, ['i2c_addr', 'grid_varibrightness']),
			self.array_settings(nvram.state, [
				(
					'followers', 
					lambda follower: self.combine(
						self.scalar_settings(follower, [
							'active',
							'track_en',
							'oct',
							'active_mode',
						])
					)
				)
			]),
            self.enum_settings(nvram.state, [
                ('connected', 'connected_t', 'conNONE'),
                ('arc_mode', 'ansible_mode_t', 'mArcLevels'),
                ('grid_mode', 'ansible_mode_t', 'mGridKria'),
                ('midi_mode', 'ansible_mode_t', 'mMidiStandard'),
                ('none_mode', 'ansible_mode_t', 'mTT'),
            ]),
        )

    def shared(self, nvram):
        return self.combine(
			self.array_2d_settings(nvram, ['scale:scales']),
			self.array_2d_settings(nvram, ['tuning_table'])
		)


    def extract_kria_state(self, state):
        return self.combine(
            self.scalar_settings(state, [
                'clock_period',
                'preset:curr_preset',
				'sync_mode',
                'note_sync',
                'loop_sync',
				'note_div_sync',
				'div_sync',
                'cue_div',
                'cue_steps',
                'meta',
            ]),
            self.array_settings(state, [
                (
                    'k:presets',
                    lambda preset: self.combine(
                        self.array_settings(preset, [
                            (
                                'p:patterns',
                                lambda pattern: self.combine(
                                    self.array_settings(pattern, [
                                        (
                                            't:tracks',
                                            lambda track: self.combine(
                                                self.array_1d_settings(track, [
                                                    'tr',
                                                    'oct',
                                                    'note',
                                                    'dur',
                                                    'rpt',
                                                    'rptBits',
                                                    'alt_note',
                                                    'glide',
                                                ]),
                                                self.array_2d_settings(track, [
                                                    'p'
                                                ]),
                                                self.scalar_settings(track, [
                                                    'dur_mul',
                                                    'direction',
                                                ]),
                                                self.array_1d_settings(track, [
                                                    'advancing',
                                                ]),
                                                self.scalar_settings(track, [
                                                    'octshift',
                                                ]),
                                                self.array_1d_settings(track, [
                                                    'lstart',
                                                    'lend',
                                                    'llen',
                                                    'lswap',
                                                    'tmul',
                                                ]),
                                                self.scalar_settings(track, [
                                                    'tt_clocked',
													'trigger_clocked'
                                                ]),
                                            ),
                                        ),
                                    ]),
                                    self.scalar_settings(pattern, ['scale']),
                                ),
                            ),
                        ]),
                        self.scalar_settings(preset, [
                            'pattern:curr_pattern',
                        ]),
                        self.array_1d_settings(preset, [
                            'meta_pat',
                            'meta_steps',
                        ]),
                        self.scalar_settings(preset, [
                            'meta_start',
                            'meta_end',
                            'meta_len',
                            'meta_lswap',
                        ]),
                        self.array_1d_settings(preset, [
                            'glyph',
                        ]),
                    ),
                ),
            ]),
        )

    def extract_es_state(self, state):
        return {
            'curr_preset': state.preset,
            'presets': [
                self.combine(
                    self.scalar_settings(preset, [
                        'arp',
                        'p_select',
                        'voices',
                        'octave',
                        'scale',
                    ]),
                    self.array_1d_settings(preset, [
                        'keymap',
                        'glyph',
                    ]),
                    {
                        'patterns': [
                            self.combine(
                                {
                                    'events': [
                                        self.scalar_settings(event, [
                                            'on',
                                            'index',
                                            'interval',
                                        ])
                                        for event in pattern.e
                                    ]
                                },
                                self.scalar_settings(pattern, [
                                    'interval_ind',
                                    'length',
                                    'loop',
                                    'root_x',
                                    'root_y',
                                    'edge',
                                    'edge_time',
                                    'voices',
                                    'dir',
                                    'linearize',
                                    'start',
                                    'end',
                                ]),
                            )
                            for pattern in preset.p
                        ]
                    }
                )
                for preset in state.e
            ]
        }
