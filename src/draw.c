/* Drawing the desktop from the state in A. Nothing here reads the mouse or keyboard. */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "edit.h"
#include "gamecols.h"
#include "games.h"
#include "gfx.h"
#include "util.h"

#define T (A.theme)

/* Small list icons, 8 pixels wide, drawn twice as wide (pixels are tall). x outline, o fill. */
static const char *const FOLDER[] = {
    ".xxx....", "xooxxxxx", "xoooooox", "xoooooox", "xoooooox", "xoooooox", "xxxxxxxx"
};
static const char *const FILE_ICON[] = {
    "xxxxx...", "xooxox..", "xooxxxx.", "xooooox.", "xooooox.", "xooooox.", "xxxxxxx."
};
static const char *const GAME_ICON[] = {
    "....xx..", "....xx..", "...xoox.", "....xx..", ".xxxxxxx", "xooxooox", "xxxxxxxx"
};
static const char *const CHECK[] = {
    ".......x", "......x.", "x....x..", ".x..x...", "..xx....", "........", "........"
};
#define ICON_ROWS 7

static uint16_t folder_line[ICON_ROWS], folder_fill[ICON_ROWS];
static uint16_t file_line[ICON_ROWS], file_fill[ICON_ROWS];
static uint16_t game_line[ICON_ROWS], game_fill[ICON_ROWS];
static uint16_t check_mask[ICON_ROWS], check_unused[ICON_ROWS];

/* Turn picture rows into bit masks, each source pixel 2 wide. */
static void make_masks(const char *const *rows, uint16_t *line, uint16_t *fill)
{
    int r, q;
    uint16_t two;

    for (r = 0; r < ICON_ROWS; r++) {
        line[r] = fill[r] = 0;
        for (q = 0; q < 8; q++) {
            two = (uint16_t)(0xC000u >> (q * 2));
            if (rows[r][q] == 'x')
                line[r] |= two;
            else if (rows[r][q] == 'o')
                fill[r] |= two;
        }
    }
}

void draw_init(void)
{
    make_masks(FOLDER, folder_line, folder_fill);
    make_masks(FILE_ICON, file_line, file_fill);
    make_masks(GAME_ICON, game_line, game_fill);
    make_masks(CHECK, check_mask, check_unused);
}

static int pct(int v, int p)
{
    return (v * p + 50) / 100;
}

static int imax(int a, int b)
{
    return a > b ? a : b;
}

static void hline(int x, int y, int w, uint8_t c)
{
    gfx_fill(x, y, w, 1, c);
}

static void vline(int x, int y, int h, uint8_t c)
{
    gfx_fill(x, y, 1, h, c);
}

/* A triangle n rows tall, growing 2 pixels a side per row (1 row is 2.4 pixels wide). */
static void triangle(int cx, int y0, int n, int up, uint8_t c)
{
    int r, k;

    for (r = 0; r < n; r++) {
        k = up ? r : n - 1 - r;
        hline(cx - k * 2, y0 + r, k * 4 + 1, c);
    }
}

static void shadow(int x, int y, int w, int h)
{
    gfx_fill(x + w, y + SHADOW_Y, SHADOW_X, h, T->shadow);
    gfx_fill(x + SHADOW_X, y + h, w, SHADOW_Y, T->shadow);
}

/* Disabled text: a colour of its own, or dithered where there isn't one (Paper). */
static void text_dim(int x, int y, const char *s, int dim, uint8_t c)
{
    if (!dim)
        gfx_text(x, y, s, c, 0);
    else if (T->disabled < 0)
        gfx_text(x, y, s, c, 1);
    else
        gfx_text(x, y, s, (uint8_t)T->disabled, 0);
}

/* ---- Menu bar and menus ---- */

void draw_clock(void)
{
    int x = SCREEN_W - 8 - gfx_text_w("00:00");

    gfx_fill(x, 0, gfx_text_w("00:00"), MENU_H, T->menu_bg);
    gfx_text(x, 2, A.clock, T->menu_text, 0);
}

void draw_menubar(void)
{
    int i, w;
    Menu *m;

    gfx_fill(0, 0, SCREEN_W, MENU_H, T->menu_bg);
    hline(0, MENU_H, SCREEN_W, T->frame);
    for (i = 0; i < NUM_MENUS; i++) {
        m = &A.menu[i];
        w = gfx_text_w(m->title);
        if (i == A.open_menu)
            gfx_fill(m->x - 8, 0, w + 16, MENU_H, T->sel);
        gfx_text(m->x, 2, m->title, i == A.open_menu ? T->sel_text : T->menu_text, 0);
    }
    draw_clock();
}

