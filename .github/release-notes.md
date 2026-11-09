HI5, a 5-voice FM glitch beat generator for the TrimUI Brick, Brick Hammer and Brick Pro running
MustardOS.

**Install on MustardOS**

Copy `HI5-mustardos-brick.muxapp` (Brick, Brick Hammer) or `HI5-mustardos-brickpro.muxapp` (Brick Pro) into
the `ARCHIVE` folder of the first SD card, install it from **Applications → Archive Manager**, then open
**Applications → HI5**. Press MENU to quit.

Or, with the TrimUI's SSH server on, run this on a computer on the same network:

```sh
curl -fsSL https://raw.githubusercontent.com/sydilaxe/Trimui-Hi5-dev/HEAD/install.sh | bash
```

**Change: MustardOS only**
- HI5 is now built only for MustardOS, on the Brick (and Brick Hammer) and the Brick Pro. The TrimUI OS zips are
  no longer released; a copy installed from one won't find its update, so install the `.muxapp` instead.

**New: the Brick Pro's lights follow the music**
- Each stick's ring points where the stick is pushed: the left in the filter's blue, the right in the colour of
  the FX it plays. The top bar is a stereo level meter of the output, and the lights under L1, L2, R1 and R2
  flash on the beat.
- When HI5 closes the lights go back to how they were. In `launch.sh`, `HI5_LEDS=0` leaves them alone,
  `HI5_LED_BAR=0` keeps the top bar dark and `HI5_LED_BRIGHTNESS=1..60` sets their brightness.

**Fix: the black screen on MustardOS**
- The picture is now sent to the screen opaque. Its pixels went out with their alpha (transparency) at 0, which
  TrimUI OS ignores but MustardOS's display shows, so on MustardOS nothing the app drew was visible.
- After MustardOS's screen wake the app makes its renderer again, and it rebuilds the picture whenever SDL reports
  it lost or drawing fails. The log's `SCREEN` lines record each rebuild.

**New: the important settings by MIDI CC**
- The voice, mixer, pattern and play settings that matter most each have a MIDI CC, on any channel: CC 2 chooses
  the voice, 14-24 set its params, 25-30 its level, pan, drive, send, mute and solo, 85-90 the tempo and bus
  (BPM, MASTER, MASTER DRIVE, REVERB type, time and tone), 102-113 its HIT, STUT and GLITCH cells, 114-115 the
  pattern slot and the kit.
- `docs/MIDI.md` is the MIDI implementation chart.

**Fix: a loud click when the mix ran hot**
- With MASTER DRIVE up, the master's make-up gain could lift a sample past full scale, and it wrapped round to the
  other side: a loud click. The output is now held to full scale.

**New: deeper logs, to find clicks and tune the app**
- `log.txt` adds a **PROC** line every 10 s: the processor's time by thread (the audio thread marked), memory and
  its peak, page faults, free memory, and how often the audio thread was preempted.
- **JUMP** lines: HI5 watches its own output for clicks and logs each with its time (it found the one above). A
  click heard with no JUMP and no dropout comes from after the app, in the sound server.
- STAT counts late audio calls (dropout or not) and jumps; the log names SDL's audio driver and what the device took.
- `sysinfo.txt` records the sound path: PipeWire's devices, rate and quantum, the mixer, and every 10 s PipeWire's
  nodes with their xrun (ERR) counts.

**New: MustardOS, and logs for tuning**
- Packages for MustardOS on the Brick, Brick Hammer and Brick Pro, installed from the Archive Manager. The app
  reads MustardOS's controller, so every button (and the Brick Pro's sticks) works, and it
  updates itself from the MustardOS package.
- The screen no longer stays dark on a Brick Pro under MustardOS, and the loading screen stays up while the
  samples load.
- `log.txt` now has a performance line every 10 s (audio load, dropouts, the screen, the processor's clock and
  temperature) with hints on what to change, and `sysinfo.txt` a report of the system. `HI5_LOG=2` in
  `launch.sh` logs every second and every button press.

**New on TrimUI**
- Ported from the Miyoo Mini Plus (HI5 0.0.11): the same voices, sequencer, FX and sound, on SDL2, the pages
  scaled 1.6x to fill the 1024x768 screen. Songs saved on the Miyoo load as they are.
- **Tighter Wi-Fi sync**, from Columbia Street: followers line up what is heard to within a millisecond or
  so. Still syncs with older HI5 units, Columbia Street and Jaffedrone.
- Half the Miyoo's audio delay (512-frame buffers), memory locked, the screen below the sound.
- **USB MIDI clock:** clock, START and STOP out to a drum machine or synth while it plays; clock in leads the
  tempo and phase, START / CONTINUE / STOP / song position follow it.
- **USB audio:** a USB sound card or DAC takes over the output while it is plugged in.
- **Brick Pro:** the left stick closes the L2 filter by hand; the right stick plays FX slots 1-4; L3 taps
  the tempo, R3 cycles the L2 sweep rate.
- **Updates itself** from this repo's releases, each device fetching its own package.

If you hear crackles, set `HI5_BUFFER=1024` in `launch.sh` in the app folder
(`MUOS/application/HI5`). The logs are written to `log.txt` and `sysinfo.txt` in the same folder.
