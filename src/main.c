/* PetiteGUI: a small desktop for CGA and EGA PCs.
 *
 *   PETITE            Ink on an EGA or VGA, Paper on a CGA
 *   PETITE /CGA       Paper, whatever the card
 *   PETITE /EGA       Ink (needs an EGA or VGA)
 *   PETITE /SCAN      scan for games now, rather than when the list is first opened
 *   PETITE /LIST      print the games list (scanning if there's no index) and quit
 *   PETITE /RUN=name  run the first game whose name contains name, then quit
 *   PETITE /MEM       print what a swap would leave for a program, and quit
 *   PETITE /SET=key:value   change one setting in PETITE.CFG, and quit
 *   PETITE /SAVER=name [/STEPS=n] [/DUMP=FILE.BIN]
 *                     run a screensaver n steps (300) over the desktop, dump the screen, quit
 *   PETITE /SHOT=n /DUMP=FILE.BIN
 *                     draw scene n (0 desktop, 1 File menu open, 2 alert, 3 games list,
 *                     4 the first game run from the list and back, 5 the editor,
 *                     6 the right-click menu, 7 an edit saved, 8 windows resized and zoomed,
 *                     9 the About box, 10 a command run, 11 a batch file opened, 12 Run...,
 *                     13 file operations, 14 deleting a folder),
 *   PETITE /DRIVES    list the drives found, and quit
 *                     write video memory to FILE.BIN and quit: tools/shot.sh makes a PNG
 *
 * The program polls: it reads the mouse and keyboard, and acts on presses. Anything that
 * changes redraws only the rectangle it changed, back to front. */
#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "drives.h"
#include "edit.h"
#include "files.h"
#include "fileops.h"
#include "gamecols.h"
#include "games.h"
#include "gfx.h"
#include "launch.h"
#include "mouse.h"
#include "saver.h"
#include "swap.h"
#include "util.h"
#include "version.h"

App A;

enum {
    CMD_NONE, CMD_ABOUT, CMD_OPEN, CMD_INFO, CMD_NEW_FOLDER, CMD_COPY, CMD_DELETE,
    CMD_DO_DELETE, CMD_CLOSE, CMD_QUIT, CMD_BY_NAME, CMD_BY_SIZE, CMD_PAPER, CMD_INK,
    CMD_GAMES, CMD_RUN, CMD_RUN_SETUP, CMD_RESCAN, CMD_RUN_ANYWAY,
    CMD_EDIT_FILE, CMD_SAVE, CMD_SAVE_CLOSE, CMD_CLOSE_NOW, CMD_SAVE_QUIT, CMD_QUIT_NOW, CMD_UP,
    CMD_SAVER_NOW, CMD_RUN_DIALOG, CMD_DOS_PROMPT, CMD_RENAME, CMD_MOVE
};
#define CMD_SAVER_PICK 300 /* + a saver's index + 2: SAVER_RANDOM is 300, SAVER_OFF 301 */
#define CMD_SAVER_DELAY 400 /* + minutes */
#define CMD_COLUMN 500 /* + COL_...: show or hide a Games window column */

static MenuItem system_items[] = {
    { "About PetiteGUI...", NULL, 0, CMD_ABOUT },
};
enum { /* file_items' places */
    FI_OPEN, FI_OPEN_TEXT, FI_RUN, FI_PROMPT, FI_INFO, FI_SEP1, FI_NEW_FOLDER, FI_RENAME, FI_COPY,
    FI_MOVE, FI_DELETE, FI_SEP2, FI_SAVE, FI_CLOSE, FI_QUIT
};
static MenuItem file_items[] = {
    { "Open", "Ctrl+O", 0, CMD_OPEN },
    { "Open as text", "Ctrl+E", 0, CMD_EDIT_FILE },
    { "Run...", "Ctrl+R", 0, CMD_RUN_DIALOG },
    { "DOS prompt", "Ctrl+D", 0, CMD_DOS_PROMPT },
    { "Info...", "Ctrl+I", 0, CMD_INFO },
    { "", NULL, MI_SEP, 0 },
    { "New folder...", "F7", 0, CMD_NEW_FOLDER },
    { "Rename...", "F2", 0, CMD_RENAME },
    { "Copy...", "F5", 0, CMD_COPY },
    { "Move...", "F6", 0, CMD_MOVE },
    { "Delete...", "Del", 0, CMD_DELETE },
    { "", NULL, MI_SEP, 0 },
    { "Save", "Ctrl+S", 0, CMD_SAVE },
    { "Close", "Ctrl+W", 0, CMD_CLOSE },
    { "Quit", "Ctrl+Q", 0, CMD_QUIT },
};
/* The right-click menus: on a file, and on a game. */
enum { FC_OPEN, FC_OPEN_TEXT, FC_RUN, FC_INFO, FC_SEP, FC_RENAME, FC_COPY, FC_MOVE, FC_DELETE };
static MenuItem file_context[] = {
    { "Open", "Enter", 0, CMD_OPEN },
    { "Open as text", "Ctrl+E", 0, CMD_EDIT_FILE },
    { "Run...", "Ctrl+R", 0, CMD_RUN_DIALOG },
    { "Info...", "Ctrl+I", 0, CMD_INFO },
    { "", NULL, MI_SEP, 0 },
    { "Rename...", "F2", 0, CMD_RENAME },
    { "Copy...", "F5", 0, CMD_COPY },
    { "Move...", "F6", 0, CMD_MOVE },
    { "Delete...", "Del", 0, CMD_DELETE },
};
static MenuItem game_context[] = {
    { "Run", "Enter", 0, CMD_RUN },
    { "Run setup", NULL, 0, CMD_RUN_SETUP },
    { "Game info...", "Ctrl+I", 0, CMD_INFO },
};
static MenuItem view_items[] = {
    { "By name", NULL, MI_CHECK, CMD_BY_NAME },
    { "By size", NULL, 0, CMD_BY_SIZE },
};
static MenuItem games_items[] = {
    { "Show games", "Ctrl+G", 0, CMD_GAMES },
    { "", NULL, MI_SEP, 0 },
    { "Run", "Enter", 0, CMD_RUN },
    { "Run setup", NULL, 0, CMD_RUN_SETUP },
    { "Game info...", "Ctrl+I", 0, CMD_INFO },
    { "", NULL, MI_SEP, 0 },
    { "Rescan", NULL, 0, CMD_RESCAN },
    { "", NULL, MI_SEP, 0 },
    { "Year", NULL, 0, CMD_COLUMN + COL_YEAR }, /* the columns, in gamecols.c's order */
    { "Genre", NULL, 0, CMD_COLUMN + COL_GENRE },
    { "Developer", NULL, 0, CMD_COLUMN + COL_DEVELOPER },
    { "Type", NULL, 0, CMD_COLUMN + COL_TYPE },
    { "Video", NULL, 0, CMD_COLUMN + COL_VIDEO },
    { "Sound", NULL, 0, CMD_COLUMN + COL_SOUND },
    { "Memory needed", NULL, 0, CMD_COLUMN + COL_MEMORY },
    { "Times played", NULL, 0, CMD_COLUMN + COL_PLAYS },
    { "Last played", NULL, 0, CMD_COLUMN + COL_LAST },
    { "Time played", NULL, 0, CMD_COLUMN + COL_TIME },
};
#define GAMES_FIRST_COLUMN 8 /* the index of "Year" in games_items */
static MenuItem saver_items[] = { /* in the order of savers[] in saver.c */
    { "Start now", NULL, 0, CMD_SAVER_NOW },
    { "", NULL, MI_SEP, 0 },
    { "Off", NULL, 0, CMD_SAVER_PICK + 1 },
    { "Random", NULL, 0, CMD_SAVER_PICK },
    { "Colour cycle", NULL, 0, CMD_SAVER_PICK + 2 },
    { "Blank", NULL, 0, CMD_SAVER_PICK + 3 },
    { "Starfield", NULL, 0, CMD_SAVER_PICK + 4 },
    { "Lines", NULL, 0, CMD_SAVER_PICK + 5 },
    { "Maze", NULL, 0, CMD_SAVER_PICK + 6 },
    { "Melt", NULL, 0, CMD_SAVER_PICK + 7 },
    { "Life", NULL, 0, CMD_SAVER_PICK + 8 },
    { "Game showcase", NULL, 0, CMD_SAVER_PICK + 9 },
    { "Clock", NULL, 0, CMD_SAVER_PICK + 10 },
    { "", NULL, MI_SEP, 0 },
    { "After 1 minute", NULL, 0, CMD_SAVER_DELAY + 1 },
    { "After 5 minutes", NULL, 0, CMD_SAVER_DELAY + 5 },
    { "After 15 minutes", NULL, 0, CMD_SAVER_DELAY + 15 },
};
static MenuItem options_items[] = {
    { "Paper (CGA)", NULL, 0, CMD_PAPER },
    { "Ink (EGA)", NULL, 0, CMD_INK },
};
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define FILE_MENU 1
#define FILE_DELETE FI_DELETE

/* ---- Setting up ---- */

static Row dir_rows[DIR_WINS][MAX_ROWS];

/* Sort the directory windows, keeping each one's selected row selected. */
static void sort_windows(void)
{
    int i, k;
    char name[13];

    for (i = 0; i < NUM_WINS; i++) {
        Win *w = &A.win[i];

        if (w->kind != WIN_FILES)
            continue;
        name[0] = '\0';
        if (w->sel >= 0)
            strcpy(name, w->rows[w->sel].name);
        files_sort(w, A.sort_by_size);
        for (k = 0; k < w->nrows; k++)
            if (strcmp(w->rows[k].name, name) == 0)
                w->sel = k;
    }
}

/* An icon for each drive, then the games, in columns from the right; the trash below. */
static void layout_icons(void)
{
    Drive d[MAX_DRIVES];
    int n = drives_find(d), i, k = 0, col = 0, row = 0;
    Icon *ic;

    for (i = 0; i < n && k < MAX_ICONS - 2; i++, k++) {
        ic = &A.icon[k];
        sprintf(ic->label, "%c:", d[i].letter);
        ic->drive = d[i].letter;
        ic->kind = d[i].kind == DRIVE_FLOPPY ? ICON_FLOPPY : d[i].kind == DRIVE_CDROM ? ICON_CDROM :
                   d[i].kind == DRIVE_REMOTE ? ICON_NETWORK : ICON_DISK;
    }
    strcpy(A.icon[k].label, "Games");
    A.icon[k].kind = ICON_GAMES;
    A.icon[k++].drive = 0;
    for (i = 0; i < k; i++) {
        A.icon[i].x = SCREEN_W - 44 - col * 56;
        A.icon[i].y = MENU_H + 12 + row * 36;
        if (++row >= (col == 0 ? 3 : 4)) { /* the first column leaves room for the trash */
            row = 0;
            col++;
        }
    }
    strcpy(A.icon[k].label, "Trash");
    A.icon[k].kind = ICON_TRASH;
    A.icon[k].drive = 0;
    A.icon[k].x = SCREEN_W - 44;
    A.icon[k].y = SCREEN_H - 42;
    A.nicons = k + 1;
    A.sel_icon = -1;
}

