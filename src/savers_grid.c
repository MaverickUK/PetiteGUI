/* Maze, Melt and Life: the ones on a grid. */
#include <string.h>
#include "saver_int.h"

/* ---- Maze: built a passage at a time, then solved, then a new one ----
 *
 * Blocks are 16 x 7 pixels, which look square on a 4:3 screen and are whole bytes wide, so
 * drawing one is a few byte writes. 19 x 13 cells make 39 x 27 blocks: 624 x 189 pixels. */

#define MAZE_CW 19
#define MAZE_CH 13
#define MAZE_X 8
#define MAZE_Y 5

static unsigned char maze_blocks[(MAZE_CW * 2 + 1) * (MAZE_CH * 2 + 1)];
static SlMaze maze;
static unsigned long maze_pause;

static void maze_block(int bx, int by)
{
    int x = MAZE_X + bx * 16, y = MAZE_Y + by * 7, v = maze_blocks[by * maze.bw + bx];

    if (!sv_colour()) {
        if (v == MZ_PATH)
            gfx_pattern(x, y, 16, 7, PAT_CHECKER, 1, 0);
        else
            gfx_fill(x, y, 16, 7, v == MZ_WALL ? 1 : 0);
        return;
    }
    gfx_fill(x, y, 16, 7, v == MZ_WALL ? 1 : v == MZ_PATH ? 14 : v == MZ_DEAD ? 8 : 0);
}

void maze_start(void)
{
    sv_clear();
    sl_maze_start(&maze, maze_blocks, MAZE_CW, MAZE_CH);
    gfx_fill(MAZE_X, MAZE_Y, maze.bw * 16, maze.bh * 7, sv_col(1));
    maze_block(1, 1);
    maze_pause = 0;
}

void maze_step(void)
{
    int bx[2], by[2], n, i, k;

    if (maze_pause) { /* solved: show it for three seconds */
        if (sv_ticks() >= maze_pause)
            maze_start();
        else
            sv_wait_frame();
        return;
    }
    sv_wait_frame();
    for (k = 0; k < 3; k++) {
        n = sl_maze_step(&maze, bx, by);
        if (n < 0) {
            maze_pause = sv_ticks() + 55;
            return;
        }
        for (i = 0; i < n; i++)
            maze_block(bx[i], by[i]);
    }
}

/* ---- Melt: the desktop slides off the bottom of the screen ----
 *
 * Each frame a dozen byte-wide columns, picked at random, move down a few rows, leaving black
 * above. Columns are whole bytes, so a move is a byte copy per row (the EGA copies all four
 * planes at once through its latches). When it's all gone, the desktop comes back and melts
 * again. */

static unsigned char melt_top[SCREEN_W / 8];
static int melt_done;
static unsigned long melt_pause;

void melt_start(void)
{
    memset(melt_top, 0, sizeof melt_top);
    melt_done = 0;
    melt_pause = 0;
}

void melt_step(void)
{
    int k, c, d, top;

    if (melt_pause) {
        if (sv_ticks() >= melt_pause) {
            sv_redraw_desktop();
            melt_start();
        } else {
            sv_wait_frame();
        }
        return;
    }
    sv_wait_frame();
    for (k = 0; k < 12; k++) {
        c = (int)sl_rand_n(SCREEN_W / 8);
        top = melt_top[c];
        if (top >= SCREEN_H)
            continue;
        d = 1 + (int)sl_rand_n(4);
        if (top + d > SCREEN_H)
            d = SCREEN_H - top;
        gfx_move_down(c * 8, top, 8, SCREEN_H - top - d, d);
        gfx_fill(c * 8, top, 8, d, 0);
        melt_top[c] = (unsigned char)(top + d);
        if (top + d >= SCREEN_H && ++melt_done == SCREEN_W / 8)
            melt_pause = sv_ticks() + 36;
    }
}

/* ---- Life: Conway's Life, seeded from the desktop ----
 *
 * 80 x 50 cells of 8 x 4 pixels. A cell starts alive if there's black where the desktop
 * pattern never is (window frames, text, icons), so the windows turn into the first
 * generation. When it settles, or after 1,500 generations, it starts again from random. */

#define LIFE_W 80
#define LIFE_H 50
#define LIFE_N (LIFE_W * LIFE_H)

static unsigned char life_a[LIFE_N], life_b[LIFE_N];
static unsigned char *life_cur, *life_next;
static int life_gen, life_quiet;

static void life_cell(int i, int alive)
{
    gfx_fill((i % LIFE_W) * 8, (i / LIFE_W) * 4, 8, 4, alive ? sv_col(10) : 0);
}

static void life_draw_all(void)
{
    int i;

    sv_clear();
    for (i = 0; i < LIFE_N; i++)
        if (life_cur[i])
            life_cell(i, 1);
    life_gen = 0;
    life_quiet = 0;
}

static void life_random(void)
{
    int i;

    for (i = 0; i < LIFE_N; i++)
        life_cur[i] = (unsigned char)(sl_rand_n(4) == 0);
    life_draw_all();
}

void life_start(void)
{
    int i, x, y, alive = 0;

    life_cur = life_a;
    life_next = life_b;
    /* The desktop pattern is on at even pixels of even rows, in both looks: black there is
     * something drawn on it */
    for (i = 0; i < LIFE_N; i++) {
        x = (i % LIFE_W) * 8;
        y = (i / LIFE_W) * 4;
        life_cur[i] = (unsigned char)(gfx_peek(x, y) == 0 || gfx_peek(x + 4, y) == 0 ||
                                      gfx_peek(x + 2, y + 2) == 0 || gfx_peek(x + 6, y + 2) == 0);
        alive += life_cur[i];
    }
    if (alive < 40)
        life_random();
    else
        life_draw_all();
}

void life_step(void)
{
    unsigned changed;
    unsigned char *t;
    int i;

    sv_wait_frame();
    changed = sl_life_step(life_cur, life_next, LIFE_W, LIFE_H);
    for (i = 0; i < LIFE_N; i++)
        if (life_cur[i] != life_next[i])
            life_cell(i, life_next[i]);
    t = life_cur;
    life_cur = life_next;
    life_next = t;
    life_gen++;
    life_quiet = changed <= 12 ? life_quiet + 1 : 0; /* only blinkers left */
    if (changed == 0 || life_quiet > 150 || life_gen > 1500)
        life_random();
}
