# HI5 — FM glitch beat generator for the TrimUI Brick, Brick Hammer and Brick Pro, running
# MustardOS
#
#   make          cross-compile both builds with zig (aarch64, glibc 2.28, against the device's SDL2):
#                   build/brick/hi5      TrimUI Brick and Brick Hammer
#                   build/brickpro/hi5   TrimUI Brick Pro (adds the analog sticks)
#   make package  both, as MustardOS application archives:
#                   build/HI5-mustardos-brick.muxapp, build/HI5-mustardos-brickpro.muxapp
#   make test     build the host harness and render screenshots + audio into build/test
#   make install SD=/path/to/card [DEVICE=brickpro]
#                 copy the app into SD/MUOS/application/HI5
#   make update-test   install a fake release over a test app folder, the way the app updates itself
#   make version  print the version built (HI5_VERSION in src/engine.h, or VERSION=x.y.z);
#                 releases are tagged v<version>, and CI builds them with VERSION from the tag
#   make build/hi5_synctest   host-side Wi-Fi sync interop check (run alongside another unit)
#
# zig: ZIG=/path/to/zig, or `pip install ziglang==0.14.1` (used as `python3 -m ziglang`)

ZIG    ?= $(shell command -v zig 2>/dev/null || echo python3 -m ziglang)
TARGET := aarch64-linux-gnu.2.28
CPU    := cortex_a53
SDL_INC := third_party/SDL2-2.30.9-headers

SRC    := src/engine.c src/ui.c src/gfx.c src/sync.c src/aclock.c
HDR    := src/engine.h src/ui.h src/gfx.h src/sync.h src/aclock.h src/font_data.h src/update.h
NET    := src/net.c src/update.c # the self-updater, on the device and in build/hi5_update
PLAT   := src/midi.c src/link.c src/perflog.c src/ringleds.c src/main_trimui.c
PLATH  := src/midi.h src/link.h src/net.h src/perflog.h src/ringleds.h
CFLAGS := -O2 -std=gnu11 -Wall -Wextra -fno-math-errno
APP    := dist/HI5
# the app's version: HI5_VERSION in src/engine.h, or VERSION=x.y.z (CI passes the release tag's)
VERSION ?= $(shell sed -n 's/^\#define HI5_VERSION "\([^"]*\)".*/\1/p' src/engine.h)
VERDEF := -DHI5_VERSION='"$(VERSION)"'
MAGICK := $(shell command -v magick 2>/dev/null || echo convert)

all: build/brick/hi5 build/brickpro/hi5 $(APP)/VERSION $(APP)/LICENSE-mbedtls.txt

# changes only when the version does, so a new VERSION= rebuilds what has it compiled in
build/version: FORCE
	@mkdir -p build
	@test "$$(cat $@ 2>/dev/null)" = "$(VERSION)" || echo $(VERSION) > $@
FORCE:

# read by install.sh to tell whether the device is up to date
$(APP)/VERSION: build/version
	cp $< $@

src/font_data.h: tools/mkfont.py
	python3 tools/mkfont.py /usr/share/fonts/google-noto/NotoSansMono-Bold.ttf $@

# a stand-in for the device's libSDL2-2.0.so.0, to link against (the device has the real one)
build/sdl2/libSDL2-2.0.so.0: tools/mksdlstub.py src/main_trimui.c
	@mkdir -p build/sdl2
	python3 tools/mksdlstub.py build/sdl2/sdl2_stub.c src/main_trimui.c
	$(ZIG) cc -target $(TARGET) -shared -Wl,-soname,libSDL2-2.0.so.0 -o $@ build/sdl2/sdl2_stub.c

# Mbed TLS (Apache-2.0) for the updater's HTTPS, from a pinned release, not committed; each
# toolchain builds its own libmbedtls.a from it with tools/mbedtls.mk
MBED_VER := 3.6.7
MBED_SHA := a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6
MBED_DIR := third_party/mbedtls
MBED_URL := https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$(MBED_VER)/mbedtls-$(MBED_VER).tar.bz2
MBED_SRC := $(MBED_DIR)/.v$(MBED_VER)
$(MBED_SRC):
	rm -rf $(MBED_DIR) && mkdir -p $(MBED_DIR)
	curl -sSL -o $(MBED_DIR).tar.bz2 $(MBED_URL) || wget -qO $(MBED_DIR).tar.bz2 $(MBED_URL)
	echo "$(MBED_SHA)  $(MBED_DIR).tar.bz2" | sha256sum -c --quiet || (rm -f $(MBED_DIR).tar.bz2; false)
	tar -xjf $(MBED_DIR).tar.bz2 -C $(MBED_DIR) --strip-components=1
	rm $(MBED_DIR).tar.bz2
	touch $@

# the binary carries Mbed TLS, so the app folder carries its licence
$(APP)/LICENSE-mbedtls.txt: $(MBED_SRC)
	cp $(MBED_DIR)/LICENSE $@

build/zig/libmbedtls.a: $(MBED_SRC)
	$(MAKE) -f tools/mbedtls.mk OUT=build/zig MBED_DIR=$(MBED_DIR) \
		CC="$(ZIG) cc -target $(TARGET) -mcpu=$(CPU)" AR="$(ZIG) ar" MBED_CFLAGS=-O2

build/host/libmbedtls.a: $(MBED_SRC)
	$(MAKE) -f tools/mbedtls.mk OUT=build/host MBED_DIR=$(MBED_DIR) CC=cc AR=ar MBED_CFLAGS=-O2

