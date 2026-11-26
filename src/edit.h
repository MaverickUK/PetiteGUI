/* The text editor: one file at a time, in a window. TXT, INI and CFG files (and AUTOEXEC.BAT
 * and CONFIG.SYS) open in it on a double-click, and any file with Open as text. The text
 * lives in a DOS memory block (up to 60,000 bytes of file), as a gap buffer (textbuf.h).
 *
 * Positions are character offsets; lines and columns count from 0. The view is the rows and
 * columns the window shows, from top (the start of its first line) and left (its first
 * column). The selection runs between the anchor and the caret; with nothing selected the
 * anchor is the caret. This module knows nothing about drawing: main.c redraws what changed. */
#ifndef EDIT_H
#define EDIT_H

#include "textbuf.h"

#define EDIT_MAX_FILE 60000u

enum { EDIT_OK, EDIT_TOO_BIG, EDIT_NO_MEMORY, EDIT_CANT_READ, EDIT_CANT_WRITE, EDIT_FULL };

typedef struct Editor {
    int open;
    char path[80];            /* "" for a new file not saved yet */
    char name[13];
    unsigned seg;
    TextBuf t;
    unsigned cur, cur_line;   /* the caret */
    unsigned anchor, anchor_line; /* the selection's other end */
    unsigned top, top_line;   /* the view's first line */
    unsigned left;            /* the view's first column */
    unsigned want_col;        /* the column up and down keep to */
    unsigned lines;
    int rows, cols;           /* the view's size */
    int crlf, modified;
} Editor;

extern Editor E;

int edit_open(const char *path);
/* An empty file, called Untitled until it's saved. */
int edit_new(void);
/* Save to E.path (which mustn't be ""), or to a new path, which becomes E.path. */
int edit_save(void);
int edit_save_as(const char *path);
void edit_close(void);
void edit_set_view(int rows, int cols);

/* A key: ch, or 0 and an extended scan code in ext. With shift, moving the caret selects.
 * Returns EDIT_OK, or EDIT_FULL if the buffer has no room for the character. */
int edit_key(int ch, int ext, int shift);
/* Put the caret at a view row and column (a mouse click), or extend the selection to it.
 * Row -1 is the line above the view and rows past the view are below it: the view scrolls
 * to show them (dragging a selection past the window's edge). */
void edit_click(int row, int col, int extend);
/* Move the view by lines, leaving the caret (the scroll bar). */
void edit_scroll(int lines);

/* The selection, start before end; zero if nothing is selected. */
int edit_selection(unsigned *start, unsigned *end);
void edit_select_all(void);
/* The clipboard: a DOS block of its own, kept until the next copy (or a program runs). */
int edit_copy(void);    /* EDIT_OK or EDIT_NO_MEMORY */
int edit_cut(void);
int edit_paste(void);   /* EDIT_OK or EDIT_FULL */
int edit_can_paste(void);
void edit_clipboard_free(void);
/* Select the next place s is, after the caret, going round to the top; zero if it's nowhere. */
int edit_find(const char *s);

/* The text of a view row, cols wide at most, control characters as spaces. Returns its
 * length, or -1 past the last line. */
int edit_row_text(int row, char *out);
/* The selected view columns of a view row, [*c0, *c1); a selected line break is one column
 * past the text. Zero if none of the row is selected. */
int edit_row_selection(int row, int *c0, int *c1);
/* The caret's view row and column; zero if it's out of view. */
int edit_caret(int *row, int *col);

/* Free the text while a game runs, and read it back after, where it was. */
void edit_suspend(void);
int edit_resume(void);

#endif
