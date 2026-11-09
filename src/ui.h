/* Pages, controls and drawing. Platform code feeds button edges and time. */
#pragma once
#include <stdint.h>
#include <stdio.h>

enum {
    BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
    BTN_A, BTN_B, BTN_X, BTN_Y,
    BTN_L1, BTN_R1, BTN_L2, BTN_R2,
    BTN_SELECT, BTN_START, BTN_MENU,
    BTN_COUNT
};

enum { PAGE_PATTERN, PAGE_MIX, PAGE_SYNTH, PAGE_FX, PAGE_SONG, PAGE_COUNT };

/* Set by the platform to serialise UI edits against the audio callback. */
extern void (*ui_lock)(void);
extern void (*ui_unlock)(void);

/* The self-update prompt (src/update.h): the platform passes the updater's status
 * and a function that starts the install. Without it the prompt never shows. */
struct UpdateStatus;
void ui_set_update(const struct UpdateStatus *status, void (*install)(void));

void ui_init(void);
void ui_button(int btn, int down);
void ui_tick(int ms);
void ui_draw(uint32_t *fb);
int ui_quit_requested(void);
int ui_take_changed(void);
void ui_draw_loading(uint32_t *fb, const char *msg);
int ui_page(void);
void ui_notice(const char *msg); /* a message on screen (main thread only): USB devices coming and going */
/* MIDI CC: the important settings, by CC number (any channel); 1 if it was
 * one. Main thread. ui_midi_chart writes the CC table of the MIDI chart. */
int ui_midi_cc(int ch, int cc, int v);
void ui_midi_chart(FILE *f);
/* the Brick Pro's sticks: the FX slot (0-3, as R2 + up / down / left / right)
 * the right stick plays, or -1; and a tap of the tempo (L3) */
void ui_stick_fx(int slot);
void ui_tap_tempo(void);
/* every voice's sound parameters (the synth page, the hat's sample and
 * extras included) set at random; the mix, the patterns and the FX stay.
 * The Brick Pro's HOME key, the Brick's side switch. */
void ui_randomize_sounds(void);
void ui_set_page(int p); /* for the test harness */
