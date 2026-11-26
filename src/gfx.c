#include <i86.h>
#include <malloc.h>
#include <string.h>
#include "gfx.h"

const uint8_t PAT_CHECKER[2] = { 0xAA, 0x55 };
const uint8_t PAT_DOTS[2] = { 0xAA, 0xAA };

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

void gfx_mask(int x, int y, const uint16_t *rows, int h, int n, uint8_t c)
{
    int r;

    for (r = 0; r < h; r++)
        gfx_bits(x, y + r, rows[r], n, c);
}

void gfx_text(int x, int y, const char *s, uint8_t c, int dither)
{
    const uint8_t __far *g;
    uint16_t bits;
    unsigned char ch;
    int r;

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
        g = font + ch * 8;
        for (r = 0; r < 8; r++) {
            bits = (uint16_t)((uint16_t)g[r] << 8);
            if (dither)
                bits &= ((x + y + r) & 1) ? 0x5555 : 0xAAAA;
            if (bits)
                gfx_bits(x, y + r, bits, 8, c);
        }
    }
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

uint8_t __far *gfx_save(const Rect *r)
{
    Rect a;
    long size;
    uint8_t __far *buf;

    if (!rect_intersect(&a, r, &SCREEN))
        return NULL;
    size = D->save_size(a.x, a.y, a.w, a.h);
    if (size > 65000L)
        return NULL;
    buf = (uint8_t __far *)_fmalloc((size_t)size);
    if (buf != NULL)
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
    _ffree(buf);
}
