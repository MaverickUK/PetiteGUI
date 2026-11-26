/* The desktop's state, its layout constants and the drawing functions (draw.c). */
#ifndef APP_H
#define APP_H

#include <stdint.h>
#include "rect.h"
#include "theme.h"

/* Layout, in pixels of the 640x200 screen. Columns are 8 pixels; a row of text is 8 high. */
#define MENU_H 11    /* menu bar; its bottom line is at y = MENU_H */
#define TITLE_H 11   /* window title bar */
#define ROW_H 9      /* a line in a list */
#define SCROLL_W 16  /* scroll bar */
#define ARROW_H 7    /* scroll arrow box: 16 x 7 looks square at 2.4:1 */
#define SHADOW_X 2
#define SHADOW_Y 1

#define MAX_ROWS 160 /* files a directory window lists */
#define NUM_WINS 4
#define NUM_ICONS 4
#define NUM_MENUS 6    /* on the menu bar */
#define CONTEXT_MENU NUM_MENUS /* the right-click menu, after them */
#define GAMES_WIN 2    /* the games list's window */
#define EDIT_WIN 3     /* the text editor's */

typedef struct Row {
    char name[13];
    long size;
    uint8_t dir;
} Row;

enum { WIN_FILES, WIN_GAMES, WIN_EDIT };

typedef struct Win {
    int kind;
    Rect r;
    const char *title;
    char path[80];     /* a directory window's, ending in a backslash */
    Row *rows;         /* a directory window's, cap of them */
    int cap;
    int nrows, sel, top;
    uint8_t open;
    uint8_t truncated; /* more files than cap */
    uint8_t not_ready; /* the drive couldn't be read */
    long free_kb;
} Win;

/* The parts of a window, worked out from its rectangle. */
typedef struct WinParts {
    Rect title, close, info, list, scroll, up, down, track;
    int visible; /* list rows that fit */
} WinParts;

enum { ICON_FLOPPY, ICON_DISK, ICON_GAMES, ICON_TRASH };

typedef struct Icon {
    const char *label;
    int kind, y;
    int win; /* the window it opens, or -1 */
} Icon;

enum { MI_SEP = 1, MI_OFF = 2, MI_CHECK = 4 };

typedef struct MenuItem {
    const char *label;
    const char *key;
    uint8_t flags;
    int cmd;
} MenuItem;

typedef struct Menu {
    const char *title;
    MenuItem *items;
    int n;
    int x; /* the title's text; for the right-click menu, its left edge */
    int y; /* the right-click menu's top; 0 for the menu bar's */
} Menu;

typedef struct Alert {
    char line1[40], line2[40];
    const char *button[2];
    int nbuttons; /* the last is the default */
    int cmd;      /* run when the default (last) button is chosen */
    int cmd_alt;  /* run when the first of two is chosen (Esc runs neither) */
    Rect r, b[2];
} Alert;

typedef struct App {
    Win win[NUM_WINS];
    int z[NUM_WINS]; /* back to front */
    Icon icon[NUM_ICONS];
    int sel_icon;
    Menu menu[NUM_MENUS + 1];
    int open_menu, hot; /* -1 when none */
    uint8_t __far *menu_save;
    int alert_on;
    Alert alert;
    uint8_t __far *alert_save;
    int sort_by_size;
    int has_ega;
    const Theme *theme;
    char clock[6];
    int quit;
} App;

extern App A;

/* draw.c */
void draw_init(void);
void draw_desktop(const Rect *area); /* everything but menus and alerts, clipped to area */
void draw_menubar(void);
void draw_window(int i);
Rect window_bounds(int i);          /* with its shadow */
void window_parts(int i, WinParts *p);
int front_window(void);             /* -1 if none open */
Rect icon_bounds(int i);            /* icon and label */
void draw_icon(int i);
Rect menu_rect(int m);              /* with its shadow */
void draw_menu(int m);
int menu_title_at(int x, int y);
int menu_item_at(int m, int x, int y);
void alert_layout(void);
void draw_alert(void);
int alert_button_at(int x, int y);
void draw_clock(void);

#endif
