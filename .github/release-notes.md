HI5, a 5-voice FM glitch beat generator for the TrimUI Brick, Brick Hammer and Brick Pro (TrimUI OS).

**Install**

Unzip `HI5-brick.zip` (Brick, Brick Hammer) or `HI5-brickpro.zip` (Brick Pro) at the root of the SD card
(it contains `Apps/HI5/`), then open **Apps → HI5**. Press MENU to quit.

Or, with the TrimUI's SSH server on, run this on a computer on the same network (add `HI5_DEVICE=brickpro`
before `bash` the first time on a Brick Pro):

```sh
curl -fsSL https://raw.githubusercontent.com/sydilaxe/Trimui-Hi5-dev/main/install.sh | bash
```

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
- **Updates itself** from this repo's releases, each device fetching its own zip.

If you hear crackles, set `HI5_BUFFER=1024` in `Apps/HI5/launch.sh`. The log is written to
`Apps/HI5/log.txt`.
