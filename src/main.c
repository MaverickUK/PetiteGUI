/* PetiteGUI: a small desktop for CGA and EGA PCs.
 *
 *   PETITE            Ink on an EGA or VGA, Paper on a CGA
 *   PETITE /CGA       Paper, whatever the card
 *   PETITE /EGA       Ink (needs an EGA or VGA)
 *   PETITE /SCAN      scan for games now, rather than when the list is first opened
 *   PETITE /LIST      print the games list (scanning if there's no index) and quit
 *   PETITE /RUN=name  run the first game whose name contains name, then quit
 *   PETITE /MEM       print what a swap would leave for a program, and quit
 *   PETITE /SHOT=n /DUMP=FILE.BIN
 *                     draw scene n (0 desktop, 1 File menu open, 2 alert, 3 games list,
 *                     4 the first game run from the list and back),
 *                     write video memory to FILE.BIN and quit: tools/shot.sh makes a PNG
 *
 * The program polls: it reads the mouse and keyboard, and acts on presses. Anything that
 * changes redraws only the rectangle it changed, back to front. */
#include <conio.h>
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "games.h"
#include "gfx.h"
#include "launch.h"
#include "mouse.h"
#include "swap.h"
#include "util.h"
#include "version.h"

App A;

enum {
    CMD_NONE, CMD_ABOUT, CMD_OPEN, CMD_INFO, CMD_NEW_FOLDER, CMD_COPY, CMD_DELETE,
    CMD_DO_DELETE, CMD_CLOSE, CMD_QUIT, CMD_BY_NAME, CMD_BY_SIZE, CMD_PAPER, CMD_INK,
    CMD_GAMES, CMD_RUN, CMD_RUN_SETUP, CMD_RESCAN, CMD_RUN_ANYWAY
};

static MenuItem system_items[] = {
    { "About PetiteGUI...", NULL, 0, CMD_ABOUT },
};
static MenuItem file_items[] = {
    { "Open", "Ctrl+O", 0, CMD_OPEN },
    { "Info...", "Ctrl+I", 0, CMD_INFO },
    { "", NULL, MI_SEP, 0 },
    { "New folder", "Ctrl+N", MI_OFF, CMD_NEW_FOLDER },
    { "Copy...", "Ctrl+C", MI_OFF, CMD_COPY },
    { "Delete...", "Del", 0, CMD_DELETE },
    { "", NULL, MI_SEP, 0 },
    { "Close", "Ctrl+W", 0, CMD_CLOSE },
    { "Quit", "Ctrl+Q", 0, CMD_QUIT },
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
};
static MenuItem options_items[] = {
    { "Paper (CGA)", NULL, 0, CMD_PAPER },
    { "Ink (EGA)", NULL, 0, CMD_INK },
};
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define FILE_MENU 1
#define FILE_DELETE 5 /* its index in file_items */

/* ---- Setting up ---- */

static void add_row(Win *w, const char *name, long size, int dir)
{
    Row *r = &w->rows[w->nrows++];

    strncpy(r->name, name, sizeof r->name - 1);
    r->name[sizeof r->name - 1] = '\0';
    r->size = size;
    r->dir = (uint8_t)dir;
}

static int compare_rows(const void *a, const void *b)
{
    const Row *x = (const Row *)a, *y = (const Row *)b;

    if (x->dir != y->dir)
        return x->dir ? -1 : 1;
    if (A.sort_by_size && x->size != y->size)
        return x->size > y->size ? -1 : 1;
    return strcmp(x->name, y->name);
}

/* Sort every window, keeping each one's selected row selected. */
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
        qsort(w->rows, w->nrows, sizeof(Row), compare_rows);
        for (k = 0; k < w->nrows; k++)
            if (strcmp(w->rows[k].name, name) == 0)
                w->sel = k;
    }
}