/* The first hard disk's letter (or the first drive's) */
static char first_drive(void)
{
    int i;

    for (i = 0; i < A.nicons; i++)
        if (A.icon[i].kind == ICON_DISK)
            return A.icon[i].drive;
    return A.icon[0].drive ? A.icon[0].drive : 'C';
}

static void app_init(void)
{
    Win *c = &A.win[1], *g = &A.win[GAMES_WIN], *e = &A.win[EDIT_WIN];
    static const Rect dir_rects[DIR_WINS] = {
        { 24, MENU_H + 14, 264, 100 }, { 176, MENU_H + 30, 336, 122 },
        { 64, MENU_H + 46, 336, 122 }, { 104, MENU_H + 22, 336, 122 }
    };
    static const char *const titles[NUM_MENUS] = { "System", "File", "View", "Games", "Saver", "Options" };
    MenuItem *items[NUM_MENUS];
    int counts[NUM_MENUS], i, x = 16;

    memset(&A, 0, sizeof A);
    /* Four folder windows, closed until a drive is opened; the first hard disk's is open */
    for (i = 0; i < DIR_WINS; i++) {
        A.win[i].r = dir_rects[i];
        A.win[i].title = A.win[i].path;
        A.win[i].rows = dir_rows[i];
        A.win[i].cap = MAX_ROWS;
        A.win[i].sel = -1;
    }
    layout_icons();
    sprintf(c->path, "%c:\\", first_drive());
    c->open = 1;

    g->kind = WIN_GAMES;
    g->r = rect_make(40, MENU_H + 20, 432, 142);
    g->title = "Games";
    g->sel = -1;

    e->kind = WIN_EDIT;
    e->r = rect_make(8, MENU_H + 12, 560, 172);
    e->title = "";
    e->sel = -1;

    A.z[0] = EDIT_WIN; /* closed until they're opened, then in front */
    A.z[1] = GAMES_WIN;
    A.z[2] = 3;
    A.z[3] = 2;
    A.z[4] = 0;
    A.z[5] = 1;


    items[0] = system_items;
    counts[0] = COUNT(system_items);
    items[1] = file_items;
    counts[1] = COUNT(file_items);
    items[2] = view_items;
    counts[2] = COUNT(view_items);
    items[3] = games_items;
    counts[3] = COUNT(games_items);
    items[4] = saver_items;
    counts[4] = COUNT(saver_items);
    items[5] = options_items;
    counts[5] = COUNT(options_items);
    for (i = 0; i < NUM_MENUS; i++) {
        A.menu[i].title = titles[i];
        A.menu[i].items = items[i];
        A.menu[i].n = counts[i];
        A.menu[i].x = x;
        x += gfx_text_w(titles[i]) + 16;
    }
    A.menu[CONTEXT_MENU].title = "";
    A.menu[CONTEXT_MENU].items = file_context;
    A.menu[CONTEXT_MENU].n = COUNT(file_context);
    A.open_menu = A.hot = -1;
    strcpy(A.clock, "10:42");
    files_load(c, 0);
}

/* Check marks and greyed items that follow the state. */
static void update_menus(void)
{
    int f = front_window(), game = f == GAMES_WIN && A.win[GAMES_WIN].sel >= 0;
    const Win *w = f >= 0 ? &A.win[f] : NULL;
    const Row *row = w != NULL && w->kind == WIN_FILES && w->sel >= 0 && w->sel < w->nrows ? &w->rows[w->sel] : NULL;
    int file = row != NULL && !row->dir, up = row != NULL && strcmp(row->name, "..") == 0;

    file_items[FI_OPEN_TEXT].flags = file_context[FC_OPEN_TEXT].flags = (uint8_t)(file ? 0 : MI_OFF);
    {
        uint8_t item = (uint8_t)(row != NULL && !up ? 0 : MI_OFF); /* a file or folder, not ".." */
        uint8_t here = (uint8_t)(w != NULL && w->kind == WIN_FILES && !w->not_ready ? 0 : MI_OFF);

        file_items[FI_RENAME].flags = file_context[FC_RENAME].flags = item;
        file_items[FI_COPY].flags = file_context[FC_COPY].flags = item;
        file_items[FI_MOVE].flags = file_context[FC_MOVE].flags = item;
        file_items[FI_DELETE].flags = file_context[FC_DELETE].flags = (uint8_t)(f == GAMES_WIN ? 0 : item);
        file_items[FI_NEW_FOLDER].flags = here;
    }
    file_context[FC_INFO].flags = (uint8_t)(up ? MI_OFF : 0);
    file_context[FC_RUN].flags = (uint8_t)(file && path_is_program(row->name) ? 0 : MI_OFF);
    file_items[FI_SAVE].flags = (uint8_t)(f == EDIT_WIN && E.modified ? 0 : MI_OFF);
    game_context[1].flags = (uint8_t)(game ? 0 : MI_OFF);
    {
        int c;

        for (c = 0; c < NUM_COLS; c++)
            games_items[GAMES_FIRST_COLUMN + c].flags = (uint8_t)(S.game_cols & (1u << c) ? MI_CHECK : 0);
    }
    {
        int k;

        for (k = 2; k < COUNT(saver_items); k++) {
            int cmd = saver_items[k].cmd;

            if (saver_items[k].flags & MI_SEP)
                continue;
            if (cmd >= CMD_SAVER_DELAY)
                saver_items[k].flags = (uint8_t)(cmd - CMD_SAVER_DELAY == S.saver_delay ? MI_CHECK : 0);
            else
                saver_items[k].flags = (uint8_t)(cmd - CMD_SAVER_PICK - 2 == S.saver ? MI_CHECK : 0);
        }
    }

    games_items[2].flags = (uint8_t)(game ? 0 : MI_OFF);
    games_items[3].flags = (uint8_t)(game ? 0 : MI_OFF);
    games_items[4].flags = (uint8_t)(game ? 0 : MI_OFF);
    view_items[0].flags = (uint8_t)(A.sort_by_size ? 0 : MI_CHECK);
    view_items[1].flags = (uint8_t)(A.sort_by_size ? MI_CHECK : 0);
    options_items[0].flags = (uint8_t)(A.theme == &theme_paper ? MI_CHECK : 0);
    options_items[1].flags = (uint8_t)(!A.has_ega ? MI_OFF : (A.theme == &theme_ink ? MI_CHECK : 0));
}

static int detect_ega(void)
{
    union REGS r;

    r.h.ah = 0x12; /* alternate select: get EGA information */
    r.h.bl = 0x10;
    int86(0x10, &r, &r);
    return r.h.bl != 0x10;
}

static int video_mode(void)
{
    union REGS r;

    r.h.ah = 0x0F;
    int86(0x10, &r, &r);
    return r.h.al;
}

static void set_video_mode(int m)
{
    union REGS r;

    r.h.ah = 0;
    r.h.al = (unsigned char)m;
    int86(0x10, &r, &r);
}

static unsigned long ticks(void)
{
    return *(volatile unsigned long __far *)MK_FP(0x40, 0x6C); /* 18.2 a second */
}

/* DOS asks "Abort, Retry, Fail?" over the screen when a drive can't be read (A: with no
 * disk). While the desktop is up, this answers Fail, and the window says the drive isn't
 * ready. It's removed while a game runs: it points into PetiteGUI's memory. */
static void (__interrupt __far *old_int24)(void);

static void __interrupt __far on_critical_error(union INTPACK r)
{
    r.h.al = 3; /* fail the call */
}

static void critical_errors(int ours)
{
    if (ours && old_int24 == NULL) {
        old_int24 = _dos_getvect(0x24);
        _dos_setvect(0x24, on_critical_error);
    } else if (!ours && old_int24 != NULL) {
        _dos_setvect(0x24, old_int24);
        old_int24 = NULL;
    }
}

static void read_clock(char *out)
{
    union REGS r;

    r.h.ah = 0x2C;
    int86(0x21, &r, &r);
    sprintf(out, "%02d:%02d", r.h.ch, r.h.cl);
}

/* ---- Redrawing ---- */

static const Rect FULL = { 0, 0, SCREEN_W, SCREEN_H };

static void redraw(const Rect *area)
{
    mouse_hide();
    draw_desktop(area);
    mouse_show();
}

static void redraw_window(int i)
{
    Rect r = window_bounds(i);

    redraw(&r);
}

/* ---- Menus ---- */

static void open_menu(int m)
{
    Rect r;

    update_menus();
    A.open_menu = m;
    A.hot = -1;
    r = menu_rect(m);
    mouse_hide();
    A.menu_save = gfx_save(&r);
    draw_menubar();
    draw_menu(m);
    mouse_show();
}

static void close_menu(void)
{
    Rect r;
    int m = A.open_menu;

    if (m < 0)
        return;
    r = menu_rect(m);
    A.open_menu = A.hot = -1;
    mouse_hide();
    if (A.menu_save != NULL)
        gfx_restore(&r, A.menu_save);
    else
        draw_desktop(&r);
    A.menu_save = NULL;
    draw_menubar();
    mouse_show();
}

static void set_hot(int hot)
{
    if (hot == A.hot)
        return;
    A.hot = hot;
    mouse_hide();
    draw_menu(A.open_menu);
    mouse_show();
}

/* ---- Alerts ---- */

static void show_alert(const char *line1, const char *line2, const char *b0, const char *b1, int cmd)
{
    Rect r;

    strncpy(A.alert.line1, line1, sizeof A.alert.line1 - 1);
    strncpy(A.alert.line2, line2, sizeof A.alert.line2 - 1);
    A.alert.button[0] = b0;
    A.alert.button[1] = b1;
    A.alert.nbuttons = b0 == NULL ? 0 : (b1 ? 2 : 1);
    A.alert.cmd = cmd;
    A.alert.cmd_alt = CMD_NONE;
    alert_layout();
    r = A.alert.r;
    r.w += SHADOW_X;
    r.h += SHADOW_Y;
    A.alert_on = 1;
    mouse_hide();
    A.alert_save = gfx_save(&r);
    draw_alert();
    mouse_show();
}

/* An alert whose first button runs a command too: "Don't save" / "Save". */
static void show_alert2(const char *line1, const char *line2, const char *b0, const char *b1,
                        int cmd, int cmd_alt)
{
    show_alert(line1, line2, b0, b1, cmd);
    A.alert.cmd_alt = cmd_alt;
}

static void run(int cmd);
static void launch_selected(int setup, int checked);
static void open_row(int i);
static void sort_games(int col);
static int drag_item(int i, int k, int mx, int my);

/* Close the alert: button is the one chosen, or -1 (Esc) for neither. The last button runs
 * the alert's command, and the first of two its other one. */
static void close_alert(int button)
{
    Rect r = A.alert.r;

    r.w += SHADOW_X;
    r.h += SHADOW_Y;
    A.alert_on = 0;
    mouse_hide();
    if (A.alert_save != NULL)
        gfx_restore(&r, A.alert_save);
    else
        draw_desktop(&r);
    A.alert_save = NULL;
    mouse_show();
    if (button >= 0 && button == A.alert.nbuttons - 1)
        run(A.alert.cmd);
    else if (button == 0 && A.alert.nbuttons == 2)
        run(A.alert.cmd_alt);
}

/* ---- Windows ---- */

