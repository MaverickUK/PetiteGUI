/* What the screensavers share (saver.c): timing, colours, drawing helpers. */
#ifndef SAVER_INT_H
#define SAVER_INT_H

#include <stdint.h>
#include "gfx.h"
#include "saverlib.h"

/* The BIOS clock, 18.2 ticks a second. */
unsigned long sv_ticks(void);
/* Wait for the start of the next vertical retrace (about 60 a second), to pace a frame. */
void sv_wait_frame(void);
/* 16 colours (Ink) or 2 (Paper). */
int sv_colour(void);
/* A colour for this mode: c in Ink, 1 (ink) for any non-zero c in Paper. */
uint8_t sv_col(uint8_t c);
void sv_clear(void);
/* Text from the BIOS font, each pixel sx wide and sy tall. */
void sv_text(int x, int y, const char *s, uint8_t c, int sx, int sy);
/* A line XORed in colour c, so drawing it again takes it away. */
void sv_xor_line(int x0, int y0, int x1, int y1, uint8_t c);
/* Puts the desktop back (melt and life start from it again). */
void sv_redraw_desktop(void);

/* The savers, in savers_*.c */
void cycle_start(void);
void cycle_step(void);
void cycle_stop(void);
void blank_start(void);
void blank_step(void);
void blank_stop(void);
void stars_start(void);
void stars_step(void);
void lines_start(void);
void lines_step(void);
void maze_start(void);
void maze_step(void);
void melt_start(void);
void melt_step(void);
void life_start(void);
void life_step(void);
void games_saver_start(void);
void games_saver_step(void);
void clock_start(void);
void clock_step(void);
void sv_nothing(void);

#endif
