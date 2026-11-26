/* Paper's driver: CGA mode 06h, 640x200 in black and white.
 *
 * One bit a pixel, 80 bytes a row, bit 7 the leftmost pixel. Even rows are at B800:0000 and
 * odd rows at B800:2000, each bank 8 KB. Colour 0 is black, colour 1 white. */
#include <conio.h>
#include <i86.h>
#include <string.h>
#include "gfx.h"

#define ROW(y) ((uint8_t __far *)MK_FP(0xB800, (((y) & 1) ? 0x2000 : 0) + ((unsigned)(y) >> 1) * 80))

static void bios_mode(int m)
{
    union REGS r;

    r.h.ah = 0;
    r.h.al = (unsigned char)m;
    int86(0x10, &r, &r);
}

static void cga_init(void)
{
    bios_mode(6);
}

/* Set pixels x..x+w-1 of one row to the byte pattern v, keeping the rest of the edge bytes. */
static void row_set(uint8_t __far *p, int x, int w, uint8_t v)
{
    int x0 = x >> 3, x1 = (x + w - 1) >> 3, n;
    uint8_t lm = (uint8_t)(0xFF >> (x & 7));
    uint8_t rm = (uint8_t)(0xFF << (7 - ((x + w - 1) & 7)));

    p += x0;
    if (x0 == x1) {
        lm &= rm;
        *p = (uint8_t)((*p & ~lm) | (v & lm));
        return;
    }
    *p = (uint8_t)((*p & ~lm) | (v & lm));
    p++;
    n = x1 - x0 - 1;
    if (n > 0) {
        _fmemset(p, v, n);
        p += n;
    }
    *p = (uint8_t)((*p & ~rm) | (v & rm));
}

static void row_xor(uint8_t __far *p, int x, int w)
{
    int x0 = x >> 3, x1 = (x + w - 1) >> 3, n;
    uint8_t lm = (uint8_t)(0xFF >> (x & 7));
    uint8_t rm = (uint8_t)(0xFF << (7 - ((x + w - 1) & 7)));

    p += x0;
    if (x0 == x1) {
        *p ^= (uint8_t)(lm & rm);
        return;
    }
    *p++ ^= lm;
    for (n = x1 - x0 - 1; n > 0; n--)
        *p++ ^= 0xFF;
    *p ^= rm;
}

static void cga_fill(int x, int y, int w, int h, uint8_t c)
{
    uint8_t v = c ? 0xFF : 0x00;

    for (; h > 0; h--, y++)
        row_set(ROW(y), x, w, v);
}

static void cga_pattern(int x, int y, int w, int h, const uint8_t *pat, uint8_t on, uint8_t off)
{
    uint8_t p, v;

    for (; h > 0; h--, y++) {
        p = pat[y & 1];
        v = (uint8_t)((p & (on ? 0xFF : 0)) | (~p & (off ? 0xFF : 0)));
        row_set(ROW(y), x, w, v);
    }
}

static void cga_span(int x, int y, uint16_t bits, int n, uint8_t c)
{
    uint8_t __far *p = ROW(y) + (x >> 3);
    int s = x & 7;
    uint8_t b[3];
    int k;

    (void)n;
    b[0] = (uint8_t)(bits >> (8 + s));
    b[1] = (uint8_t)(bits >> s);
    b[2] = (uint8_t)(bits << (8 - s));
    for (k = 0; k < 3; k++) {
        if (b[k] == 0)
            continue;
        if (c)
            p[k] |= b[k];
        else
            p[k] &= (uint8_t)~b[k];
    }
}

static void cga_xor_fill(int x, int y, int w, int h)
{
    for (; h > 0; h--, y++)
        row_xor(ROW(y), x, w);
}

static int row_bytes(int x, int w)
{
    return ((x + w - 1) >> 3) - (x >> 3) + 1;
}