static void bring_to_front(int i)
{
    int k, old = front_window();

    if (old == i)
        return;
    for (k = 0; A.z[k] != i; k++)
        ;
    for (; k < NUM_WINS - 1; k++)
        A.z[k] = A.z[k + 1];
    A.z[NUM_WINS - 1] = i;
    if (old >= 0)
        redraw_window(old); /* its title bar goes inactive */
    redraw_window(i);
}

static void open_window(int i)
{
    if (!A.win[i].open) {
        A.win[i].open = 1;
        bring_to_front(i);
        redraw_window(i);
    } else {
        bring_to_front(i);
    }
}

static void close_window(int i)
{
    int f;

    A.win[i].open = 0;
    redraw_window(i);
    f = front_window();
    if (f >= 0)
        redraw_window(f); /* now active */
}

static void select_row(int i, int k)
{
    Win *w = &A.win[i];
    WinParts p;

    if (k < 0 || k >= w->nrows)
        return;
    w->sel = k;
    window_parts(i, &p);
    if (k < w->top)
        w->top = k;
    if (k >= w->top + p.visible)
        w->top = k - p.visible + 1;
    redraw_window(i);
}

static void scroll_by(int i, int d)
{
    Win *w = &A.win[i];
    WinParts p;
    int top;

    window_parts(i, &p);
    if (w->kind == WIN_EDIT) {
        edit_scroll(d);
        redraw_window(i);
        return;
    }
    top = w->top + d;
    if (top > w->nrows - p.visible)
        top = w->nrows - p.visible;
    if (top < 0)
        top = 0;
    if (top != w->top) {
        w->top = top;
        redraw_window(i);
    }
}

static void wait_release(int *x, int *y)
{
    int b;

    do
        mouse_read(x, y, &b);
    while (b & 1);
}

static void editor_view(void);

/* After a window changes size: keep its list scrolled within range, and the editor's view
 * the window's size. */
static void after_resize(int i)
{
    Win *w = &A.win[i];
    WinParts p;

    window_parts(i, &p);
    if (w->kind == WIN_EDIT) {
        if (E.open)
            editor_view();
        return;
    }
    if (w->top > w->nrows - p.visible)
        w->top = w->nrows - p.visible;
    if (w->top < 0)
        w->top = 0;
}

static void set_window_rect(int i, Rect r)
{
    Rect old = window_bounds(i);

    A.win[i].r = r;
    after_resize(i);
    redraw(&old);
    redraw_window(i);
}

/* Drag the size box: an outline follows the pointer, and the window takes its size when
 * the button is let go. The top-left corner stays where it is. */
static void resize_window(int i, int mx, int my)
{
    Win *w = &A.win[i];
    Rect cur = w->r, next;
    int ox = w->r.x + w->r.w - mx, oy = w->r.y + w->r.h - my, x, y, b;

    mouse_hide();
    gfx_xor_frame(&cur);
    mouse_show();
    for (;;) {
        mouse_read(&x, &y, &b);
        next = rect_resize(&w->r, x + ox - w->r.x, y + oy - w->r.y, MIN_WIN_W, MIN_WIN_H,
                           SCREEN_W - SHADOW_X, SCREEN_H - SHADOW_Y);
        if (next.w != cur.w || next.h != cur.h) {
            mouse_hide();
            gfx_xor_frame(&cur);
            gfx_xor_frame(&next);
            mouse_show();
            cur = next;
        }
        if (!(b & 1))
            break;
    }
    mouse_hide();
    gfx_xor_frame(&cur);
    mouse_show();
    if (cur.w != w->r.w || cur.h != w->r.h) {
        w->zoomed = 0;
        set_window_rect(i, cur);
    }
}

/* Fill the screen, or go back to the size it was. */
static void zoom_window(int i)
{
    Win *w = &A.win[i];

    if (w->zoomed) {
        w->zoomed = 0;
        set_window_rect(i, w->normal);
    } else {
        w->normal = w->r;
        w->zoomed = 1;
        set_window_rect(i, rect_make(0, MENU_H + 1, SCREEN_W - 8, SCREEN_H - MENU_H - 1 - SHADOW_Y));
    }
}

/* Drag an outline of the window; move it when the button is let go. Returns whether it
 * moved. */
static int drag_window(int i, int mx, int my)
{
    Win *w = &A.win[i];
    Rect cur = w->r, next, old;
    int dx = mx - cur.x, dy = my - cur.y, x, y, b;

    mouse_hide();
    gfx_xor_frame(&cur);
    mouse_show();
    for (;;) {
        mouse_read(&x, &y, &b);
        next = cur;
        next.x = SNAP8(x - dx);
        next.y = y - dy;
        if (next.y < MENU_H + 1)
            next.y = MENU_H + 1;
        if (next.y > SCREEN_H - TITLE_H - 2)
            next.y = SCREEN_H - TITLE_H - 2;
        if (next.x < 40 - next.w)
            next.x = SNAP8_UP(40 - next.w);
        if (next.x > SCREEN_W - 40)
            next.x = SCREEN_W - 40;
        if (next.x != cur.x || next.y != cur.y) {
            mouse_hide();
            gfx_xor_frame(&cur);
            gfx_xor_frame(&next);
            mouse_show();
            cur = next;
        }
        if (!(b & 1))
            break;
    }
    mouse_hide();
    gfx_xor_frame(&cur);
    mouse_show();
    if (cur.x != w->r.x || cur.y != w->r.y) {
        old = window_bounds(i);
        w->r = cur;
        w->zoomed = 0;
        redraw(&old);
        redraw_window(i);
        return 1;
    }
    return 0;
}

/* Repeat an arrow while it's held: once, then after a pause, then about 9 a second. */
static void hold_arrow(int i, int d)
{
    unsigned long next = ticks() + 6;
    int x, y, b;

    scroll_by(i, d);
    for (;;) {
        mouse_read(&x, &y, &b);
        if (!(b & 1))
            break;
        if (ticks() >= next) {
            scroll_by(i, d);
            next = ticks() + 2;
        }
    }
}

static int window_at(int x, int y)
{
    int k, i;
    Rect r;

    for (k = NUM_WINS - 1; k >= 0; k--) {
        i = A.z[k];
        r = A.win[i].r;
        if (A.win[i].open && rect_contains(&r, x, y))
            return i;
    }
    return -1;
}

static void press_window(int i, int x, int y)
{
    WinParts p;
    Win *w = &A.win[i];
    int k, ux, uy;

    if (front_window() != i) {
        bring_to_front(i);
        window_parts(i, &p);
        if (rect_contains(&p.title, x, y))
            drag_window(i, x, y);
        else
            wait_release(&ux, &uy);
        return;
    }
    window_parts(i, &p);
    if (rect_contains(&p.close, x, y)) {
        wait_release(&ux, &uy);
        if (rect_contains(&p.close, ux, uy))
            run(CMD_CLOSE); /* the editor asks to save first */
    } else if (rect_contains(&p.title, x, y)) {
        static unsigned long last_time;
        static int last_win = -1;
        int twice = i == last_win && ticks() - last_time < 9;

        if (!drag_window(i, x, y) && twice) { /* a double-click on the title bar zooms */
            last_win = -1;
            zoom_window(i);
        } else {
            last_win = i;
            last_time = ticks();
        }
    } else if (rect_contains(&p.grow, x, y)) {
        resize_window(i, x, y);
    } else if (w->kind == WIN_GAMES && rect_contains(&p.header, x, y)) {
        k = games_heading_at(&p, x);
        wait_release(&ux, &uy);
        if (k != -2)
            sort_games(k);
    } else if (rect_contains(&p.up, x, y)) {
        hold_arrow(i, -1);
    } else if (rect_contains(&p.down, x, y)) {
        hold_arrow(i, 1);
    } else if (rect_contains(&p.track, x, y)) {
        scroll_by(i, y < p.track.y + p.track.h / 2 ? -p.visible : p.visible);
        wait_release(&ux, &uy);
    } else if (rect_contains(&p.list, x, y) && w->kind == WIN_EDIT) {
        edit_click((y - p.list.y - 1) / ROW_H, (x - p.list.x - 4) / 8);
        redraw_window(i);
        wait_release(&ux, &uy);
    } else if (rect_contains(&p.list, x, y)) {
        static unsigned long last_time;
        static int last_win = -1, last_row = -1;

        k = w->top + (y - p.list.y - 1) / ROW_H;
        if (k < w->nrows) {
            int twice = i == last_win && k == last_row && ticks() - last_time < 9;

            select_row(i, k);
            if (w->kind == WIN_FILES && drag_item(i, k, x, y)) {
                last_win = -1;
                return;
            }
            wait_release(&ux, &uy);
            last_win = i;
            last_row = k;
            last_time = ticks();
            if (twice) {
                last_row = -1;
                if (w->kind == WIN_GAMES)
                    launch_selected(0, 0);
                else
                    open_row(i);
            }
        } else {
            wait_release(&ux, &uy);
        }
    } else {
        wait_release(&ux, &uy);
    }
}

/* ---- Icons ---- */

static void select_icon(int i)
{
    int old = A.sel_icon;
    Rect r;

    if (old == i)
        return;
    A.sel_icon = i;
    if (old >= 0) {
        r = icon_bounds(old);
        redraw(&r);
    }
    if (i >= 0) {
        r = icon_bounds(i);
        redraw(&r);
    }
}

static void open_games(void);

/* A drive's window: the one already showing it, else a closed one, else the one furthest
 * back, which moves to this drive. */
static void open_drive(char letter)
{
    int i, k, use = -1;
    Win *w;

    for (i = 0; i < DIR_WINS; i++) {
        if (A.win[i].open && (A.win[i].path[0] & ~0x20) == letter) {
            bring_to_front(i);
            return;
        }
        if (!A.win[i].open && use < 0)
            use = i;
    }
    if (use < 0) {
        for (k = 0; k < NUM_WINS && A.z[k] >= DIR_WINS; k++)
            ;
        use = A.z[k];
        close_window(use);
    }
    w = &A.win[use];
    sprintf(w->path, "%c:\\", letter);
    files_load(w, A.sort_by_size);
    w->sel = -1;
    w->top = 0;
    open_window(use);
}

static void open_icon(int i)
{
    if (A.icon[i].kind == ICON_GAMES)
        open_games();
    else if (A.icon[i].drive)
        open_drive(A.icon[i].drive);
    else
        show_alert("The trash is empty.", "", "OK", NULL, CMD_NONE);
}

/* ---- Directory windows ---- */

/* Into a folder, or up with "..": the folder we came out of stays selected. */
static void enter_dir(int i, const char *name)
{
    Win *w = &A.win[i];
    char child[13];
    int k;

    child[0] = '\0';
    if (strcmp(name, "..") == 0) {
        if (!path_parent(w->path, child, sizeof child))
            return;
    } else {
        if (strlen(w->path) + strlen(name) + 2 > sizeof w->path) {
            show_alert("That folder is too deep", "for PetiteGUI to open.", "OK", NULL, CMD_NONE);
            return;
        }
        strcat(w->path, name);
        strcat(w->path, "\\");
    }
    files_load(w, A.sort_by_size);
    w->sel = w->nrows > 0 ? 0 : -1;
    w->top = 0;
    for (k = 0; child[0] && k < w->nrows; k++)
        if (strcmp(w->rows[k].name, child) == 0)
            w->sel = k;
    if (w->sel > 0)
        select_row(i, w->sel); /* scrolls to it, and redraws */
    else
        redraw_window(i);
}