int menu_title_at(int x, int y)
{
    int i, w;

    if (y < 0 || y >= MENU_H)
        return -1;
    for (i = 0; i < NUM_MENUS; i++) {
        w = gfx_text_w(A.menu[i].title);
        if (x >= A.menu[i].x - 8 && x < A.menu[i].x + w + 8)
            return i;
    }
    return -1;
}

static int menu_has_check(const Menu *m)
{
    int i;

    for (i = 0; i < m->n; i++)
        if (m->items[i].flags & MI_CHECK)
            return 1;
    return 0;
}

static int text_left(const Menu *m)
{
    return menu_has_check(m) ? 24 : 8;
}

#define SEP_H 5
#define MENU_PAD 2

/* The box itself, without its shadow. */
static Rect menu_box(int mi)
{
    const Menu *m = &A.menu[mi];
    int i, h = MENU_PAD * 2, name_w = 0, key_w = 0, w;

    for (i = 0; i < m->n; i++) {
        if (m->items[i].flags & MI_SEP) {
            h += SEP_H;
            continue;
        }
        h += ROW_H + 1;
        name_w = imax(name_w, gfx_text_w(m->items[i].label));
        if (m->items[i].key)
            key_w = imax(key_w, gfx_text_w(m->items[i].key));
    }
    w = SNAP8_UP(text_left(m) + name_w + (key_w ? 24 + key_w : 0) + 8);
    if (m->y == 0) /* under its title on the menu bar */
        return rect_make(SNAP8(m->x - 8), MENU_H, w, h);
    /* the right-click menu: at the pointer, kept on the screen */
    return rect_make(imax(0, m->x + w + SHADOW_X > SCREEN_W ? SCREEN_W - w - SHADOW_X : m->x),
                     imax(MENU_H + 1, m->y + h + SHADOW_Y > SCREEN_H ? SCREEN_H - h - SHADOW_Y : m->y), w, h);
}

Rect menu_rect(int mi)
{
    Rect r = menu_box(mi);

    r.w += SHADOW_X;
    r.h += SHADOW_Y;
    return r;
}

void draw_menu(int mi)
{
    const Menu *m = &A.menu[mi];
    Rect r = menu_box(mi);
    int i, y, on, off, left = text_left(m);
    const MenuItem *it;
    uint8_t c;

    shadow(r.x, r.y, r.w, r.h);
    gfx_fill(r.x, r.y, r.w, r.h, T->menu_bg);
    gfx_frame(r.x, r.y, r.w, r.h, T->frame);
    y = r.y + MENU_PAD;
    for (i = 0; i < m->n; i++) {
        it = &m->items[i];
        if (it->flags & MI_SEP) {
            if (T->separator < 0)
                gfx_pattern(r.x + 1, y + SEP_H / 2, r.w - 2, 1, PAT_DOTS, T->frame, T->menu_bg);
            else
                hline(r.x + 1, y + SEP_H / 2, r.w - 2, (uint8_t)T->separator);
            y += SEP_H;
            continue;
        }
        off = (it->flags & MI_OFF) != 0;
        on = i == A.hot && !off;
        if (on)
            gfx_fill(r.x + 1, y, r.w - 2, ROW_H + 1, T->sel);
        c = on ? T->sel_text : T->menu_text;
        if (it->flags & MI_CHECK)
            gfx_mask(r.x + 6, y + 2, check_mask, ICON_ROWS - 1, 16, c);
        text_dim(r.x + left, y + 1, it->label, off, c);
        if (it->key)
            text_dim(r.x + r.w - 8 - gfx_text_w(it->key), y + 1, it->key, off, c);
        y += ROW_H + 1;
    }
}

int menu_item_at(int mi, int x, int y)
{
    const Menu *m = &A.menu[mi];
    Rect r = menu_box(mi);
    int i, top = r.y + MENU_PAD;

    if (!rect_contains(&r, x, y))
        return -1;
    for (i = 0; i < m->n; i++) {
        if (m->items[i].flags & MI_SEP) {
            top += SEP_H;
            continue;
        }
        if (y >= top && y < top + ROW_H + 1)
            return (m->items[i].flags & MI_OFF) ? -1 : i;
        top += ROW_H + 1;
    }
    return -1;
}

/* ---- Desktop icons ---- */

