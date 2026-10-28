# AGENTS.md

HI5 is a 5-voice FM glitch beat generator for the TrimUI Brick, Brick Hammer and Brick Pro running
TrimUI OS (Allwinner A133P: four Cortex-A53 cores, 1 GB, a 1024x768 screen), ported from the Miyoo
Mini Plus version (mercurious/hi5) the way Columbia Street was (sydilaxe/Trimui-Columbiastreet-dev).
Four FM percussion voices and a micro-loop hat sampler, each sequenced by three Euclidean cells
(HIT, STUT, GLCH).

`make` cross-compiles two builds with zig (aarch64, glibc 2.28) against the device's SDL2:
`build/brick/hi5` and `build/brickpro/hi5` (`-DHI5_BRICKPRO` adds the analog sticks and makes the
updater fetch the Brick Pro's zip). `make package` zips both for `Apps/HI5`. `make test` renders
screenshots and audio on the host into `build/test` and prints the checks. Without zig on the
path, `pip install ziglang==0.14.1`.

Where the code comes from:

- `src/engine.c`, `src/ui.c`, `src/gfx.c`: the Miyoo version's. Keep the engine the same sound as
  there: with the sticks at rest, `build/test/session.wav` matches the Miyoo harness's sample for
  sample. The TrimUI additions are small and marked (the audio clock hooks, the Brick Pro's
  `live.sweep_hand`, 64-bit ARM flush-to-zero, the update prompt).
- `src/sync.c`, `src/aclock.c`: Columbia Street's, the newer HI5S sync. Shared with the sibling
  apps; keep the copies the same (they are byte for byte Columbia Street's).
- `src/main_trimui.c`, `src/midi.c`, `src/update.c`, `src/net.c`, `install.sh`, `tools/`: Columbia
  Street's TrimUI platform layer, renamed for HI5.

When you change a control or a screen, update the README's Controls section (and the Brick Pro
table for the sticks) and the footer hints in `src/ui.c`. When you change the engine, run `make
test` and keep its checks passing (CI greps for them in `.github/workflows/build.yml`).

The app updates itself from the latest GitHub release of this repo (`src/update.c`, HTTPS through
Mbed TLS): each build fetches its own zip (`HI5-brick.zip` or `-brickpro.zip`). Before tagging a
release, set `HI5_VERSION` in `src/engine.h` to match the tag; CI refuses a mismatch. When you
change the updater or the package layout, run `make update-test`.

## Identity

This repo is in the `sydilaxe` lane: it is pushed from `sydilaxe`'s account. The Miyoo original lives under
another identity; keep that repo's owner email, and anyone's real name or other accounts, out of
code, docs, commit messages, issues and PRs here. Don't import its git history.
