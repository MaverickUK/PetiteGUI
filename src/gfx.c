#include <dos.h>
#include <i86.h>
#include <string.h>
#include "gfx.h"

const uint8_t PAT_CHECKER[2] = { 0xAA, 0x55 };
const uint8_t PAT_DOTS[2] = { 0xAA, 0xAA };

int gfx_has_ega;
static const Driver *D;
static Rect clip;
static const Rect SCREEN = { 0, 0, SCREEN_W, SCREEN_H };
/* The system BIOS keeps an 8x8 font for characters 0-127 here, on every PC. */
static const uint8_t __far *font;

void gfx_start(const Driver *d)
{
    D = d;
    clip = SCREEN;
    font = (const uint8_t __far *)MK_FP(0xF000, 0xFA6E);
    D->init();
}

const Driver *gfx_driver(void)
{
    return D;
}

void gfx_set_clip(const Rect *r)
{
    if (r == NULL)
        clip = SCREEN;
    else
        rect_intersect(&clip, r, &SCREEN);
}

Rect gfx_get_clip(void)
{
    return clip;
}

void gfx_fill(int x, int y, int w, int h, uint8_t c)
{
    Rect a;

    a = rect_make(x, y, w, h);
    if (rect_intersect(&a, &a, &clip))
        D->fill(a.x, a.y, a.w, a.h, c);
}

void gfx_pattern(int x, int y, int w, int h, const uint8_t *pat, uint8_t on, uint8_t off)
{
    Rect a;

    a = rect_make(x, y, w, h);
    if (rect_intersect(&a, &a, &clip))
        D->pattern(a.x, a.y, a.w, a.h, pat, on, off);
}

void gfx_frame(int x, int y, int w, int h, uint8_t c)
{
    gfx_fill(x, y, w, 1, c);
    gfx_fill(x, y + h - 1, w, 1, c);
    gfx_fill(x, y + 1, 1, h - 2, c);
    gfx_fill(x + w - 1, y + 1, 1, h - 2, c);
}

void gfx_bits(int x, int y, uint16_t bits, int n, uint8_t c)
{
    int cut;

    if (y < clip.y || y >= clip.y + clip.h || n <= 0)
        return;
    cut = clip.x - x;
    if (cut > 0) {
        if (cut >= n)
            return;
        bits <<= cut;
        x += cut;
        n -= cut;
    }
    cut = x + n - (clip.x + clip.w);
    if (cut > 0) {
        n -= cut;
        if (n <= 0)
            return;
    }
    if (n < 16)
        bits &= (uint16_t)(0xFFFFu << (16 - n));
    if (bits)
        D->span(x, y, bits, n, c);
}

void gfx_xor_bits(int x, int y, uint16_t bits, int n, uint8_t c)
{
    int cut;

    if (y < 0 || y >= SCREEN_H || n <= 0)
        return;
    cut = -x;
    if (cut > 0) {
        if (cut >= n)
            return;
        bits <<= cut;
        x = 0;
        n -= cut;
    }
    cut = x + n - SCREEN_W;
    if (cut > 0)
        n -= cut;
    if (n <= 0)
        return;
    if (n < 16)
        bits &= (uint16_t)(0xFFFFu << (16 - n));
    if (bits)
        D->xor_span(x, y, bits, n, c);
}

void gfx_put_row(int x, int y, int n, const uint8_t *pix)
{
    D->put_row(x, y, n, pix);
}

void gfx_move_down(int x, int y, int w, int h, int dy)
{
    if (h > 0 && dy > 0)
        D->move_down(x, y, w, h, dy);
}

uint8_t gfx_peek(int x, int y)
{
    return D->peek(x, y);
}

void gfx_blank(int off)
{
    D->blank(off);
}

void gfx_palette(const uint8_t *map)
{
    D->palette(map);
}

void gfx_palette_reset(void)
{
    uint8_t map[16];
    int i;

    for (i = 0; i < 16; i++)
        map[i] = (uint8_t)i;
    if (D == &drv_cga)
        map[1] = 15; /* Paper's ink is white */
    D->palette(map);
}

const uint8_t __far *gfx_glyph(unsigned char ch)
{
    return font + (ch > 127 ? '?' : ch) * 8;
}