#define ICON_W 40
#define ICON_H 18
#define ICON_CX (SCREEN_W - 44)

static Rect label_rect(int i)
{
    int w = gfx_text_w(A.icon[i].label) + 8;

    return rect_make(ICON_CX - w / 2, A.icon[i].y + ICON_H + 2, w, 10);
}

Rect icon_bounds(int i)
{
    Rect box = rect_make(ICON_CX - ICON_W / 2, A.icon[i].y, ICON_W, ICON_H);
    Rect label = label_rect(i);

    return rect_union(&box, &label);
}

void draw_icon(int i)
{
    int x = ICON_CX - ICON_W / 2, y = A.icon[i].y, w = ICON_W, h = ICON_H;
    int sx, sw, sh, lx, ly, lw, lh, bx, bw, bt, bh, k;
    Rect lr;

    switch (A.icon[i].kind) {
    case ICON_FLOPPY:
        gfx_fill(x, y, w, h, T->icon_floppy);
        gfx_frame(x, y, w, h, T->icon_line);
        sx = x + pct(w, 28);
        sw = pct(w, 44);
        sh = pct(h, 38);
        gfx_fill(sx, y, sw, sh, T->icon_shutter);
        gfx_frame(sx, y, sw, sh, T->icon_line);
        gfx_fill(sx + pct(sw, 60), y + pct(sh, 25), imax(1, pct(sw, 16)), imax(1, pct(sh, 50)), T->icon_line);
        lx = x + pct(w, 14);
        ly = y + pct(h, 52);
        lw = w - 2 * (lx - x);
        lh = y + h - pct(h, 8) - ly;
        gfx_fill(lx, ly, lw, lh, T->icon_label);
        gfx_frame(lx, ly, lw, lh, T->icon_line);
        hline(lx + 4, ly + pct(lh, 45), lw - 8, T->icon_detail);
        break;
    case ICON_DISK:
        bt = y + pct(h, 22);
        bh = pct(h, 60);
        gfx_fill(x, bt, w, bh, T->icon_fill);
        gfx_frame(x, bt, w, bh, T->icon_line);
        hline(x, bt + pct(bh, 62), w, T->icon_line);
        gfx_fill(x + w - 9, bt + pct(bh, 74), 4, imax(1, pct(bh, 14)), T->icon_led);
        hline(x + 4, bt + pct(bh, 30), pct(w, 50), T->icon_detail);
        break;
    case ICON_GAMES: /* a joystick */
        bx = x + pct(w, 12);
        bw = pct(w, 76);
        bt = y + pct(h, 60);
        bh = y + h - bt;
        gfx_fill(bx, bt, bw, bh, T->icon_fill);
        gfx_frame(bx, bt, bw, bh, T->icon_line);
        gfx_fill(bx + 4, bt + 2, 6, imax(1, bh - 4), T->icon_led);
        gfx_fill(x + w / 2 - 1, y + pct(h, 25), 3, bt - y - pct(h, 25), T->icon_line);
        gfx_fill(x + w / 2 - 6, y, 13, pct(h, 30), T->icon_detail);
        gfx_frame(x + w / 2 - 6, y, 13, pct(h, 30), T->icon_line);
        break;
    default:
        gfx_fill(x + pct(w, 15), y + pct(h, 12), pct(w, 70), imax(1, pct(h, 8)), T->icon_line);
        gfx_frame(x + pct(w, 40), y, pct(w, 20), pct(h, 14), T->icon_line);
        bx = x + pct(w, 22);
        bw = pct(w, 56);
        bt = y + pct(h, 24);
        bh = y + h - bt;
        gfx_fill(bx, bt, bw, bh, T->icon_fill);
        gfx_frame(bx, bt, bw, bh, T->icon_line);
        for (k = 1; k < 4; k++)
            vline(bx + pct(bw, k * 25), bt + 2, bh - 4, T->icon_detail);
        break;
    }

    lr = label_rect(i);
    if (i == A.sel_icon)
        gfx_fill(lr.x, lr.y, lr.w, lr.h, T->sel);
    else if (T->label_bg >= 0)
        gfx_fill(lr.x, lr.y, lr.w, lr.h, (uint8_t)T->label_bg);
    gfx_text(lr.x + 4, lr.y + 1, A.icon[i].label, i == A.sel_icon ? T->sel_text : T->label_text, 0);
}

/* ---- Windows ---- */

