## v2.x.x

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
