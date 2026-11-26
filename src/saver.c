#include <conio.h>
#include <i86.h>
#include <stdlib.h>
#include <string.h>
#include "keyboard.h"
#include "mouse.h"
#include "saver.h"
#include "saver_int.h"

const Saver savers[] = {
    { "cycle", "Colour cycle", cycle_start, cycle_step, cycle_stop },
    { "blank", "Blank", blank_start, blank_step, blank_stop },
    { "stars", "Starfield", stars_start, stars_step, sv_nothing },
    { "lines", "Lines", lines_start, lines_step, sv_nothing },
    { "maze", "Maze", maze_start, maze_step, sv_nothing },
    { "melt", "Melt", melt_start, melt_step, sv_nothing },
    { "life", "Life", life_start, life_step, sv_nothing },
    { "games", "Game showcase", games_saver_start, games_saver_step, sv_nothing },
    { "clock", "Clock", clock_start, clock_step, sv_nothing },
};
const int num_savers = (int)(sizeof savers / sizeof savers[0]);

static void (*redraw_desktop)(void);
static unsigned long started;

int saver_by_key(const char *key)
{
    int i;

    if (stricmp(key, "off") == 0)
        return SAVER_OFF;
    if (stricmp(key, "random") == 0)
        return SAVER_RANDOM;
    for (i = 0; i < num_savers; i++)
        if (stricmp(key, savers[i].key) == 0)
            return i;
    return SAVER_UNKNOWN;
}

const char *saver_key(int index)
{
    if (index == SAVER_OFF)
        return "off";
    if (index == SAVER_RANDOM)
        return "random";
    return index >= 0 && index < num_savers ? savers[index].key : "off";
}

/* ---- Shared ---- */

unsigned long sv_ticks(void)
{
    return *(volatile unsigned long __far *)MK_FP(0x40, 0x6C);
}

void sv_wait_frame(void)
{
    long n;

    /* Port 3DAh bit 3 is the vertical retrace on CGA and EGA. Give up after a while, in case
     * a card doesn't show it. */
    for (n = 0; (inp(0x3DA) & 8) && n < 20000L; n++)
        ;
    for (n = 0; !(inp(0x3DA) & 8) && n < 20000L; n++)
        ;
}

int sv_colour(void)
{
    return gfx_driver() == &drv_ega;
}

uint8_t sv_col(uint8_t c)
{
    return sv_colour() ? c : (uint8_t)(c != 0);
}

void sv_clear(void)
{
    gfx_set_clip(NULL);
    gfx_fill(0, 0, SCREEN_W, SCREEN_H, 0);
}

void sv_text(int x, int y, const char *s, uint8_t c, int sx, int sy)
{
    const uint8_t __far *g;
    int r, b;

    for (; *s; s++, x += 8 * sx) {
        g = gfx_glyph((unsigned char)*s);
        for (r = 0; r < 8; r++)
            for (b = 0; b < 8; b++)
                if (g[r] & (0x80 >> b))
                    gfx_fill(x + b * sx, y + r * sy, sx, sy, c);
    }
}

/* Bresenham's line, with each row's run of pixels XORed in one go (up to 16 at a time). */
void sv_xor_line(int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2, lo = x0, hi = x0, ry = y0;

    for (;;) {
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
        if (y0 != ry || hi - lo == 15) { /* a new row, or the run is 16 long */
            gfx_xor_bits(lo, ry, (uint16_t)(0xFFFFu << (15 - (hi - lo))), hi - lo + 1, c);
            lo = hi = x0;
            ry = y0;
        } else if (x0 < lo) {
            lo = x0;
        } else {
            hi = x0;
        }
    }
    gfx_xor_bits(lo, ry, (uint16_t)(0xFFFFu << (15 - (hi - lo))), hi - lo + 1, c);
}

void sv_redraw_desktop(void)
{
    if (redraw_desktop != NULL)
        redraw_desktop();
}

void sv_nothing(void)
{
}

/* ---- Running one ---- */

/* Anything the person does: a key (eaten, so it doesn't reach the desktop), a button, or the
 * mouse moving (its motion counters, INT 33h function 0Bh, catch the smallest nudge). */
static int woken(void)
{
    union REGS r;
    int ext;

    if (kb_hit()) {
        kb_read(&ext);
        return 1;
    }
    if (!mouse_present())
        return 0;
    r.x.ax = 0x0B;
    int86(0x33, &r, &r);
    if (sv_ticks() - started < 9)
        return 0; /* half a second's grace for the hand that started it */
    if (abs((int)r.x.cx) > 2 || abs((int)r.x.dx) > 2)
        return 1;
    r.x.ax = 3;
    int86(0x33, &r, &r);
    return r.x.bx != 0;
}

void saver_run(int index, void (*redraw)(void), long steps)
{
    const Saver *s;
    union REGS r;
    long n;

    if (index == SAVER_RANDOM) { /* any but Blank, which is a choice rather than a show */
        sl_seed(sv_ticks());
        index = (int)sl_rand_n((unsigned)num_savers - 1);
        if (index >= 1)
            index++;
    }
    if (index < 0 || index >= num_savers)
        return;
    s = &savers[index];
    redraw_desktop = redraw;
    sl_seed(sv_ticks());
    sl_make_sin();
    gfx_set_clip(NULL);
    if (mouse_present()) {
        r.x.ax = 0x0B; /* clear the motion counters */
        int86(0x33, &r, &r);
    }
    s->start();
    started = sv_ticks();
    for (n = 0; steps > 0 ? n < steps : !woken(); n++)
        s->step();
    s->stop();
    gfx_palette_reset();
    gfx_blank(0);
    if (mouse_present()) { /* the click that woke it isn't for the desktop */
        do {
            r.x.ax = 3;
            int86(0x33, &r, &r);
        } while (r.x.bx != 0);
    }
}