int front_window(void)
{
    int k;

    for (k = NUM_WINS - 1; k >= 0; k--)
        if (A.win[A.z[k]].open)
            return A.z[k];
    return -1;
}

Rect window_bounds(int i)
{
    Rect r = A.win[i].r;

    r.w += SHADOW_X;
    r.h += SHADOW_Y;
    return r;
}

void window_parts(int i, WinParts *p)
{
    const Rect *r = &A.win[i].r;
    int ix = r->x + 1, iy = r->y + 1, iw = r->w - 2, bottom = r->y + r->h - 1, top;

    p->title = rect_make(ix, iy, iw, TITLE_H);
    p->close = rect_make(ix + 8, iy + 3, 12, 5);
    top = iy + TITLE_H + 1;
    p->info = rect_make(ix, top, iw, ROW_H + 1);
    top += ROW_H + 2;
    p->header = rect_make(ix, top, 0, 0);
    if (A.win[i].kind == WIN_GAMES) {
        p->header = rect_make(ix, top, iw, ROW_H + 1);
        top += ROW_H + 2;
    }
    p->scroll = rect_make(ix + iw - SCROLL_W, top, SCROLL_W, bottom - top);
    p->up = rect_make(p->scroll.x + 1, top, SCROLL_W - 1, ARROW_H);
    p->down = rect_make(p->scroll.x + 1, bottom - ARROW_H, SCROLL_W - 1, ARROW_H);
    p->track = rect_make(p->scroll.x + 1, top + ARROW_H + 1, SCROLL_W - 1,
                         (bottom - ARROW_H - 1) - (top + ARROW_H + 1));
    p->list = rect_make(ix, top, iw - SCROLL_W, bottom - top);
    p->visible = (p->list.h - 2) / ROW_H;
}

static void draw_scrollbar(const Win *w, const WinParts *p, int active)
{
    int th, ty;

    vline(p->scroll.x, p->scroll.y, p->scroll.h, T->frame);
    if (!active && T->stripes) { /* Paper leaves an inactive window's scroll bar empty */
        gfx_fill(p->scroll.x + 1, p->scroll.y, SCROLL_W - 1, p->scroll.h, T->win_bg);
        return;
    }
    gfx_pattern(p->scroll.x + 1, p->scroll.y, SCROLL_W - 1, p->scroll.h, PAT_CHECKER, T->track_on, T->track_off);
    gfx_fill(p->up.x, p->up.y, p->up.w, p->up.h, T->win_bg);
    hline(p->scroll.x, p->up.y + ARROW_H, SCROLL_W, T->frame);
    triangle(p->scroll.x + 8, p->up.y + 2, 3, 1, T->frame);
    gfx_fill(p->down.x, p->down.y, p->down.w, p->down.h, T->win_bg);
    hline(p->scroll.x, p->down.y - 1, SCROLL_W, T->frame);
    triangle(p->scroll.x + 8, p->down.y + 2, 3, 0, T->frame);
    if (w->nrows <= p->visible)
        return; /* nothing to scroll: no thumb */
    th = imax(6, (int)((long)p->track.h * p->visible / w->nrows));
    ty = p->track.y + (int)((long)(p->track.h - th) * w->top / (w->nrows - p->visible));
    gfx_fill(p->track.x, ty, p->track.w, th, T->thumb);
    gfx_frame(p->scroll.x, ty - 1, SCROLL_W, th + 2, T->frame);
}

enum { ROW_FILE, ROW_DIR, ROW_GAME };

/* A row's name, right-hand column (size or year) and icon. */
#define NAME_X(p) ((p)->list.x + 8 + 16 + 8) /* after the icon */
#define MIN_NAME 12

unsigned games_layout(const WinParts *p, int *col_x, int *name_chars)
{
    unsigned shown = S.game_cols;
    int c, used, avail = (p->list.x + p->list.w - 8 - NAME_X(p)) / 8, x;

    for (;;) {
        used = 0;
        for (c = 0; c < NUM_COLS; c++)
            if (shown & (1u << c))
                used += game_columns[c].width + 2;
        if (avail - used >= MIN_NAME || shown == 0)
            break;
        for (c = NUM_COLS - 1; c >= 0 && !(shown & (1u << c)); c--)
            ;
        shown &= ~(1u << c); /* too many: drop the rightmost */
    }
    *name_chars = avail - used;
    x = NAME_X(p) + *name_chars * 8 + 16;
    for (c = 0; c < NUM_COLS; c++) {
        col_x[c] = -1;
        if (shown & (1u << c)) {
            col_x[c] = x;
            x += (game_columns[c].width + 2) * 8;
        }
    }
    return shown;
}

