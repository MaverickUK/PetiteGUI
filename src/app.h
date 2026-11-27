/* The desktop's state, its layout constants and the drawing functions (draw.c). */
#ifndef APP_H
#define APP_H

#include <stdint.h>
#include "rect.h"
#include "theme.h"
#include "field.h"

/* Layout, in pixels of the 640x200 screen. Columns are 8 pixels; a row of text is 8 high. */
#define MENU_H 11    /* menu bar; its bottom line is at y = MENU_H */
#define TITLE_H 11   /* window title bar */
#define ROW_H 9      /* a line in a list */
#define SCROLL_W 16  /* scroll bar */
#define ARROW_H 7    /* scroll arrow box: 16 x 7 looks square at 2.4:1 */
#define SHADOW_X 2
#define SHADOW_Y 1
#define GROW_H 8       /* the size box, under the scroll bar */
#define MIN_WIN_W 176  /* the smallest a window can be made */
#define MIN_WIN_H 72

#define MAX_ROWS 160 /* files a directory window lists */
#define DIR_WINS 4     /* folder windows: 0 to 3 */
#define NUM_WINS 6
#define MAX_ICONS 27   /* a drive each, and the games */
#define NUM_MENUS 7    /* on the menu bar */
#define CONTEXT_MENU NUM_MENUS /* the right-click menu, after them */
#define GAMES_WIN 4    /* the games list's window */
#define EDIT_WIN 5     /* the text editor's */

typedef struct Row {
    char name[13];
    long size;
    uint8_t dir;
    uint8_t attrib;         /* DOS's: read-only 1, hidden 2, system 4, folder 16, archive 32 */
    uint8_t picked;         /* selected as one of several (Shift- or Ctrl-click) */
    unsigned date, time;    /* last written, as DOS packs them */
} Row;

enum { SORT_NAME, SORT_SIZE, SORT_DATE };

enum { WIN_FILES, WIN_GAMES, WIN_EDIT };

typedef struct Win {
    int kind;
    Rect r;
    const char *title;
    char path[80];     /* a directory window's, ending in a backslash */
    Row *rows;         /* a directory window's, cap of them */
    int cap;
    int nrows, sel, top; /* sel: the selected row, or the last one clicked of several */
    int anchor;          /* where a Shift-click selects from */
    uint8_t open;
    uint8_t zoomed;    /* filling the screen; normal is the size to go back to */
    Rect normal;
    uint8_t truncated; /* more files than cap */
    uint8_t not_ready; /* the drive couldn't be read */
    long free_kb;
    int name_w, size_w; /* the widest name and size, in pixels: worked out once, when read */
} Win;

/* Is row k selected? The focus row always is; in a folder window, so is any picked one. */
#define ROW_SELECTED(w, k) ((k) == (w)->sel || ((w)->kind == WIN_FILES && (w)->rows[k].picked))

/* The parts of a window, worked out from its rectangle. */
typedef struct WinParts {
    Rect title, close, info, header, list, scroll, up, down, track, grow; /* header: the Games window's */
    int visible; /* list rows that fit */
} WinParts;

enum { ICON_FLOPPY, ICON_DISK, ICON_CDROM, ICON_NETWORK, ICON_GAMES };

typedef struct Icon {
    char label[8];
    int kind;
    int x, y;    /* x is its centre */
    char drive;  /* the drive it opens, or 0 */
} Icon;

/* The text-entry box: a prompt, a field, and Cancel / OK. */
typedef struct Input {
    char prompt[48];
    Field field;
    Rect r, f, b[2]; /* the box, the field, the buttons */
} Input;

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

/* File > Info...: a file's or folder's details, and its attributes, two of them switchable. */
typedef struct Info {
    char name[13], size[32], date[32], also[40];
    int read_only, hidden;
    Rect r, check[2], b[2]; /* the box, the two check boxes (with their labels), the buttons */
} Info;

typedef struct Alert {
    char line1[40], line2[40];
    const char *button[2];
    int nbuttons; /* the last is the default */
    int cmd;      /* run when the default (last) button is chosen */
    int cmd_alt;  /* run when the first of two is chosen (Esc runs neither) */
    int logo;     /* PetiteGUI's logo in place of the "!" sign (the About box) */
    Rect r, b[2];
} Alert;

typedef struct App {
    Win win[NUM_WINS];
    int z[NUM_WINS]; /* back to front */
    Icon icon[MAX_ICONS];
    int nicons;
    int sel_icon;
    Menu menu[NUM_MENUS + 1];
    int open_menu, hot; /* -1 when none */
    uint8_t __far *menu_save;
    int alert_on;
    Alert alert;
    Input input;
    Info info;
    uint8_t __far *alert_save;
    int sort_by; /* SORT_... */
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
/* The Games window's columns: where each shown one starts (col_x[COL_...]), and how many
 * characters the name gets. Returns the columns that fit (those that don't are dropped from
 * the right). */
unsigned games_layout(const WinParts *p, int *col_x, int *name_chars);
/* The heading under x in the Games window: a column, COL_NAME, or -2 for none. */
int games_heading_at(const WinParts *p, int x);
Rect icon_bounds(int i);            /* icon and label */
void draw_icon(int i);
Rect menu_rect(int m);              /* with its shadow */
void draw_menu(int m);
void draw_menu_item(int m, int i);
int menu_title_at(int x, int y);
int menu_item_at(int m, int x, int y);
void alert_layout(void);
void draw_alert(void);
int alert_button_at(int x, int y);
void draw_clock(void);
void input_layout(void);
void draw_input(void);       /* the whole box */
void draw_input_field(void); /* just the field, as it's typed in */
void info_layout(void);
void draw_info(void);
void draw_info_checks(void); /* just the check boxes, as they change */

#endif
