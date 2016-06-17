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
	ansible_mode_t mode;
	ansible_mode_t arc_mode;
	ansible_mode_t grid_mode;
	ansible_mode_t midi_mode;
	ansible_mode_t none_mode;
} ansible_state_t;

ansible_state_t ansible_state;
