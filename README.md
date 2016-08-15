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

handlers:
	arc enc
	grid key
	monome refresh
	key
	tr
	tr-norm
	(front/preset)





I2C protocol map





---

in TT slave mode
	map TRs/CVs sequentially
	inputs
		poll state. pulse counters? period measurement

---

## mode key function depends on USB device present

### arc/grid

long - toggle modes
short - preset mode

### midi

long - toggle modes

### none 

short - toggle to tt (and back)
long - set i2c

### flash

short - cancel/eject

---


arc display functions
	- range x0-x1 with value
		- segment (16 split, draw single)
	- anti-aliased "point"
		- scale 0-10 position

### ARC

#### mode 0 - LEVELS

4x "levels" with indication
		>> according to quantize mode:
		>> free: 0-10 (like returns) with anti-alias
		>> semitones: fixed number of segments with gap
		>>>> show target value and current value (for slew)
	output cv
	output pulse trains
		++ map CV to pulse train rates

key 1: alt.
	hold: recall (segment), save-to (segment), seg start, seg length
		>> highlight active action,
		++ action completed on key release
		++ segs can be adjusted in realtime while clock input is changing 
		>> seg start/length display linked. 16 segments total. 0 is top.
	fast: next
		++ respects seg length
in 1: next
		++ respects seg length

key 2: 
	hold: quantize mode: free vs. semitones with scale setup (mode/range, offset/octave, slew)
		++ push key 1 to cycle through edit channel: 1-4, all (blink for a second)
		>> free vs semi: two icons-- anti-aliased point vs. block
		>> mode: show scale stackup (intervals) highlight octaves
		>> range: show with ticks for volts, include offset in shading
		>> octave: left side, octave setting 0-8 (segmented by 2's)
		>> offset: include range with shading
		>> slew: bar with highlighted point (anti-aliased)
	fast: reset to seg start
in 2: reset to segment start

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
			OR slew mode -- linear interp between all stages vs.
		3: force (sensitivity)
		4: friction
	fast: edit mode (speed/length/start)
in 1: jack present: ext sync mode on. measure tr period. speeds are multiples.

key 2: reset
in 2: reset




### GRID

#### mode 0: kria

toggle vs hold key modes?

key 1: tempo (grid mode: tap, push, jump, halve/double) / (tt clock)
in 1: clock

key 2: config (graphic selector)
	a: four voice, two voice, disconnected
	b: sync: all (note+tr in voice mode, loops), independent
in 2: reset

grid layout:
	voice(1-4)

	trigger
		4voice: 64 steps (? in unsync mode only? how does probability work then?)
		2voice: 16 step + accent
		ind: 16 steps
	gate time
	note
	octave

	loop length
	speed multiple
		fullscreen squares?
	probability
		four levels

	scale

	pattern
		(indicate blank vs. not)

	alt
		(voice: mute)







#### mode 1: meadowphysics

toggle vs hold key modes?

key 1: tempo (grid mode: tap, push, jump, halve/double) / (tt clock)
	if in1 jack present: division/mult
in 1: clock

key 2: config
	a: scale modes (shared with kria?)
	b: voice shift register vs allocate (steal vs. not) vs fixed four vs tt driven
	c: trig on position set
in 2: reset

> tt param read for trigger/gate states mapped overriding to tt tr inputs
> driven on tt clock (must prevent self-clocking infinite loop)

identical mp layout





### MIDI

converter. modes. arpy. tt.

#### mode 0: voice allocator / mono out

key 1: panic
in 1: panic

(alt: jack present, quantize mode. release new note on pulse up)

key 2: change data
in 2:

data 0: poly
	cv/tr rotating outputs

data 1: mono
	cv 1: pitch
	cv 2: velocity
	cv 3: mod
	cv 4: bend?
	tr 1: gate
	tr 2: damper/sustain (cc64)
	tr 3: generic (cc80)
	tr 4: sync?

data 2: cc+trigs
	fixed cc map (16-19) + note trigs

#### mode 1: arpeggiator

tr 1-4: different width gates, or speed mults?
cv 1-4: different arp modes (up, down, tri, random)
		or different speed multiples??

key 1: tap tempo
in 1: sync

key 2: arp mode (cycle entire bank)
in 2: reset

cc control for internal speed with no jack
cc control for pulse length multiplier
cc control for speed mult?

send midi sync to midi out port?
what to do with midi sync in??




### USB mass storage (not a real "mode")

mode white: read all presets
mode orange: write all presets
key 1: go
key 2: cancel

(blink during read/write, off when ready for eject)





### NONE?

resume previous mode
tap for TT toggle


