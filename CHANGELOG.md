## v3.1.0
- **FIX**: tuning customization behavior in midi modes
- **FIX**: send original rather than customized pitch table values to ii followers
- **FIX**: bugs preventing saving / loading some presets to USB
- **FIX**: disable i2c during preset writes to prevent possible loss of data
- **FIX**: improve delay when connecting grid and arc
- **FIX**: reset JF velocity when exiting leader mode
- **NEW**: freeze one pattern for editing while meta sequence continues to run
- **NEW**: optionally reset the entire meta pattern on a reset trigger
- **NEW**: optionally always tie notes when duration is maxed
- **NEW**: Disting EX as ii follower
- **NEW**: W/syn as ii follower
- **NEW**: crow as ii follower
- **NEW**: supports new `ES.CV` teletype op


## v3.0.0

- **NEW**: i2c leader mode for controlling Just Friends, TELEXo, or ER-301 from ansible
- **NEW**: kria: playhead shows on probability page
- **FIX**: avoid some types of i2c crashes
- **FIX**: kria: glitches when stopping and restarting external clock
- **NEW**: compensate-shift scales by holding scale key when changing scale notes
- **NEW**: supports new kria teletype ops: `KR.CUE`, `KR.DIR`, `KR.DUR`


## v2.0.0

- **FIX**: meadowphysics: fix trigger behavior in 1 CV/TR mode
- **NEW**: earthsea grid app
- **NEW**: save/load presets to USB disk
- **NEW**: grid interface for tuning CV outputs
- **NEW**: kria: step direction modes (forward, reverse, pendulum, drunk, random)
- **NEW**: kria: track-wide octave shift
- **NEW**: kria: quantize clock division changes to loop endpoints (configurable)
- **NEW**: kria: sync clock division changes across parameters or tracks (configurable)
- **NEW**: kria: toggle individual triggers when ratcheting
- **NEW**: kria: clock advances the note only when a trigger happens (configurable)
- **NEW**: kria: ability to have note sync ON and loop sync OFF
- **NEW**: shift the value of scale notes without affecting the rest of the scale
- **NEW**: supports new `ANS` teletype ops for grid, arc, and app state


for changes in older versions see [releases](https://github.com/monome/ansible/releases)
