/* Headless host harness: plays a scripted button sequence, writes a WAV of the
 * audio, PPM screenshots of each page, and times the DSP.
 *
 * usage: hi5_test OUTDIR [SAMPLE_DIR] */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "aclock.h"
#include "engine.h"
#include "gfx.h"
#include "link.h"
#include "midi.h"
#include "perflog.h"
#include "ringleds.h"
#include "sync.h"
#include "ui.h"
#include "update.h"

#define LED_PREFIX "HI5"

#define RATE 48000

typedef struct {
    int ms, btn, down;
    const char *shot;
} Ev;

#define PRESS(t, b) {t, b, 1, NULL}, {t + 60, b, 0, NULL}
#define SHOT(t, name) {t, -1, 0, name}

static const Ev script[] = {
    PRESS(0, BTN_START),
    SHOT(2900, "pattern"),
    /* punch-in FX */
    {4000, BTN_R2, 1, NULL},
    {4050, BTN_UP, 1, NULL},
    SHOT(5200, "fx_repeat"),
    {6000, BTN_UP, 0, NULL},
    {6100, BTN_RIGHT, 1, NULL},
    {7000, BTN_RIGHT, 0, NULL},
    {7100, BTN_LEFT, 1, NULL},
    {8500, BTN_LEFT, 0, NULL},
    {8600, BTN_DOWN, 1, NULL},
    {10000, BTN_DOWN, 0, NULL},
    {10100, BTN_R2, 0, NULL},
    /* L2 filter sweep */
    {11000, BTN_L2, 1, NULL},
    SHOT(12300, "filter"),
    {15000, BTN_L2, 0, NULL},
    /* pattern edits: move to PERC/STUT, permute, thin out */
    PRESS(15200, BTN_RIGHT), PRESS(15300, BTN_RIGHT), PRESS(15400, BTN_RIGHT),
    PRESS(15500, BTN_DOWN), PRESS(15600, BTN_Y), PRESS(15700, BTN_B), PRESS(15800, BTN_B),
    SHOT(15900, "pattern_edit"),
    /* rotate the cell twice, then tap tempo at 140 BPM with R2 held */
    PRESS(15910, BTN_X), PRESS(15980, BTN_X),
    {16050, BTN_R2, 1, NULL},
    PRESS(16100, BTN_X), PRESS(16529, BTN_X), PRESS(16957, BTN_X), PRESS(17386, BTN_X),
    {17460, BTN_R2, 0, NULL},
    /* mix page */
    PRESS(17500, BTN_R1), PRESS(17600, BTN_RIGHT), PRESS(17700, BTN_RIGHT), PRESS(17800, BTN_Y),
    PRESS(17900, BTN_RIGHT), PRESS(18000, BTN_X), PRESS(18100, BTN_DOWN), PRESS(18200, BTN_DOWN),
    /* A -> PAN, nudge right twice; A -> DRIVE, up three */
    PRESS(18270, BTN_A), PRESS(18340, BTN_UP), PRESS(18410, BTN_UP),
    PRESS(18480, BTN_A), PRESS(18550, BTN_UP), PRESS(18620, BTN_UP), PRESS(18690, BTN_UP),
    SHOT(18800, "mix"),
    PRESS(18850, BTN_X), PRESS(18900, BTN_LEFT), PRESS(18950, BTN_Y),
    /* synth page: KICK ratio up, hat loop tweaks */
    PRESS(19000, BTN_R1), PRESS(19100, BTN_RIGHT), PRESS(19200, BTN_DOWN), PRESS(19300, BTN_X),
    {19400, BTN_X, 1, NULL}, {20400, BTN_X, 0, NULL},
    SHOT(20500, "synth"),
    PRESS(21000, BTN_START),
    SHOT(21100, "paused"),
    /* FX page: slot DOWN -> GATE, play it, then switch output to mono + clock */
    PRESS(21500, BTN_START), PRESS(21600, BTN_R1), PRESS(21700, BTN_DOWN),
    PRESS(21800, BTN_X), PRESS(21900, BTN_X), PRESS(22000, BTN_X),
    {22300, BTN_R2, 1, NULL}, {22350, BTN_DOWN, 1, NULL}, {22400, BTN_RIGHT, 1, NULL},
    SHOT(23000, "fx_gate"),
    {23500, BTN_RIGHT, 0, NULL}, {23550, BTN_DOWN, 0, NULL}, {23600, BTN_R2, 0, NULL},
    PRESS(24000, BTN_DOWN), PRESS(24100, BTN_DOWN), PRESS(24200, BTN_DOWN), PRESS(24300, BTN_X),
    PRESS(24400, BTN_RIGHT), PRESS(24500, BTN_RIGHT),
    SHOT(25000, "fx_clock"),
    /* song page: save the pattern to A and B, chain A x2 then B, copy B to
     * step 3, start the song from step 1 */
    PRESS(25100, BTN_R1), PRESS(25200, BTN_X), PRESS(25300, BTN_RIGHT), PRESS(25400, BTN_X),
    /* seven rights: B -> H -> kit 1, save the kit, eight more to chain node 1 */
    PRESS(25500, BTN_RIGHT), PRESS(25570, BTN_RIGHT), PRESS(25640, BTN_RIGHT), PRESS(25710, BTN_RIGHT),
    PRESS(25780, BTN_RIGHT), PRESS(25850, BTN_RIGHT), PRESS(25920, BTN_RIGHT), PRESS(25990, BTN_X),
    PRESS(26060, BTN_RIGHT), PRESS(26130, BTN_RIGHT), PRESS(26200, BTN_RIGHT), PRESS(26270, BTN_RIGHT),
    PRESS(26340, BTN_RIGHT), PRESS(26410, BTN_RIGHT), PRESS(26480, BTN_RIGHT), PRESS(26550, BTN_RIGHT),
    PRESS(26620, BTN_UP), PRESS(26690, BTN_A), PRESS(26760, BTN_RIGHT), PRESS(26830, BTN_UP),
    PRESS(26900, BTN_UP), PRESS(26970, BTN_Y), PRESS(27040, BTN_RIGHT), PRESS(27110, BTN_X),
    PRESS(27180, BTN_LEFT), PRESS(27250, BTN_LEFT), {27320, BTN_R2, 1, NULL}, PRESS(27370, BTN_A),
    {27450, BTN_R2, 0, NULL},
    SHOT(28200, "song"),
};

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

static void write_wav(const char *path, const int16_t *pcm, int frames)
{
    FILE *f = fopen(path, "wb");
    fwrite("RIFF", 1, 4, f);
    put32(f, 36 + frames * 4);
    fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16);
    put16(f, 1);
    put16(f, 2);
    put32(f, RATE);
    put32(f, RATE * 4);
    put16(f, 4);
    put16(f, 16);
    fwrite("data", 1, 4, f);
    put32(f, frames * 4);
    fwrite(pcm, 4, frames, f);
    fclose(f);
}

