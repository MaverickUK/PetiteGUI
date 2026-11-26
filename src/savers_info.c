/* Game showcase and Clock: savers that show something worth reading, a little further over
 * each time so nothing stays in one place on the tube. */
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "games.h"
#include "launch.h"
#include "saver_int.h"

static int imax(int a, int b)
{
    return a > b ? a : b;
}

/* ---- Game showcase: a game from your collection every seven seconds ----
 *
 * Its name, what it is, and how much you've played it, all from the games index (which has
 * GameMinder's details and play log for each). It never scans. */

static unsigned long show_next;

static void show_game(void)
{
    char lines[3][48];
    const char *name;
    int k, i, w, h, x, y;
    long minutes;
    Game g;

    sv_clear();
    if (games_count() == 0) {
        name = "No games yet";
        strcpy(lines[0], "Open Games to look for them.");
        lines[1][0] = lines[2][0] = '\0';
    } else {
        k = (int)sl_rand_n((unsigned)games_count());
        games_get(k, &g);
        name = g.name;
        lines[0][0] = '\0';
        if (g.genre[0])
            strcat(lines[0], g.genre);
        if (g.year[0])
            sprintf(lines[0] + strlen(lines[0]), "%s%s", lines[0][0] ? ", " : "", g.year);
        if (g.developer[0])
            sprintf(lines[0] + strlen(lines[0]), "%s%.30s", lines[0][0] ? ", " : "", g.developer);
        minutes = (long)g.minutes;
        if (g.plays == 0)
            strcpy(lines[1], "Not played yet");
        else if (g.plays == 1)
            sprintf(lines[1], "Played once, on %s", g.last);
        else
            sprintf(lines[1], "Played %u times, last on %s", g.plays, g.last);
        if (minutes >= 60)
            sprintf(lines[2], "%ld hour%s %ld minute%s in all", minutes / 60, minutes / 60 == 1 ? "" : "s",
                    minutes % 60, minutes % 60 == 1 ? "" : "s");
        else if (minutes > 0)
            sprintf(lines[2], "%ld minute%s in all", minutes, minutes == 1 ? "" : "s");
        else
            lines[2][0] = '\0';
    }

    /* Somewhere new each time: the name twice the size, then the lines under it */
    w = (int)strlen(name) * 16;
    for (i = 0; i < 3; i++)
        w = imax(w, (int)strlen(lines[i]) * 8);
    h = 16 + 8 + 3 * 11;
    x = (int)sl_rand_n((unsigned)imax(1, SCREEN_W - w));
    y = (int)sl_rand_n((unsigned)imax(1, SCREEN_H - h));
    sv_text(x, y, name, sv_col(14), 2, 2);
    sv_text(x, y + 24, lines[0], sv_col(7), 1, 1);
    sv_text(x, y + 35, lines[1], sv_col(11), 1, 1);
    sv_text(x, y + 46, lines[2], sv_col(11), 1, 1);
}

void games_saver_start(void)
{
    if (!games_loaded())
        games_load_index();
    show_next = 0;
}

void games_saver_step(void)
{
    sv_wait_frame();
    if (sv_ticks() < show_next)
        return;
    show_next = sv_ticks() + 7 * 18;
    show_game();
}

/* ---- Clock: large seven-segment digits and the date, moved each minute ---- */

#define DIGIT_W 40
#define DIGIT_H 44
#define SEG_H 5   /* thickness of the across segments, in rows */
#define SEG_W 10  /* and of the up-and-down ones, in pixels (they look the same) */
#define DIGIT_GAP 12
#define COLON_W 12
#define CLOCK_W (4 * DIGIT_W + 4 * DIGIT_GAP + COLON_W)

static const char *const day_names[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
static const char *const month_names[12] = {
    "January", "February", "March", "April", "May", "June", "July", "August",
    "September", "October", "November", "December"
};
static int clock_minute, clock_second, clock_x, clock_y;
static unsigned long clock_tick;

static void digit(int x, int y, int d)
{
    unsigned char s = sl_segments[d];
    uint8_t c = sv_col(11);
    int half = DIGIT_H / 2;

    if (s & 0x01) gfx_fill(x, y, DIGIT_W, SEG_H, c);                              /* a */
    if (s & 0x02) gfx_fill(x + DIGIT_W - SEG_W, y, SEG_W, half, c);               /* b */
    if (s & 0x04) gfx_fill(x + DIGIT_W - SEG_W, y + half, SEG_W, half, c);        /* c */
    if (s & 0x08) gfx_fill(x, y + DIGIT_H - SEG_H, DIGIT_W, SEG_H, c);            /* d */
    if (s & 0x10) gfx_fill(x, y + half, SEG_W, half, c);                          /* e */
    if (s & 0x20) gfx_fill(x, y, SEG_W, half, c);                                 /* f */
    if (s & 0x40) gfx_fill(x, y + half - SEG_H / 2, DIGIT_W, SEG_H, c);           /* g */
}

static void colon(int on)
{
    int x = clock_x + 2 * (DIGIT_W + DIGIT_GAP);
    uint8_t c = on ? sv_col(11) : 0;

    gfx_fill(x, clock_y + DIGIT_H / 3 - 3, COLON_W, 5, c);
    gfx_fill(x, clock_y + DIGIT_H * 2 / 3 - 2, COLON_W, 5, c);
}

static void draw_clock_face(const struct dostime_t *t)
{
    struct dosdate_t d;
    char date[40];
    int w, x, step = DIGIT_W + DIGIT_GAP;

    _dos_getdate(&d);
    sprintf(date, "%s %d %s %d", day_names[d.dayofweek % 7], d.day, month_names[(d.month + 11) % 12], d.year);
    w = imax(CLOCK_W, (int)strlen(date) * 8);
    sv_clear();
    clock_x = (int)sl_rand_n((unsigned)(SCREEN_W - w));
    clock_y = (int)sl_rand_n((unsigned)(SCREEN_H - DIGIT_H - 22));
    x = clock_x;
    digit(x, clock_y, t->hour / 10);
    digit(x + step, clock_y, t->hour % 10);
    x += 2 * step + COLON_W + DIGIT_GAP;
    digit(x, clock_y, t->minute / 10);
    digit(x + step, clock_y, t->minute % 10);
    sv_text(clock_x + (CLOCK_W - (int)strlen(date) * 8) / 2, clock_y + DIGIT_H + 12, date, sv_col(7), 1, 1);
}

void clock_start(void)
{
    clock_minute = clock_second = -1;
    clock_tick = 0;
}

void clock_step(void)
{
    struct dostime_t t;

    sv_wait_frame();
    if (sv_ticks() == clock_tick)
        return;
    clock_tick = sv_ticks();
    _dos_gettime(&t);
    if (t.minute != clock_minute) {
        clock_minute = t.minute;
        draw_clock_face(&t);
    }
    if (t.second != clock_second) { /* the colon blinks with the seconds */
        clock_second = t.second;
        colon(t.second % 2 == 0);
    }
}
