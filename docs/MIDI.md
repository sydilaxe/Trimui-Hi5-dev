# HI5 MIDI implementation chart

HI5 reads and sends MIDI over USB: plug a class-compliant drum machine, sequencer, controller or interface into the
TrimUI's USB host port (the Brick's top USB-C, through an OTG adapter or hub). Ports are found as they are plugged in. Everything below works from any page.

## Summary

| Function | Transmitted | Recognized | Remarks |
|---|---|---|---|
| Basic channel | none | 1-16 (any) | Every message is heard on any channel. |
| Mode | none | Omni on | |
| Note number | none | no | |
| Velocity | none | no | |
| Aftertouch | none | no | |
| Pitch bend | none | no | |
| Control change | none | yes | The settings in the CC table below. |
| Program change | none | no | |
| System exclusive | none | no | |
| Song position | none | yes | Places the sequencer, in 16ths. |
| Song select | none | no | |
| Clock | yes | yes | Out: 24 a quarter note while it plays and no clock comes in, timed to the sound as it is heard. In: the tempo follows it, and its phase is held by trimming the playback speed. `HI5_MIDI_CLOCK=0` sends none; `HI5_MIDI_FOLLOW=0` ignores it. |
| Start, Continue, Stop | yes | yes | Out with the sequencer. In: START from the top, CONTINUE from where it stopped, STOP. |
| Active sensing | none | no | |
| All notes off, all sound off | none | no | |
| Reset | none | no | |

`HI5_MIDI=0` in `launch.sh` turns MIDI off.

## Settings by CC

The important voice, mixer, pattern and play settings, all on one channel (any channel is heard). A CC's 0-127 spans
the setting's whole range; an on / off setting is on from 64. The voice settings act on the voice CC 2 chooses: CC
14-24 are its params in the order of its page, so their meaning depends on the voice. The CC numbers are ones MIDI
leaves free, in blocks: 14-30 the voice, 85-90 the tempo and bus, 102-113 the voice's cells, 114-115 the pattern
slot and kit (each saved slot or kit a knob passes is loaded).

<!-- cc table -->
| CC | Setting | Range |
|---|---|---|
| 2 | VOICE: the one the voice CCs act on | 0-25 SUB, 26-50 KICK, 51-76 SNARE, 77-101 PERC, 102-127 HAT |
| 14 | Voice param 1 | SUB: TUNE -24 to 24; KICK: TUNE -24 to 24; SNARE: TUNE -24 to 24; PERC: TUNE -24 to 24; HAT: SAMPLE (what is loaded) |
| 15 | Voice param 2 | SUB: RATIO 0 to 12; KICK: RATIO 0 to 12; SNARE: RATIO 0 to 12; PERC: RATIO 0 to 12; HAT: START 0 to 100 |
| 16 | Voice param 3 | SUB: INDEX 0 to 100; KICK: INDEX 0 to 100; SNARE: INDEX 0 to 100; PERC: INDEX 0 to 100; HAT: LOOP 0 to 100 |
| 17 | Voice param 4 | SUB: DECAY 0 to 100; KICK: DECAY 0 to 100; SNARE: DECAY 0 to 100; PERC: DECAY 0 to 100; HAT: DECAY 0 to 100 |
| 18 | Voice param 5 | SUB: SWEEP 0 to 100; KICK: SWEEP 0 to 100; SNARE: SWEEP 0 to 100; PERC: SWEEP 0 to 100; HAT: PITCH -24 to 24 |
| 19 | Voice param 6 | SUB: FEEDBK 0 to 100; KICK: FEEDBK 0 to 100; SNARE: FEEDBK 0 to 100; PERC: FEEDBK 0 to 100; HAT: TONE 0 to 100 |
| 20 | Voice param 7 | SUB: DRIVE 0 to 100; KICK: NOISE 0 to 100; SNARE: NOISE 0 to 100; PERC: NOISE 0 to 100; HAT: DRIFT 0 to 100 |
| 21 | Voice param 8 | SUB: STUTTR 2 to 8; KICK: STUTTR 2 to 8; SNARE: STUTTR 2 to 8; PERC: STUTTR 2 to 8; HAT: STUTTR 2 to 8 |
| 22 | Voice param 9 | SUB: GLITCH 0 to 100; KICK: GLITCH 0 to 100; SNARE: GLITCH 0 to 100; PERC: GLITCH 0 to 100; HAT: GLITCH 0 to 100 |
| 23 | Voice param 10 | HAT: RESO 0 to 100 |
| 24 | Voice param 11 | HAT: SHINE 0 to 100 |
| 25 | Voice LEVEL | 0 to 100 |
| 26 | Voice PAN | -100 to 100 |
| 27 | Voice DRIVE | 0 to 100 |
| 28 | Voice SEND (reverb) | 0 to 100 |
| 29 | Voice MUTE | 0-63 off, 64-127 on |
| 30 | Voice SOLO | 0-63 off, 64-127 on |
| 85 | BPM | 40 to 250 |
| 86 | MASTER | 0 to 100 |
| 87 | MASTER DRIVE | 0 to 100 |
| 88 | REVERB type | 0-63 DIGITAL, 64-127 SPRING |
| 89 | REVERB TIME | 0 to 100 |
| 90 | REVERB TONE | 0 to 100 |
| 102 | Voice HIT cell ON | 0-63 off, 64-127 on |
| 103 | Voice HIT cell STEPS | 1 to 16 |
| 104 | Voice HIT cell HITS | 0 to its steps |
| 105 | Voice HIT cell ROTATE | 0 to its steps - 1 |
| 106 | Voice STUT cell ON | 0-63 off, 64-127 on |
| 107 | Voice STUT cell STEPS | 1 to 16 |
| 108 | Voice STUT cell HITS | 0 to its steps |
| 109 | Voice STUT cell ROTATE | 0 to its steps - 1 |
| 110 | Voice GLITCH cell ON | 0-63 off, 64-127 on |
| 111 | Voice GLITCH cell STEPS | 1 to 16 |
| 112 | Voice GLITCH cell HITS | 0 to its steps |
| 113 | Voice GLITCH cell ROTATE | 0 to its steps - 1 |
| 114 | Pattern SLOT | 0-15 A, 16-31 B ... 112-127 H (saved slots) |
| 115 | KIT | 0-15 1, 16-31 2 ... 112-127 8 (saved kits) |
<!-- end cc table -->