int games_heading_at(const WinParts *p, int x)
{
    int col_x[NUM_COLS], name_chars, c;

    games_layout(p, col_x, &name_chars);
    for (c = NUM_COLS - 1; c >= 0; c--)
        if (col_x[c] >= 0 && x >= col_x[c] - 8)
            return c;
    return x >= p->list.x ? COL_NAME : -2;
}

/* The Games window's headings; the column it's sorted by is highlighted. */
static void draw_headings(const WinParts *p)
{
    int col_x[NUM_COLS], name_chars, c, sorted = games_sort_column(), x, w;
    const char *title;

    games_layout(p, col_x, &name_chars);
    gfx_fill(p->header.x, p->header.y, p->header.w, p->header.h, T->menu_bg);
    for (c = COL_NAME; c < NUM_COLS; c++) {
        if (c != COL_NAME && col_x[c] < 0)
            continue;
        title = c == COL_NAME ? "Name" : game_columns[c].title;
        x = c == COL_NAME ? NAME_X(p) : col_x[c];
        if (c != COL_NAME && game_columns[c].right)
            x += (game_columns[c].width - (int)strlen(title)) * 8;
        w = gfx_text_w(title);
        if (c == sorted)
            gfx_fill(x - 4, p->header.y, w + 8, p->header.h, T->sel);
        gfx_text(x, p->header.y + 1, title, c == sorted ? T->sel_text : T->menu_text, 0);
    }
    hline(p->header.x, p->header.y + p->header.h, p->header.w, T->frame);
}

static void row_text(const Win *w, int k, char *name, char *right, int *kind)
{
    if (w->kind == WIN_GAMES) {
        Game g;

        games_get(k, &g);
        strcpy(name, g.name);
        strcpy(right, g.year);
        *kind = ROW_GAME;
        return;
    }
    strcpy(name, w->rows[k].name);
    right[0] = '\0';
    if (!w->rows[k].dir)
        fmt_thousands(w->rows[k].size, right);
    *kind = w->rows[k].dir ? ROW_DIR : ROW_FILE;
}

/* The editor's window below its title bar: where the caret is, the text, the caret. */
static void draw_editor(const Win *w, const WinParts *p, int active)
{
    char buf[40], text[100];
    int row, col, cr, cc, ry, x0 = p->list.x + 4;
    unsigned len = tb_len(&E.t);
    Rect old_clip, list;
    Win view = *w;

    sprintf(buf, "Line %u, Col %u", E.cur_line + 1, tb_col(&E.t, E.cur) + 1);
    gfx_text(p->info.x + 8, p->info.y + 1, buf, T->text, 0);
    if (E.modified)
        strcpy(buf, "Not saved");
    else
        fmt_thousands((long)len, buf), strcat(buf, " characters");
    gfx_text(p->info.x + p->info.w - 8 - gfx_text_w(buf), p->info.y + 1, buf, T->text, 0);
    hline(p->info.x, p->info.y + ROW_H + 1, p->info.w, T->frame);

    view.nrows = (int)E.lines; /* the scroll bar follows the view */
    view.top = (int)E.top_line;
    draw_scrollbar(&view, p, active);

    old_clip = gfx_get_clip();
    rect_intersect(&list, &p->list, &old_clip);
    if (rect_empty(&list))
        return;
    gfx_set_clip(&list);
    for (row = 0; row < E.rows; row++) {
        ry = p->list.y + 2 + row * ROW_H;
        if (ry >= list.y + list.h || ry + ROW_H <= list.y)
            continue;
        if (edit_row_text(row, text) < 0)
            break;
        gfx_text(x0, ry, text, T->text, 0);
    }
    if (edit_caret(&cr, &cc) && active) { /* the character under the caret, inverted */
        ry = p->list.y + 2 + cr * ROW_H;
        edit_row_text(cr, text);
        col = cc;
        gfx_fill(x0 + col * 8, ry - 1, 8, ROW_H, T->sel);
        if (col < (int)strlen(text)) {
            buf[0] = text[col];
            buf[1] = '\0';
            gfx_text(x0 + col * 8, ry, buf, T->sel_text, 0);
        }
    }
    gfx_set_clip(&old_clip);
}

