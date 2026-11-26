/* Drawing the desktop from the state in A. Nothing here reads the mouse or keyboard. */
#include <stdio.h>
#include <string.h>
#include "app.h"
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
static const char *const CHECK[] = {
    ".......x", "......x.", "x....x..", ".x..x...", "..xx....", "........", "........"
};
#define ICON_ROWS 7

static uint16_t folder_line[ICON_ROWS], folder_fill[ICON_ROWS];
static uint16_t file_line[ICON_ROWS], file_fill[ICON_ROWS];
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
    w = text_left(m) + name_w + (key_w ? 24 + key_w : 0) + 8;
    return rect_make(SNAP8(m->x - 8), MENU_H, SNAP8_UP(w), h);
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

void draw_window(int i)
{
    const Win *w = &A.win[i];
    WinParts p;
    int active = i == front_window(), x = w->r.x, y = w->r.y;
    int r, k, tx, tw, ry, nx, size_x, name_w = 0, size_w = 0, show_sizes;
    uint8_t tc, c;
    char buf[24];
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
    tw = gfx_text_w(w->title);
    tx = p.title.x + (p.title.w - tw) / 2;
    if (T->stripes && active)
        gfx_fill(tx - 8, p.title.y + 1, tw + 16, TITLE_H - 2, T->win_bg);
    gfx_text(tx, p.title.y + 2, w->title, tc, 0);
    if (!T->stripes || active) {
        if (T->stripes)
            gfx_fill(p.close.x - 3, p.title.y + 1, p.close.w + 6, TITLE_H - 2, T->win_bg);
        else
            gfx_fill(p.close.x, p.close.y, p.close.w, p.close.h, active ? T->title_on : T->title_off);
        gfx_frame(p.close.x, p.close.y, p.close.w, p.close.h, tc);
    }

    /* Info line */
    sprintf(buf, "%d item%s", w->nrows, w->nrows == 1 ? "" : "s");
    gfx_text(p.info.x + 8, p.info.y + 1, buf, T->text, 0);
    fmt_thousands(w->free_kb, buf);
    strcat(buf, " KB free");
    if (gfx_text_w(buf) + 16 + 9 * 8 < p.info.w)
        gfx_text(p.info.x + p.info.w - 8 - gfx_text_w(buf), p.info.y + 1, buf, T->text, 0);
    hline(p.info.x, p.info.y + ROW_H + 1, p.info.w, T->frame);

    draw_scrollbar(w, &p, active);

    /* Rows, clipped to the list */
    for (k = 0; k < w->nrows; k++) {
        name_w = imax(name_w, gfx_text_w(w->rows[k].name));
        fmt_thousands(w->rows[k].size, buf);
        if (!w->rows[k].dir)
            size_w = imax(size_w, gfx_text_w(buf));
    }
    nx = p.list.x + 8 + 16 + 8;
    size_x = p.list.x + p.list.w - 8;
    show_sizes = nx + name_w + 16 + size_w <= size_x;

    old_clip = gfx_get_clip();
    rect_intersect(&list, &p.list, &old_clip);
    if (!rect_empty(&list)) {
        gfx_set_clip(&list);
        for (k = w->top; k < w->nrows && k - w->top <= p.visible; k++) {
            const Row *row = &w->rows[k];
            int sel = k == w->sel;

            ry = p.list.y + 2 + (k - w->top) * ROW_H;
            if (sel)
                gfx_fill(p.list.x, ry - 1, p.list.w, ROW_H, T->sel);
            c = sel ? T->sel_text : T->text;
            gfx_mask(p.list.x + 8, ry, row->dir ? folder_fill : file_fill, ICON_ROWS, 16,
                     sel ? T->sel : (row->dir ? T->folder_fill : T->file_fill));
            gfx_mask(p.list.x + 8, ry, row->dir ? folder_line : file_line, ICON_ROWS, 16,
                     sel ? T->sel_text : (row->dir ? T->folder_line : T->file_line));
            gfx_text(nx, ry, row->name, c, 0);
            if (show_sizes && !row->dir) {
                fmt_thousands(row->size, buf);
                gfx_text(size_x - gfx_text_w(buf), ry, buf, c, 0);
            }
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
