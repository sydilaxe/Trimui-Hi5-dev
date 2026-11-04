# AGENTS.md

HI5 is a 5-voice FM glitch beat generator for the TrimUI Brick, Brick Hammer and Brick Pro running
TrimUI OS or MustardOS (Allwinner A133P: four Cortex-A53 cores, 1 GB, a 1024x768 screen), ported from the Miyoo
Mini Plus version (mercurious/hi5) the way Columbia Street was (sydilaxe/Trimui-Columbiastreet-dev).
Four FM percussion voices and a micro-loop hat sampler, each sequenced by three Euclidean cells
(HIT, STUT, GLCH).

`make` cross-compiles two builds with zig (aarch64, glibc 2.28) against the device's SDL2:
`build/brick/hi5` and `build/brickpro/hi5` (`-DHI5_BRICKPRO` adds the analog sticks and makes the
updater fetch the Brick Pro's packages). `make package` zips both for TrimUI OS (`Apps/HI5`) and as
MustardOS application archives (`HI5-mustardos-*.muxapp`: the same binaries in `HI5/`, plus
`dist/mustardos/mux_launch.sh`, `wake_screen.sh` (run through `HI5_SCREEN_WAKE` once the window is
up, as the screen can stay dark on MustardOS until a sleep and wake) and the list glyph). The
samples load on a thread while the loading screen keeps being presented. `make test` renders
screenshots and audio on the host into `build/test` and prints the checks. Without zig on the
path, `pip install ziglang==0.14.1`.

Where the code comes from:

- `src/engine.c`, `src/ui.c`, `src/gfx.c`: the Miyoo version's. Keep the engine the same sound as
  there: with the sticks at rest, `build/test/session.wav` matches the Miyoo harness's sample for
  sample. The TrimUI additions are small and marked (the audio clock hooks, the Brick Pro's
  `live.sweep_hand`, 64-bit ARM flush-to-zero, the update prompt).
- `src/sync.c`, `src/aclock.c`: Columbia Street's, the newer HI5S sync. Shared with the sibling
  apps; keep the copies the same (they are byte for byte Columbia Street's).
- `src/main_trimui.c`, `src/midi.c`, `src/update.c`, `src/net.c`, `install.sh`, `tools/`,
  `dist/mustardos/`: Columbia Street's TrimUI platform layer, renamed for HI5. It reads TrimUI OS's
  controller or MustardOS's "muOS-Keys" one, found at run time.
- `src/perflog.c` and `dist/HI5/sysinfo.sh`: the performance log (`log.txt`) and system report
  (`sysinfo.txt`), shared with RadioScan (`sysinfo.sh` with Jaffedrone too); keep the copies the same.
  The audio callback only calls `perf_audio_begin` / `perf_audio_end`. When you add a setting that
  changes performance, give the HINT lines a word on it.
- `src/ringleds.c`: the Brick Pro's lights one by one (shared with RadioScan; keep the copies the same).

When you change a control or a screen, update the README's Controls section (and the Brick Pro
table for the sticks) and the footer hints in `src/ui.c`. When you change the engine, run `make
test` and keep its checks passing (CI greps for them in `.github/workflows/build.yml`).

The app updates itself from the latest GitHub release of this repo (`src/update.c`, HTTPS through
Mbed TLS): each build fetches its own zip (`HI5-brick.zip` or `-brickpro.zip`, or the `.muxapp` of
the same name with `-mustardos-` when the app folder has `mux_launch.sh`). Keep `mux_launch.sh` out
of the TrimUI OS zips: it is how the app tells the two apart. Releases take
their version from the tag (CI builds with `make VERSION=<tag without v>`, compiled in as
`HI5_VERSION`); `HI5_VERSION` in `src/engine.h` is only the default for plain builds: keep it, and the README's **Latest release** line, at the latest release (CI checks the two match). When you
change the updater or the package layout, run `make update-test`.

## Identity

This repo is in the `sydilaxe` lane: it is pushed from `sydilaxe`'s account. The Miyoo original lives under
another identity; keep that repo's owner email, and anyone's real name or other accounts, out of
code, docs, commit messages, issues and PRs here. Don't import its git history.
