/* The Games window's columns: which there are, the setting that picks them, and what a cell
 * says. Pure C, so the host tests build it. */
#ifndef GAMECOLS_H
#define GAMECOLS_H

#include "games.h"

enum {
    COL_YEAR, COL_GENRE, COL_DEVELOPER, COL_TYPE, COL_VIDEO, COL_SOUND, COL_MEMORY,
    COL_PLAYS, COL_LAST, COL_TIME, NUM_COLS
};
#define COL_NAME (-1)

typedef struct GameColumn {
    const char *key;   /* in PETITE.CFG's gamecolumns: */
    const char *title; /* its heading */
    const char *label; /* in the Games menu */
    int width;         /* in characters */
    int right;         /* numbers: right-aligned */
} GameColumn;

extern const GameColumn game_columns[NUM_COLS];

/* "year,genre,plays" to a mask of 1 << COL_; "none" or "" is 0, unknown names are skipped. */
unsigned gc_parse(const char *list);
/* And back: out needs 80 characters. */
void gc_format(unsigned mask, char *out);
/* What a cell says (out needs 24 characters); "" for nothing. */
void gc_text(const Game *g, int col, char *out);
/* "3h 05m", "45m", or "" for none. */
void gc_minutes(unsigned long minutes, char *out);
/* One line of GameMinder's !GM_LOG.TXT: ">2026/11/20 14:05" a play started, "<35" minutes. */
void gc_log_line(Game *g, const char *line);

#endif