static void write_ppm(const char *path, const uint32_t *fb)
{
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        uint8_t rgb[3] = {fb[i] >> 16, fb[i] >> 8, fb[i]};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static UpdateStatus fake_update; /* the update prompt, as if v9.9.9 were out */
static void fake_install(void)
{
    fake_update.state = UP_INSTALLING;
    fake_update.progress = 42;
}

/* render n frames to a mono float buffer (left channel) */
static void render_mono(float *dst, int n)
{
    static int16_t buf[2048 * 2];
    for (int done = 0; done < n;) {
        int k = n - done < 1024 ? n - done : 1024;
        engine_render(buf, k);
        for (int i = 0; i < k; i++)
            dst[done + i] = buf[2 * i] / 32768.f;
        done += k;
    }
}

/* Wi-Fi sync over a simulated network, as in Columbia Street's harness: a
 * leader 1.7 steps ahead whose packets arrive 3 ms late at best and up to 40
 * ms later at random (one in twenty held 100 ms, as for a power-saving unit),
 * read only every 33 ms, with the audio made in 512-frame buffers (the
 * TrimUI's). echo: the leader echoes our packets (a current unit), or not
 * (an older one). Returns how far the follower is heard from the leader over
 * the last 4 of 20 s, in ms. */
static unsigned sim_rand = 12345;
static double sim_jitter(void)
{
    sim_rand = sim_rand * 1103515245u + 12345u;
    double u = (sim_rand >> 8) / 16777216.0;
    return 3000.0 + (u < 0.05 ? 100000.0 : u * 40000.0);
}

typedef struct {
    uint8_t b[SYNC_PKT_LEN];
    uint64_t at;
    int live;
} SimPkt;

static void sim_sync(int echo, double *mean_ms, double *worst_ms, float *delay)
{
    const double bpm = 120.0, spu = bpm / 15e6; /* 16ths per us */
    const uint64_t off = 987654321ULL;           /* the leader's clock runs elsewhere */
    const int buf = 512;
    song_default();
    engine_init(RATE);
    song.bpm = 97.f;
    song.sync_mode = SYNC_FOLLOW;
    sync_core_init(100);
    aclock_init(RATE, 0);
    static int16_t pcm[2 * 1024];
    SimPkt q[64];
    memset(q, 0, sizeof q);
    uint64_t heard_mine = 0, heard_mine_at = 0;
    double sum = 0, worst = 0;
    int n = 0;
    uint64_t next_cb = 0, next_poll = 0, next_lead = 7000, next_mine = 57000;
    for (uint64_t t = 0; t < 20000000ULL; t += 250) {
        double lead_pos = 1.7 + spu * (double)t, wire_pos = lead_pos + SYNC_WIRE_LEAD_US * spu;
        if (t >= next_cb) { /* the audio device asks for a buffer */
            aclock_request(t, buf);
            engine_render(pcm, buf);
            aclock_made(buf, engine_position(), engine_steps_per_frame());
            next_cb += (uint64_t)(buf * 1e6 / RATE);
        }
        if (t >= next_lead) { /* the leader sends */
            next_lead += 100000;
            for (int i = 0; i < 64; i++)
                if (!q[i].live) {
                    uint8_t *b = q[i].b;
                    uint64_t ts = t + off;
                    float fb = (float)bpm;
                    uint32_t id = 7;
                    memset(b, 0, sizeof q[i].b);
                    memcpy(b, "HI5S", 4);
                    b[4] = 1, b[5] = 1, b[6] = SYNC_LEAD;
                    memcpy(b + 8, &id, 4);
                    memcpy(b + 12, &fb, 4);
                    memcpy(b + 16, &wire_pos, 8);
                    memcpy(b + 24, &ts, 8);
                    if (echo && heard_mine_at && t - heard_mine_at < 1000000) {
                        uint32_t me = 100, hold = (uint32_t)(t - heard_mine_at);
                        b[7] = 1;
                        memcpy(b + 32, &me, 4);
                        memcpy(b + 36, &heard_mine, 8);
                        memcpy(b + 44, &hold, 4);
                    }
                    q[i].at = t + (uint64_t)sim_jitter();
                    q[i].live = 1;
                    break;
                }
        }
        if (t >= next_mine) { /* we send; the leader hears it a trip later */
            next_mine += 100000;
            uint8_t b[SYNC_PKT_LEN];
            sync_make_packet(b, t);
            memcpy(&heard_mine, b + 24, 8);
            heard_mine_at = t + (uint64_t)sim_jitter();
        }
        if (t >= next_poll) { /* the screen's frame reads what has come in */
            next_poll += 33000;
            for (int i = 0; i < 64; i++)
                if (q[i].live && q[i].at <= t) {
                    sync_handle_packet(q[i].b, SYNC_PKT_LEN, q[i].at);
                    q[i].live = 0;
                }
        }
        if (t >= 16000000ULL && t % 10000 == 0) {
            double mine;
            if (aclock_position(t, &mine)) {
                double e = fmod(lead_pos - mine, 16.0);
                if (e > 8)
                    e -= 16;
                if (e <= -8)
                    e += 16;
                e = fabs(e / spu / 1000.0);
                sum += e;
                worst = e > worst ? e : worst;
                n++;
            }
        }
    }
    *mean_ms = n ? sum / n : 1e9;
    *worst_ms = worst;
    *delay = sync_status()->delay_ms;
    engine_set_playing(0);
    engine_set_rate(1.0);
    aclock_init(RATE, 0); /* the rest of the harness runs without an audio clock */
    sync_core_init(1);
    song.sync_mode = SYNC_OFF;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--midi-chart")) { /* the CC table of docs/MIDI.md */
        song_default();
        ui_midi_chart(stdout);
        return 0;
    }
    const char *out = argc > 1 ? argv[1] : ".";
    srand(1);
    song_default();
    engine_init(RATE);
    if (argc > 2)
        samples_load_dir(argv[2]);
    ui_init();

    const int total_ms = 28500, chunk_ms = 10, chunk = RATE * chunk_ms / 1000;
    int16_t *pcm = malloc(sizeof(int16_t) * 2 * RATE * total_ms / 1000);
    static uint32_t fb[SCREEN_W * SCREEN_H];
    size_t ev = 0;
    char path[512];
    for (int t = 0, f = 0; t < total_ms; t += chunk_ms, f += chunk) {
        for (; ev < sizeof script / sizeof *script && script[ev].ms <= t; ev++) {
            if (script[ev].shot) {
                ui_draw(fb);
                snprintf(path, sizeof path, "%s/%s.ppm", out, script[ev].shot);
                write_ppm(path, fb);
            }
            else
                ui_button(script[ev].btn, script[ev].down);
        }
        engine_render(pcm + 2 * f, chunk);
        ui_tick(chunk_ms);
    }
    snprintf(path, sizeof path, "%s/session.wav", out);
    write_wav(path, pcm, RATE * total_ms / 1000);
    printf("bpm after tap: %.1f, samples: %d, PERC/STUT rot after 2x X: %d\n", song.bpm, nsamples,
           song.cell[V_PERC][ROW_STUT].rot);
    printf("mix: PERC vol=%d pan=%d drive=%d\n", song.vol[V_PERC], song.pan[V_PERC], song.drive[V_PERC]);
    printf("song page: slots used %d%d, kit1 used %d, chain", song.slot_used[0], song.slot_used[1],
           song.kit_used[0]);
    for (int i = 0; i < chain_len(); i++)
        printf(" %c x%d", 'A' + song.chain_slot[i] - 1, song.chain_bars[i]);
    printf(", mode=%d pos=%d\n", song.song_mode, live.chain_pos);

    /* song chain: A for 1 bar, B (SUB off) for 2 bars, loop; watch the
     * pattern swap at the bar lines */
    engine_set_playing(0);
    song_default();
    song.out_mode = OUT_STEREO;
    slot_save(0);
    song.cell[V_SUB][ROW_HIT].on = 0;
    slot_save(1);
    song.chain_slot[0] = 1, song.chain_bars[0] = 1;
    song.chain_slot[1] = 2, song.chain_bars[1] = 2;
    chain_play_from(0);
    engine_set_playing(1);
    float sps16 = RATE * 15.f / song.bpm * 16.f;
    printf("chain:");
    for (int bar = 0; bar < 7; bar++) {
        engine_render(pcm, (int)sps16);
        printf(" bar%d=%c(sub %s)", bar + 1, 'A' + live.chain_pos, song.cell[V_SUB][ROW_HIT].on ? "on" : "off");
    }
    printf("\n");
    engine_set_playing(0);

    /* wifi sync: an older (32-byte) leader at 128 BPM, 5.3 steps ahead; we
     * start at 120 BPM and must lock tempo, transport and phase. Its packets
     * carry the position being made, which runs SYNC_WIRE_LEAD_US ahead of
     * the one heard: that's where we line up. */
    {
        song_default();
        song.sync_mode = SYNC_AUTO;
        sync_core_init(0x80000000u); /* high id: the fake leader wins */
        uint8_t pkt[SYNC_PKT_LEN] = {0};
        uint64_t t = 0;
        double lpos = 5.3, lbpm = 128.0;
        int chunk_sync = RATE / 100; /* 10 ms */
        double last_err = 99;
        for (int i = 0; i < 400; i++) { /* 4 s */
            if (i % 10 == 0) {
                memcpy(pkt, "HI5S", 4);
                pkt[4] = 1, pkt[5] = 1, pkt[6] = SYNC_AUTO, pkt[7] = 0;
                uint32_t id = 7;
                memcpy(pkt + 8, &id, 4);
                float b = (float)lbpm;
                memcpy(pkt + 12, &b, 4);
                memcpy(pkt + 16, &lpos, 8);
                memcpy(pkt + 24, &t, 8);
                sync_handle_packet(pkt, SYNC_PKT_MIN, t); /* an older unit's packet */
            }
            engine_render(pcm, chunk_sync);
            lpos += lbpm / 60.0 * 4.0 * 0.01;
            t += 10000;
            if (i == 399) {
                double e = fmod(lpos - SYNC_WIRE_LEAD_US * lbpm / 15e6 - engine_position(), 16.0);
                if (e > 8)
                    e -= 16;
                if (e <= -8)
                    e += 16;
                last_err = e;
            }
        }
        const SyncStatus *ss = sync_status();
        printf("sync: bpm=%.1f playing=%d peers=%d leader=%d phase err after 4 s = %.3f steps (%.1f ms)\n", song.bpm,
               live.playing, ss->peers, ss->have_leader, last_err, last_err * 15000.0 / song.bpm);
        engine_set_playing(0);
        engine_set_rate(1.0);
        song.sync_mode = SYNC_OFF;
    }

    /* kits: save, change, load restores synth params and mix balance */
    song_default();
    song.param[V_KICK][P_RATIO] = 5;
    song.hat_x[HX_SHINE] = 40;
    song.pan[V_PERC] = -60;
    kit_save(2);
    song.param[V_KICK][P_RATIO] = 1;
    song.hat_x[HX_SHINE] = 0;
    song.pan[V_PERC] = 0;
    kit_load(2);
    printf("kit: ratio=%d shine=%d perc pan=%d cur_kit=%d\n", song.param[V_KICK][P_RATIO], song.hat_x[HX_SHINE],
           song.pan[V_PERC], live.cur_kit);

    /* DELAY / GRAIN inserts: hold R2+UP through a kick, release, and the
     * tail keeps going (CRUSH for comparison dies at once) */
    {
        static const int types[3] = {FXT_CRUSH, FXT_DELAY, FXT_GRAIN};
        for (int k = 0; k < 3; k++) {
            song_default();
            song.out_mode = OUT_STEREO;
            fx_slot_set_type(&song.fx[0], types[k]);
            song.fx[0].mix = 100;
            engine_set_playing(0);
            for (int i = 0; i < 4; i++)
                engine_render(pcm, RATE);
            live.fx_held[0] = 1;
            engine_audition(V_KICK);
            engine_render(pcm, RATE);
            int pk = 0;
            for (int i = 0; i < RATE; i++)
                if (abs(pcm[2 * i]) > pk)
                    pk = abs(pcm[2 * i]);
            live.fx_held[0] = 0;
            engine_render(pcm, RATE / 10); /* let the dry kick finish */
            engine_render(pcm, RATE);
            double rms = 0;
            for (int i = 0; i < RATE; i++)
                rms += (double)pcm[2 * i] * pcm[2 * i];
            printf("fx %s held: peak %d; tail rms after release %.0f\n", fx_type_name(types[k]), pk,
                   sqrt(rms / RATE));
        }
    }

    /* reverb: full send on the kick, both types, tail after the kick */
    for (int type = 0; type < NVERB; type++) {
        song_default();
        song.out_mode = OUT_STEREO;
        song.verb_type = type;
        song.verb_time = 70;
        song.send[V_KICK] = 100;
        for (int i = 0; i < 4; i++)
            engine_render(pcm, RATE);
        engine_audition(V_KICK);
        engine_render(pcm, RATE / 4);
        int pk = 0;
        for (int i = 0; i < RATE / 4; i++)
            if (abs(pcm[2 * i]) > pk)
                pk = abs(pcm[2 * i]);
        double rms[3];
        for (int w = 0; w < 3; w++) { /* 0.25..0.75, 1.25..1.75, 2.25..2.75 s */
            engine_render(pcm, RATE / 2);
            rms[w] = 0;
            for (int i = 0; i < RATE / 2; i++)
                rms[w] += (double)pcm[2 * i] * pcm[2 * i];
            rms[w] = sqrt(rms[w] / (RATE / 2));
            engine_render(pcm, RATE / 2);
        }
        printf("reverb %s: peak %d, tail rms at 0.5 s %.0f, 1.5 s %.0f, 2.5 s %.0f\n", type ? "SPRING" : "DIGITAL",
               pk, rms[0], rms[1], rms[2]);
    }

    /* hat RESO / SHINE: must stay sane in level */
    for (int k = 0; k < 5; k++) {
        song_default();
        song.out_mode = OUT_STEREO;
        song.hat_x[HX_RESO] = k == 1 ? 50 : k == 2 || k == 3 ? 100 : 0;
        song.hat_x[HX_SHINE] = k >= 3 ? 100 : 0;
        for (int i = 0; i < 4; i++)
            engine_render(pcm, RATE);
        engine_audition(V_HAT);
        engine_render(pcm, RATE / 2);
        int pk = 0;
        double rms = 0;
        for (int i = 0; i < RATE / 2; i++) {
            if (abs(pcm[2 * i]) > pk)
                pk = abs(pcm[2 * i]);
            rms += (double)pcm[2 * i] * pcm[2 * i];
        }
        printf("hat reso=%d shine=%d: peak %d rms %.0f\n", song.hat_x[HX_RESO], song.hat_x[HX_SHINE], pk,
               sqrt(rms / (RATE / 2)));
    }

    /* pan hard left: the right channel goes quiet; drive raises the RMS;
     * in mono + clock mode pan is ignored (audio on L, clock on R) */
    for (int k = 0; k < 4; k++) {
        song_default();
        song.out_mode = k == 3 ? OUT_MONO_CLOCK : OUT_STEREO;
        song.pan[V_KICK] = k == 0 || k == 3 ? -100 : 0;
        song.drive[V_KICK] = k == 2 ? 100 : 0;
        engine_set_playing(0);
        for (int i = 0; i < 4; i++) /* let every earlier voice tail die */
            engine_render(pcm, RATE);
        engine_audition(V_KICK);
        engine_render(pcm, RATE / 2);
        int pl = 0, pr = 0;
        double rms = 0;
        for (int i = 0; i < RATE / 2; i++) {
            if (abs(pcm[2 * i]) > pl)
                pl = abs(pcm[2 * i]);
            if (abs(pcm[2 * i + 1]) > pr)
                pr = abs(pcm[2 * i + 1]);
            rms += (double)pcm[2 * i] * pcm[2 * i];
        }
        printf("kick pan=%d drive=%d%s: peak L %d R %d, rms L %.0f\n", song.pan[V_KICK], song.drive[V_KICK],
               song.out_mode == OUT_MONO_CLOCK ? " mono+clk" : "", pl, pr, sqrt(rms / (RATE / 2)));
    }

    /* SUB at max decay: time for an audition to fall 40 dB below its peak */
    song_default();
    song.out_mode = OUT_STEREO;
    song.param[V_SUB][P_DECAY] = 100;
    engine_set_playing(0);
    engine_render(pcm, RATE); /* let earlier voices die */
    engine_audition(V_SUB);
    engine_render(pcm, RATE * 8);
    float pk = 0.f;
    int last = 0;
    for (int i = 0; i < RATE * 8; i++)
        if (abs(pcm[2 * i]) > pk)
            pk = (float)abs(pcm[2 * i]);
    for (int i = 0; i < RATE * 8; i++)
        if (abs(pcm[2 * i]) > pk * 0.01f)
            last = i;
    printf("sub: decay 100 -> -40 dB after %.2f s\n", (float)last / RATE);
    snprintf(path, sizeof path, "%s/sub808.wav", out);
    write_wav(path, pcm, RATE * 8);

    /* mono + clock output: pulse spacing and width on the clock channel */
    int ppqn_idx[3] = {1, 5, 6};
    for (int k = 0; k < 3; k++) {
        song_default();
        song.bpm = 120.f;
        song.out_mode = OUT_MONO_CLOCK;
        song.clk_ppqn = ppqn_idx[k];
        engine_set_playing(0);
        engine_render(pcm, 4800);
        engine_set_playing(1);
        engine_render(pcm, RATE * 4);
        int edges = 0, first = -1, prev = 0, width = 0, inside = 0;
        double sum_gap = 0;
        float lmax = 0.f;
        for (int i = 0; i < RATE * 4; i++) {
            int hi = pcm[2 * i + 1] > 16000;
            if (hi && !prev) {
                if (first >= 0)
                    sum_gap += i - inside;
                if (first < 0)
                    first = i;
                inside = i;
                edges++;
            }
            if (hi && edges == 1)
                width++;
            prev = hi;
            if (abs(pcm[2 * i]) > lmax)
                lmax = (float)abs(pcm[2 * i]);
        }
        printf("clock: %d PPQN at 120 BPM -> %d pulses in 4 s, mean gap %.2f ms, width %.2f ms, "
               "audio peak on L %.2f\n",
               clk_ppqn_value(ppqn_idx[k]), edges, sum_gap / (edges - 1) * 1000.0 / RATE,
               width * 1000.0 / RATE, lmax / 32768.f);
    }
    engine_set_playing(0);

    /* a v0.0.1 save (shorter Song, old magic) still loads */
    song_default();
    song.bpm = 99.f;
    song.magic = 0x48493501u;
    snprintf(path, sizeof path, "%s/v1.sav", out);
    FILE *sf = fopen(path, "wb");
    fwrite(&song, SONG_V1_SIZE, 1, sf);
    fclose(sf);
    song_default();
    int ok = song_load(path);
    printf("v0.0.1 save: loaded=%d bpm=%.0f fx0=%s out_mode=%d\n", ok, song.bpm,
           fx_type_name(song.fx[0].type), song.out_mode);

    /* a v0.0.2 save at 24 PPQN (then index 3) still means 24 PPQN, and
     * saves without pan / drive get the default pans */
    song_default();
    song.magic = 0x48493502u;
    song.clk_ppqn = 3;
    song.pan[V_HAT] = 99;
    snprintf(path, sizeof path, "%s/v2.sav", out);
    sf = fopen(path, "wb");
    fwrite(&song, SONG_V3_SIZE, 1, sf);
    fclose(sf);
    song_default();
    song.pan[V_HAT] = 0;
    ok = song_load(path);
    printf("v0.0.2 save: loaded=%d ppqn=%d hat pan=%d\n", ok, clk_ppqn_value(song.clk_ppqn), song.pan[V_HAT]);

    /* a v0.0.5 save (no slots / chain) loads with an empty chain */
    song_default();
    song.magic = 0x48493504u;
    song.chain_slot[0] = 3;
    snprintf(path, sizeof path, "%s/v5.sav", out);
    sf = fopen(path, "wb");
    fwrite(&song, SONG_V4_SIZE, 1, sf);
    fclose(sf);
    ok = song_load(path);
    printf("v0.0.5 save: loaded=%d chain len=%d bars0=%d\n", ok, chain_len(), song.chain_bars[0]);

    /* a v0.0.7 save (no kits) loads with empty kits */
    song_default();
    song.magic = 0x48493506u;
    song.sync_mode = SYNC_LEAD;
    snprintf(path, sizeof path, "%s/v7.sav", out);
    sf = fopen(path, "wb");
    fwrite(&song, SONG_V6_SIZE, 1, sf);
    fclose(sf);
    ok = song_load(path);
    int kits = 0;
    for (int i = 0; i < NKIT; i++)
        kits += song.kit_used[i];
    printf("v0.0.7 save: loaded=%d sync=%d kits used=%d\n", ok, song.sync_mode, kits);

    /* ---- TrimUI: the newer sync (Columbia Street's): a leader's packet sets
     * our tempo, and over a slow, uneven network the follower is heard within
     * a millisecond of the leader */
    {
        song_default();
        uint8_t spkt[SYNC_PKT_LEN];
        song.bpm = 140.f;
        song.sync_mode = SYNC_LEAD;
        sync_core_init(5);
        sync_make_packet(spkt, 1000);
        song_default();
        song.sync_mode = SYNC_AUTO;
        sync_core_init(100);
        sync_handle_packet(spkt, sizeof spkt, 2000);
        printf("sync: new packet %d bytes, bpm=%.1f following=%d\n", (int)sizeof spkt, song.bpm,
               sync_status()->have_leader);
        engine_set_rate(1.0);
        double mean, worst;
        float delay;
        sim_sync(1, &mean, &worst, &delay);
        printf("sync timing: packets 3-43 ms late, read every 33 ms -> off by %.2f ms on average, %.2f at worst, "
               "trip measured %.1f ms (want under 1 and 2: %d)\n",
               mean, worst, delay, mean < 1.0 && worst < 2.0);
        sim_sync(0, &mean, &worst, &delay);
        printf("sync timing, a leader without echo: off by %.2f ms on average (want under 6: %d)\n", mean, mean < 6.0);
    }

    /* ---- the update prompt: A starts the install; B closes it without
     * reaching the page underneath (B on PATTERN takes a hit away) */
    {
        song_default();
        engine_set_playing(0);
        ui_set_page(PAGE_PATTERN);
        ui_tick(10);
        ui_draw(fb);
        int before_px = (fb[120 * SCREEN_W + SCREEN_W / 2] & 0xffffff) == 0x8c64ff;
        snprintf(fake_update.latest, sizeof fake_update.latest, "9.9.9");
        fake_update.state = UP_AVAILABLE;
        ui_set_update(&fake_update, fake_install);
        ui_tick(10);
        ui_draw(fb);
        int shown = !before_px && (fb[120 * SCREEN_W + SCREEN_W / 2] & 0xffffff) == 0x8c64ff; /* the panel's border */
        snprintf(path, sizeof path, "%s/update.ppm", out);
        write_ppm(path, fb);
        ui_button(BTN_A, 1);
        ui_button(BTN_A, 0);
        ui_draw(fb);
        snprintf(path, sizeof path, "%s/update_install.ppm", out);
        write_ppm(path, fb);
        int started = fake_update.state == UP_INSTALLING;
        fake_update.state = UP_AVAILABLE;
        Song before = song;
        ui_button(BTN_B, 1);
        ui_button(BTN_B, 0);
        ui_tick(10);
        ui_draw(fb);
        int closed = (fb[120 * SCREEN_W + SCREEN_W / 2] & 0xffffff) != 0x8c64ff;
        int same = !memcmp(&before, &song, sizeof before);
        printf("update prompt: shown %d, A installs %d, B closes it %d, pattern untouched %d\n", shown, started, closed,
               same);
        ui_set_update(NULL, NULL);
    }

    /* ---- USB MIDI (midi.c), bytes in as from a port: START plays from the
     * first step, the tempo and the phase follow the clock, STOP stops */
    {
        song_default();
        engine_init(RATE);
        engine_set_playing(0);
        midi_reset();
        midi_set_latency(0.f);
        const uint8_t start = 0xfa, stop = 0xfc, tick = 0xf8;
        uint64_t t = 1000000;
        uint32_t trig0 = 0;
        for (int v = 0; v < NV; v++)
            trig0 += live.trig_count[v];
        midi_feed(&start, 1, t);
        int started = live.playing;
        engine_render(pcm, 64);
        uint32_t trig1 = 0;
        for (int v = 0; v < NV; v++)
            trig1 += live.trig_count[v];
        int first = live.step == 0 && trig1 > trig0;
        double worst = 0.0;
        for (int i = 1; i <= 24 * 16; i++) { /* 16 beats at 120 BPM: a tick each 20.833 ms */
            t += 20833;
            midi_feed(&tick, 1, t);
            engine_render(pcm, 1000); /* the time between ticks */
            if (i > 24 * 12) {
                double err = fabs(engine_position() - i / 6.0);
                worst = err > worst ? err : worst;
            }
        }
        float bpm_in = song.bpm;
        int following = midi_following();
        midi_feed(&stop, 1, t);
        printf("midi clock in: START plays %d from the first step %d, 120 BPM -> %.1f (%d), "
               "phase within %.3f steps (want under 0.1: %d), STOP stops %d\n",
               started, first, bpm_in, fabsf(bpm_in - 120.f) < 0.5f && following, worst, worst < 0.1, !live.playing);

        { /* MIDI CC: the hat chosen, its first param, its HIT cell, the tempo, a mute */
            song_default();
            midi_reset();
            uint8_t m[3] = {0xb9, 2, 127}; /* channel 10: any channel is heard */
            int mch, mcc, mv, n = 0;
            midi_feed(m, 3, 1000);
            m[1] = 14, m[2] = 0;
            midi_feed(m, 3, 2000);
            m[1] = 103, m[2] = 127; /* HIT steps: 16 */
            midi_feed(m, 3, 3000);
            m[1] = 85, m[2] = 127;  /* BPM: 250 */
            midi_feed(m, 3, 4000);
            m[1] = 29, m[2] = 127;  /* muted */
            midi_feed(m, 3, 5000);
            m[1] = 7, m[2] = 100;   /* not one of ours */
            midi_feed(m, 3, 6000);
            while (midi_take_cc(&mch, &mcc, &mv))
                n += ui_midi_cc(mch, mcc, mv);
            int ok = n == 5 && song.param[V_HAT][0] == param_def(V_HAT, 0)->min &&
                     song.cell[V_HAT][ROW_HIT].steps == MAX_STEPS && song.bpm == 250.f && song.mute[V_HAT] &&
                     !song.mute[V_SUB];
            printf("midi cc: hat chosen, its first param at the bottom, HIT 16 steps, BPM 250, muted, CC7 not ours: %d\n", ok);
            song_default();
            midi_reset();
        }

        song_default();
        song.bpm = 120.f;
        engine_init(RATE);
        engine_set_playing(1);
        midi_reset();
        midi_set_latency(0.f);
        uint8_t mout[64];
        int ticks_out = 0, starts = 0, stops = 0;
        t = 5000000;
        for (int i = 0; i < 400; i++) { /* 4 s in 10 ms steps */
            engine_render(pcm, RATE / 100);
            t += 10000;
            int n = midi_clock_step(t, mout, sizeof mout);
            for (int k = 0; k < n; k++)
                ticks_out += mout[k] == 0xf8, starts += mout[k] == 0xfa;
        }
        engine_set_playing(0);
        int n = midi_clock_step(t + 10000, mout, sizeof mout);
        for (int k = 0; k < n; k++)
            stops += mout[k] == 0xfc;
        printf("midi clock out: START %d, %d ticks in 4 s at 120 BPM (want 192: %d), STOP %d\n", starts, ticks_out,
               abs(ticks_out - 192) <= 1, stops);
        midi_reset();
    }
    /* ---- the SONG page's MIDI setting: AUTO follows a clock coming in and
     * sends one, SEND only sends (and ignores one coming in), FOLLOW only
     * follows, OFF neither; the setting cycles on the page with X */
    {
        static int16_t pcm[2048 * 2];
        const uint8_t start = 0xfa, stop = 0xfc, tick = 0xf8;
        int got[NMIDIMODE][2], ok = 1;
        static const int want[NMIDIMODE][2] = {{1, 1}, {0, 1}, {1, 0}, {0, 0}}; /* follows, sends */
        for (int m = 0; m < NMIDIMODE; m++) {
            song_default();
            song.midi_mode = m;
            engine_init(RATE);
            engine_set_playing(0);
            midi_reset();
            midi_set_latency(0.f);
            uint64_t t = 1000000;
            midi_feed(&start, 1, t);
            for (int i = 0; i < 48; i++) {
                t += 20833;
                midi_feed(&tick, 1, t);
                engine_render(pcm, 1000);
            }
            got[m][0] = midi_following() && live.playing;
            midi_feed(&stop, 1, t);
            engine_set_playing(0);
            midi_reset();
            engine_set_playing(1);
            uint8_t out[64];
            int ticks = 0;
            for (int i = 0; i < 100; i++) {
                engine_render(pcm, RATE / 100);
                t += 10000;
                int n = midi_clock_step(t, out, sizeof out);
                for (int k = 0; k < n; k++)
                    ticks += out[k] == 0xf8;
            }
            engine_set_playing(0);
            midi_clock_step(t + 10000, out, sizeof out);
            got[m][1] = ticks > 0;
            ok &= got[m][0] == want[m][0] && got[m][1] == want[m][1];
        }
        midi_reset();
        /* on the page: R2+B, LEFT, R2+B lands on the chain's first step from
         * anywhere; past the chain and WIFI SYNC is MIDI, where A steps it on */
        song_default();
        ui_set_page(PAGE_SONG);
        ui_button(BTN_R2, 1), ui_button(BTN_B, 1), ui_button(BTN_B, 0), ui_button(BTN_R2, 0);
        ui_button(BTN_LEFT, 1), ui_button(BTN_LEFT, 0);
        ui_button(BTN_R2, 1), ui_button(BTN_B, 1), ui_button(BTN_B, 0), ui_button(BTN_R2, 0);
        for (int k = 0; k < NCHAIN + 1; k++)
            ui_button(BTN_RIGHT, 1), ui_button(BTN_RIGHT, 0);
        ui_button(BTN_A, 1), ui_button(BTN_A, 0);
        int page = song.midi_mode == MIDI_SEND;
        ui_button(BTN_RIGHT, 1), ui_button(BTN_RIGHT, 0);
        ui_button(BTN_A, 1), ui_button(BTN_A, 0);
        page &= song.midi_clock_only == 1;
        ui_draw(fb);
        char mp[600];
        snprintf(mp, sizeof mp, "%s/song_midi.ppm", out);
        write_ppm(mp, fb);
        song.midi_mode = MIDI_AUTO;
        song.midi_clock_only = 0;
        ui_set_page(PAGE_PATTERN);
        printf("midi modes (follows, sends): AUTO %d %d, SEND %d %d, FOLLOW %d %d, OFF %d %d; MIDI and START set on the SONG page %d "
               "(want 1: %d)\n",
               got[0][0], got[0][1], got[1][0], got[1][1], got[2][0], got[2][1], got[3][0], got[3][1], page,
               ok && page);
    }
    /* ---- the Brick clock link and MIDI clock-only (midi.c, link.c) */
    {
        static int16_t pcm[2048 * 2];
        const uint8_t start = 0xfa, stop = 0xfc, tick = 0xf8;
        /* clock only: START and STOP pass by, the tempo still follows */
        song_default();
        song.midi_clock_only = 1;
        engine_init(RATE);
        engine_set_playing(0);
        midi_reset();
        midi_set_latency(0.f);
        uint64_t t = 1000000;
        midi_feed(&start, 1, t);
        int start_ignored = !live.playing;
        for (int i = 0; i < 48; i++) {
            t += 20833;
            midi_feed(&tick, 1, t);
            engine_render(pcm, 1000);
        }
        int tempo = midi_following() && fabsf(song.bpm - 120.f) < 0.2f;
        engine_set_playing(1); /* started by hand */
        midi_feed(&stop, 1, t);
        int stop_ignored = live.playing;
        engine_set_playing(0);

        /* the link: a leader at 137.37 BPM sends where its sound is every 20 ms;
         * this one, stopped, joins it mid-song and holds it */
        static const double tempos[] = {137.37, 200.0, 224.5, 250.0};
        double worst_ms_at[4], worst_ms = 0.0, bpm = 0.0, lat = 512.0 / RATE, lead0 = 37.25;
        int joined = 1, exact_tempo = 1;
        uint8_t msg[16];
        for (int k = 0; k < 4; k++) {
            bpm = tempos[k];
            song_default();
            song.midi_clock_only = 0;
            engine_init(RATE);
            engine_set_playing(0);
            midi_reset();
            midi_set_latency((float)lat);
            t = 5000000;
            double worst = 0.0;
            for (int i = 0; i < 400; i++) { /* 8 s */
                double lead = lead0 + (double)(t - 5000000) * 1e-6 * bpm / 15.0;
                /* after a few seconds, how far this one's sound is from the leader's, now */
                if (i > 150) {
                    double heard = engine_position() - lat * song.bpm / 15.0;
                    double e = fabs(lead - heard);
                    worst = e > worst ? e : worst;
                }
                int n = midi_link_pos(msg, sizeof msg, lead, bpm, 1);
                /* it arrives 0.15 to 0.45 ms after it was sent (USB through the hub and
                 * the thread waking), at random, while this one plays on */
                int late = (int)(0.00015 * RATE) + rand() % (int)(0.0003 * RATE + 1);
                engine_render(pcm, late);
                midi_feed(msg, n, t + (uint64_t)late * 1000000 / RATE);
                if (i == 0)
                    joined &= live.playing;
                engine_render(pcm, RATE / 50 - late);
                t += 20000;
            }
            worst_ms_at[k] = worst * 15000.0 / bpm;
            worst_ms = fmax(worst_ms, worst_ms_at[k]);
            exact_tempo &= fabs(song.bpm - bpm) < 0.001;
        }
        /* the leader stops: so does this one */
        int n = midi_link_pos(msg, sizeof msg, 0.0, bpm, 0);
        midi_feed(msg, n, t);
        int stops = !live.playing;

        /* clock only over the link: started by hand somewhere else, it lines up the bar */
        song.midi_clock_only = 1;
        midi_reset();
        engine_init(RATE);
        engine_set_playing(0);
        engine_set_playing(1);
        engine_set_position(5.3);
        double bar_err = 99.0;
        for (int i = 0; i < 150; i++) {
            double lead = lead0 + (double)(t - 5000000) * 1e-6 * bpm / 15.0;
            n = midi_link_pos(msg, sizeof msg, lead, bpm, 1);
            midi_feed(msg, n, t);
            engine_render(pcm, RATE / 50);
            t += 20000;
            if (i == 149) {
                double heard = engine_position() - lat * song.bpm / 15.0;
                double d = lead + (20000 * 1e-6 + 0.0003) * bpm / 15.0 - heard;
                bar_err = fabs(d - 16.0 * floor((d + 8.0) / 16.0));
            }
        }
        n = midi_link_pos(msg, sizeof msg, 0.0, bpm, 0);
        midi_feed(msg, n, t);
        int no_stop = live.playing; /* the clock only: the leader stopping doesn't stop it */
        engine_set_playing(0);
        song.midi_clock_only = 0;

        /* a Brick following another clock passes it on to the ones linked to it */
        midi_reset();
        midi_set_latency(0.f);
        engine_init(RATE);
        t = 9000000;
        midi_feed(&start, 1, t);
        for (int i = 0; i < 48; i++) {
            t += 20833;
            midi_feed(&tick, 1, t);
            engine_render(pcm, 1000);
        }
        n = midi_link_step(t + 1, msg, sizeof msg);
        int relays = midi_following() && n == 14 && msg[0] == 0xf0 && msg[1] == 0x7d && msg[13] == 0xf7;
        midi_feed(&stop, 1, t);
        midi_reset();

        /* the follower's USB descriptors, and the leader finding a linked Brick
         * in a stand-in sysfs: not the test memory stick with the same ids */
        uint8_t d[256];
        int dn = link_descriptors(d, sizeof d), sn = link_strings(d + 128, 128);
        /* the head (12 bytes) and the full- and high-speed counts (8), then the
         * interface (9) and its bulk OUT endpoint (7), for each speed */
        int desc = dn == 52 && d[0] == 3 && d[20 + 5] == 0xff && d[20 + 6] == LINK_SUBCLASS &&
                   d[20 + 7] == LINK_PROTOCOL && d[29 + 2] == 0x01 && d[29 + 3] == 2 && d[45 + 4] == 0 &&
                   d[45 + 5] == 2 && sn > 16;
        char fs[600], fp[800];
        snprintf(fs, sizeof fs, "%s/fakeusb", out);
        mkdir(fs, 0755);
        const char *const tree[][2] = {
            {"1-1.3/idVendor", "1d6b"},           {"1-1.3/idProduct", "0104"},
            {"1-1.3/busnum", "1"},                {"1-1.3/devnum", "5"},
            {"1-1.3/1-1.3:1.0/bInterfaceClass", "ff"}, {"1-1.3/1-1.3:1.0/bInterfaceSubClass", "42"},
            {"1-1.3/1-1.3:1.0/bInterfaceProtocol", "4c"}, {"1-1.3/1-1.3:1.0/ep_02/direction", "out"},
            {"1-1.3/1-1.3:1.0/ep_02/type", "Bulk"}, {"1-1.3/1-1.3:1.0/ep_02/bEndpointAddress", "02"},
            {"1-1.4/idVendor", "1d6b"},           {"1-1.4/idProduct", "0104"},
            {"1-1.4/busnum", "1"},                {"1-1.4/devnum", "6"},
            {"1-1.4/1-1.4:1.0/bInterfaceClass", "08"},
        };
        for (size_t k = 0; k < sizeof tree / sizeof tree[0]; k++) {
            snprintf(fp, sizeof fp, "%s/%s", fs, tree[k][0]);
            for (char *c = fp + strlen(fs) + 1; *c; c++)
                if (*c == '/') {
                    *c = 0;
                    mkdir(fp, 0755);
                    *c = '/';
                }
            FILE *f = fopen(fp, "w");
            if (f) {
                fprintf(f, "%s\n", tree[k][1]);
                fclose(f);
            }
        }
        LinkDev ld[4];
        int found = link_find(fs, ld, 4);
        int finds = found == 1 && ld[0].bus == 1 && ld[0].dev == 5 && ld[0].ep == 0x02;
        /* plain MIDI clock from outside gear at 250 BPM (a tick every 10 ms): how close the ticks alone hold it */
        song_default();
        engine_init(RATE);
        engine_set_playing(0);
        midi_reset();
        midi_set_latency(0.f);
        t = 20000000;
        midi_feed(&start, 1, t);
        engine_render(pcm, 64);
        /* each tick arrives 0 to 2 ms late at random (USB, the thread waking) while this one plays on;
         * how much its sound wobbles around its average offset (that average is the cable's own delay) */
        static double tk_err[24 * 8];
        int ne = 0;
        for (int i = 1; i <= 24 * 32; i++) {
            int late = rand() % (int)(0.002 * RATE + 1);
            engine_render(pcm, late);
            midi_feed(&tick, 1, t + 10000 + (uint64_t)late * 1000000 / RATE);
            engine_render(pcm, RATE / 100 - late);
            t += 10000;
            if (i > 24 * 24 && ne < 24 * 8)
                tk_err[ne++] = engine_position() - i / 6.0;
        }
        midi_feed(&stop, 1, t);
        midi_reset();
        double mean = 0.0, wob = 0.0;
        for (int k = 0; k < ne; k++)
            mean += tk_err[k] / ne;
        for (int k = 0; k < ne; k++)
            wob = fmax(wob, fabs(tk_err[k] - mean));
        double tick_ms = wob * 15000.0 / 250.0;
        printf("midi clock in at 250 BPM: the tempo %.1f, the sound within %.3f ms of steady, ticks up to 2 ms late "
               "(want under 0.5: %d)\n",
               song.bpm, tick_ms, tick_ms < 0.5);
        printf("clock only: START ignored %d, STOP ignored %d, the tempo followed %d (want 1: %d)\n", start_ignored,
               stop_ignored, tempo, start_ignored && stop_ignored && tempo);
        printf("link sync: joins mid-song %d, the exact tempo %d, the sound within %.3f / %.3f / %.3f / %.3f ms of the "
               "leader's at 137.37 / 200 / 224.5 / 250 BPM (want under 0.25: %d), stops with it %d; clock only: the bar within %.3f steps %d, keeps playing %d; "
               "a follower passes the clock on %d (want 1: %d)\n",
               joined, exact_tempo, worst_ms_at[0], worst_ms_at[1], worst_ms_at[2], worst_ms_at[3], worst_ms < 0.25,
               stops, bar_err, bar_err < 0.01, no_stop, relays,
               joined && exact_tempo && worst_ms < 0.25 && stops && bar_err < 0.01 && no_stop && relays);
        printf("link usb: descriptors %d (%d bytes), strings %d, a linked Brick found %d (bus %d device %d endpoint "
               "0x%02x), the memory stick passed over %d (want 1: %d)\n",
               desc, dn, sn > 16, finds, found ? ld[0].bus : 0, found ? ld[0].dev : 0, found ? ld[0].ep : 0,
               found == 1, desc && finds);
    }

    /* ---- the Brick Pro's sticks: the left closes the L2 filter by hand and
     * lets it go again; the right plays the FX slots, R2's d-pad first */
    {
        song_default();
        engine_init(RATE);
        engine_set_playing(1);
        static float mono[RATE];
        double hf[3];
        for (int k = 0; k < 3; k++) {
            live.sweep_hand = k == 1 ? 1.f : 0.f;
            render_mono(mono, RATE / 2); /* settle */
            render_mono(mono, RATE);
            double e = 0.0;
            for (int i = 1; i < RATE; i++)
                e += (mono[i] - mono[i - 1]) * (double)(mono[i] - mono[i - 1]);
            hf[k] = e;
        }
        live.sweep_hand = 0.f;
        ui_stick_fx(BTN_LEFT); /* slot 3: REVERSE */
        int fx_stick = live.fx_src == BTN_LEFT && live.fx_held[BTN_LEFT];
        ui_button(BTN_R2, 1);
        ui_button(BTN_UP, 1); /* slot 1: REPEAT */
        int fx_r2 = live.fx_src == BTN_UP;
        ui_button(BTN_UP, 0);
        ui_button(BTN_R2, 0);
        int fx_back = live.fx_src == BTN_LEFT;
        ui_stick_fx(BTN_RIGHT); /* slot 4: CRUSH, an insert */
        int fx_insert = live.fx_src == -1 && live.fx_held[BTN_RIGHT] && !live.fx_held[BTN_LEFT];
        ui_stick_fx(-1);
        int fx_off = live.fx_src == -1;
        for (int sl = 0; sl < NFXSLOT; sl++)
            fx_off &= !live.fx_held[sl];
        engine_set_playing(0);
        printf("stick: filter closed by hand %.1f%% of the highs (darker: %d), let go %.0f%% (open again: %d); "
               "fx: stick %d, R2 first %d, back %d, insert %d, off %d\n",
               100.0 * hf[1] / hf[0], hf[1] < 0.2 * hf[0], 100.0 * hf[2] / hf[0], hf[2] > 0.5 * hf[0], fx_stick, fx_r2,
               fx_back, fx_insert, fx_off);
    }

    /* DSP benchmark: 60 s of the default song with every glitch row on */
    song_default();
    for (int v = 0; v < NV; v++)
        for (int r = 0; r < NROW; r++)
            song.cell[v][r].on = 1;
    engine_set_playing(1);
    live.filter_held = 1;
    clock_t c0 = clock();
    for (int i = 0; i < 60 * RATE / 1024; i++)
        engine_render(pcm, 1024);
    double secs = (double)(clock() - c0) / CLOCKS_PER_SEC;
    printf("dsp: 60 s of audio rendered in %.3f s (%.2f%% of one host core)\n", secs, secs / 60 * 100);

    c0 = clock();
    for (int i = 0; i < 300; i++)
        ui_draw(fb);
    secs = (double)(clock() - c0) / CLOCKS_PER_SEC;
    printf("ui: %.2f ms per frame\n", secs / 300 * 1000);

    /* the performance log's dropout count: steady 10 ms callbacks, then one
     * that comes 60 ms late (the device ran dry), then steady again */
    {
        setenv("HI5_LOG", "0", 1); /* the counting, without the lines */
        perf_init("HI5", "brick");
        perf_audio_open(48000, 480, NULL);
        uint64_t t = perf_now_us();
        struct timespec ts;
        int steady = -1, stall = -1;
        for (int i = 0; i < 60; i++) {
            if (i == 30) {
                steady = (int)perf_dropouts();
                t += 60000;
            }
            if (i == 31)
                stall = (int)perf_dropouts();
            for (uint64_t now; (now = perf_now_us()) < t;) {
                uint64_t w = t - now;
                ts.tv_sec = 0, ts.tv_nsec = (long)(w > 2000 ? w - 1000 : w) * 1000;
                nanosleep(&ts, NULL);
            }
            perf_audio_end(perf_audio_begin(480));
            t += 10000;
        }
        int after = (int)perf_dropouts();
        printf("perflog: dropouts steady %d, after a 60 ms stall %d, steady again %d (want 0, 1, 1: %d)\n", steady,
               stall, after, steady == 0 && stall == 1 && after == 1);
    }
    { /* JUMP: a click in a clean tone is found; a tone, a square wave, a crushed tone and HI5's own beat are not */
        static int16_t b[48000 * 2];
        unsigned seg[5];
        for (int k = 0; k < 4; k++) {
            for (int i = 0; i < 48000; i++) {
                double ph = 2 * M_PI * 220.0 * i / 48000.0, x = k == 2 ? (sin(ph) > 0 ? 0.5 : -0.5) : 0.4 * sin(ph);
                if (k == 3)
                    x = floor(x * 8.0) / 8.0;
                if (k == 1 && i == 30000)
                    x += 0.5;
                b[i * 2] = b[i * 2 + 1] = (int16_t)(x * 32767);
            }
            if (k >= 2)
                perf_audio_output(b, 48000); /* a second of it first: the switch to it is a jump of its own */
            unsigned before = perf_jumps();
            perf_audio_output(b, 48000);
            seg[k] = perf_jumps() - before;
        }
        song_default();
        engine_init(RATE);
        engine_set_playing(1);
        for (int i = 0; i < 4; i++) /* settle */
            engine_render(b, 512), perf_audio_output(b, 512);
        unsigned before = perf_jumps();
        static int16_t beat[20 * RATE * 2];
        for (int i = 0; i < 20 * RATE / 512; i++) {
            engine_render(b, 512);
            perf_audio_output(b, 512);
            memcpy(beat + i * 1024, b, 1024 * sizeof b[0]);
        }
        snprintf(path, sizeof path, "%s/jumpbeat.wav", out);
        write_wav(path, beat, 20 * RATE / 512 * 512);
        seg[4] = perf_jumps() - before;
        engine_set_playing(0);
        printf("perflog jumps: tone %u, tone with a click %u, square %u, crushed %u, 20 s of the beat %u "
               "(want 0 1 0 0 0: %d)\n", seg[0], seg[1], seg[2], seg[3], seg[4],
               seg[0] == 0 && seg[1] == 1 && seg[2] == 0 && seg[3] == 0 && seg[4] == 0);
    }
    {
        /* the Brick Pro's LEDs (ringleds.h), against a stand-in for the driver's folder */
        char d[300], p[400], v[256];
        snprintf(d, sizeof d, "%s/led_anim", out);
        mkdir(d, 0755);
        static const char *const files[][2] = {
            {"frame_hex", "\n"}, {"effect_enable", "1\n"}, {"max_scale", "0\n"}, {"effect_lr", "4\n"},
            {"effect_rgb_hex_lr", "0000FF \n"}, {"effect_f1", "1\n"}, {"effect_rgb_hex_f1", "FF0000 \n"}};
        for (unsigned i = 0; i < sizeof files / sizeof files[0]; i++) {
            snprintf(p, sizeof p, "%s/%s", d, files[i][0]);
            FILE *f = fopen(p, "w");
            if (f)
                fputs(files[i][1], f), fclose(f);
        }
        setenv(LED_PREFIX "_LED_DIR", d, 1);
        setenv(LED_PREFIX "_LED_FRAME", "1", 1);
        remove("ringleds_saved.txt");
#define LED_READ(name)                                                                                                 \
    do {                                                                                                               \
        snprintf(p, sizeof p, "%s/%s", d, name);                                                                       \
        FILE *f = fopen(p, "r");                                                                                       \
        v[0] = 0;                                                                                                      \
        if (f) {                                                                                                       \
            if (!fgets(v, sizeof v, f))                                                                                \
                v[0] = 0;                                                                                              \
            fclose(f);                                                                                                 \
        }                                                                                                              \
    } while (0)
        int opened = ringleds_open(LED_PREFIX, 0);
        LED_READ("effect_enable");
        int effects_off = atoi(v) == 0;
        LED_READ("max_scale");
        int bright = atoi(v) == 60;
        /* full scale on the left channel, silence on the right */
        static int16_t s[256 * 2];
        for (int i = 0; i < 256; i++)
            s[i * 2] = i & 1 ? 32000 : -32000, s[i * 2 + 1] = 0;
        ringleds_audio(s, 256);
        /* the left stick up, the right stick right, the bar's first beat */
        ringleds_update(0.f, 1.f, 0x46b4ff, 1.f, 0.f, 0xff2030, 1, 0, 120.f, 0, 0);
        LED_READ("frame_hex");
        unsigned c[23] = {0};
        int n = 0;
        for (char *q = v; n < 23 && *q;) {
            char *e;
            unsigned long x = strtoul(q, &e, 16);
            if (e == q)
                break;
            c[n++] = (unsigned)x, q = e;
        }
        /* the left ring's upper corners (11 and 14) lit, the lower dark; the right ring's right corners (17, 18) */
        int rings = n == 23 && c[10] && c[13] && !c[11] && !c[12] && !c[14] && !c[15] && c[16] && c[17] &&
                    (c[10] & 0xff) > (c[10] >> 16) && (c[16] >> 16) > (c[16] & 0xff);
        /* the bar: the left channel all the way out (LEDs 4 .. 1, red at the end), the right dark */
        int bar = c[0] == 0xff2010 && c[3] && !c[4] && !c[5] && !c[6] && !c[7];
        int beat = c[18] && c[19] && c[20] && c[21] && (c[18] >> 16) > (c[18] & 0xff), others = !c[8] && !c[9] && !c[22];
        ringleds_close();
        LED_READ("frame_hex");
        int dark = !strncmp(v, "000000 000000", 13);
        LED_READ("effect_enable");
        int back = atoi(v) == 1;
        LED_READ("effect_rgb_hex_lr");
        back &= !strncmp(v, "0000FF", 6);
        LED_READ("effect_lr");
        back &= atoi(v) == 4;
        LED_READ("max_scale");
        back &= atoi(v) == 0;
        back &= access("ringleds_saved.txt", F_OK) != 0;
        printf("leds: driven %d, effects off %d, bright %d; rings point with the sticks %d, level bar %d, beat on the "
               "shoulders %d, the rest dark %d; given back (dark frame, effects on): %d\n",
               opened, effects_off, bright, rings, bar, beat, others, dark && back);
        unsetenv(LED_PREFIX "_LED_FRAME");
    }
    free(pcm);
    return 0;
}
