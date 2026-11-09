# HI5

A 5-voice FM glitch beat generator for the TrimUI Brick, Brick Hammer and Brick Pro running
[MustardOS](https://muos.dev), ported from the Miyoo Mini Plus version.

It has four FM percussion voices (SUB, KICK, SNARE, PERC) and a HAT voice, which is a
micro-loop sampler. Each voice is sequenced by three polymetric Euclidean cells: HIT, STUT
(ratchets) and GLCH (variations). On top of that are punch-in performance FX, a tempo-synced
resonant filter sweep, a reverb bus, pattern and kit memory, a song chain, a mono + clock-pulse
output for syncing other gear, Wi-Fi tempo sync between units, and USB MIDI clock in and out.

The engine is the Miyoo version's, unchanged: the same song sounds the same, sample for sample,
and a `hi5.sav` saved on the Miyoo loads as it is.

**Latest release:** [v0.5.0](../../releases/tag/v0.5.0) · [all releases](../../releases)

## Install

HI5 is built only for MustardOS. Each release has a package for each device; the Brick Pro's adds
the analog sticks.

| Device | Package |
| --- | --- |
| **Brick**, **Brick Hammer** | `HI5-mustardos-brick.muxapp` |
| **Brick Pro** | `HI5-mustardos-brickpro.muxapp` |

### MustardOS

1. Download the `.muxapp` for your device from the latest [release](../../releases), or from the
   artifacts of the latest run on the Actions tab, or build it (see below).
2. Copy it into the `ARCHIVE` folder at the root of the first SD card (SD1).
3. On the device, open **Applications → Archive Manager** and install it. It lands in
   `MUOS/application/HI5` (on SD2 if you have one, else SD1).
4. Launch **HI5** from **Applications**. Press MENU to quit.

By hand instead: unzip the `.muxapp` (it is a zip) and copy its `HI5` folder into
`MUOS/application/` on the card. `make install SD=/path/to/card` (add
`DEVICE=brickpro` for the Brick Pro) does the same from a build.

The app reads MustardOS's own controller, so the buttons are as labelled whatever A/B layout
MustardOS is set to. `log.txt` in the app folder names the controller it found.

On the Brick Pro the screen can stay dark after an app opens under MustardOS (it plays, but
nothing shows until the power button sleeps and wakes the device). Once its window is up, HI5
runs `wake_screen.sh`, which does what MustardOS does on waking: the framebuffer blanked and
unblanked, and the saved brightness applied again. `HI5_SCREEN_WAKE=` (empty) in `mux_launch.sh`
turns it off.

The state autosaves to `hi5.sav` in the app folder. A song saved on the Miyoo loads as it is: copy its `hi5.sav` into the folder.

### Over Wi-Fi

With the TrimUI's SSH server on (on MustardOS, Secure Shell under Web Services), run this on a
computer on the same network:

```sh
curl -fsSL https://raw.githubusercontent.com/sydilaxe/Trimui-Hi5-dev/HEAD/install.sh | bash
```

The script scans the local /24 for an SSH server (a handheld's dropbear first), checks the
installed version against the latest release, and if the release is newer copies it to
MustardOS's application folder. Your song (`hi5.sav`) and samples are
kept. It logs in as `root` (`HI5_USER` changes it). MustardOS says which device it is, so it
picks the Brick or Brick Pro build by itself (`HI5_DEVICE=brickpro` or `brick` overrides it). The address that worked is
saved in `~/.config/hi5-trimui/host`; `HI5_HOST=192.168.1.28` skips the scan. `HI5_FORCE=1`
reinstalls even when the version matches. Quit HI5 on the TrimUI before updating.

### Updating from the TrimUI

HI5 keeps itself up to date. Five seconds after it starts, it asks GitHub for the latest
release, retrying each minute (up to five times) while Wi-Fi comes up. When there's a newer one
it asks **A: update and restart / B: not now**. A downloads the release's package for this
device while the music keeps playing. It checks every file, swaps in the ones that changed and starts
the new version. Your song and your own samples stay as they are. If anything goes wrong on the
way, nothing is changed. B asks again next time you open the app. `HI5_UPDATE=0` in `launch.sh`
turns the check off. An update replaces `launch.sh`, so set any of its switches again
afterwards.

### Your own hat samples

Put `.wav` files in the app folder's `samples/` (`MUOS/application/HI5/samples/`): 8/16/24/32-bit PCM or 32-bit float, mono or stereo,
any sample rate, up to 3 s each. They load in filename order after the built-in sounds
(CHAT, OHAT, PEDAL, SHAKR, RIDE). Pick them on the SYNTH page under HAT > SAMPLE.

## Controls

These work on every page:

| Button | Action |
| --- | --- |
| L1 / R1 | Previous / next page |
| START | Play / stop |
| L2 (hold) | Tempo-synced resonant filter sweep |
| SELECT | Cycle the L2 sweep rate (1/4, 1/2, 1 bar, 2 bars) |
| R2 + d-pad | Play the punch-in FX in slots 1-4 (up, down, left, right) |
| R2 + X | Tap tempo |
| MENU | Quit |

When a new version is out, a prompt covers the page: A updates and restarts, B (or MENU) says
not now. The sequencer keeps playing, and the rest of the buttons wait until it closes.

The pages are PATTERN, MIX, SYNTH, FX and SONG. The footer of each page shows its own keys.

- **PATTERN** shows a 5 × 3 grid of cells. Each voice has a HIT, STUT and GLCH cell.
  Use the d-pad to select a cell, A to toggle it, B for fewer hits, Y to re-permute the
  Euclidean rhythm and X to rotate it.
- **MIX** has a column per voice plus a master column. Left / right picks the column,
  A picks the row and up / down edits it. A voice column has LEVEL, PAN, DRIVE and SEND
  (reverb). The master column has LEVEL, DRIVE, VERB (DIGITAL / SPRING), TIME and TONE.
  Y mutes the voice and X solos it.
- **SYNTH** edits each voice's parameters. Use the d-pad to select, Y / X for - / + and
  A to audition. The FM voices have TUNE, RATIO, INDEX, DECAY, SWEEP, FEEDBK, NOISE
  (DRIVE on SUB), STUTTR and GLITCH. The hat has SAMPLE, START, LOOP, PITCH, TONE, DRIFT,
  RESO and SHINE.
- **FX** sets up the four R2 slots. Each slot has a type, two parameters and a mix. The
  types are REPEAT, TAPESTOP, REVERSE, CRUSH, GATE, DELAY and GRAIN. The OUT row switches
  between stereo and mono + clock, and sets the clock side, PPQN (1-32) and pulse width.
- **SONG** has eight pattern slots (A-H), eight kits (synth params + mix balance), a 16-step
  chain, the MIDI and START settings and the Wi-Fi sync panel.
  - On MIDI or START (left / right past the chain): up / down or A changes it (see
    [USB: MIDI clock and audio](#usb-midi-clock-and-audio)).
  - On a pattern slot or kit: A loads, X saves, Y copies a pattern slot and R2+Y clears.
  - On a chain node: up / down picks the slot, A cycles the bars (1/2/4/8), X pastes,
    Y copies, R2+Y clears and R2+A plays the song from that node.
  - B toggles song mode and loop mode. R2+B hops between the slots and the chain.

## Brick Pro: the sticks

The Brick Pro build adds the two analog sticks, for playing live. They spring back when let
go, and nothing they do is saved.

| Stick | Does |
| --- | --- |
| left, pushed any way | Closes the L2 filter by hand: the further, the darker. Let go, it opens again. L2 held wins |
| right, up / down / left / right | Plays FX slot 1 / 2 / 3 / 4, as R2 + the d-pad |
| L3 (click) | Tap tempo |
| R3 (click) | The next L2 sweep rate, as SELECT |

R2 + the d-pad wins over the right stick while both are held.

### The Brick Pro's lights

While HI5 runs on the Brick Pro it drives the lights one by one:

| Lights | Show |
| --- | --- |
| left stick's ring | Points where the left stick is pushed, in the filter's blue: the further, the brighter |
| right stick's ring | Points where the right stick is pushed, in the colour of the FX it plays (REPEAT orange, TAPESTOP red, REVERSE violet, CRUSH amber, GATE green, DELAY blue, GRAIN pink) |
| top bar | A stereo level meter of the output, from the centre out: the left channel to the left, the right to the right, green rising to amber and red near full scale |
| under L1, L2, R1, R2 | The beat while playing: a flash each quarter note, orange on the bar's first |
| F1, F2 | Dark |

When HI5 closes, the lights go back to how they were (colours, effects, brightness). Settings
in `launch.sh`: `HI5_LEDS=0` leaves the lights alone, `HI5_LED_BAR=0` keeps the top bar dark,
`HI5_LED_BRIGHTNESS=1..60` (60 by default).

## Linking Bricks over USB

Several Bricks can play in time with one, by cable, with no MIDI interface between them: the
**leader** has a USB hub in its **top** port, and each **follower** plugs its **bottom** (charging)
port into the same hub, with a data cable. The wiring decides who leads, and any number of Bricks
can follow one (up to eight MIDI devices and Bricks on the leader together). Use a **powered hub**:
each follower charges from it. The link is the same in Columbia Street, HI5, Jaffedrone and
RadioScan, so a Brick running one can lead Bricks running the others.

Nothing to set up: when HI5 starts it offers its bottom port as a "TrimUI clock link" (TrimUI's
kernel has no USB MIDI gadget, so the link is the apps' own), and a leader finds the Bricks on its
hub within a second and sends them its clock with START and STOP, as to MIDI gear, plus where its
sound is about 50 times a second. Each follower moves its own sound onto that, measured by its
audio clock, so they sound together within a tenth of a millisecond, at any tempo up to 250 BPM. A
follower plugged in mid-song comes in at the leader's place. The leader can still use USB audio and
MIDI gear on the same hub, and its sound is what the followers line up with, through whichever
output it plays. A leader that itself follows a drum machine's MIDI clock passes it on, so the
whole chain follows the drum machine.

On the SONG page the MIDI setting's label reads LINK while Bricks are linked; its value is blue
while following and green while sending. Set START to IGNORE on a follower to start and stop it
yourself: it keeps the leader's tempo and lines up with its bar as soon as it plays.
`HI5_LINK=0` in `launch.sh` leaves the bottom port alone.

## USB: MIDI clock and audio

The Brick has two USB-C ports: the top one is the USB host port, the bottom one charges. Plug
class-compliant MIDI and audio devices into the **top** port, directly with a USB-C cable or
through a hub. They are found as they are plugged in, and a message names each one. The port
gives about 500 mA, so a device that draws more needs a powered hub.

MIDI and audio work at the same time, from one interface that does both or from separate devices
on a hub.

- **MIDI on the SONG page:** picks which way the clock goes. **AUTO** (the default) sends it, and
  follows a clock that comes in instead; **SEND** always sends it and ignores one coming in;
  **FOLLOW** only follows; **OFF** neither (CCs still come in). Its value is blue while following a
  clock and green while sending one, dim with no MIDI plugged in.
- **START on the SONG page:** FOLLOW (the default) starts, stops and places the sequencer with the
  clock's START, STOP, CONTINUE and song position; **IGNORE** follows the clock's tempo only, so you
  start and stop it yourself (from a linked Brick it still lines up with the leader's bar).
- **MIDI clock out:** while the sequencer plays and no clock is coming in (AUTO), or always (SEND),
  HI5 sends clock (24 a quarter note), START and STOP to every MIDI port. The clock is timed to the
  sound as it is heard (the audio clock, through whichever output plays it), so a drum machine or
  synth lands on the beat.
- **MIDI clock in** (AUTO or FOLLOW): the tempo follows it (a straight line through the last four
  beats of ticks, so USB's jitter averages out: within a few tenths of a millisecond at 250 BPM),
  and its phase is held by trimming the playback speed, as Wi-Fi sync does. START plays from the
  first step, CONTINUE and song position pick up where they say, STOP stops.
- **Settings by MIDI CC:** the important voice, mixer, pattern and play settings each have a CC, on
  any channel: CC 2 chooses the voice, 14-24 its params, 25-30 its level, pan, drive, send, mute and
  solo, 85-90 the tempo and bus, 102-113 its HIT, STUT and GLITCH cells, 114-115 the pattern slot and
  the kit. [docs/MIDI.md](docs/MIDI.md) is the full MIDI implementation chart.
- **Audio:** a USB sound card or DAC takes the output while it is plugged in, and the speaker and
  headphones take it back when it is unplugged.

Settings in `launch.sh` in the app folder, before `./hi5`: `HI5_MIDI=0` turns MIDI off (and the link),
`HI5_MIDI_CLOCK=0` never sends clock, `HI5_MIDI_FOLLOW=0` never follows it, whatever the SONG
page says; `HI5_LINK=0` leaves the bottom port alone. `HI5_AUDIO=internal`
keeps the speaker; `HI5_AUDIO=<name>` picks a device by name (`log.txt` lists what was opened).

## Audio

The sound is made in 512-frame buffers at 48 kHz (about 11 ms, half the Miyoo's delay), with
memory locked so it never waits for a page to be read back in; the screen runs below it at 30
frames a second, and the buttons are read every few milliseconds. If you hear crackles, set
`HI5_BUFFER=1024`. `HI5_RATE=44100` does a little less work. `HI5_ROTATE=180` turns the picture and
`HI5_JOYMAP=A=1,B=0,...` renumbers the buttons (also `L2`, `R2`, `L3`, `R3`), for a firmware that
differs.

## Logs and tuning

Two files in the app folder tell you how HI5 runs on your device, Brick or Brick Pro:

- **`log.txt`**, written by the app. At start: the firmware and board, the build, the processor's
  governor and clock range, the memory, the screen and how it is drawn, the controllers found
  (and which numbering is used), the sound device and buffer, and how long start-up took. Then
  a **STAT** line every 10 seconds:

  ```
     30.512 STAT  audio load avg 9% peak 21% | dropouts 0, late calls 0, jumps 0 | gap max 11.0 ms (buffer 10.7) | 94 calls/s | screen 30 fps, avg 6.2 ms max 9.8 ms | cpu 1800 MHz 51 C cores 0-3 load 0.62
     30.512 PROC  cpu 24% (audio:SDLAudioP2 11%, hi5 8%, midi 1%) | memory 22.4 MB, peak 23.0 | faults 0 major, 4 minor | free 540 MB | audio thread waited 940, preempted 1
  ```

  *audio load* is the time spent making sound against the time it lasts (over 60%, crackles
  are near); *dropouts* count the times the sound device ran dry (a crackle each); *late calls*
  the buffers asked for over one and a half buffers late (a dropout or not); *jumps* the times
  HI5's own output jumped far more sharply than the sound around it (a click made inside the
  app, each also logged as a **JUMP** line with its time; a click heard with no jump and no
  dropout comes from the sound server, see `sysinfo.txt`); *gap max* is the longest wait
  between two buffers. The **PROC** line shows where the processor's time goes (the app and its
  busiest threads, as % of one core; the audio thread is marked), the app's memory and its peak,
  page faults, the system's free memory and how often the audio thread was preempted. **HINT** lines say what to change, such as
  `HI5_BUFFER=1024` when the load is high or there were dropouts, or that the processor was
  slowed down or is hot. At quit an **END** line sums up the session's worst figures.
- **`sysinfo.txt`**, written by `sysinfo.sh` from `launch.sh`: the firmware, the processor,
  memory, sound (SDL's driver settings, PipeWire's devices, rate and quantum, the mixer), USB
  and input devices and the processes running at start, then every 10 seconds the system load,
  clock, temperature, the busiest processes and PipeWire's nodes with their **ERR** count: the
  times the sound server itself ran short.

`HI5_LOG=2` in `launch.sh` writes the STAT line every second and logs every button press, hat
and trigger as the controller sends it and as HI5 reads it, and on the Brick Pro the sticks'
moves: the way to check a firmware's button numbering. `HI5_LOG=0` turns the log off and
`HI5_SYSINFO=0` the system report. Both files are rewritten each time HI5 starts.

## Wi-Fi sync

HI5 units on the same network share tempo, bar phase and transport over UDP broadcast on
port 5150 (the HI5S protocol, also spoken by Columbia Street and Jaffedrone). Set the mode on the
SONG page's sync panel: OFF, AUTO, LEAD or FOLLOW. A unit set to LEAD wins. Otherwise the unit
with the lowest id leads. The TrimUI's Wi-Fi must be on.

This build has the newer sync from Columbia Street: followers line up what is heard, to within a
millisecond or so. Each packet carries the time it was sent, so Wi-Fi delays don't count, and
units echo each other's packets to time the trip. Older HI5S units (HI5 on the Miyoo, before this
sync) still sync with these, a few milliseconds less closely. If one unit sounds early or late
against another, `HI5_SYNC_LATENCY` in `launch.sh` sets the frames its sound driver holds (the
buffer size, 512, by default).

## Building

You need [zig](https://ziglang.org/) 0.14 for the cross-compile: `zig` on the path, `ZIG=...`, or
`pip install ziglang==0.14.1` (the Makefile then uses `python3 -m ziglang`). You also need `curl`,
`bzip2`, `zip` and `python3`, and for `make test` a host C compiler and ImageMagick.

```sh
make              # cross-compile build/brick/hi5 and build/brickpro/hi5
make package      # the MustardOS .muxapps, as CI does
make test         # host harness: screenshots, audio and checks into build/test
make update-test  # the self-update end to end, against tools/fake_github.py
make install SD=/path/to/mounted/sdcard [DEVICE=brickpro]
make build/hi5_synctest   # host-side Wi-Fi sync interop check
```

Both builds are aarch64 for glibc 2.28, for the Cortex-A53. They link against a stand-in
`libSDL2-2.0.so.0` made by `tools/mksdlstub.py`; the device's own SDL2 is used at run time.
`-DHI5_BRICKPRO` adds the sticks and makes the updater fetch the Brick Pro's packages.

At run time the app reads MustardOS's "muOS-Keys" controller (its numbering is MustardOS's own,
from its `device/tui-brick*` controller maps), and updates from its device's `.muxapp`.

`src/font_data.h` is a generated file, made by `tools/mkfont.py` (from Noto Sans Mono Bold).

### Releases

CI (`.github/workflows/build.yml`) builds both MustardOS packages with zig and runs the tests on
every push. To publish a release, push a tag (`git tag v1.2.3 && git push origin v1.2.3`), or run
the workflow from the Actions tab with the tag in `release_tag`. The version comes from the tag:
CI builds the release with `make package VERSION=1.2.3`, which the app compares with the latest
release's tag to tell whether it's out of date. A tag must be `v` and dotted numbers. Plain builds
use `HI5_VERSION` in `src/engine.h` (`make version` prints the version a build gets); keep it, and the README's **Latest release** line, at the latest release (CI checks the two match). Releases are full releases marked Latest, since the updater and `install.sh` read
GitHub's latest release, and that skips pre-releases. They have
`HI5-mustardos-brick.muxapp` and `HI5-mustardos-brickpro.muxapp` attached and
`.github/release-notes.md` as their notes.

`build/hi5_update APP_DIR [--force]` (built by `make update-test`) checks for a newer release
and installs it into `APP_DIR`, the way the app does. Point `HI5_UPDATE_API` at
`tools/fake_github.py` (`http://127.0.0.1:8766/latest.json`) to test an update without
publishing one.

The updater's HTTPS comes from [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) 3.6.7, which
`make` downloads (pinned by its SHA-256) into `third_party/mbedtls/` (not committed) and links
in statically.

### Layout

| Path | Contents |
| --- | --- |
| `src/engine.c` | Voices, sequencer, FX, reverb, save / load (the Miyoo version's) |
| `src/ui.c` | Pages, input, drawing |
| `src/gfx.c`, `src/font_data.h` | Framebuffer drawing and the font |
| `src/sync.c` | Wi-Fi sync (Columbia Street's) |
| `src/aclock.c` | Audio clock: where the music is at the speaker, for Wi-Fi sync |
| `src/update.c`, `src/net.c` | Self-update: GitHub's latest release, the download and unzip; HTTPS |
| `src/main_trimui.c` | Device entry point (SDL2: video scaled 1.6x, buttons, sticks, USB audio) |
| `src/midi.c` | USB MIDI clock in and out (ALSA raw MIDI) |
| `src/link.c` | The clock link between Bricks (the follower's bottom port as a USB gadget) |
| `src/perflog.c` | The performance log in `log.txt` (shared with RadioScan) |
| `src/ringleds.c` | The Brick Pro's lights, one by one (shared with RadioScan) |
| `src/main_test.c` | Host test harness |
| `src/main_synctest.c` | Host sync interop test |
| `src/main_update.c` | Host updater (`build/hi5_update`) for tests |
| `install.sh` | The one-line installer, over SSH |
| `tools/fake_github.py`, `tools/update_test.sh` | A local GitHub stand-in and the update test |
| `tools/mbedtls.mk` | Builds Mbed TLS for each toolchain |
| `tools/mksdlstub.py` | The stand-in SDL2 library to link against |
| `dist/HI5/` | The app folder as installed on the card (`sysinfo.sh`: the system report) |
| `dist/mustardos/` | What the MustardOS package adds: `mux_launch.sh`, `wake_screen.sh`, the list glyph |

## License

MIT. See [LICENSE](LICENSE). The SDL2 headers in `third_party/` are under the zlib licence; see
`third_party/SDL2-2.30.9-headers/LICENSE.txt`. The font is Noto Sans Mono (SIL OFL 1.1). Mbed TLS
is under Apache-2.0 (or GPL-2.0-or-later); the app folder carries its licence as
`LICENSE-mbedtls.txt`. `dist/HI5/cacert.pem` is curl's extract of Mozilla's CA certificates
(MPL 2.0).