DEVICE_CC = $(ZIG) cc -target $(TARGET) -mcpu=$(CPU) $(CFLAGS) $(VERDEF) -I$(SDL_INC) -I$(MBED_DIR)/include -Isrc
DEVICE_LINK = build/zig/libmbedtls.a build/sdl2/libSDL2-2.0.so.0 -lm -lpthread -Wl,--allow-shlib-undefined -s

build/brick/hi5: $(SRC) $(NET) $(PLAT) $(HDR) $(PLATH) build/version build/sdl2/libSDL2-2.0.so.0 build/zig/libmbedtls.a
	@mkdir -p build/brick
	$(DEVICE_CC) -o $@ $(SRC) $(NET) $(PLAT) $(DEVICE_LINK)

build/brickpro/hi5: $(SRC) $(NET) $(PLAT) $(HDR) $(PLATH) build/version build/sdl2/libSDL2-2.0.so.0 build/zig/libmbedtls.a
	@mkdir -p build/brickpro
	$(DEVICE_CC) -DHI5_BRICKPRO -o $@ $(SRC) $(NET) $(PLAT) $(DEVICE_LINK)

build/hi5_test: $(SRC) src/midi.c src/link.c src/perflog.c src/ringleds.c src/main_test.c $(HDR) src/midi.h src/link.h src/perflog.h src/ringleds.h build/version
	@mkdir -p build
	cc $(CFLAGS) $(VERDEF) -o $@ $(SRC) src/midi.c src/link.c src/perflog.c src/ringleds.c src/main_test.c -lm -lpthread

test: build/hi5_test
	@mkdir -p build/test
	build/hi5_test build/test $(APP)/samples | tee build/test/results.txt
	cd build/test && for f in *.ppm; do $(MAGICK) $$f $${f%.ppm}.png && rm $$f; done

# host-side Wi-Fi sync interop check: run two (or one beside a HI5, Columbia Street or Jaffedrone)
build/hi5_synctest: $(SRC) src/midi.c src/link.c src/main_synctest.c $(HDR) src/midi.h src/link.h build/version
	@mkdir -p build
	cc $(CFLAGS) $(VERDEF) -o $@ $(SRC) src/midi.c src/link.c src/main_synctest.c -lm -lpthread

build/hi5_update: $(NET) src/main_update.c src/engine.h src/net.h src/update.h build/version build/host/libmbedtls.a
	cc $(CFLAGS) $(VERDEF) -I$(MBED_DIR)/include -o $@ $(NET) src/main_update.c build/host/libmbedtls.a -lpthread

update-test: build/hi5_update
	sh tools/update_test.sh

version:
	@echo $(VERSION)

# MustardOS: a .muxapp is a zip that its Archive Manager unpacks into the application folder
# (MUOS/application on the card), and the folder's name is the name in the Applications list.
# The app folder and binary, plus mux_launch.sh (how MustardOS starts an app), wake_screen.sh and the list glyph.
MUX := dist/mustardos
MUX_APP := HI5
build/HI5-mustardos-%.muxapp: build/%/hi5 $(APP)/VERSION $(APP)/LICENSE-mbedtls.txt $(MUX)/mux_launch.sh $(MUX)/wake_screen.sh $(MUX)/glyph/hi5.svg
	rm -rf build/mux-$* $@
	mkdir -p "build/mux-$*/$(MUX_APP)"
	cp -r $(APP)/. "build/mux-$*/$(MUX_APP)/"
	cp -r $(MUX)/. "build/mux-$*/$(MUX_APP)/"
	rm -f "build/mux-$*/$(MUX_APP)/"*.sav* "build/mux-$*/$(MUX_APP)/"log*.txt
	cp build/$*/hi5 "build/mux-$*/$(MUX_APP)/"
	echo $* > "build/mux-$*/$(MUX_APP)/DEVICE"
	chmod +x "build/mux-$*/$(MUX_APP)/launch.sh" "build/mux-$*/$(MUX_APP)/mux_launch.sh" \
		"build/mux-$*/$(MUX_APP)/wake_screen.sh" "build/mux-$*/$(MUX_APP)/hi5"
	cd build/mux-$* && zip -qr ../HI5-mustardos-$*.muxapp "$(MUX_APP)"

package: build/HI5-mustardos-brick.muxapp build/HI5-mustardos-brickpro.muxapp

DEVICE ?= brick
INSTALL_DIR = $(SD)/MUOS/application/$(MUX_APP)
INSTALL_ROOT = $(SD)/MUOS
install: build/$(DEVICE)/hi5 $(APP)/VERSION $(APP)/LICENSE-mbedtls.txt
	@test -n "$(SD)" || (echo "usage: make install SD=/path/to/mounted/sdcard [DEVICE=brickpro]" && exit 1)
	@test -d "$(INSTALL_ROOT)" || (echo "$(INSTALL_ROOT) not found — is this the MustardOS card?" && exit 1)
	mkdir -p "$(INSTALL_DIR)"
	cp -r $(APP)/. "$(INSTALL_DIR)/"
	cp -r $(MUX)/. "$(INSTALL_DIR)/"
	cp build/$(DEVICE)/hi5 "$(INSTALL_DIR)/"
	echo $(DEVICE) > "$(INSTALL_DIR)/DEVICE"
	sync

clean:
	rm -rf build $(APP)/VERSION $(APP)/LICENSE-mbedtls.txt

.PHONY: FORCE all test update-test version package install clean