/* The Games window's rows: the icon, the name, then the columns chosen. */
static void draw_game_rows(const Win *w, const WinParts *p)
{
    int col_x[NUM_COLS], name_chars, c, k, ry, x;
    unsigned shown = games_layout(p, col_x, &name_chars);
    char text[40];
    Rect old_clip, list;
    uint8_t col;
    Game g;

    old_clip = gfx_get_clip();
    rect_intersect(&list, &p->list, &old_clip);
    if (rect_empty(&list))
        return;
    gfx_set_clip(&list);
    for (k = w->top; k < w->nrows && k - w->top <= p->visible; k++) {
        int sel = k == w->sel;

        games_get(k, &g);
        ry = p->list.y + 2 + (k - w->top) * ROW_H;
        if (ry > list.y + list.h || ry + ROW_H < list.y)
            continue;
        if (sel)
            gfx_fill(p->list.x, ry - 1, p->list.w, ROW_H, T->sel);
        col = sel ? T->sel_text : T->text;
        gfx_mask(p->list.x + 8, ry, game_fill, ICON_ROWS, 16, sel ? T->sel : T->file_fill);
        gfx_mask(p->list.x + 8, ry, game_line, ICON_ROWS, 16, sel ? T->sel_text : T->file_line);
        if ((int)strlen(g.name) > name_chars)
            g.name[name_chars] = '\0';
        gfx_text(NAME_X(p), ry, g.name, col, 0);
        for (c = 0; c < NUM_COLS; c++) {
            if (!(shown & (1u << c)))
                continue;
            gc_text(&g, c, text);
            if ((int)strlen(text) > game_columns[c].width)
                text[game_columns[c].width] = '\0';
            x = col_x[c];
            if (game_columns[c].right)
                x += (game_columns[c].width - (int)strlen(text)) * 8;
            gfx_text(x, ry, text, col, 0);
        }
    }
    gfx_set_clip(&old_clip);
}