static void open_in_editor(const char *path);
static void run_program(const char *dir, const char *prog, const char *args, int shell,
                        const char *banner, int wait);

/* Open a row: a folder goes in, a text file into the editor, anything else shows its info. */
static void open_row(int i)
{
    Win *w = &A.win[i];
    char path[96];
    const Row *row;

    if (w->kind != WIN_FILES || w->sel < 0 || w->sel >= w->nrows)
        return;
    row = &w->rows[w->sel];
    if (row->dir) {
        enter_dir(i, row->name);
    } else if (path_is_text(row->name)) {
        sprintf(path, "%s%s", w->path, row->name);
        open_in_editor(path);
    } else if (path_is_program(row->name)) {
        sprintf(path, "%s%s", w->path, row->name);
        if (gm_is_batch(path)) /* DOS can't EXEC a batch file: COMMAND.COM runs it */
            run_program(w->path, "", path, 1, NULL, 0);
        else
            run_program(w->path, path, "", 0, NULL, 0);
    } else {
        run(CMD_INFO);
    }
}

static void reload_dir(int i)
{
    Win *w = &A.win[i];
    int sel = w->sel;

    files_load(w, A.sort_by_size);
    if (sel >= w->nrows)
        sel = w->nrows - 1;
    w->sel = sel;
    if (w->top > 0 && w->top >= w->nrows)
        w->top = 0;
    redraw_window(i);
}

/* ---- File operations ---- */

static int ask_choice(const char *line1, const char *line2, const char *no, const char *yes);
static int ask_text(const char *prompt, char *text, int size);

/* The selected row's full path in folder window i ("C:\GAMES\DOOM"); zero if there isn't
 * one, or it's "..". */
static int selected_path(int i, char *out, int size)
{
    const Win *w = &A.win[i];

    if (i < 0 || i >= DIR_WINS || w->sel < 0 || w->sel >= w->nrows || strcmp(w->rows[w->sel].name, "..") == 0)
        return 0;
    return path_join(out, size, w->path, w->rows[w->sel].name);
}

static const char *base_name(const char *path)
{
    const char *s = strrchr(path, '\\');

    return s != NULL ? s + 1 : path;
}

/* Read every open folder window again, keeping each one's selection by name. */
static void reload_dirs(void)
{
    char name[13];
    int i, k;

    for (i = 0; i < DIR_WINS; i++) {
        Win *w = &A.win[i];

        if (!w->open)
            continue;
        name[0] = '\0';
        if (w->sel >= 0 && w->sel < w->nrows)
            strcpy(name, w->rows[w->sel].name);
        files_load(w, A.sort_by_size);
        w->sel = w->nrows > 0 ? 0 : -1;
        for (k = 0; name[0] && k < w->nrows; k++)
            if (stricmp(w->rows[k].name, name) == 0)
                w->sel = k;
        if (w->top > w->nrows - 1)
            w->top = 0;
        redraw_window(i);
    }
}

/* Select a name in folder window i, scrolling to it. */
static void select_name(int i, const char *name)
{
    int k;

    for (k = 0; k < A.win[i].nrows; k++)
        if (stricmp(A.win[i].rows[k].name, name) == 0) {
            select_row(i, k);
            return;
        }
}

/* The progress box: an alert with no buttons, its lines changed as each file goes. */
static long progress_total;
static const char *progress_verb;

static int show_progress(const char *name, long done)
{
    char line[40];
    size_t n = strlen(name);

    if (kbhit() && getch() == 27)
        return 0;
    sprintf(line, "%s %ld of %ld file%s...", progress_verb, done + 1, progress_total, progress_total == 1 ? "" : "s");
    sprintf(A.alert.line1, "%-30s", line);
    if (n > 30)
        sprintf(line, "...%s", name + n - 27);
    else
        strcpy(line, name);
    sprintf(A.alert.line2, "%-30s", line);
    mouse_hide();
    draw_alert();
    mouse_show();
    return 1;
}

static void progress_open(const char *verb, const char *src)
{
    long files = 0, bytes = 0;
    char blank[32];

    fo_count(src, &files, &bytes);
    progress_total = files > 0 ? files : 1;
    progress_verb = verb;
    sprintf(blank, "%-30s", "");
    show_alert(blank, blank, NULL, NULL, CMD_NONE);
}

/* If the editor has a file that's moved or gone, follow it or close. */
static void editor_follow(const char *from, const char *to)
{
    if (!E.open || stricmp(E.path, from) != 0)
        return;
    if (to != NULL) {
        strncpy(E.path, to, sizeof E.path - 1);
        strncpy(E.name, base_name(to), sizeof E.name - 1);
        A.win[EDIT_WIN].title = E.path;
        redraw_window(EDIT_WIN);
    } else if (!E.modified) {
        edit_close();
        close_window(EDIT_WIN);
    }
}

/* Copy or move src to dst (full paths), asking before replacing, showing progress. */
static void transfer(const char *src, const char *dst, int move)
{
    char line[48];
    int rc, replace = 0;

    if (stricmp(src, dst) == 0)
        return;
    if (move && E.open && E.modified && path_inside(E.path, src)) {
        show_alert("It's open with changes.", "Save or close it first.", "OK", NULL, CMD_NONE);
        return;
    }
    if (fo_exists(dst)) {
        sprintf(line, "There's a %.13s there already.", base_name(dst));
        if (!ask_choice(line, fo_is_dir(dst) ? "Copy into it?" : "Replace it?", "Cancel", "Replace"))
            return;
        replace = 1;
    }
    progress_open(move ? "Moving" : "Copying", src);
    rc = move ? fo_move(src, dst, replace, show_progress) : fo_copy(src, dst, replace, show_progress);
    close_alert(-1);
    if (move && rc == FO_OK)
        editor_follow(src, dst);
    reload_dirs();
    if (rc != FO_OK) {
        sprintf(line, "Couldn't %s %.13s:", move ? "move" : "copy", base_name(src));
        show_alert(line, fo_error_text(rc), "OK", NULL, CMD_NONE);
    }
}

/* Where Copy... and Move... suggest: the folder in the next window back, else the same one. */
static const char *other_dir(int from)
{
    int k;

    for (k = NUM_WINS - 1; k >= 0; k--) {
        int i = A.z[k];

        if (i < DIR_WINS && i != from && A.win[i].open && !A.win[i].not_ready)
            return A.win[i].path;
    }
    return A.win[from].path;
}

/* Copy... or Move...: ask where, then go. A folder that exists means into it; anything else
 * is the new path (a bare name, in the same folder). */
static void copy_or_move(int move)
{
    int f = front_window();
    char src[96], dest[FIELD_MAX + 1], dst[96], prompt[48];
    size_t n;

    if (!selected_path(f, src, sizeof src))
        return;
    strcpy(dest, other_dir(f));
    sprintf(prompt, "%s %.13s to:", move ? "Move" : "Copy", base_name(src));
    if (!ask_text(prompt, dest, sizeof dest) || dest[0] == '\0')
        return;
    n = strlen(dest);
    if (strchr(dest, ':') == NULL && dest[0] != '\\') { /* a bare name: in the same folder */
        char rel[FIELD_MAX + 1];

        strcpy(rel, dest);
        if (!path_join(dest, sizeof dest, A.win[f].path, rel))
            return;
        n = strlen(dest);
    }
    if (dest[n - 1] == '\\' || fo_is_dir(dest)) {
        if (dest[n - 1] != '\\' && n + 1 < sizeof dest)
            strcat(dest, "\\");
        if (!path_join(dst, sizeof dst, dest, base_name(src)))
            return;
    } else {
        if (!path_valid_name(base_name(dest))) {
            show_alert("That isn't a name DOS takes:", "8 characters, a dot, then 3.", "OK", NULL, CMD_NONE);
            return;
        }
        strncpy(dst, dest, sizeof dst - 1);
        dst[sizeof dst - 1] = '\0';
    }
    transfer(src, dst, move);
}

static void rename_item(void)
{
    int f = front_window();
    char src[96], name[FIELD_MAX + 1], dst[96], prompt[48];
    int rc;

    if (!selected_path(f, src, sizeof src))
        return;
    strcpy(name, base_name(src));
    sprintf(prompt, "Rename %.13s to:", name);
    if (!ask_text(prompt, name, sizeof name) || name[0] == '\0' || stricmp(name, base_name(src)) == 0)
        return;
    if (!path_valid_name(name)) {
        show_alert("That isn't a name DOS takes:", "8 characters, a dot, then 3.", "OK", NULL, CMD_NONE);
        return;
    }
    path_join(dst, sizeof dst, A.win[f].path, name);
    rc = fo_rename(src, dst);
    if (rc != FO_OK) {
        show_alert("Couldn't rename it:", fo_error_text(rc), "OK", NULL, CMD_NONE);
        return;
    }
    editor_follow(src, dst);
    reload_dirs();
    select_name(f, name);
}

static void new_folder(void)
{
    int f = front_window();
    char name[FIELD_MAX + 1], dst[96], prompt[48];
    int rc;

    if (f < 0 || f >= DIR_WINS || A.win[f].not_ready)
        return;
    name[0] = '\0';
    if (strlen(A.win[f].path) > 26)
        sprintf(prompt, "New folder in ...%s:", A.win[f].path + strlen(A.win[f].path) - 23);
    else
        sprintf(prompt, "New folder in %s:", A.win[f].path);
    if (!ask_text(prompt, name, sizeof name) || name[0] == '\0')
        return;
    if (!path_valid_name(name) || !path_join(dst, sizeof dst, A.win[f].path, name)) {
        show_alert("That isn't a name DOS takes:", "8 characters, a dot, then 3.", "OK", NULL, CMD_NONE);
        return;
    }
    rc = fo_mkdir(dst);
    if (rc != FO_OK) {
        show_alert("Couldn't make the folder:", fo_error_text(rc), "OK", NULL, CMD_NONE);
        return;
    }
    reload_dirs();
    select_name(f, name);
}

/* Delete what's selected, once it's been confirmed (CMD_DELETE asks). */
static void delete_selected(int f)
{
    char path[96], line[48];
    int rc;

    if (!selected_path(f, path, sizeof path))
        return;
    if (E.open && path_inside(E.path, path) && E.modified) {
        show_alert("It's open with changes.", "Save or close it first.", "OK", NULL, CMD_NONE);
        return;
    }
    progress_open("Deleting", path);
    rc = fo_delete(path, show_progress);
    close_alert(-1);
    if (E.open && path_inside(E.path, path))
        editor_follow(E.path, NULL);
    reload_dirs();
    if (rc != FO_OK) {
        sprintf(line, "Couldn't delete %.13s:", base_name(path));
        show_alert(line, fo_error_text(rc), "OK", NULL, CMD_NONE);
    }
}

/* Is the Ctrl key down? (The BIOS keeps the shift keys at 0040:0017; Ctrl is bit 2.) */
static int ctrl_down(void)
{
    return (*(volatile unsigned char __far *)MK_FP(0x40, 0x17) & 4) != 0;
}

/* Drag a file or folder from window i's row k: an outline follows the pointer; let go on
 * another folder window (or a folder in one) or a drive's icon to move it there (the same
 * drive) or copy it (another drive, or with Ctrl). Returns zero if the pointer never left
 * the row, so it was just a click. */
