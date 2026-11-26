#include "saverlib.h"

/* ---- Random numbers ---- */

static unsigned long rng = 1;

void sl_seed(unsigned long seed)
{
    rng = seed ? seed : 1;
}

unsigned sl_rand(void)
{
    rng = rng * 1103515245UL + 12345UL;
    return (unsigned)((rng >> 16) & 0x7FFF);
}

unsigned sl_rand_n(unsigned n)
{
    return n ? sl_rand() % n : 0;
}

/* ---- Sine ---- */

int sl_sin[256];

/* A quarter turn from the Taylor series x - x^3/3! + x^5/5! - ..., in whole numbers (there's
 * no FPU to count on) with x scaled by 16384, then mirrored for the rest. */
void sl_make_sin(void)
{
    long x, x2, t, s;
    int i, k;

    for (i = 0; i <= 64; i++) {
        x = (long)i * 25736L / 64L; /* i steps of 2 pi / 256, times 16384 */
        x2 = (x * x) >> 14;
        t = s = x;
        for (k = 1; k <= 4; k++) {
            t = -((t * x2) >> 14) / ((2L * k) * (2L * k + 1));
            s += t;
        }
        sl_sin[i] = (int)s;
    }
    sl_sin[64] = 16384;
    for (i = 65; i < 128; i++)
        sl_sin[i] = sl_sin[128 - i];
    sl_sin[128] = 0;
    for (i = 129; i < 256; i++)
        sl_sin[i] = -sl_sin[i - 128];
}

/* ---- Life ---- */

unsigned sl_life_step(const unsigned char *cur, unsigned char *next, int w, int h)
{
    int x, y, up, down, left, right, n;
    unsigned changed = 0;
    unsigned char alive;

    for (y = 0; y < h; y++) {
        up = (y == 0 ? h - 1 : y - 1) * w;
        down = (y == h - 1 ? 0 : y + 1) * w;
        for (x = 0; x < w; x++) {
            left = x == 0 ? w - 1 : x - 1;
            right = x == w - 1 ? 0 : x + 1;
            n = cur[up + left] + cur[up + x] + cur[up + right] +
                cur[y * w + left] + cur[y * w + right] +
                cur[down + left] + cur[down + x] + cur[down + right];
            alive = (unsigned char)(n == 3 || (n == 2 && cur[y * w + x]));
            if (alive != cur[y * w + x])
                changed++;
            next[y * w + x] = alive;
        }
    }
    return changed;
}

/* ---- The maze ---- */

#define BLOCK(m, x, y) ((m)->blocks[(y) * (m)->bw + (x)])

void sl_maze_start(SlMaze *m, unsigned char *blocks, int cw, int ch)
{
    int i;

    m->blocks = blocks;
    m->cw = cw;
    m->ch = ch;
    m->bw = cw * 2 + 1;
    m->bh = ch * 2 + 1;
    for (i = 0; i < m->bw * m->bh; i++)
        blocks[i] = MZ_WALL;
    BLOCK(m, 1, 1) = MZ_OPEN;
    m->stack[0] = 0;
    m->sp = 1;
    m->solving = 0;
    m->done = 0;
}

static const int DX[4] = { 1, 0, -1, 0 }, DY[4] = { 0, 1, 0, -1 };

/* A block for cell c, or a wall between c and its neighbour in direction d. */
static void cell_block(const SlMaze *m, int c, int *bx, int *by)
{
    *bx = (c % m->cw) * 2 + 1;
    *by = (c / m->cw) * 2 + 1;
}

static int build_step(SlMaze *m, int *bx, int *by)
{
    int c, x, y, d, n, dirs[4], nx, ny, cx, cy;

    while (m->sp > 0) {
        c = m->stack[m->sp - 1];
        x = c % m->cw;
        y = c / m->cw;
        n = 0;
        for (d = 0; d < 4; d++) {
            nx = x + DX[d];
            ny = y + DY[d];
            if (nx >= 0 && ny >= 0 && nx < m->cw && ny < m->ch && BLOCK(m, nx * 2 + 1, ny * 2 + 1) == MZ_WALL)
                dirs[n++] = d;
        }
        if (n == 0) {
            m->sp--; /* a dead end: back up, no blocks change */
            continue;
        }
        d = dirs[sl_rand_n((unsigned)n)];
        cell_block(m, c, &cx, &cy);
        bx[0] = cx + DX[d];
        by[0] = cy + DY[d];
        bx[1] = cx + DX[d] * 2;
        by[1] = cy + DY[d] * 2;
        BLOCK(m, bx[0], by[0]) = MZ_OPEN;
        BLOCK(m, bx[1], by[1]) = MZ_OPEN;
        m->stack[m->sp++] = (y + DY[d]) * m->cw + x + DX[d];
        return 2;
    }
    /* Built: start solving from the first cell */
    m->solving = 1;
    m->stack[0] = 0;
    m->sp = 1;
    BLOCK(m, 1, 1) = MZ_PATH;
    bx[0] = 1;
    by[0] = 1;
    return 1;
}

static int solve_step(SlMaze *m, int *bx, int *by)
{
    int c, x, y, d, cx, cy, goal = m->cw * m->ch - 1, p;

    if (m->sp == 0 || m->stack[m->sp - 1] == goal) {
        m->done = 1;
        return -1;
    }
    c = m->stack[m->sp - 1];
    x = c % m->cw;
    y = c / m->cw;
    cell_block(m, c, &cx, &cy);
    for (d = 0; d < 4; d++) {
        if (BLOCK(m, cx + DX[d], cy + DY[d]) == MZ_OPEN && BLOCK(m, cx + DX[d] * 2, cy + DY[d] * 2) == MZ_OPEN) {
            bx[0] = cx + DX[d];
            by[0] = cy + DY[d];
            bx[1] = cx + DX[d] * 2;
            by[1] = cy + DY[d] * 2;
            BLOCK(m, bx[0], by[0]) = MZ_PATH;
            BLOCK(m, bx[1], by[1]) = MZ_PATH;
            m->stack[m->sp++] = (y + DY[d]) * m->cw + x + DX[d];
            return 2;
        }
    }
    /* A dead end: mark it and the passage back to where we came from */
    BLOCK(m, cx, cy) = MZ_DEAD;
    bx[0] = cx;
    by[0] = cy;
    m->sp--;
    if (m->sp == 0)
        return 1;
    p = m->stack[m->sp - 1];
    bx[1] = (cx + (p % m->cw) * 2 + 1) / 2;
    by[1] = (cy + (p / m->cw) * 2 + 1) / 2;
    BLOCK(m, bx[1], by[1]) = MZ_DEAD;
    return 2;
}

int sl_maze_step(SlMaze *m, int *bx, int *by)
{
    if (m->done)
        return -1;
    return m->solving ? solve_step(m, bx, by) : build_step(m, bx, by);
}

/* ---- Seven-segment digits ---- */

const unsigned char sl_segments[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};
