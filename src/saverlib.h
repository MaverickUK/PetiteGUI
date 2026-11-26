/* What the screensavers compute, as pure C (no DOS, no drawing), so the host tests build it:
 * random numbers, a sine table, Conway's Life, a maze built and solved a step at a time, and
 * seven-segment digits. */
#ifndef SAVERLIB_H
#define SAVERLIB_H

/* ---- Random numbers: a 32-bit linear congruential generator ---- */
void sl_seed(unsigned long seed);
unsigned sl_rand(void);           /* 0..32767 */
unsigned sl_rand_n(unsigned n);   /* 0..n-1 */

/* ---- Sine: 256 steps to a turn, scaled to +-16384 ---- */
extern int sl_sin[256];
void sl_make_sin(void);

/* ---- Life: one generation of a w x h grid of 0/1 cells, wrapping at the edges ---- */
/* Returns how many cells changed. */
unsigned sl_life_step(const unsigned char *cur, unsigned char *next, int w, int h);

/* ---- The maze ----
 * Cells cw x ch, drawn as blocks on a (2cw+1) x (2ch+1) grid: cells at odd block positions,
 * walls between them. Building carves a passage from cell (0,0) a step at a time (a random
 * depth-first search); solving then searches from (0,0) to the last cell. */
enum { MZ_WALL, MZ_OPEN, MZ_PATH, MZ_DEAD };
#define MZ_MAX_CELLS 300

typedef struct SlMaze {
    unsigned char *blocks; /* (2cw+1) * (2ch+1) */
    int cw, ch, bw, bh;
    int stack[MZ_MAX_CELLS];
    int sp;
    int solving, done;
} SlMaze;

void sl_maze_start(SlMaze *m, unsigned char *blocks, int cw, int ch);
/* One step of building, then of solving. Puts the blocks that changed in bx/by (up to 2) and
 * returns how many; returns -1 when the maze is built and solved. */
int sl_maze_step(SlMaze *m, int *bx, int *by);

/* ---- Seven-segment digits: bit 0 top (a), then b c d e f clockwise, bit 6 middle (g) ---- */
extern const unsigned char sl_segments[10];

#endif