static int drag_item(int i, int k, int mx, int my)
{
    Win *w = &A.win[i];
    char src[96], dir[96], dst[96], child[13];
    Rect box, next;
    int x, y, b, t, n, dragging = 0;
    WinParts p;

    if (!selected_path(i, src, sizeof src))
        return 0;
    box = rect_make(mx - 40, my - 4, 80, 9);
    for (;;) {
        mouse_read(&x, &y, &b);
        if (!dragging && (abs(x - mx) > 6 || abs(y - my) > 3)) {
            dragging = 1;
            mouse_hide();
            gfx_xor_frame(&box);
            mouse_show();
        }
        if (dragging) {
            next = rect_make(x - 40, y - 4, 80, 9);
            if (next.x != box.x || next.y != box.y) {
                mouse_hide();
                gfx_xor_frame(&box);
                gfx_xor_frame(&next);
                mouse_show();
                box = next;
            }
        }
        if (!(b & 1))
            break;
    }
    if (!dragging)
        return 0;
    mouse_hide();
    gfx_xor_frame(&box);
    mouse_show();

    dir[0] = '\0';
    t = window_at(x, y);
    if (t >= 0 && t < DIR_WINS && !A.win[t].not_ready) {
        strcpy(dir, A.win[t].path);
        window_parts(t, &p);
        n = A.win[t].top + (y - p.list.y - 1) / ROW_H;
        if (rect_contains(&p.list, x, y) && n < A.win[t].nrows && A.win[t].rows[n].dir &&
            !(t == i && n == k)) { /* onto a folder in it */
            strcpy(child, A.win[t].rows[n].name);
            if (strcmp(child, "..") == 0)
                path_parent(dir, NULL, 0);
            else if (strlen(dir) + strlen(child) + 2 < sizeof dir)
                strcat(strcat(dir, child), "\\");
        }
    } else if (t < 0) {
        for (n = 0; n < A.nicons; n++) {
            Rect ib = icon_bounds(n);

            if (A.icon[n].drive && rect_contains(&ib, x, y))
                sprintf(dir, "%c:\\", A.icon[n].drive);
        }
    }
    if (dir[0] == '\0' || stricmp(dir, w->path) == 0 || !path_join(dst, sizeof dst, dir, base_name(src)))
        return 1;
    transfer(src, dst, path_same_drive(src, dst) && !ctrl_down());
    return 1;
}

/* ---- The editor ---- */

static void editor_view(void)
{
    WinParts p;

    window_parts(EDIT_WIN, &p);
    edit_set_view(p.visible, (p.list.w - 8) / 8);
}

static void open_in_editor(const char *path)
{
    char line[40];
    int rc;

    if (E.open && E.modified) {
        sprintf(line, "Save %s first.", E.name);
        show_alert(line, "It has changes not saved.", "OK", NULL, CMD_NONE);
        return;
    }
    if (E.open)
        edit_close();
    rc = edit_open(path);
    if (rc != EDIT_OK) {
        const char *slash = strrchr(path, '\\');

        sprintf(line, "Can't open %.24s:", slash ? slash + 1 : path);
        show_alert(line, rc == EDIT_TOO_BIG ? "it's over 60,000 bytes." :
                         rc == EDIT_NO_MEMORY ? "there isn't enough memory." : "it couldn't be read.",
                   "OK", NULL, CMD_NONE);
        if (A.win[EDIT_WIN].open)
            close_window(EDIT_WIN);
        return;
    }
    A.win[EDIT_WIN].title = E.path;
    editor_view();
    if (A.win[EDIT_WIN].open) {
        bring_to_front(EDIT_WIN);
        redraw_window(EDIT_WIN);
    } else {
        open_window(EDIT_WIN);
    }
}

static int save_editor(void)
{
    char line[40];
    int i;

    if (edit_save() != EDIT_OK) {
        sprintf(line, "Couldn't save %s.", E.name);
        show_alert(line, "Is the disk full or read-only?", "OK", NULL, CMD_NONE);
        return 0;
    }
    redraw_window(EDIT_WIN);
    for (i = 0; i < DIR_WINS; i++) /* its size changed */
        if (A.win[i].open)
            reload_dir(i);
    return 1;
}

static void close_editor(void)
{
    edit_close();
    close_window(EDIT_WIN);
}

/* Ask before losing changes: "Don't save" runs cmd_discard, "Save" cmd_save, Esc neither. */
static int ask_to_save(int cmd_save, int cmd_discard)
{
    char line[40];

    if (!E.open || !E.modified)
        return 0;
    sprintf(line, "Save changes to %s?", E.name);
    show_alert2(line, "", "Don't save", "Save", cmd_save, cmd_discard);
    return 1;
}

/* A key for the editor: redraw only the rows that changed, unless the view moved. */
static void editor_key(int ch, int ext)
{
    WinParts p;
    unsigned top = E.top_line, left = E.left, lines = E.lines;
    int old_row = (int)(E.cur_line - E.top_line), modified = E.modified, row, col;
    Rect r;

    if (edit_key(ch, ext) == EDIT_FULL) {
        show_alert("The file can't grow any more.", "Save it, or remove some text.", "OK", NULL, CMD_NONE);
        return;
    }
    window_parts(EDIT_WIN, &p);
    if (E.top_line != top || E.left != left || E.lines != lines || E.modified != modified) {
        redraw_window(EDIT_WIN);
        return;
    }
    r = rect_make(p.list.x, p.list.y + 1 + old_row * ROW_H, p.list.w, ROW_H);
    redraw(&r);
    edit_caret(&row, &col);
    if (row != old_row) {
        r.y = p.list.y + 1 + row * ROW_H;
        redraw(&r);
    }
    redraw(&p.info);
}

/* ---- Games ---- */

static int old_video_mode = 3;
static int pending_setup;

/* The scan's progress, in an alert with no buttons: how many found, and where it is. */
static void scan_progress(const char *dir, int found)
{
    char line[40];
    size_t n = strlen(dir);

    sprintf(line, "Looking for games: %d found", found);
    strcpy(A.alert.line1, line);
    if (n > 28)
        sprintf(line, "...%s", dir + n - 25);
    else
        strcpy(line, dir);
    sprintf(A.alert.line2, "%-28s", line);
    mouse_hide();
    draw_alert();
    mouse_show();
}

static void games_window_reset(void)
{
    Win *w = &A.win[GAMES_WIN];

    w->nrows = games_count();
    w->sel = w->nrows > 0 ? 0 : -1;
    w->top = 0;
}

/* Make the Games window wide enough for its columns (and a 20-character name), up to the
 * screen. */
static void fit_games_window(void)
{
    Win *w = &A.win[GAMES_WIN];
    Rect before = window_bounds(GAMES_WIN);
    int need = 2 + 32 + 20 * 8 + 8 + SCROLL_W, c;

    for (c = 0; c < NUM_COLS; c++)
        if (S.game_cols & (1u << c))
            need += (game_columns[c].width + 2) * 8;
    need = SNAP8_UP(need);
    if (need < 432)
        need = 432;
    if (need > 624)
        need = 624;
    w->r.w = need;
    w->zoomed = 0;
    if (w->r.x + need > 632)
        w->r.x = SNAP8(632 - need);
    if (w->r.x < 0)
        w->r.x = 0;
    if (w->open) {
        redraw(&before);
        redraw_window(GAMES_WIN);
    }
}

static void toggle_column(int c)
{
    char list[80];

    S.game_cols ^= 1u << c;
    gc_format(S.game_cols, list);
    settings_set("gamecolumns", list);
    fit_games_window();
    if (!A.win[GAMES_WIN].open)
        open_games();
}

/* Sort the games by a heading, keeping the selected game selected. */
static void sort_games(int col)
{
    Win *w = &A.win[GAMES_WIN];
    char dir[67];
    Game g;
    int k;

    dir[0] = '\0';
    if (w->sel >= 0) {
        games_get(w->sel, &g);
        strcpy(dir, g.dir);
    }
    games_sort(col);
    for (k = 0; dir[0] && k < games_count(); k++) {
        games_get(k, &g);
        if (strcmp(g.dir, dir) == 0) {
            w->sel = k;
            break;
        }
    }
    w->top = 0;
    if (w->sel >= 0)
        select_row(GAMES_WIN, w->sel);
    else
        redraw_window(GAMES_WIN);
}

static void scan_now(void)
{
    char blank[30];
    int n;

    sprintf(blank, "%-28s", "");
    show_alert("Looking for games: 0 found", blank, NULL, NULL, CMD_NONE);
    n = games_scan(scan_progress);
    close_alert(-1);
    games_window_reset();
    if (n < 0)
        show_alert("No memory for the games list.", "", "OK", NULL, CMD_NONE);
}

/* Open the games list: the first time, read the index, or scan if there isn't one. */
static void open_games(void)
{
    if (!games_loaded()) {
        if (games_load_index())
            games_window_reset();
        else
            scan_now();
    }
    open_window(GAMES_WIN);
    if (games_loaded() && games_count() == 0)
        show_alert("No games found. Add !GM_INFO.TXT", "files, then Games > Rescan.", "OK", NULL, CMD_NONE);
}

static void rescan(void)
{
    scan_now();
    if (A.win[GAMES_WIN].open)
        redraw_window(GAMES_WIN);
    else
        open_window(GAMES_WIN);
}

/* Leaving the desktop for a program: everything PetiteGUI holds outside its own memory
 * block goes back to DOS (the games list, the editor's text), its interrupt handler comes
 * out, the mouse is reset and the screen goes back to text. */
static int games_were_loaded;

static void leave_desktop(void)
{
    union REGS r;

    mouse_hide();
    mouse_shutdown();
    games_were_loaded = games_loaded();
    games_free();
    edit_suspend();
    critical_errors(0);
    r.h.ah = 0;
    r.h.al = (unsigned char)old_video_mode;
    int86(0x10, &r, &r);
}

/* And back: everything put back as it was, the folders read again (the program may have
 * changed them), the screen drawn afresh. */
static void return_desktop(void)
{
    int i;

    while (kbhit())
        getch();
    critical_errors(1);
    if (A.win[EDIT_WIN].open) {
        if (edit_resume())
            A.win[EDIT_WIN].title = E.path;
        else
            A.win[EDIT_WIN].open = 0;
    }
    for (i = 0; i < DIR_WINS; i++)
        if (A.win[i].open)
            files_load(&A.win[i], A.sort_by_size);
    gfx_start(gfx_driver());
    mouse_init();
    if (games_were_loaded) {
        if (!games_load_index())
            games_window_reset();
        A.win[GAMES_WIN].nrows = games_count();
    }
    draw_desktop(&FULL);
    mouse_show();
}

static int unsaved_edits(void)
{
    char line[40];

    if (!E.open || !E.modified)
        return 0;
    sprintf(line, "Save %s before", E.name);
    show_alert(line, "running anything.", "OK", NULL, CMD_NONE);
    return 1;
}

static unsigned run_game(const Game *g, const GameInfo *info, int setup)
{
    unsigned status;

    leave_desktop();
    status = launch_run(g->dir, info, setup);
    return_desktop();
    games_refresh(g->dir); /* its play count and time */
    if (A.win[GAMES_WIN].open)
        redraw_window(GAMES_WIN);
    return status;
}

