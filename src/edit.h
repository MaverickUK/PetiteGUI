/* The text editor: one file at a time, in a window. TXT, INI and CFG files open in it on a
 * double-click, and any file with Open as text. The text lives in a DOS memory block (up to
 * 60,000 bytes of file), as a gap buffer (textbuf.h).
 *
 * Positions are character offsets; lines and columns count from 0. The view is the rows and
 * columns the window shows, from top (the start of its first line) and left (its first
 * column). This module knows nothing about drawing: main.c redraws what changed. */
#ifndef EDIT_H
#define EDIT_H

#include "textbuf.h"

#define EDIT_MAX_FILE 60000u

enum { EDIT_OK, EDIT_TOO_BIG, EDIT_NO_MEMORY, EDIT_CANT_READ, EDIT_CANT_WRITE, EDIT_FULL };

typedef struct Editor {
    int open;
    char path[80];
    char name[13];
    unsigned seg;
    TextBuf t;
    unsigned cur, cur_line;   /* the caret */
    unsigned top, top_line;   /* the view's first line */
    unsigned left;            /* the view's first column */
    unsigned want_col;        /* the column up and down keep to */
    unsigned lines;
    int rows, cols;           /* the view's size */
    int crlf, modified;
} Editor;

extern Editor E;

int edit_open(const char *path);
int edit_save(void);
void edit_close(void);
void edit_set_view(int rows, int cols);

/* A key: ch, or 0 and an extended scan code in ext. Returns EDIT_OK, or EDIT_FULL if the
 * buffer has no room for the character. */
int edit_key(int ch, int ext);
/* Put the caret at a view row and column (a mouse click). */
void edit_click(int row, int col);
/* Move the view by lines, leaving the caret (the scroll bar). */
void edit_scroll(int lines);

/* The text of a view row, cols wide at most, control characters as spaces. Returns its
 * length, or -1 past the last line. */
int edit_row_text(int row, char *out);
/* The caret's view row and column; zero if it's out of view. */
int edit_caret(int *row, int *col);

/* Free the text while a game runs, and read it back after, where it was. */
void edit_suspend(void);
int edit_resume(void);

#endif