void gfx_mask(int x, int y, const uint16_t *rows, int h, int n, uint8_t c)
{
    int r;

    for (r = 0; r < h; r++)
        gfx_bits(x, y + r, rows[r], n, c);
}

/* Characters wholly inside the clip go to the driver a run at a time; one cut by the clip's
 * edge (or dithered) goes a row at a time through gfx_bits, which clips. */
void gfx_text(int x, int y, const char *s, uint8_t c, int dither)
{
    const uint8_t __far *g;
    uint16_t bits;
    unsigned char ch;
    char run[80];
    int r, n = 0, run_x = 0;
    int whole_rows = y >= clip.y && y + 8 <= clip.y + clip.h;

    if (y >= clip.y + clip.h || y + 8 <= clip.y)
        return;
    for (; *s; s++, x += 8) {
        if (x >= clip.x + clip.w)
            break;
        if (x + 8 <= clip.x)
            continue;
        ch = (unsigned char)*s;
        if (ch > 127)
            ch = '?';
        if (whole_rows && !dither && x >= clip.x && x + 8 <= clip.x + clip.w) {
            if (n == 0)
                run_x = x;
            run[n++] = (char)ch;
            if (n == (int)sizeof run) {
                D->text(run_x, y, font, run, n, c);
                n = 0;
            }
            continue;
        }
        if (n > 0) {
            D->text(run_x, y, font, run, n, c);
            n = 0;
        }
        g = font + ch * 8;
        for (r = 0; r < 8; r++) {
            bits = (uint16_t)((uint16_t)g[r] << 8);
            if (dither)
                bits &= ((x + y + r) & 1) ? 0x5555 : 0xAAAA;
            if (bits)
                gfx_bits(x, y + r, bits, 8, c);
        }
    }
    if (n > 0)
        D->text(run_x, y, font, run, n, c);
}

int gfx_scroll(const Rect *r, int dy)
{
    if ((r->x & 7) || (r->w & 7) || r->x < 0 || r->y < 0 || r->x + r->w > SCREEN_W ||
        r->y + r->h > SCREEN_H || r->w <= 0)
        return 0;
    if (dy == 0 || dy >= r->h || -dy >= r->h)
        return 1; /* nothing that stays in r to move */
    if (dy > 0)
        D->copy_rows(r->x, r->y, r->w, r->h - dy, r->y + dy);
    else
        D->copy_rows(r->x, r->y - dy, r->w, r->h + dy, r->y);
    return 1;
}

int gfx_text_w(const char *s)
{
    return (int)strlen(s) * 8;
}

static void xor_part(int x, int y, int w, int h)
{
    Rect a;

    a = rect_make(x, y, w, h);
    if (rect_intersect(&a, &a, &SCREEN))
        D->xor_fill(a.x, a.y, a.w, a.h);
}

void gfx_xor_frame(const Rect *r)
{
    xor_part(r->x, r->y, r->w, 1);
    xor_part(r->x, r->y + r->h - 1, r->w, 1);
    xor_part(r->x, r->y + 1, 2, r->h - 2);
    xor_part(r->x + r->w - 2, r->y + 1, 2, r->h - 2);
}

/* Saved pixels go in DOS memory blocks of their own, not the C far heap: Open Watcom's
 * _ffree keeps freed blocks for reuse, so they'd stay allocated under a game that's run.
 * A DOS block is handed back the moment it's freed. */
uint8_t __far *gfx_save(const Rect *r)
{
    Rect a;
    long size;
    unsigned seg;
    uint8_t __far *buf;

    if (!rect_intersect(&a, r, &SCREEN))
        return NULL;
    size = D->save_size(a.x, a.y, a.w, a.h);
    if (size > 65000L || _dos_allocmem((unsigned)((size + 15) / 16), &seg) != 0)
        return NULL;
    buf = (uint8_t __far *)MK_FP(seg, 0);
    D->save(a.x, a.y, a.w, a.h, buf);
    return buf;
}

void gfx_restore(const Rect *r, uint8_t __far *buf)
{
    Rect a;

    if (buf == NULL)
        return;
    if (rect_intersect(&a, r, &SCREEN))
        D->restore(a.x, a.y, a.w, a.h, buf);
    _dos_freemem(FP_SEG(buf));
}