static int shot_mode; /* screenshots: nothing waits for a key */

/* Run a program (see launch_command), then come back; wait shows its output until a key. */
static void run_program(const char *dir, const char *prog, const char *args, int shell,
                        const char *banner, int wait)
{
    unsigned status;

    if (unsaved_edits())
        return;
    leave_desktop();
    status = launch_command(dir, prog, args, shell, banner);
    if (wait && !shot_mode) {
        printf("\nPress any key to return to PetiteGUI");
        if (getch() == 0)
            getch();
    }
    return_desktop();
    if (status != 0)
        show_alert("Couldn't run it:", launch_error_text(status), "OK", NULL, CMD_NONE);
}

/* The folder a command runs in: the front folder window's, else C: (or the first drive). */
static const char *current_dir(void)
{
    static char root[4];
    int k;

    for (k = NUM_WINS - 1; k >= 0; k--)
        if (A.z[k] < DIR_WINS && A.win[A.z[k]].open && !A.win[A.z[k]].not_ready)
            return A.win[A.z[k]].path;
    sprintf(root, "%c:\\", first_drive());
    return root;
}

/* ---- The text-entry box ---- */

static void show_input(const char *prompt, const char *text)
{
    strncpy(A.input.prompt, prompt, sizeof A.input.prompt - 1);
    A.input.prompt[sizeof A.input.prompt - 1] = '\0';
    input_layout();
    field_set(&A.input.field, text, A.input.field.width);
}

/* Ask for a line of text, starting from what's in text. Returns 1 with it in text (OK or
 * Enter), 0 if it was cancelled (Cancel or Esc). */
static int ask_text(const char *prompt, char *text, int size)
{
    Rect r;
    uint8_t __far *save;
    int x, y, b, prev = 1, ch, ext, result = -1, ux, uy, i;

    show_input(prompt, text);
    r = A.input.r;
    r.w += SHADOW_X;
    r.h += SHADOW_Y;
    mouse_hide();
    save = gfx_save(&r);
    draw_input();
    mouse_show();
    while (result < 0) {
        if (kbhit()) {
            ch = getch();
            ext = ch == 0 ? getch() : 0;
            if (ch == 13)
                result = 1;
            else if (ch == 27)
                result = 0;
            else if (field_key(&A.input.field, ch, ext)) {
                mouse_hide();
                draw_input_field();
                mouse_show();
            }
        }
        mouse_read(&x, &y, &b);
        if ((b & 1) && !(prev & 1)) {
            for (i = 0; i < 2; i++)
                if (rect_contains(&A.input.b[i], x, y)) {
                    wait_release(&ux, &uy);
                    if (rect_contains(&A.input.b[i], ux, uy))
                        result = i;
                }
            if (rect_contains(&A.input.f, x, y)) { /* a click places the caret */
                Field *f = &A.input.field;

                f->caret = f->scroll + (x - A.input.f.x) / 8;
                if (f->caret > f->len)
                    f->caret = f->len;
                mouse_hide();
                draw_input_field();
                mouse_show();
            }
        }
        prev = b;
    }
    mouse_hide();
    if (save != NULL)
        gfx_restore(&r, save);
    else
        draw_desktop(&r);
    mouse_show();
    if (result == 1) {
        strncpy(text, A.input.field.text, (size_t)size - 1);
        text[size - 1] = '\0';
    }
    return result == 1;
}

/* An alert that waits for its answer: 1 for yes (the last button, or Enter), 0 for no (the
 * first, or Esc). */
static int ask_choice(const char *line1, const char *line2, const char *no, const char *yes)
{
    int x, y, b, prev = 1, ch, i, ux, uy, result = -1;

    show_alert(line1, line2, no, yes, CMD_NONE);
    while (result < 0) {
        if (kbhit()) {
            ch = getch();
            if (ch == 0)
                getch();
            else if (ch == 13)
                result = 1;
            else if (ch == 27)
                result = 0;
        }
        mouse_read(&x, &y, &b);
        if ((b & 1) && !(prev & 1)) {
            i = alert_button_at(x, y);
            wait_release(&ux, &uy);
            if (i >= 0 && alert_button_at(ux, uy) == i)
                result = i;
        }
        prev = b;
    }
    close_alert(-1);
    return result == 1;
}

/* File > Run...: a command line, filled in with the selected program, run in the folder
 * through COMMAND.COM (so the PATH and DOS's own commands work), its output kept on the
 * screen until a key. */
static void run_dialog(void)
{
    char cmd[FIELD_MAX + 1], prompt[48];
    const char *dir = current_dir();
    int f = front_window();
    const Win *w = f >= 0 && f < DIR_WINS ? &A.win[f] : NULL;

    cmd[0] = '\0';
    if (w != NULL && w->sel >= 0 && !w->rows[w->sel].dir && path_is_program(w->rows[w->sel].name))
        sprintf(cmd, "%s ", w->rows[w->sel].name);
    if (strlen(dir) > 30)
        sprintf(prompt, "Run a command in ...%s:", dir + strlen(dir) - 27);
    else
        sprintf(prompt, "Run a command in %s:", dir);
    if (!ask_text(prompt, cmd, sizeof cmd) || cmd[0] == '\0')
        return;
    run_program(dir, "", cmd, 1, NULL, 1);
}

static void launch_selected(int setup, int checked)
{
    Win *w = &A.win[GAMES_WIN];
    char prog[GM_PATH_MAX], line[40];
    GameInfo info;
    Game g;
    long free_kb;
    unsigned status;

    if (w->sel < 0 || w->sel >= games_count())
        return;
    if (unsaved_edits())
        return;
    games_get(w->sel, &g);
    if (!launch_read_info(g.dir, &info)) {
        show_alert(g.name, launch_error_text(LAUNCH_NO_INFO), "OK", NULL, CMD_NONE);
        return;
    }
    if ((setup ? info.setup : info.exec)[0] == '\0') {
        show_alert(g.name, setup ? "It has no setup program." : launch_error_text(LAUNCH_NO_EXEC),
                   "OK", NULL, CMD_NONE);
        return;
    }
    launch_program(g.dir, &info, setup, prog, sizeof prog);
    if (!launch_exists(prog)) {
        sprintf(line, "Can't find %.26s", strrchr(prog, '\\') ? strrchr(prog, '\\') + 1 : prog);
        show_alert(g.name, line, "OK", NULL, CMD_NONE);
        return;
    }
    free_kb = swap_free_kb();
    if (!setup && !checked && info.memory_kb > 0 && (long)info.memory_kb > free_kb) {
        pending_setup = setup;
        sprintf(line, "Needs %u KB; %ld KB is free.", info.memory_kb, free_kb);
        show_alert(line, "It may not run.", "Cancel", "Run", CMD_RUN_ANYWAY);
        return;
    }
    status = run_game(&g, &info, setup);
    if (status != 0)
        show_alert("Couldn't run it:", launch_error_text(status), "OK", NULL, CMD_NONE);
}

static void game_info(void)
{
    Win *w = &A.win[GAMES_WIN];
    char line[40];
    Game g;

    if (w->sel < 0)
        return;
    games_get(w->sel, &g);
    if (g.genre[0] && g.year[0])
        sprintf(line, "%s, %s", g.genre, g.year);
    else
        strcpy(line, g.genre[0] ? g.genre : g.year);
    show_alert(g.name, line, "OK", NULL, CMD_NONE);
}

/* ---- Screensavers ---- */

static void redraw_everything(void)
{
    draw_desktop(&FULL);
}

static void start_saver(int index)
{
    mouse_hide();
    saver_run(index, redraw_everything, 0);
    draw_desktop(&FULL);
    mouse_show();
}

static void choose_saver(int cmd)
{
    char minutes[8];

    if (cmd >= CMD_SAVER_DELAY) {
        S.saver_delay = cmd - CMD_SAVER_DELAY;
        sprintf(minutes, "%d", S.saver_delay);
        settings_set("saverdelay", minutes);
    } else {
        S.saver = cmd - CMD_SAVER_PICK - 2;
        settings_set("saver", saver_key(S.saver));
    }
}

/* ---- Commands ---- */

static void switch_theme(const Driver *d, const Theme *t)
{
    if (A.theme == t)
        return;
    mouse_hide();
    A.theme = t;
    gfx_start(d);
    mouse_init();
    draw_desktop(&FULL);
    mouse_show();
}

static void run(int cmd)
{
    int f = front_window();
    Win *w = f >= 0 ? &A.win[f] : NULL;
    char line[40], size[16];

    switch (cmd) {
    case CMD_ABOUT:
        sprintf(line, "PetiteGUI %s, for CGA and EGA", PETITE_VERSION);
        show_alert(line, "github.com/MaverickUK/PetiteGUI", "OK", NULL, CMD_NONE);
        break;
    case CMD_OPEN:
        if (f == GAMES_WIN)
            launch_selected(0, 0);
        else if (w != NULL && w->kind == WIN_FILES && w->sel >= 0)
            open_row(f);
        else if (A.sel_icon >= 0)
            open_icon(A.sel_icon);
        break;
    case CMD_UP:
        if (w != NULL && w->kind == WIN_FILES)
            enter_dir(f, "..");
        break;
    case CMD_EDIT_FILE:
        if (w != NULL && w->kind == WIN_FILES && w->sel >= 0 && !w->rows[w->sel].dir) {
            char path[96];

            sprintf(path, "%s%s", w->path, w->rows[w->sel].name);
            open_in_editor(path);
        }
        break;
    case CMD_SAVE:
        if (E.open)
            save_editor();
        break;
    case CMD_SAVE_CLOSE:
        if (save_editor())
            close_editor();
        break;
    case CMD_CLOSE_NOW:
        close_editor();
        break;
    case CMD_SAVE_QUIT:
        if (save_editor())
            A.quit = 1;
        break;
    case CMD_QUIT_NOW:
        A.quit = 1;
        break;
    case CMD_RUN_DIALOG:
        run_dialog();
        break;
    case CMD_DOS_PROMPT:
        run_program(current_dir(), "", "", 1, "Type EXIT to return to PetiteGUI.", 0);
        break;
    case CMD_SAVER_NOW:
        start_saver(S.saver == SAVER_OFF ? SAVER_RANDOM : S.saver);
        break;
    default:
        if (cmd >= CMD_COLUMN && cmd < CMD_COLUMN + NUM_COLS)
            toggle_column(cmd - CMD_COLUMN);
        else if (cmd >= CMD_SAVER_PICK && cmd < CMD_COLUMN)
            choose_saver(cmd);
        break;
    case CMD_GAMES:
        open_games();
        break;
    case CMD_RUN:
        if (f == GAMES_WIN)
            launch_selected(0, 0);
        break;
    case CMD_RUN_SETUP:
        if (f == GAMES_WIN)
            launch_selected(1, 0);
        break;
    case CMD_RUN_ANYWAY:
        launch_selected(pending_setup, 1);
        break;
    case CMD_RESCAN:
        rescan();
        break;
    case CMD_INFO:
        if (f == GAMES_WIN)
            game_info();
        else if (w == NULL || w->kind != WIN_FILES || w->sel < 0) {
            show_alert("Select a file first.", "", "OK", NULL, CMD_NONE);
        } else if (w->rows[w->sel].dir) {
            show_alert(w->rows[w->sel].name, strcmp(w->rows[w->sel].name, "..") == 0 ?
                       "The folder above this one." : "A folder.", "OK", NULL, CMD_NONE);
        } else {
            fmt_thousands(w->rows[w->sel].size, size);
            sprintf(line, "%s bytes.", size);
            show_alert(w->rows[w->sel].name, line, "OK", NULL, CMD_NONE);
        }
        break;
    case CMD_DELETE:
        if (f == GAMES_WIN) {
            show_alert("Remove games in GameMinder:", "it deletes their !GM_INFO.TXT.", "OK", NULL, CMD_NONE);
        } else if (w == NULL || w->kind != WIN_FILES || w->sel < 0) {
            show_alert("Select a file first.", "", "OK", NULL, CMD_NONE);
        } else if (strcmp(w->rows[w->sel].name, "..") == 0) {
            break;
        } else if (w->rows[w->sel].dir) {
            char path[96], what[32];
            long files = 0, bytes = 0;

            path_join(path, sizeof path, w->path, w->rows[w->sel].name);
            fo_count(path, &files, &bytes);
            if (files == 0) {
                sprintf(line, "Delete the empty folder %s?", w->rows[w->sel].name);
                strcpy(what, "");
            } else {
                sprintf(line, "Delete %s and its %ld file%s?", w->rows[w->sel].name, files, files == 1 ? "" : "s");
                strcpy(what, "They can't be got back.");
            }
            show_alert(line, what[0] ? what : "It can't be undone.", "Cancel", "Delete", CMD_DO_DELETE);
        } else {
            sprintf(line, "Delete %s?", w->rows[w->sel].name);
            show_alert(line, "It can't be undone.", "Cancel", "Delete", CMD_DO_DELETE);
        }
        break;
    case CMD_DO_DELETE:
        delete_selected(f);
        break;
    case CMD_NEW_FOLDER:
        new_folder();
        break;
    case CMD_RENAME:
        rename_item();
        break;
    case CMD_COPY:
        copy_or_move(0);
        break;
    case CMD_MOVE:
        copy_or_move(1);
        break;
    case CMD_CLOSE:
        if (f == EDIT_WIN) {
            if (!ask_to_save(CMD_SAVE_CLOSE, CMD_CLOSE_NOW))
                close_editor();
        } else if (f >= 0) {
            close_window(f);
        }
        break;
    case CMD_QUIT:
        if (!ask_to_save(CMD_SAVE_QUIT, CMD_QUIT_NOW))
            A.quit = 1;
        break;
    case CMD_BY_NAME:
    case CMD_BY_SIZE:
        A.sort_by_size = cmd == CMD_BY_SIZE;
        sort_windows();
        redraw(&FULL);
        break;
    case CMD_PAPER:
        switch_theme(&drv_cga, &theme_paper);
        break;
    case CMD_INK:
        if (A.has_ega)
            switch_theme(&drv_ega, &theme_ink);
        break;
    }
}