static void app_init(void)
{
    Win *a = &A.win[0], *c = &A.win[1], *g = &A.win[GAMES_WIN];
    static const char *const titles[NUM_MENUS] = { "System", "File", "View", "Games", "Options" };
    MenuItem *items[NUM_MENUS];
    int counts[NUM_MENUS], i, x = 16;

    memset(&A, 0, sizeof A);
    a->r = rect_make(24, MENU_H + 14, 264, 100);
    a->title = "A:\\";
    a->free_kb = 291;
    a->sel = -1;
    add_row(a, "DRIVERS", 0, 1);
    add_row(a, "INSTALL.EXE", 24576L, 0);
    add_row(a, "SETUP.INI", 412, 0);
    a->open = 1;

    c->r = rect_make(176, MENU_H + 30, 336, 122);
    c->title = "C:\\";
    c->free_kb = 18432L;
    c->sel = -1;
    add_row(c, "DOS", 0, 1);
    add_row(c, "GAMES", 0, 1);
    add_row(c, "SYNTH", 0, 1);
    add_row(c, "UTILS", 0, 1);
    add_row(c, "AUTOEXEC.BAT", 1024, 0);
    add_row(c, "COMMAND.COM", 54645L, 0);
    add_row(c, "CONFIG.SYS", 512, 0);
    add_row(c, "IO.SYS", 40566L, 0);
    add_row(c, "MOUSE.COM", 14336L, 0);
    add_row(c, "MSDOS.SYS", 38138L, 0);
    add_row(c, "README.TXT", 3210, 0);
    add_row(c, "TREE.COM", 6383, 0);
    c->sel = 10; /* README.TXT */
    c->open = 1;
    g->kind = WIN_GAMES;
    g->r = rect_make(40, MENU_H + 20, 432, 142);
    g->title = "Games";
    g->sel = -1;
    A.z[0] = GAMES_WIN; /* closed until it's opened, then in front */
    A.z[1] = 0;
    A.z[2] = 1;
    sort_windows();
    {
        WinParts p; /* start scrolled so the selected row shows */

        window_parts(1, &p);
        if (c->sel >= c->top + p.visible)
            c->top = c->sel - p.visible + 1;
    }

    A.icon[0].label = "A:";
    A.icon[0].kind = ICON_FLOPPY;
    A.icon[0].y = MENU_H + 12;
    A.icon[0].win = 0;
    A.icon[1].label = "C:";
    A.icon[1].kind = ICON_DISK;
    A.icon[1].y = MENU_H + 48;
    A.icon[1].win = 1;
    A.icon[2].label = "Games";
    A.icon[2].kind = ICON_GAMES;
    A.icon[2].y = MENU_H + 84;
    A.icon[2].win = GAMES_WIN;
    A.icon[3].label = "Trash";
    A.icon[3].kind = ICON_TRASH;
    A.icon[3].y = SCREEN_H - 42;
    A.icon[3].win = -1;
    A.sel_icon = 1;

    items[0] = system_items;
    counts[0] = COUNT(system_items);
    items[1] = file_items;
    counts[1] = COUNT(file_items);
    items[2] = view_items;
    counts[2] = COUNT(view_items);
    items[3] = games_items;
    counts[3] = COUNT(games_items);
    items[4] = options_items;
    counts[4] = COUNT(options_items);
    for (i = 0; i < NUM_MENUS; i++) {
        A.menu[i].title = titles[i];
        A.menu[i].items = items[i];
        A.menu[i].n = counts[i];
        A.menu[i].x = x;
        x += gfx_text_w(titles[i]) + 16;
    }
    A.open_menu = A.hot = -1;
    strcpy(A.clock, "10:42");
}