void draw_window(int i)
{
    const Win *w = &A.win[i];
    WinParts p;
    int active = i == front_window(), x = w->r.x, y = w->r.y;
    int r, k, tx, tw, ry, nx, name_w = 0;
    int right_w = 0, right_x, show_right, max_chars;
    uint8_t tc, c;
    char buf[24], right[24], name[40], title[72];
    const char *info_right;
    Rect old_clip, list;

    window_parts(i, &p);
    shadow(x, y, w->r.w, w->r.h);
    gfx_fill(x, y, w->r.w, w->r.h, T->win_bg);
    gfx_frame(x, y, w->r.w, w->r.h, T->frame);

    /* Title bar */
    if (T->stripes) {
        gfx_fill(p.title.x, p.title.y, p.title.w, TITLE_H, T->win_bg);
        if (active)
            for (r = 2; r < TITLE_H - 2; r += 2)
                hline(p.title.x + 2, p.title.y + r, p.title.w - 4, T->frame);
        tc = T->text;
    } else {
        gfx_fill(p.title.x, p.title.y, p.title.w, TITLE_H, active ? T->title_on : T->title_off);
        tc = active ? T->title_on_text : T->title_off_text;
    }
    hline(p.title.x, p.title.y + TITLE_H, p.title.w, T->frame);
    /* The title, cut from the left if it's too long: "...\GAMES\KEEN\" */
    k = (p.title.w - 64) / 8;
    if ((int)strlen(w->title) > k && k > 4)
        sprintf(title, "...%s", w->title + strlen(w->title) - (k - 3));
    else
        strncpy(title, w->title, sizeof title - 1), title[sizeof title - 1] = '\0';
    if (w->kind == WIN_EDIT && E.modified && strlen(title) < sizeof title - 3)
        strcat(title, " *");
    tw = gfx_text_w(title);
    tx = p.title.x + (p.title.w - tw) / 2;
    if (T->stripes && active)
        gfx_fill(tx - 8, p.title.y + 1, tw + 16, TITLE_H - 2, T->win_bg);
    gfx_text(tx, p.title.y + 2, title, tc, 0);
    if (!T->stripes || active) {
        if (T->stripes)
            gfx_fill(p.close.x - 3, p.title.y + 1, p.close.w + 6, TITLE_H - 2, T->win_bg);
        else
            gfx_fill(p.close.x, p.close.y, p.close.w, p.close.h, active ? T->title_on : T->title_off);
        gfx_frame(p.close.x, p.close.y, p.close.w, p.close.h, tc);
    }

    if (w->kind == WIN_EDIT) {
        draw_editor(w, &p, active);
        return;
    }

    /* Info line */
    if (w->kind == WIN_GAMES) {
        sprintf(buf, "%d game%s", w->nrows, w->nrows == 1 ? "" : "s");
        info_right = S.scan_path;
    } else if (w->not_ready) {
        sprintf(buf, "%c: isn't ready", w->path[0]);
        info_right = "";
    } else {
        sprintf(buf, "%s%d item%s", w->truncated ? "First " : "", w->nrows, w->nrows == 1 ? "" : "s");
        fmt_thousands(w->free_kb, right);
        strcat(right, " KB free");
        info_right = right;
    }
    gfx_text(p.info.x + 8, p.info.y + 1, buf, T->text, 0);
    if (gfx_text_w(info_right) + 16 + 9 * 8 < p.info.w)
        gfx_text(p.info.x + p.info.w - 8 - gfx_text_w(info_right), p.info.y + 1, info_right, T->text, 0);
    hline(p.info.x, p.info.y + ROW_H + 1, p.info.w, T->frame);

    draw_scrollbar(w, &p, active);

    if (w->kind == WIN_GAMES) {
        draw_headings(&p);
        draw_game_rows(w, &p);
        return;
    }

    /* Rows, clipped to the list. The right column (sizes) shows if it fits beside the widest
     * name; names too long for the space are cut short. */
    {
        for (k = 0; k < w->nrows; k++) {
            name_w = imax(name_w, gfx_text_w(w->rows[k].name));
            fmt_thousands(w->rows[k].size, buf);
            if (!w->rows[k].dir)
                right_w = imax(right_w, gfx_text_w(buf));
        }
    }
    nx = p.list.x + 8 + 16 + 8;
    right_x = p.list.x + p.list.w - 8;
    show_right = nx + name_w + 16 + right_w <= right_x;
    max_chars = ((show_right ? right_x - right_w - 16 : right_x) - nx) / 8;

    old_clip = gfx_get_clip();
    rect_intersect(&list, &p.list, &old_clip);
    if (!rect_empty(&list)) {
        gfx_set_clip(&list);
        for (k = w->top; k < w->nrows && k - w->top <= p.visible; k++) {
            int sel = k == w->sel, kind;
            const uint16_t *line_mask, *fill_mask;

            row_text(w, k, name, right, &kind);
            if ((int)strlen(name) > max_chars && max_chars > 0)
                name[max_chars] = '\0';
            ry = p.list.y + 2 + (k - w->top) * ROW_H;
            if (sel)
                gfx_fill(p.list.x, ry - 1, p.list.w, ROW_H, T->sel);
            c = sel ? T->sel_text : T->text;
            line_mask = kind == ROW_DIR ? folder_line : kind == ROW_GAME ? game_line : file_line;
            fill_mask = kind == ROW_DIR ? folder_fill : kind == ROW_GAME ? game_fill : file_fill;
            gfx_mask(p.list.x + 8, ry, fill_mask, ICON_ROWS, 16,
                     sel ? T->sel : (kind == ROW_DIR ? T->folder_fill : T->file_fill));
            gfx_mask(p.list.x + 8, ry, line_mask, ICON_ROWS, 16,
                     sel ? T->sel_text : (kind == ROW_DIR ? T->folder_line : T->file_line));
            gfx_text(nx, ry, name, c, 0);
            if (show_right && right[0])
                gfx_text(right_x - gfx_text_w(right), ry, right, c, 0);
        }
        gfx_set_clip(&old_clip);
    }
}

/* ---- Everything under the menus ---- */

void draw_desktop(const Rect *area)
{
    Rect clip, r;
    int k, i;

    gfx_set_clip(area);
    clip = gfx_get_clip();
    if (rect_empty(&clip))
        return;
    gfx_pattern(clip.x, clip.y, clip.w, clip.h, PAT_CHECKER, T->desk_on, T->desk_off);
    for (i = 0; i < NUM_ICONS; i++) {
        r = icon_bounds(i);
        if (rect_intersect(&r, &r, &clip))
            draw_icon(i);
    }
    for (k = 0; k < NUM_WINS; k++) {
        i = A.z[k];
        r = window_bounds(i);
        if (A.win[i].open && rect_intersect(&r, &r, &clip))
            draw_window(i);
    }
    r = rect_make(0, 0, SCREEN_W, MENU_H + 1);
    if (rect_intersect(&r, &r, &clip))
        draw_menubar();
    gfx_set_clip(NULL);
}