/* ---- Input ---- */

static int rect_contains_menu(int x, int y)
{
    Rect r = menu_rect(A.open_menu);

    return rect_contains(&r, x, y);
}

/* Follow the mouse along the menu bar and down the open menu while the button is held.
 * Returns once it's let go: on an item, that item runs; on a title, the menu stays open. */
static void track_menu(int button)
{
    int x, y, b, t, item;

    for (;;) {
        mouse_read(&x, &y, &b);
        t = A.open_menu == CONTEXT_MENU ? -1 : menu_title_at(x, y);
        if (t >= 0 && t != A.open_menu) {
            close_menu();
            open_menu(t);
        }
        item = menu_item_at(A.open_menu, x, y);
        set_hot(item);
        if (!(b & button))
            break;
    }
    if (item >= 0) {
        int cmd = A.menu[A.open_menu].items[item].cmd;

        close_menu();
        run(cmd);
    } else if (t < 0 && !(A.open_menu == CONTEXT_MENU && rect_contains_menu(x, y))) {
        close_menu(); /* a right-click menu stays open if the button is let go on it */
    }
}

/* Right-click on a file or a game: select it and show what can be done with it. */
static void on_right_press(int x, int y)
{
    int i = window_at(x, y), k;
    WinParts p;
    Win *w;

    if (A.alert_on)
        return;
    if (A.open_menu >= 0) {
        close_menu();
        return;
    }
    if (i < 0 || (A.win[i].kind != WIN_FILES && A.win[i].kind != WIN_GAMES))
        return;
    w = &A.win[i];
    bring_to_front(i);
    window_parts(i, &p);
    if (!rect_contains(&p.list, x, y))
        return;
    k = w->top + (y - p.list.y - 1) / ROW_H;
    if (k >= w->nrows)
        return;
    if (w->sel != k)
        select_row(i, k);
    A.menu[CONTEXT_MENU].items = w->kind == WIN_GAMES ? game_context : file_context;
    A.menu[CONTEXT_MENU].n = w->kind == WIN_GAMES ? COUNT(game_context) : COUNT(file_context);
    A.menu[CONTEXT_MENU].x = x;
    A.menu[CONTEXT_MENU].y = y;
    open_menu(CONTEXT_MENU);
    track_menu(2);
}

static unsigned long last_click;
static int last_icon = -1;

static void on_press(int x, int y)
{
    int i, ux, uy;
    Rect r;

    if (A.alert_on) {
        i = alert_button_at(x, y);
        wait_release(&ux, &uy);
        if (i >= 0 && alert_button_at(ux, uy) == i)
            close_alert(i);
        return;
    }
    if (A.open_menu >= 0) {
        if (menu_item_at(A.open_menu, x, y) >= 0 ||
            (A.open_menu != CONTEXT_MENU && menu_title_at(x, y) >= 0)) {
            track_menu(1);
        } else {
            close_menu();
            wait_release(&ux, &uy);
        }
        return;
    }
    i = menu_title_at(x, y);
    if (i >= 0) {
        open_menu(i);
        track_menu(1);
        return;
    }
    i = window_at(x, y);
    if (i >= 0) {
        press_window(i, x, y);
        return;
    }
    for (i = 0; i < A.nicons; i++) {
        r = icon_bounds(i);
        if (rect_contains(&r, x, y)) {
            int twice = i == last_icon && ticks() - last_click < 9;

            select_icon(i);
            wait_release(&ux, &uy);
            last_icon = i;
            last_click = ticks();
            if (twice) {
                last_icon = -1;
                open_icon(i);
            }
            return;
        }
    }
    select_icon(-1);
    wait_release(&ux, &uy);
}

static void on_key(int ch)
{
    int f = front_window();

    if (A.alert_on) {
        if (ch == 27)
            close_alert(A.alert.nbuttons == 1 ? 0 : -1);
        else if (ch == 13)
            close_alert(A.alert.nbuttons - 1);
        return;
    }
    if (A.open_menu >= 0) {
        if (ch == 27)
            close_menu();
        return;
    }
    if (f == EDIT_WIN && E.open && ch != 4 && ch != 7 && ch != 15 && ch != 17 && ch != 18 && ch != 19 && ch != 23) {
        editor_key(ch, ch == 0 ? getch() : 0);
        return;
    }
    switch (ch) {
    case 13: run(f == GAMES_WIN ? CMD_RUN : CMD_OPEN); break; /* Enter */
    case 8: run(CMD_UP); break;        /* Backspace */
    case 5: run(CMD_EDIT_FILE); break; /* Ctrl+E */
    case 18: run(CMD_RUN_DIALOG); break; /* Ctrl+R */
    case 4: run(CMD_DOS_PROMPT); break; /* Ctrl+D */
    case 19: run(CMD_SAVE); break;     /* Ctrl+S */
    case 7: run(CMD_GAMES); break;   /* Ctrl+G */
    case 15: run(CMD_OPEN); break;   /* Ctrl+O */
    case 9: run(CMD_INFO); break;    /* Ctrl+I */
    case 23: run(CMD_CLOSE); break;  /* Ctrl+W */
    case 17: run(CMD_QUIT); break;   /* Ctrl+Q */
    case 0:
        ch = getch(); /* an extended key */
        if (ch == 83 || ch == 66) /* Delete, F8 */
            run(CMD_DELETE);
        else if (ch == 60) /* F2 */
            run(CMD_RENAME);
        else if (ch == 63) /* F5 */
            run(CMD_COPY);
        else if (ch == 64) /* F6 */
            run(CMD_MOVE);
        else if (ch == 65) /* F7 */
            run(CMD_NEW_FOLDER);
        else if (f >= 0 && ch == 72)
            select_row(f, A.win[f].sel - 1);
        else if (f >= 0 && ch == 80)
            select_row(f, A.win[f].sel + 1);
        break;
    }
}

/* ---- Screenshots ---- */

static int show_progress_quiet(const char *name, long done)
{
    (void)name;
    (void)done;
    return 1;
}

static int find_row(int i, const char *name)
{
    int k;

    for (k = 0; k < A.win[i].nrows; k++)
        if (stricmp(A.win[i].rows[k].name, name) == 0)
            return k;
    return 0;
}