static long cga_save_size(int x, int y, int w, int h)
{
    (void)y;
    return (long)row_bytes(x, w) * h;
}

static void cga_save(int x, int y, int w, int h, uint8_t __far *buf)
{
    int nb = row_bytes(x, w);

    for (; h > 0; h--, y++, buf += nb)
        _fmemcpy(buf, ROW(y) + (x >> 3), nb);
}

static void cga_restore(int x, int y, int w, int h, const uint8_t __far *buf)
{
    int nb = row_bytes(x, w);

    for (; h > 0; h--, y++, buf += nb)
        _fmemcpy(ROW(y) + (x >> 3), buf, nb);
}

static int cga_dump(FILE *f)
{
    uint8_t tmp[256];
    unsigned o;

    for (o = 0; o < 0x4000; o += sizeof tmp) {
        _fmemcpy(tmp, MK_FP(0xB800, o), sizeof tmp);
        if (fwrite(tmp, 1, sizeof tmp, f) != sizeof tmp)
            return 0;
    }
    return 1;
}

static void cga_put_row(int x, int y, int n, const uint8_t *pix)
{
    uint8_t __far *p = ROW(y) + (x >> 3);
    int i, b;
    uint8_t v;

    for (i = 0; i < n; i += 8) {
        v = 0;
        for (b = 0; b < 8; b++)
            if (pix[i + b])
                v |= (uint8_t)(0x80 >> b);
        *p++ = v;
    }
}

static void cga_move_down(int x, int y, int w, int h, int dy)
{
    int r, nb = w >> 3;

    for (r = h - 1; r >= 0; r--) /* from the bottom, so nothing is overwritten before it moves */
        _fmemcpy(ROW(y + r + dy) + (x >> 3), ROW(y + r) + (x >> 3), nb);
}

static void cga_xor_span(int x, int y, uint16_t bits, int n, uint8_t c)
{
    uint8_t __far *p = ROW(y) + (x >> 3);
    int s = x & 7;

    (void)n;
    if (!c)
        return;
    p[0] ^= (uint8_t)(bits >> (8 + s));
    p[1] ^= (uint8_t)(bits >> s);
    p[2] ^= (uint8_t)(bits << (8 - s));
}

static uint8_t cga_peek(int x, int y)
{
    return (uint8_t)((ROW(y)[x >> 3] >> (7 - (x & 7))) & 1);
}

/* On an EGA or VGA, mode 06h goes through the attribute controller like any other mode, so
 * blanking and the ink colour use it; on a CGA, the mode and colour select registers. */
static void attribute_blank(int off)
{
    (void)inp(0x3DA);              /* the attribute controller's next write is an index */
    outp(0x3C0, off ? 0x00 : 0x20); /* bit 5 clear: the palette is cut off, so is the picture */
}

static uint8_t ega_value(uint8_t c)
{
    return (uint8_t)((c & 7) | ((c & 8) << 1)); /* 200-line modes: bits 0-2 RGB, 4 intensity */
}

static void cga_blank(int off)
{
    if (gfx_has_ega)
        attribute_blank(off);
    else
        outp(0x3D8, off ? 0x16 : 0x1E); /* mode 06h with video enable (bit 3) off or on */
}

static void cga_palette(const uint8_t *map)
{
    union REGS r;

    if (gfx_has_ega) {
        r.x.ax = 0x1000; /* set one palette register */
        r.h.bl = 1;
        r.h.bh = ega_value(map[1]);
        int86(0x10, &r, &r);
    } else {
        outp(0x3D9, map[1] & 0x0F); /* colour select: the ink in 640 x 200 */
    }
}

const Driver drv_cga = {
    "Paper (CGA)",
    cga_init, cga_fill, cga_pattern, cga_span, cga_xor_fill,
    cga_save_size, cga_save, cga_restore, cga_dump,
    cga_put_row, cga_move_down, cga_xor_span, cga_peek, cga_blank, cga_palette
};
