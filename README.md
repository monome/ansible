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

## mode depends on USB device present

### ARC

#### mode 0 - LEVELS

4x "levels" with indication
	output cv
	output pulse trains

key 1: alt.
	hold: recall (segment), next (segment), slew, pulse-base-speed
	fast: next
in 1: next

key 2: reset to segment zero (or set individual slews multipliers?)
in 2: reset to segment zero

#### mode 1 - MORPH

each knob drives own pattern: show "cycle" position and current segment. trigger on axis (4x per rev)
trig sync per on each segment change

key 1: alt
	hold: changes config
		1: scrub/cycle toggle
		2: sync all on/off
		3: trig pulse width
		4:
	fast: sync? 
in 1: (sync?)

key 2: reset
in 2: reset

#### mode 2 - PHYSICS

outputs
1: speed
2: acc
3: x
4: y

digitals: ?? quadrants?

1: speed
2: friction
3: gravity
4: gravity direction

key 1: toggle views output vs. param (dim switch)
in 1: push left

key 2: momentary brake
in 2: push right


### GRID

kria rework, with modes and integrate switches



### MIDI

converter. modes. arpy. tt.


#### NONE?

cycle between other modes with mode key? + noise gen?