static int shot(int scene, const char *path)
{
    FILE *f;
    int ok;

    draw_desktop(&FULL);
    if (scene == 1) {
        open_menu(FILE_MENU);
        set_hot(FILE_DELETE);
    } else if (scene == 2) {
        select_row(1, find_row(1, "PETITE.CFG"));
        run(CMD_DELETE);
    } else if (scene == 5) { /* the editor */
        open_in_editor("C:\\GAMES\\MEMTEST\\!GM_INFO.TXT");
        editor_key(0, 80);
        editor_key(0, 79);
    } else if (scene == 6) { /* the right-click menu on a file */
        WinParts p;
        int k = find_row(1, "PETITE.CFG");

        select_row(1, k);
        window_parts(1, &p);
        A.menu[CONTEXT_MENU].x = p.list.x + 120;
        A.menu[CONTEXT_MENU].y = p.list.y + 2 + (k - A.win[1].top) * ROW_H + 4;
        open_menu(CONTEXT_MENU);
        set_hot(1);
    } else if (scene == 7) { /* type a line into C:\\OUT\\EDIT.TXT and save it (tools/dostest.sh) */
        open_in_editor("C:\\OUT\\EDIT.TXT");
        editor_key('H', 0);
        editor_key('i', 0);
        editor_key(13, 0);
        editor_key(0, 117); /* Ctrl+End */
        editor_key('!', 0);
        run(CMD_SAVE);
    } else if (scene == 8) { /* windows resized: C: smaller, the games list zoomed behind it */
        open_games();
        zoom_window(GAMES_WIN);
        bring_to_front(1);
        set_window_rect(1, rect_resize(&A.win[1].r, 240, 96, MIN_WIN_W, MIN_WIN_H, SCREEN_W, SCREEN_H));
    } else if (scene == 10) { /* File > Run... with a command (tools/dostest.sh) */
        run_program("C:\\", "", "C:\\GAMES\\MEMTEST\\FREEMEM.EXE C:\\OUT\\RUNCMD.TXT fromrun", 1, NULL, 0);
        mouse_hide();
    } else if (scene == 11) { /* a batch file double-clicked in a folder (tools/dostest.sh) */
        enter_dir(1, "GAMES");
        enter_dir(1, "BATCH");
        select_row(1, find_row(1, "GO.BAT"));
        open_row(1);
        mouse_hide();
    } else if (scene == 12) { /* the Run... box */
        show_input("Run a command in C:\\GAMES\\:", "PKUNZIP DOOM.ZIP");
        draw_input();
    } else if (scene == 13) { /* file operations, their results to C:\\OUT\\FILEOPS.TXT (tools/dostest.sh) */
        FILE *out = fopen("C:\\OUT\\FILEOPS.TXT", "w");
        int rc[12], n = 0, k;

        rc[n++] = fo_copy("C:\\GAMES\\MEMTEST", "D:\\MEMTEST", 0, show_progress_quiet);
        rc[n++] = fo_copy("C:\\GAMES\\BATCH\\GO.BAT", "C:\\OUT\\GOCOPY.BAT", 0, NULL);
        rc[n++] = fo_mkdir("C:\\OUT\\NEWDIR");
        rc[n++] = fo_move("C:\\OUT\\GOCOPY.BAT", "C:\\OUT\\NEWDIR\\GOMOVED.BAT", 0, NULL);
        rc[n++] = fo_move("C:\\OUT\\NEWDIR", "C:\\OUT\\MOVEDDIR", 0, NULL);
        rc[n++] = fo_move("D:\\MEMTEST", "C:\\OUT\\MEMBACK", 0, NULL);
        rc[n++] = fo_copy("C:\\GAMES", "C:\\GAMES\\INSIDE", 0, NULL);
        rc[n++] = fo_copy("C:\\GAMES\\BATCH\\GO.BAT", "C:\\OUT\\MOVEDDIR\\GOMOVED.BAT", 0, NULL);
        rc[n++] = fo_rename("C:\\OUT\\MOVEDDIR\\GOMOVED.BAT", "C:\\OUT\\MOVEDDIR\\RENAMED.BAT");
        rc[n++] = fo_copy("C:\\OUT\\MEMBACK", "C:\\OUT\\DELME", 0, NULL);
        rc[n++] = fo_delete("C:\\OUT\\DELME", NULL);
        if (out != NULL) {
            for (k = 0; k < n; k++)
                fprintf(out, "%d ", rc[k]);
            fprintf(out, "\n");
            fclose(out);
        }
    } else if (scene == 14) { /* deleting a folder asks first, saying how much is in it */
        select_row(1, find_row(1, "GAMES"));
        run(CMD_DELETE);
    } else if (scene == 9) { /* System > About */
        run(CMD_ABOUT);
    } else if (scene == 3) {
        open_games();
    } else if (scene == 4) { /* run the first game from the list, then come back (tools/dostest.sh) */
        open_games();
        select_row(GAMES_WIN, 0);
        launch_selected(0, 1);
        mouse_hide(); /* the pointer isn't part of the picture */
    }
    f = fopen(path, "wb");
    if (f == NULL)
        return 0;
    ok = gfx_driver()->dump(f);
    fclose(f);
    return ok;
}

/* ---- The command line ---- */

static int print_memory(void)
{
    printf("PetiteGUI %s\n", PETITE_VERSION);
    printf("Program in memory:   %ld bytes\n", swap_bytes() + swap_resident_bytes());
    printf("Left while swapped:  %ld bytes\n", swap_resident_bytes());
    printf("Swapped out:         %ld bytes, to %s\n", swap_bytes(),
           S.use_ems && swap_ems_present() && swap_ems_free_kb() * 1024L >= swap_bytes() ? "EMS" : "SWAP.$$$");
    printf("Free for a program:  %ld KB\n", swap_free_kb());
    printf("EMS free:            %ld KB\n", swap_ems_free_kb());
    return 0;
}

static int print_games(void)
{
    Game g;
    int k;

    for (k = 0; k < games_count(); k++) {
        games_get(k, &g);
        printf("%-32s %-4s %s\n", g.name, g.year, g.dir);
    }
    printf("%d game%s in %s\n", games_count(), games_count() == 1 ? "" : "s", S.scan_path);
    return 0;
}

static int run_by_name(const char *name)
{
    char upper_name[40], upper_want[40], prog[GM_PATH_MAX];
    GameInfo info;
    Game g;
    unsigned status;
    int k;

    strncpy(upper_want, name, sizeof upper_want - 1);
    upper_want[sizeof upper_want - 1] = '\0';
    strupr(upper_want);
    for (k = 0; k < games_count(); k++) {
        games_get(k, &g);
        strcpy(upper_name, g.name);
        strupr(upper_name);
        if (strstr(upper_name, upper_want) != NULL)
            break;
    }
    if (k == games_count()) {
        printf("No game matches %s\n", name);
        return 1;
    }
    if (!launch_read_info(g.dir, &info)) {
        printf("%s\n", launch_error_text(LAUNCH_NO_INFO));
        return 1;
    }
    launch_program(g.dir, &info, 0, prog, sizeof prog);
    if (!launch_exists(prog)) {
        printf("%s: %s\n", prog, launch_error_text(2));
        return 1;
    }
    games_free();
    status = launch_run(g.dir, &info, 0);
    if (status != 0)
        printf("Couldn't run it: %s (%u)\n", launch_error_text(status), status);
    return status != 0;
}

int main(int argc, char **argv)
{
    int i, scene = -1, force = 0, old_mode = video_mode(), x, y, b, prev = 0, ch;
    int scan = 0, list = 0, mem = 0, saver = SAVER_UNKNOWN, mx = -1, my = -1;
    long saver_steps = 300;
    unsigned long last_input = 0;
    const char *run_name = NULL, *set_arg = NULL;
    const char *dump = "SHOT.BIN";
    unsigned long next_clock = 0;
    char clock[6];

    for (i = 1; i < argc; i++) {
        if (stricmp(argv[i], "/CGA") == 0)
            force = 1;
        else if (stricmp(argv[i], "/EGA") == 0)
            force = 2;
        else if (strnicmp(argv[i], "/SHOT=", 6) == 0)
            scene = atoi(argv[i] + 6);
        else if (strnicmp(argv[i], "/DUMP=", 6) == 0)
            dump = argv[i] + 6;
        else if (stricmp(argv[i], "/SCAN") == 0)
            scan = 1;
        else if (stricmp(argv[i], "/LIST") == 0)
            list = 1;
        else if (stricmp(argv[i], "/MEM") == 0)
            mem = 1;
        else if (stricmp(argv[i], "/DRIVES") == 0)
            mem = 2;
        else if (strnicmp(argv[i], "/RUN=", 5) == 0)
            run_name = argv[i] + 5;
        else if (strnicmp(argv[i], "/SAVER=", 7) == 0)
            saver = saver_by_key(argv[i] + 7);
        else if (strnicmp(argv[i], "/STEPS=", 7) == 0)
            saver_steps = atol(argv[i] + 7);
        else if (strnicmp(argv[i], "/SET=", 5) == 0 && strchr(argv[i] + 5, ':') != NULL)
            set_arg = argv[i] + 5;
        else {
            printf("PetiteGUI %s\nUsage: PETITE [/CGA | /EGA] [/SCAN] [/LIST] [/RUN=name] [/MEM] [/SET=key:value]\n",
                   PETITE_VERSION);
            return 1;
        }
    }
    settings_load(argv[0]);
    old_video_mode = old_mode;
    if (set_arg != NULL) { /* /SET=key:value: change one setting in PETITE.CFG */
        char key[16];
        const char *colon = strchr(set_arg, ':');
        int n = (int)(colon - set_arg) < (int)sizeof key - 1 ? (int)(colon - set_arg) : (int)sizeof key - 1;

        memcpy(key, set_arg, (size_t)n);
        key[n] = '\0';
        if (!settings_set(key, colon + 1)) {
            printf("Couldn't write PETITE.CFG\n");
            return 1;
        }
        return 0;
    }
    if (mem == 2) {
        Drive d[MAX_DRIVES];
        int n = drives_find(d), k;

        for (k = 0; k < n; k++)
            printf("%c: %s\n", d[k].letter, drive_kind_name(d[k].kind));
        return 0;
    }
    if (mem)
        return print_memory();
    if (scan || list || run_name != NULL) {
        if (scan || !games_load_index())
            games_scan(NULL);
        if (list)
            return print_games();
        if (run_name != NULL)
            return run_by_name(run_name);
    }

    app_init();
    fit_games_window();
    A.has_ega = detect_ega();
    if (force == 2 && !A.has_ega) {
        printf("Ink needs an EGA or VGA card.\n");
        return 1;
    }
    draw_init();
    gfx_has_ega = A.has_ega;
    if (force == 1 || (!force && !A.has_ega)) {
        A.theme = &theme_paper;
        gfx_start(&drv_cga);
    } else {
        A.theme = &theme_ink;
        gfx_start(&drv_ega);
    }

    if (saver != SAVER_UNKNOWN) { /* run a saver for some steps, then dump the screen (tests) */
        FILE *f;

        draw_desktop(&FULL);
        saver_run(saver, redraw_everything, saver_steps);
        f = fopen(dump, "wb");
        if (f != NULL) {
            gfx_driver()->dump(f);
            fclose(f);
        }
        set_video_mode(old_mode);
        return f == NULL;
    }
    if (scene >= 0) {
        int ok;

        shot_mode = 1;
        ok = shot(scene, dump);

        set_video_mode(old_mode);
        if (!ok)
            printf("Couldn't write %s\n", dump);
        return ok ? 0 : 1;
    }

    if (!mouse_init()) {
        set_video_mode(old_mode);
        printf("PetiteGUI needs a mouse driver, such as MOUSE.COM or CTMOUSE.\n");
        return 1;
    }
    read_clock(A.clock);
    critical_errors(1);
    draw_desktop(&FULL);
    mouse_show();

    last_input = ticks();
    while (!A.quit) {
        mouse_read(&x, &y, &b);
        if (x != mx || y != my || b != 0 || ticks() < last_input) { /* (ticks restart at midnight) */
            mx = x;
            my = y;
            last_input = ticks();
        }
        if (S.saver != SAVER_OFF && S.saver_delay > 0 && A.open_menu < 0 && !A.alert_on &&
            ticks() - last_input >= (unsigned long)S.saver_delay * 1092UL) { /* 18.2 ticks a second */
            start_saver(S.saver);
            last_input = ticks();
            mouse_read(&mx, &my, &b);
            prev = b;
            continue;
        }
        if ((b & 1) && !(prev & 1)) {
            on_press(x, y);
            mouse_read(&x, &y, &b);
        } else if ((b & 2) && !(prev & 2)) {
            on_right_press(x, y);
            mouse_read(&x, &y, &b);
        } else if (A.open_menu >= 0) {
            set_hot(menu_item_at(A.open_menu, x, y));
        }
        prev = b;
        if (kbhit()) {
            ch = getch();
            last_input = ticks();
            on_key(ch);
        }
        if (ticks() >= next_clock) {
            next_clock = ticks() + 18;
            read_clock(clock);
            if (strcmp(clock, A.clock) != 0 && A.open_menu < 0 && !A.alert_on) {
                strcpy(A.clock, clock);
                mouse_hide();
                draw_clock();
                mouse_show();
            }
        }
    }

    mouse_hide();
    critical_errors(0);
    edit_close();
    set_video_mode(old_mode);
    return 0;
}