/* ---- Alerts ---- */

#define ALERT_ICON_W 32
#define ALERT_ICON_H 13
#define ALERT_PAD 16
#define BUTTON_H 12

void alert_layout(void)
{
    Alert *a = &A.alert;
    int line_w = imax(gfx_text_w(a->line1), gfx_text_w(a->line2));
    int bw = 0, i, w, h, x, y, body = imax(ALERT_ICON_H, 2 * ROW_H), by, bx;

    for (i = 0; i < a->nbuttons; i++)
        bw = imax(bw, gfx_text_w(a->button[i]) + 16);
    w = SNAP8_UP(ALERT_PAD * 2 + ALERT_ICON_W + 12 + line_w);
    w = imax(w, SNAP8_UP(ALERT_PAD * 2 + a->nbuttons * bw + (a->nbuttons - 1) * 16));
    h = 8 + 8 * 3 + body + BUTTON_H;
    x = SNAP8((SCREEN_W - w) / 2);
    y = (SCREEN_H - h) / 2 + 6;
    a->r = rect_make(x, y, w, h);
    by = y + 3 + 8 + body + 8;
    bx = x + w - ALERT_PAD - bw;
    for (i = a->nbuttons - 1; i >= 0; i--) {
        a->b[i] = rect_make(bx, by, bw, BUTTON_H);
        bx -= bw + 16;
    }
}

/* A frame with its corners cut: 2 pixels across, 1 down (the pixels are 2.4 times taller). */
static void round_frame(int x, int y, int w, int h, uint8_t bg)
{
    gfx_frame(x, y, w, h, T->frame);
    gfx_fill(x, y, 2, 1, bg);
    gfx_fill(x + w - 2, y, 2, 1, bg);
    gfx_fill(x, y + h - 1, 2, 1, bg);
    gfx_fill(x + w - 2, y + h - 1, 2, 1, bg);
}

static void button(const Rect *b, const char *label, int def)
{
    uint8_t bg = T->dialog_bg;

    gfx_fill(b->x, b->y, b->w, b->h, bg);
    round_frame(b->x, b->y, b->w, b->h, bg);
    if (def) { /* the default button: a thick ring around it */
        round_frame(b->x - 4, b->y - 3, b->w + 8, b->h + 6, bg);
        vline(b->x - 3, b->y - 1, b->h + 2, T->frame);
        vline(b->x + b->w + 2, b->y - 1, b->h + 2, T->frame);
        hline(b->x - 2, b->y - 2, b->w + 4, T->frame);
        hline(b->x - 2, b->y + b->h + 1, b->w + 4, T->frame);
    }
    gfx_text(b->x + (b->w - gfx_text_w(label)) / 2, b->y + 2, label, T->text, 0);
}

void draw_alert(void)
{
    const Alert *a = &A.alert;
    const Rect *r = &a->r;
    int body = imax(ALERT_ICON_H, 2 * ROW_H), cy = r->y + 3 + 8, ax = r->x + ALERT_PAD, tx, ly, i;

    shadow(r->x, r->y, r->w, r->h);
    gfx_fill(r->x, r->y, r->w, r->h, T->dialog_bg);
    gfx_frame(r->x, r->y, r->w, r->h, T->frame);
    vline(r->x + 1, r->y, r->h, T->frame);
    vline(r->x + r->w - 2, r->y, r->h, T->frame);
    gfx_frame(r->x + 4, r->y + 3, r->w - 8, r->h - 6, T->frame);

    gfx_fill(ax, cy, ALERT_ICON_W, ALERT_ICON_H, T->alert_bg);
    gfx_frame(ax, cy, ALERT_ICON_W, ALERT_ICON_H, T->frame);
    gfx_text(ax + (ALERT_ICON_W - 8) / 2, cy + 3, "!", T->alert_text, 0);

    tx = ax + ALERT_ICON_W + 12;
    ly = cy + (body - 2 * ROW_H) / 2 + 1;
    gfx_text(tx, ly, a->line1, T->text, 0);
    gfx_text(tx, ly + ROW_H, a->line2, T->text, 0);
    for (i = 0; i < a->nbuttons; i++)
        button(&a->b[i], a->button[i], i == a->nbuttons - 1);
}

int alert_button_at(int x, int y)
{
    int i;

    for (i = 0; i < A.alert.nbuttons; i++)
        if (rect_contains(&A.alert.b[i], x, y))
            return i;
    return -1;
}
