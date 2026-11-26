/* Colour cycle, Blank, Starfield and Lines. */
#include <string.h>
#include "saver_int.h"

/* ---- Colour cycle: a zone plate drawn once, animated only by the palette ----
 *
 * Every pixel's colour is how far it is from the middle, squared, in bands: rings that get
 * closer and closer, until they beat against the pixel grid in moire patterns. Then the
 * picture never changes: each frame turns the palette (Ink's 15 colours round by one, or
 * Paper's ink through the 16), and the rings seem to flow. */

static const uint8_t spectrum[15] = { 1, 9, 3, 11, 2, 10, 14, 6, 4, 12, 5, 13, 15, 7, 8 };
static unsigned cycle_phase;
static unsigned long cycle_next;

void cycle_start(void)
{
    static uint8_t row[SCREEN_W];
    long d, dy;
    int x, y, dx, band, colour = sv_colour();

    for (y = 0; y < SCREEN_H; y++) {
        dy = (long)(y - SCREEN_H / 2) * 12 / 5; /* the pixels are 2.4 times taller than wide */
        dx = -SCREEN_W / 2;
        d = (long)dx * dx + dy * dy;
        for (x = 0; x < SCREEN_W; x++) {
            band = (int)(d >> 9);
            row[x] = colour ? (uint8_t)(1 + band % 15) : (uint8_t)(band & 1);
            d += 2L * dx + 1; /* (dx + 1)^2 = dx^2 + 2dx + 1 */
            dx++;
        }
        gfx_put_row(0, y, SCREEN_W, row);
    }
    cycle_phase = 0;
    cycle_next = 0;
}

void cycle_step(void)
{
    uint8_t map[16];
    int i;

    sv_wait_frame();
    if (sv_ticks() < cycle_next)
        return;
    cycle_next = sv_ticks() + (sv_colour() ? 1 : 4);
    cycle_phase++;
    map[0] = 0;
    for (i = 1; i < 16; i++)
        map[i] = spectrum[(i - 1 + cycle_phase) % 15];
    if (!sv_colour())
        map[1] = spectrum[cycle_phase % 15];
    gfx_palette(map);
}

void cycle_stop(void)
{
    gfx_palette_reset();
}

/* ---- Blank: the picture switched off, which costs nothing at all ---- */

void blank_start(void)
{
    sv_clear(); /* in case a card ignores the switch */
    gfx_blank(1);
}

void blank_step(void)
{
    sv_wait_frame();
}

void blank_stop(void)
{
    gfx_blank(0);
}

/* ---- Starfield: flying forwards through stars ---- */

#define STARS 110
#define STAR_FAR 1024

typedef struct Star {
    int x, y, z;   /* in space: x and y -1024..1023, z away from us */
    int sx, sy, n; /* where it was drawn, and how wide (0: not drawn) */
} Star;

static Star stars[STARS];

static void new_star(Star *s, int anywhere)
{
    s->x = (int)sl_rand_n(2048) - 1024;
    s->y = (int)sl_rand_n(2048) - 1024;
    s->z = anywhere ? 64 + (int)sl_rand_n(STAR_FAR - 64) : STAR_FAR;
    s->n = 0;
}

void stars_start(void)
{
    int i;

    sv_clear();
    for (i = 0; i < STARS; i++)
        new_star(&stars[i], 1);
}

void stars_step(void)
{
    Star *s;
    int i, px, py, n;
    uint8_t c;

    sv_wait_frame();
    for (i = 0; i < STARS; i++) {
        s = &stars[i];
        if (s->n)
            gfx_bits(s->sx, s->sy, s->n == 3 ? 0xE000 : 0xC000, s->n, 0);
        s->z -= 12;
        if (s->z < 16)
            new_star(s, 0);
        /* Perspective: x / z across, and y / z down, squashed for the tall pixels */
        px = SCREEN_W / 2 + (int)((long)s->x * 256 / s->z);
        py = SCREEN_H / 2 + (int)((long)s->y * 107 / s->z);
        if (px < 0 || px >= SCREEN_W - 2 || py < 0 || py >= SCREEN_H) {
            new_star(s, 0);
            continue;
        }
        /* Two pixels wide at least (one is a hairline at 640 x 200), three when close */
        if (s->z < 300) {
            c = 15;
            n = 3;
        } else {
            c = s->z < 650 ? 7 : 8;
            n = 2;
        }
        gfx_bits(px, py, n == 3 ? 0xE000 : 0xC000, n, sv_col(c));
        s->sx = px;
        s->sy = py;
        s->n = n;
    }
}

/* ---- Lines: two four-sided shapes, each corner on its own Lissajous curve ----
 *
 * Each shape leaves a trail of its last few positions. Lines are XORed, so drawing the oldest
 * again takes it away, and where the two shapes cross their colours mix. */

#define TRAIL 12
#define CORNERS 4

static int freq_x[2][CORNERS], freq_y[2][CORNERS], phase_x[2][CORNERS], phase_y[2][CORNERS];
static int trail_x[2][TRAIL][CORNERS], trail_y[2][TRAIL][CORNERS];
static int trail_head, trail_count;
static unsigned lines_t;

static void pick_curves(void)
{
    int p, v;

    for (p = 0; p < 2; p++)
        for (v = 0; v < CORNERS; v++) {
            freq_x[p][v] = 1 + (int)sl_rand_n(5);
            freq_y[p][v] = 1 + (int)sl_rand_n(5);
            phase_x[p][v] = (int)sl_rand_n(256);
            phase_y[p][v] = (int)sl_rand_n(256);
        }
}

static void draw_shape(int p, int slot)
{
    uint8_t c = sv_col(p == 0 ? 11 : 13);
    int v, w;

    for (v = 0; v < CORNERS; v++) {
        w = (v + 1) % CORNERS;
        sv_xor_line(trail_x[p][slot][v], trail_y[p][slot][v], trail_x[p][slot][w], trail_y[p][slot][w], c);
    }
}

void lines_start(void)
{
    sv_clear();
    pick_curves();
    trail_head = trail_count = 0;
    lines_t = 0;
}

void lines_step(void)
{
    int p, v, ax, ay;

    sv_wait_frame();
    lines_t++;
    for (p = 0; p < 2; p++) {
        if (trail_count == TRAIL)
            draw_shape(p, trail_head); /* the oldest goes */
        for (v = 0; v < CORNERS; v++) {
            ax = (int)(((long)freq_x[p][v] * lines_t >> 3) + phase_x[p][v]) & 255;
            ay = (int)(((long)freq_y[p][v] * lines_t >> 3) + phase_y[p][v]) & 255;
            trail_x[p][trail_head][v] = SCREEN_W / 2 + (int)((long)sl_sin[ax] * 300 >> 14);
            trail_y[p][trail_head][v] = SCREEN_H / 2 + (int)((long)sl_sin[ay] * 92 >> 14);
        }
        draw_shape(p, trail_head);
    }
    trail_head = (trail_head + 1) % TRAIL;
    if (trail_count < TRAIL)
        trail_count++;
    if (lines_t % 4000 == 0) /* new curves now and then; clear the trails first */
        lines_start();
}