/* Check marks and greyed items that follow the state. */
static void update_menus(void)
{
    int f = front_window(), game = f == GAMES_WIN && A.win[GAMES_WIN].sel >= 0;

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

static void run(int cmd);
static void launch_selected(int setup, int checked);

/* Close the alert; ok runs its command. */
static void close_alert(int ok)
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
    if (ok)
        run(A.alert.cmd);
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

/* Drag an outline of the window; move it when the button is let go. */
static void drag_window(int i, int mx, int my)
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
        redraw(&old);
        redraw_window(i);
    }
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
            close_window(i);
    } else if (rect_contains(&p.title, x, y)) {
        drag_window(i, x, y);
    } else if (rect_contains(&p.up, x, y)) {
        hold_arrow(i, -1);
    } else if (rect_contains(&p.down, x, y)) {
        hold_arrow(i, 1);
    } else if (rect_contains(&p.track, x, y)) {
        scroll_by(i, y < p.track.y + p.track.h / 2 ? -p.visible : p.visible);
        wait_release(&ux, &uy);
    } else if (rect_contains(&p.list, x, y)) {
        static unsigned long last_time;
        static int last_win = -1, last_row = -1;

        k = w->top + (y - p.list.y - 1) / ROW_H;
        if (k < w->nrows) {
            int twice = i == last_win && k == last_row && ticks() - last_time < 9;

            select_row(i, k);
            wait_release(&ux, &uy);
            last_win = i;
            last_row = k;
            last_time = ticks();
            if (twice) {
                last_row = -1;
                if (w->kind == WIN_GAMES)
                    launch_selected(0, 0);
                else
                    run(CMD_INFO);
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

static void open_icon(int i)
{
    if (A.icon[i].win == GAMES_WIN)
        open_games();
    else if (A.icon[i].win >= 0)
        open_window(A.icon[i].win);
    else
        show_alert("The trash is empty.", "", "OK", NULL, CMD_NONE);
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

static void scan_now(void)
{
    char blank[30];
    int n;

    sprintf(blank, "%-28s", "");
    show_alert("Looking for games: 0 found", blank, NULL, NULL, CMD_NONE);
    n = games_scan(scan_progress);
    close_alert(0);
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

/* Leave the desktop for a text screen, run the game, come back. */
static unsigned run_game(const Game *g, const GameInfo *info, int setup)
{
    unsigned status;
    union REGS r;

    mouse_hide();
    mouse_shutdown();
    games_free(); /* its memory goes back to DOS for the game */
    r.h.ah = 0;
    r.h.al = (unsigned char)old_video_mode;
    int86(0x10, &r, &r);
    status = launch_run(g->dir, info, setup);

    while (kbhit())
        getch();
    gfx_start(gfx_driver());
    mouse_init();
    if (!games_load_index())
        games_window_reset();
    A.win[GAMES_WIN].nrows = games_count();
    draw_desktop(&FULL);
    mouse_show();
    return status;
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
        sprintf(line, "PetiteGUI %s", PETITE_VERSION);
        show_alert(line, "A small desktop for CGA/EGA.", "OK", NULL, CMD_NONE);
        break;
    case CMD_OPEN:
        if (f == GAMES_WIN)
            launch_selected(0, 0);
        else if (A.sel_icon >= 0)
            open_icon(A.sel_icon);
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
        else if (w == NULL || w->sel < 0) {
            show_alert("Select a file first.", "", "OK", NULL, CMD_NONE);
        } else if (w->rows[w->sel].dir) {
            show_alert(w->rows[w->sel].name, "A folder.", "OK", NULL, CMD_NONE);
        } else {
            fmt_thousands(w->rows[w->sel].size, size);
            sprintf(line, "%s bytes.", size);
            show_alert(w->rows[w->sel].name, line, "OK", NULL, CMD_NONE);
        }
        break;
    case CMD_DELETE:
        if (f == GAMES_WIN) {
            show_alert("Remove games in GameMinder:", "it deletes their !GM_INFO.TXT.", "OK", NULL, CMD_NONE);
        } else if (w == NULL || w->sel < 0) {
            show_alert("Select a file first.", "", "OK", NULL, CMD_NONE);
        } else {
            sprintf(line, "Delete %s?", w->rows[w->sel].name);
            show_alert(line, "It can't be undone.", "Cancel", "Delete", CMD_DO_DELETE);
        }
        break;
    case CMD_DO_DELETE: /* only from the list: nothing on disk is touched */
        if (w != NULL && w->sel >= 0) {
            memmove(&w->rows[w->sel], &w->rows[w->sel + 1], (w->nrows - w->sel - 1) * sizeof(Row));
            w->nrows--;
            if (w->sel >= w->nrows)
                w->sel = w->nrows - 1;
            if (w->top > 0 && w->top + 1 > w->nrows)
                w->top--;
            redraw_window(f);
        }
        break;
    case CMD_CLOSE:
        if (f >= 0)
            close_window(f);
        break;
    case CMD_QUIT:
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

/* Follow the mouse along the menu bar and down the open menu while the button is held.
 * Returns once it's let go: on an item, that item runs; on a title, the menu stays open. */
static void track_menu(void)
{
    int x, y, b, t, item;

    for (;;) {
        mouse_read(&x, &y, &b);
        t = menu_title_at(x, y);
        if (t >= 0 && t != A.open_menu) {
            close_menu();
            open_menu(t);
        }
        item = menu_item_at(A.open_menu, x, y);
        set_hot(item);
        if (!(b & 1))
            break;
    }
    if (item >= 0) {
        int cmd = A.menu[A.open_menu].items[item].cmd;

        close_menu();
        run(cmd);
    } else if (t < 0) {
        close_menu();
    }
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
            close_alert(i == A.alert.nbuttons - 1);
        return;
    }
    if (A.open_menu >= 0) {
        if (menu_item_at(A.open_menu, x, y) >= 0 || menu_title_at(x, y) >= 0) {
            track_menu();
        } else {
            close_menu();
            wait_release(&ux, &uy);
        }
        return;
    }
    i = menu_title_at(x, y);
    if (i >= 0) {
        open_menu(i);
        track_menu();
        return;
    }
    i = window_at(x, y);
    if (i >= 0) {
        press_window(i, x, y);
        return;
    }
    for (i = 0; i < NUM_ICONS; i++) {
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
            close_alert(A.alert.nbuttons == 1);
        else if (ch == 13)
            close_alert(1);
        return;
    }
    if (A.open_menu >= 0) {
        if (ch == 27)
            close_menu();
        return;
    }
    switch (ch) {
    case 13: if (f == GAMES_WIN) run(CMD_RUN); break; /* Enter */
    case 7: run(CMD_GAMES); break;   /* Ctrl+G */
    case 15: run(CMD_OPEN); break;   /* Ctrl+O */
    case 9: run(CMD_INFO); break;    /* Ctrl+I */
    case 23: run(CMD_CLOSE); break;  /* Ctrl+W */
    case 17: run(CMD_QUIT); break;   /* Ctrl+Q */
    case 0:
        ch = getch(); /* an extended key */
        if (ch == 83)
            run(CMD_DELETE);
        else if (f >= 0 && ch == 72)
            select_row(f, A.win[f].sel - 1);
        else if (f >= 0 && ch == 80)
            select_row(f, A.win[f].sel + 1);
        break;
    }
}

/* ---- Screenshots ---- */

static int shot(int scene, const char *path)
{
    FILE *f;
    int ok;

    draw_desktop(&FULL);
    if (scene == 1) {
        open_menu(FILE_MENU);
        set_hot(FILE_DELETE);
    } else if (scene == 2) {
        run(CMD_DELETE);
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
    int scan = 0, list = 0, mem = 0;
    const char *run_name = NULL;
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
        else if (strnicmp(argv[i], "/RUN=", 5) == 0)
            run_name = argv[i] + 5;
        else {
            printf("PetiteGUI %s\nUsage: PETITE [/CGA | /EGA] [/SCAN] [/LIST] [/RUN=name] [/MEM]\n",
                   PETITE_VERSION);
            return 1;
        }
    }
    settings_load(argv[0]);
    old_video_mode = old_mode;
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
    A.has_ega = detect_ega();
    if (force == 2 && !A.has_ega) {
        printf("Ink needs an EGA or VGA card.\n");
        return 1;
    }
    draw_init();
    if (force == 1 || (!force && !A.has_ega)) {
        A.theme = &theme_paper;
        gfx_start(&drv_cga);
    } else {
        A.theme = &theme_ink;
        gfx_start(&drv_ega);
    }

    if (scene >= 0) {
        int ok = shot(scene, dump);

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
    draw_desktop(&FULL);
    mouse_show();

    while (!A.quit) {
        mouse_read(&x, &y, &b);
        if ((b & 1) && !(prev & 1)) {
            on_press(x, y);
            mouse_read(&x, &y, &b);
        } else if (A.open_menu >= 0) {
            set_hot(menu_item_at(A.open_menu, x, y));
        }
        prev = b;
        if (kbhit()) {
            ch = getch();
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
    set_video_mode(old_mode);
    return 0;
}
