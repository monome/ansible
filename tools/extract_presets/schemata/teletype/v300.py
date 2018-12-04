from preset_schema import PresetSchema


class TeletypePresetSchema_v300(PresetSchema):
    def check(self, nvram_data):
        if nvram_data.fresh != 0x22:
            print("this firmware image hasn't ever been run (or is corrupt)")
            return False
        return True

    def root_type(self):
        return 'nvram_data_t'

    def address(self):
        return 0x80040000

    def cdef(self):
        return r'''
typedef enum {
    M_LIVE,
    M_EDIT,
    M_PATTERN,
    M_PRESET_W,
    M_PRESET_R,
    M_HELP
} tele_mode_t;

typedef enum { NUMBER, OP, MOD, PRE_SEP, SUB_SEP } tele_word_t;

#define FIRSTRUN_KEY 0x22
#define BUTTON_STATE_SIZE 32
#define GRID_FADER_COUNT 64
#define PATTERN_COUNT 4
#define PATTERN_LENGTH 64
#define COMMAND_MAX_LENGTH 16
#define SCRIPT_MAX_COMMANDS 6
#define SCRIPT_COUNT 11
#define SCENE_TEXT_LINES 32
#define SCENE_TEXT_CHARS 32
#define SCENE_SLOTS 32

typedef int16_t SCALE_T;
typedef int32_t _SCALE_T;

typedef struct {
    SCALE_T p_min;
    SCALE_T p_max;
    SCALE_T i_min;
    SCALE_T i_max;
} cal_data_t;


typedef struct {
    uint8_t tag; // tele_word_t
    int16_t value;
} tele_data_t;

typedef struct {
    uint8_t length;
    int8_t separator;
    tele_data_t data[COMMAND_MAX_LENGTH];
    bool comment;  // bool
} tele_command_t;

typedef struct {
    int16_t count;
    int16_t mod;
    uint8_t skip;
} every_count_t;

typedef struct {
    int16_t idx;
    uint16_t len;
    uint16_t wrap;
    int16_t start;
    int16_t end;
    int16_t val[PATTERN_LENGTH];
} scene_pattern_t;

typedef struct {
    uint8_t l;
    tele_command_t c[SCRIPT_MAX_COMMANDS];
    every_count_t every[SCRIPT_MAX_COMMANDS];
    uint32_t last_time;
} scene_script_t;

typedef struct {
    uint8_t button_states[BUTTON_STATE_SIZE];
    uint8_t fader_states[GRID_FADER_COUNT];
} grid_data_t;

// NVRAM data structure located in the flash array.
typedef const struct {
    scene_script_t scripts[SCRIPT_COUNT - 1];  // Exclude TEMP script
    scene_pattern_t patterns[PATTERN_COUNT];
    grid_data_t grid_data;
    char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
} nvram_scene_t;

typedef const struct {
    nvram_scene_t scenes[SCENE_SLOTS];
    uint8_t last_scene;
    uint8_t last_mode;  // tele_mode_t
    uint8_t fresh;
    cal_data_t cal;
} nvram_data_t;
'''
