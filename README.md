# ansible

6hp

4x gate out
4x cv out

no analog input

2x key input
2x gate input
(1 x jack detection on left gate input)

1x mode/boot/nmi
2x LED near mode


---

in TT slave mode
	map TRs/CVs sequentially
	inputs?

---

## mode depends on USB device present

short press - change operation per USB type

long (1s) - enter "preset mode"

longer (3s) - enter TT slave mode (also can be activated via TT)
	also escape TT mode and resume previous mode

very long (5) - enter TT slave mode and set i2c address based on key1+key2

### ARC

#### mode 0 - LEVELS

4x "levels" with indication
	output cv
	output pulse trains

key 1: alt.
	hold: recall (segment), save-to (segment), slew, segments
	fast: next
in 1: next

key 2: 
	hold:quantize mode: free vs. semitones with scale setup (mode, offset, octave)
	fast: reset to seg zero
in 2: reset to segment zero

#### mode 1 - PLAY/CYCLE uses LEVELS data

cycle edit modes:
	- speed
	- length
	- start

each knob drives own pattern: show "cycle" position and current segment.
tr pulse vs. trigger on at zero (north) segment change, and off at 180 (south)
can go in reverse.

key 1: alt
	hold: changes config
		1: sync: all, relative, free --- int sync all on/off left-to-right slaving
			all = speed/length/start
			relative = left-to-right speed w/ multiples, free length/start
			free = fully free
		2: trig pulse width + mode
		3: force (sensitivity)
		4: friction
	fast: edit mode (speed/length/start)
in 1: jack present: ext sync mode on. measure tr period. speeds are multiples.

key 2: reset
in 2: reset




### GRID

#### mode 0: kria













### MIDI

converter. modes. arpy. tt.

#### mode 0: voice allocator / mono out

key 1: panic
in 1: panic

key 2: change data
in 2:

data 0: poly
	cv/tr rotating outputs

data 1: mono
	cv 1: pitch
	cv 2: velocity
	cv 3:
	cv 4: 
	tr 1: gate

data 2: cc+trigs
	fixed cc map + note trigs

#### mode 1: arpeggiator

tr 1-4: different width gates
cv 1-4: different arp modes (up, down, tri, random)
		or different speed multiples??

key 1: tap tempo
in 1: sync

key 2: arp mode (cycle entire bank)
in 2: reset

cc control for internal speed with no jack
cc control for pulse length multiplier
cc control for speed mult?





### USB mass storage (not a real "mode")

mode white: read all presets
mode orange: write all presets
key 1: go
key 2: cancel





### NONE?

resume previous mode
long hold for TT


