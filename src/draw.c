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

static int isqrt(long v)
{
    long r = 0, b = 1L << 14;

    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return (int)r;
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
    int i;

    shadow(r.x, r.y, r.w, r.h);
    gfx_fill(r.x, r.y, r.w, r.h, T->menu_bg);
    gfx_frame(r.x, r.y, r.w, r.h, T->frame);
    for (i = 0; i < m->n; i++)
        draw_menu_item(mi, i);
}

/* The top of item i in a menu */
static int item_y(const Menu *m, const Rect *r, int i)
{
    int k, y = r->y + MENU_PAD;

    for (k = 0; k < i; k++)
        y += (m->items[k].flags & MI_SEP) ? SEP_H : ROW_H + 1;
    return y;
}

/* One item: moving the highlight redraws only the two items it leaves and reaches, not the
 * whole menu (which would flash). */
void draw_menu_item(int mi, int i)
{
    const Menu *m = &A.menu[mi];
    Rect r = menu_box(mi);
    const MenuItem *it = &m->items[i];
    int y = item_y(m, &r, i), on, off, left = text_left(m);
    uint8_t c;

    if (it->flags & MI_SEP) {
        if (T->separator < 0)
            gfx_pattern(r.x + 1, y + SEP_H / 2, r.w - 2, 1, PAT_DOTS, T->frame, T->menu_bg);
        else
            hline(r.x + 1, y + SEP_H / 2, r.w - 2, (uint8_t)T->separator);
        return;
    }
    off = (it->flags & MI_OFF) != 0;
    on = i == A.hot && !off;
    gfx_fill(r.x + 1, y, r.w - 2, ROW_H + 1, on ? T->sel : T->menu_bg);
    c = on ? T->sel_text : T->menu_text;
    if (it->flags & MI_CHECK)
        gfx_mask(r.x + 6, y + 2, check_mask, ICON_ROWS - 1, 16, c);
    text_dim(r.x + left, y + 1, it->label, off, c);
    if (it->key)
        text_dim(r.x + r.w - 8 - gfx_text_w(it->key), y + 1, it->key, off, c);
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

static Rect label_rect(int i)
{
    int w = gfx_text_w(A.icon[i].label) + 8;

    return rect_make(A.icon[i].x - w / 2, A.icon[i].y + ICON_H + 2, w, 10);
}

Rect icon_bounds(int i)
{
    Rect box = rect_make(A.icon[i].x - ICON_W / 2, A.icon[i].y, ICON_W, ICON_H);
    Rect label = label_rect(i);

    return rect_union(&box, &label);
}

void draw_icon(int i)
{
    int x = A.icon[i].x - ICON_W / 2, y = A.icon[i].y, w = ICON_W, h = ICON_H;
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
    case ICON_CDROM: /* a disc: an ellipse (round on the screen), with a hole */
        for (k = 0; k < h; k++) {
            int dy = 2 * k + 1 - h, half = isqrt((long)h * h - (long)dy * dy) * w / (2 * h);

            hline(x + w / 2 - half, y + k, half * 2, k == 0 || k == h - 1 ? T->icon_line : T->icon_fill);
            gfx_fill(x + w / 2 - half, y + k, 1, 1, T->icon_line);
            gfx_fill(x + w / 2 + half - 1, y + k, 1, 1, T->icon_line);
        }
        gfx_fill(x + w / 2 - 4, y + h / 2 - 1, 8, 3, T->icon_line);
        hline(x + w / 2 + 6, y + pct(h, 30), pct(w, 20), T->icon_detail);
        break;
    case ICON_NETWORK: /* a disk on a wire */
        bt = y + pct(h, 5);
        bh = pct(h, 50);
        gfx_fill(x, bt, w, bh, T->icon_fill);
        gfx_frame(x, bt, w, bh, T->icon_line);
        gfx_fill(x + w - 9, bt + pct(bh, 60), 4, imax(1, pct(bh, 20)), T->icon_led);
        gfx_fill(x + w / 2 - 1, bt + bh, 2, y + h - 2 - bt - bh, T->icon_line);
        hline(x + 2, y + h - 2, w - 4, T->icon_line);
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
    default: /* ICON_GAMES: a joystick */
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
    p->grow = rect_make(p->scroll.x, bottom - GROW_H, SCROLL_W, GROW_H);
    p->down = rect_make(p->scroll.x + 1, p->grow.y - 1 - ARROW_H, SCROLL_W - 1, ARROW_H);
    p->track = rect_make(p->scroll.x + 1, top + ARROW_H + 1, SCROLL_W - 1,
                         (p->down.y - 1) - (top + ARROW_H + 1));
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
    /* Each pixel once: the arrow boxes, the size box, and the track around the thumb. */
    gfx_fill(p->up.x, p->up.y, p->up.w, p->up.h, T->win_bg);
    hline(p->scroll.x, p->up.y + ARROW_H, SCROLL_W, T->frame);
    triangle(p->scroll.x + 8, p->up.y + 2, 3, 1, T->frame);
    gfx_fill(p->down.x, p->down.y, p->down.w, p->down.h, T->win_bg);
    hline(p->scroll.x, p->down.y - 1, SCROLL_W, T->frame);
    triangle(p->scroll.x + 8, p->down.y + 2, 3, 0, T->frame);
    /* The size box: one square overlapping a bigger one */
    gfx_fill(p->grow.x + 1, p->grow.y, SCROLL_W - 1, GROW_H, T->win_bg);
    hline(p->scroll.x, p->grow.y - 1, SCROLL_W, T->frame);
    gfx_frame(p->grow.x + 4, p->grow.y + 2, 10, 5, T->frame);
    gfx_fill(p->grow.x + 4, p->grow.y + 2, 6, 3, T->win_bg);
    gfx_frame(p->grow.x + 4, p->grow.y + 2, 6, 3, T->frame);
    if (w->nrows <= p->visible) { /* nothing to scroll: no thumb */
        gfx_pattern(p->track.x, p->track.y, p->track.w, p->track.h, PAT_CHECKER, T->track_on, T->track_off);
        return;
    }
    th = imax(6, (int)((long)p->track.h * p->visible / w->nrows));
    ty = p->track.y + (int)((long)(p->track.h - th) * w->top / (w->nrows - p->visible));
    gfx_pattern(p->track.x, p->track.y, p->track.w, ty - 1 - p->track.y, PAT_CHECKER, T->track_on, T->track_off);
    gfx_pattern(p->track.x, ty + th + 1, p->track.w, p->track.y + p->track.h - (ty + th + 1), PAT_CHECKER,
                T->track_on, T->track_off);
    gfx_fill(p->track.x, ty, p->track.w, th, T->thumb);
    gfx_frame(p->scroll.x, ty - 1, SCROLL_W, th + 2, T->frame);
}

enum { ROW_FILE, ROW_DIR, ROW_GAME };
#define DATE_W (10 * 8) /* "1994/03/12" */

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

/* Does r touch the clip rectangle? A redraw is often a row or a character: the parts of a
 * window it doesn't touch aren't worked out at all (their text formatted, their sums done). */
static int touches(int x, int y, int w, int h)
{
    Rect r = rect_make(x, y, w, h), clip = gfx_get_clip();

    return rect_intersect(&r, &r, &clip);
}

/* Characters from..to-1 of a row's text (n long), at their columns. */
static void text_span(int x0, int y, const char *text, int n, int from, int to, uint8_t c)
{
    char part[100];

    if (to > n)
        to = n;
    if (from >= to)
        return;
    memcpy(part, text + from, (size_t)(to - from));
    part[to - from] = '\0';
    gfx_text(x0 + from * 8, y, part, c, 0);
}

/* The editor's window below its title bar: where the caret is, the text, the caret. */
static void draw_editor(const Win *w, const WinParts *p, int active)
{
    char buf[40], text[100];
    int row, col, cr, cc, ry, x0 = p->list.x + 4;
    unsigned len = tb_len(&E.t), sel_start, sel_end, line;
    Rect old_clip, list;
    Win view = *w;

    if (touches(p->info.x, p->info.y, p->info.w, ROW_H + 2)) {
        sprintf(buf, "Line %u, Col %u", E.cur_line + 1, tb_col(&E.t, E.cur) + 1);
        gfx_text(p->info.x + 8, p->info.y + 1, buf, T->text, 0);
        if (E.modified)
            strcpy(buf, "Not saved");
        else
            fmt_thousands((long)len, buf), strcat(buf, " characters");
        gfx_text(p->info.x + p->info.w - 8 - gfx_text_w(buf), p->info.y + 1, buf, T->text, 0);
        hline(p->info.x, p->info.y + ROW_H + 1, p->info.w, T->frame);
    }
    if (touches(p->scroll.x, p->scroll.y, p->scroll.w, p->scroll.h)) {
        view.nrows = (int)E.lines; /* the scroll bar follows the view */
        view.top = (int)E.top_line;
        draw_scrollbar(&view, p, active);
    }

    old_clip = gfx_get_clip();
    rect_intersect(&list, &p->list, &old_clip);
    if (rect_empty(&list))
        return;
    gfx_set_clip(&list);
    for (row = 0, line = edit_view_start(); row < E.rows && line != (unsigned)-1; row++, line = edit_next_line(line)) {
        int c0, c1, n;

        ry = p->list.y + 2 + row * ROW_H;
        if (ry - 1 >= list.y + list.h)
            break; /* below the clip: no more */
        if (ry - 1 + ROW_H <= list.y)
            continue; /* above it: only the line's start is needed, for the next */
        n = edit_line_text(line, text);
        if (!edit_line_selection(line, &c0, &c1)) {
            gfx_text(x0, ry, text, T->text, 0);
            continue;
        }
        /* before the selection, the selection inverted, after it: each drawn once */
        gfx_fill(x0 + c0 * 8, ry - 1, (c1 - c0) * 8, ROW_H, T->sel);
        text_span(x0, ry, text, n, 0, c0, T->text);
        text_span(x0, ry, text, n, c0, c1, T->sel_text);
        text_span(x0, ry, text, n, c1, n, T->text);
    }
    if (edit_selection(&sel_start, &sel_end)) /* a selection shows instead of the caret */
        active = 0;
    if (edit_caret(&cr, &cc) && active && touches(x0 + cc * 8, p->list.y + 1 + cr * ROW_H, 8, ROW_H)) {
        /* the character under the caret, inverted */
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

        ry = p->list.y + 2 + (k - w->top) * ROW_H;
        if (ry - 1 >= list.y + list.h)
            break;
        if (ry - 1 + ROW_H <= list.y)
            continue;
        games_get(k, &g);
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
    int r, k, tx, tw, ry, nx, name_w;
    int right_w, right_x, show_right, max_chars, date_x, show_date;
    uint8_t tc, c;
    char buf[24], right[24], name[40], title[72];
    const char *info_right;
    Rect old_clip, list;

    window_parts(i, &p);
    shadow(x, y, w->r.w, w->r.h);
    /* The background, under the title bar and left of the scroll bar only: those two fill
     * themselves, and filling them twice shows as a flicker. */
    gfx_fill(p.info.x, p.info.y, p.info.w, p.list.y - p.info.y, T->win_bg);
    gfx_fill(p.list.x, p.list.y, p.list.w, p.list.h, T->win_bg);
    gfx_frame(x, y, w->r.w, w->r.h, T->frame);

    /* Title bar */
    if (!touches(p.title.x, p.title.y, p.title.w, TITLE_H + 1))
        goto below_title;
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

below_title:
    if (w->kind == WIN_EDIT) {
        draw_editor(w, &p, active);
        return;
    }

    /* Info line */
    if (!touches(p.info.x, p.info.y, p.info.w, ROW_H + 2))
        ;
    else if (w->kind == WIN_GAMES) {
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
    if (touches(p.info.x, p.info.y, p.info.w, ROW_H + 2)) {
        gfx_text(p.info.x + 8, p.info.y + 1, buf, T->text, 0);
        if (gfx_text_w(info_right) + 16 + 9 * 8 < p.info.w)
            gfx_text(p.info.x + p.info.w - 8 - gfx_text_w(info_right), p.info.y + 1, info_right, T->text, 0);
        hline(p.info.x, p.info.y + ROW_H + 1, p.info.w, T->frame);
    }
    if (touches(p.scroll.x, p.scroll.y, p.scroll.w, p.scroll.h))
        draw_scrollbar(w, &p, active);

    if (w->kind == WIN_GAMES) {
        draw_headings(&p);
        draw_game_rows(w, &p);
        return;
    }

    /* Rows, clipped to the list. The sizes show if they fit beside the widest name, and the
     * dates at the right if they fit too; names too long for the space are cut short. */
    name_w = w->name_w; /* worked out when the folder was read (files_load) */
    right_w = w->size_w;
    nx = p.list.x + 8 + 16 + 8;
    date_x = p.list.x + p.list.w - 8 - DATE_W;
    show_date = nx + name_w + 16 + right_w + 16 + DATE_W <= p.list.x + p.list.w - 8;
    right_x = show_date ? date_x - 16 : p.list.x + p.list.w - 8;
    show_right = nx + name_w + 16 + right_w <= right_x;
    max_chars = ((show_right ? right_x - right_w - 16 : right_x) - nx) / 8;

    old_clip = gfx_get_clip();
    rect_intersect(&list, &p.list, &old_clip);
    if (!rect_empty(&list)) {
        gfx_set_clip(&list);
        for (k = w->top; k < w->nrows && k - w->top <= p.visible; k++) {
            int sel = ROW_SELECTED(w, k), kind, hidden = (w->rows[k].attrib & 0x02) != 0; /* _A_HIDDEN */
            const uint16_t *line_mask, *fill_mask;

            ry = p.list.y + 2 + (k - w->top) * ROW_H;
            if (ry - 1 >= list.y + list.h)
                break; /* rows outside the clip aren't formatted at all */
            if (ry - 1 + ROW_H <= list.y)
                continue;
            row_text(w, k, name, right, &kind);
            if ((int)strlen(name) > max_chars && max_chars > 0)
                name[max_chars] = '\0';
            if (sel)
                gfx_fill(p.list.x, ry - 1, p.list.w, ROW_H, T->sel);
            c = sel ? T->sel_text : T->text;
            line_mask = kind == ROW_DIR ? folder_line : kind == ROW_GAME ? game_line : file_line;
            fill_mask = kind == ROW_DIR ? folder_fill : kind == ROW_GAME ? game_fill : file_fill;
            gfx_mask(p.list.x + 8, ry, fill_mask, ICON_ROWS, 16,
                     sel ? T->sel : (kind == ROW_DIR ? T->folder_fill : T->file_fill));
            gfx_mask(p.list.x + 8, ry, line_mask, ICON_ROWS, 16,
                     sel ? T->sel_text : (kind == ROW_DIR ? T->folder_line : T->file_line));
            text_dim(nx, ry, name, hidden && !sel, c); /* hidden files dimmed */
            if (show_right && right[0])
                gfx_text(right_x - gfx_text_w(right), ry, right, c, 0);
            if (show_date && strcmp(name, "..") != 0) {
                fmt_date(w->rows[k].date, buf);
                gfx_text(date_x, ry, buf, c, 0);
            }
        }
        gfx_set_clip(&old_clip);
    }
}

/* ---- Everything under the menus ---- */

void draw_desktop(const Rect *area)
{
    Rect clip, r;
    int k, i, first;

    gfx_set_clip(area);
    clip = gfx_get_clip();
    if (rect_empty(&clip))
        return;
    /* If a window covers the whole area, start from it: drawing the desktop and the windows
     * behind first only to cover them again would flash. */
    for (first = NUM_WINS - 1; first >= 0; first--) {
        i = A.z[first];
        if (A.win[i].open && rect_intersect(&r, &A.win[i].r, &clip) && r.x == clip.x && r.y == clip.y &&
            r.w == clip.w && r.h == clip.h)
            break;
    }
    if (first < 0) {
        gfx_pattern(clip.x, clip.y, clip.w, clip.h, PAT_CHECKER, T->desk_on, T->desk_off);
        for (i = 0; i < A.nicons; i++) {
            r = icon_bounds(i);
            if (rect_intersect(&r, &r, &clip))
                draw_icon(i);
        }
        first = 0;
    }
    for (k = first; k < NUM_WINS; k++) {
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

/* A dialog's box: its shadow, a double frame, and the background. */
static void dialog_frame(const Rect *r)
{
    shadow(r->x, r->y, r->w, r->h);
    gfx_fill(r->x, r->y, r->w, r->h, T->dialog_bg);
    gfx_frame(r->x, r->y, r->w, r->h, T->frame);
    vline(r->x + 1, r->y, r->h, T->frame);
    vline(r->x + r->w - 2, r->y, r->h, T->frame);
    gfx_frame(r->x + 4, r->y + 3, r->w - 8, r->h - 6, T->frame);
}

/* The alert's icon: a sign with mark on it. */
static void dialog_icon(int x, int y, const char *mark)
{
    gfx_fill(x, y, ALERT_ICON_W, ALERT_ICON_H, T->alert_bg);
    gfx_frame(x, y, ALERT_ICON_W, ALERT_ICON_H, T->frame);
    gfx_text(x + (ALERT_ICON_W - 8) / 2, y + 3, mark, T->alert_text, 0);
}

void draw_alert(void)
{
    const Alert *a = &A.alert;
    const Rect *r = &a->r;
    int body = imax(ALERT_ICON_H, 2 * ROW_H), cy = r->y + 3 + 8, ax = r->x + ALERT_PAD, tx, ly, i;

    dialog_frame(r);
    dialog_icon(ax, cy, "!");

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

/* ---- The text-entry box ---- */

#define INPUT_CHARS 44

void input_layout(void)
{
    Input *in = &A.input;
    int w = SNAP8_UP(ALERT_PAD * 2 + (INPUT_CHARS + 1) * 8), h = 8 + 8 + 10 + 4 + ROW_H + 4 + 8 + BUTTON_H + 8;
    int x = SNAP8((SCREEN_W - w) / 2), y = (SCREEN_H - h) / 2, bw = gfx_text_w("Cancel") + 16;

    in->r = rect_make(x, y, w, h);
    in->f = rect_make(x + ALERT_PAD, y + 3 + 8 + 12, (INPUT_CHARS + 1) * 8, ROW_H + 4);
    in->b[1] = rect_make(x + w - ALERT_PAD - bw, in->f.y + in->f.h + 8, bw, BUTTON_H);
    in->b[0] = rect_make(in->b[1].x - 16 - bw, in->b[1].y, bw, BUTTON_H);
    in->field.width = INPUT_CHARS;
}

void draw_input_field(void)
{
    const Input *in = &A.input;
    const Field *f = &in->field;
    char shown[INPUT_CHARS + 2];
    int n = f->len - f->scroll, cx;

    if (n > INPUT_CHARS)
        n = INPUT_CHARS;
    if (n < 0)
        n = 0;
    memcpy(shown, f->text + f->scroll, (size_t)n);
    shown[n] = '\0';
    gfx_fill(in->f.x + 1, in->f.y + 1, in->f.w - 2, in->f.h - 2, T->win_bg);
    gfx_frame(in->f.x, in->f.y, in->f.w, in->f.h, T->frame);
    gfx_text(in->f.x + 4, in->f.y + 2, shown, T->text, 0);
    cx = in->f.x + 4 + (f->caret - f->scroll) * 8; /* the caret: a bar before its character */
    gfx_fill(cx - 1, in->f.y + 2, 2, 8, T->text);
}

void draw_input(void)
{
    const Input *in = &A.input;
    const Rect *r = &in->r;

    dialog_frame(r);
    gfx_text(in->f.x, r->y + 3 + 8, in->prompt, T->text, 0);
    draw_input_field();
    button(&in->b[0], "Cancel", 0);
    button(&in->b[1], "OK", 1);
}

/* ---- File > Info ---- */

#define CHECK_W 20 /* a check box, 20 x 8: square-looking at 2.4:1 */
#define CHECK_H 8
static const char *const CHECK_LABEL[2] = { "Read-only", "Hidden" };

/* The icon, then four lines (name, size, date, the other attributes), the two check boxes,
 * and Cancel and OK. */
void info_layout(void)
{
    Info *in = &A.info;
    int text_w = imax(imax(gfx_text_w(in->name), gfx_text_w(in->size)), imax(gfx_text_w(in->date), gfx_text_w(in->also)));
    int checks_w = 2 * (CHECK_W + 8) + gfx_text_w(CHECK_LABEL[0]) + gfx_text_w(CHECK_LABEL[1]) + 24;
    int bw = gfx_text_w("Cancel") + 16, w, h, x, y, tx, cy, k;

    w = SNAP8_UP(ALERT_PAD * 2 + ALERT_ICON_W + 12 + imax(text_w, checks_w));
    h = 3 + 8 + 4 * ROW_H + 6 + ROW_H + 8 + BUTTON_H + 8;
    x = SNAP8((SCREEN_W - w) / 2);
    y = (SCREEN_H - h) / 2 + 6;
    in->r = rect_make(x, y, w, h);
    tx = x + ALERT_PAD + ALERT_ICON_W + 12;
    cy = y + 3 + 8 + 4 * ROW_H + 6;
    for (k = 0; k < 2; k++) { /* the box and its label: a click on either */
        in->check[k] = rect_make(tx, cy - 1, CHECK_W + 8 + gfx_text_w(CHECK_LABEL[k]), ROW_H);
        tx += in->check[k].w + 24;
    }
    in->b[1] = rect_make(x + w - ALERT_PAD - bw, cy + ROW_H + 8, bw, BUTTON_H);
    in->b[0] = rect_make(in->b[1].x - 16 - bw, in->b[1].y, bw, BUTTON_H);
}

void draw_info_checks(void)
{
    const Info *in = &A.info;
    int k, on, bx, by;

    for (k = 0; k < 2; k++) {
        on = k == 0 ? in->read_only : in->hidden;
        bx = in->check[k].x;
        by = in->check[k].y + (ROW_H - CHECK_H) / 2;
        gfx_fill(bx + 1, by + 1, CHECK_W - 2, CHECK_H - 2, T->win_bg);
        gfx_frame(bx, by, CHECK_W, CHECK_H, T->frame);
        if (on)
            gfx_mask(bx + 2, by + 1, check_mask, ICON_ROWS - 1, 16, T->text);
    }
}

void draw_info(void)
{
    const Info *in = &A.info;
    const Rect *r = &in->r;
    int cy = r->y + 3 + 8, tx = r->x + ALERT_PAD + ALERT_ICON_W + 12, k;

    dialog_frame(r);
    dialog_icon(r->x + ALERT_PAD, cy, "i");
    gfx_text(tx, cy + 1, in->name, T->text, 0);
    gfx_text(tx, cy + 1 + ROW_H, in->size, T->text, 0);
    gfx_text(tx, cy + 1 + 2 * ROW_H, in->date, T->text, 0);
    gfx_text(tx, cy + 1 + 3 * ROW_H, in->also, T->text, 0);
    for (k = 0; k < 2; k++)
        gfx_text(in->check[k].x + CHECK_W + 8, in->check[k].y + 1, CHECK_LABEL[k], T->text, 0);
    draw_info_checks();
    button(&in->b[0], "Cancel", 0);
    button(&in->b[1], "OK", 1);
}
