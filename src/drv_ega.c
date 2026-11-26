/* Ink's driver: EGA mode 0Eh, 640x200 in 16 colours.
 *
 * Four bit planes at A000:0000, 80 bytes a row each, one bit a pixel per plane. Drawing goes
 * through the graphics controller (ports 3CEh/3CFh):
 *   - set/reset (index 0) holds the colour and enable set/reset (index 1) applies it to all
 *     four planes, so one write paints a colour;
 *   - the bit mask (index 8) picks which of the byte's 8 pixels change. The others come from
 *     the latches, which a read of the same byte loads: so partial bytes are read, then written.
 * Every function leaves the registers at the BIOS defaults, which the mouse driver expects
 * when it draws the pointer. */
#include <conio.h>
#include <i86.h>
#include <string.h>
#include "gfx.h"

#define GC(i, v) (outp(0x3CE, (i)), outp(0x3CF, (v)))
#define SEQ(i, v) (outp(0x3C4, (i)), outp(0x3C5, (v)))
#define ROW(y) ((uint8_t __far *)MK_FP(0xA000, (unsigned)(y) * 80))
#define PLANE_BYTES 16000u

static void defaults(void)
{
    GC(0, 0x00);  /* set/reset colour */
    GC(1, 0x00);  /* enable set/reset: off */
    GC(3, 0x00);  /* function: replace, no rotation */
    GC(4, 0x00);  /* read map: plane 0 */
    GC(5, 0x00);  /* write mode 0 */
    GC(8, 0xFF);  /* bit mask: all pixels */
    SEQ(2, 0x0F); /* map mask: all planes */
}

static void bios_mode(int m)
{
    union REGS r;

    r.h.ah = 0;
    r.h.al = (unsigned char)m;
    int86(0x10, &r, &r);
}

static void ega_init(void)
{
    bios_mode(0x0E);
    defaults();
}

/* Write the set/reset colour to the pixels of x..x+w-1 that are set in pat. With pat 0xFF and
 * fast set, whole middle bytes skip the latch read (the bit mask leaves nothing to keep). */
static void row_mask(uint8_t __far *p, int x, int w, uint8_t pat, int fast)
{
    int x0 = x >> 3, x1 = (x + w - 1) >> 3, n;
    uint8_t lm = (uint8_t)(0xFF >> (x & 7));
    uint8_t rm = (uint8_t)(0xFF << (7 - ((x + w - 1) & 7)));
    volatile uint8_t latch;

    p += x0;
    if (x0 == x1) {
        GC(8, lm & rm & pat);
        latch = *p;
        *p = 0xFF;
        return;
    }
    GC(8, lm & pat);
    latch = *p;
    *p++ = 0xFF;
    n = x1 - x0 - 1;
    if (n > 0) {
        GC(8, pat);
        if (fast && pat == 0xFF) {
            _fmemset(p, 0xFF, n);
            p += n;
        } else {
            for (; n > 0; n--, p++) {
                latch = *p;
                *p = 0xFF;
            }
        }
    }
    GC(8, rm & pat);
    latch = *p;
    *p = 0xFF;
    (void)latch;
}

static void ega_fill(int x, int y, int w, int h, uint8_t c)
{
    GC(0, c);
    GC(1, 0x0F);
    for (; h > 0; h--, y++)
        row_mask(ROW(y), x, w, 0xFF, 1);
    defaults();
}

static void ega_pattern(int x, int y, int w, int h, const uint8_t *pat, uint8_t on, uint8_t off)
{
    int yy, hh;

    GC(1, 0x0F);
    GC(0, off);
    for (yy = y, hh = h; hh > 0; hh--, yy++)
        row_mask(ROW(yy), x, w, 0xFF, 1);
    GC(0, on);
    for (yy = y, hh = h; hh > 0; hh--, yy++)
        row_mask(ROW(yy), x, w, pat[yy & 1], 0);
    defaults();
}

static void ega_span(int x, int y, uint16_t bits, int n, uint8_t c)
{
    uint8_t __far *p = ROW(y) + (x >> 3);
    int s = x & 7, k;
    uint8_t b[3];
    volatile uint8_t latch;

    (void)n;
    b[0] = (uint8_t)(bits >> (8 + s));
    b[1] = (uint8_t)(bits >> s);
    b[2] = (uint8_t)(bits << (8 - s));
    GC(0, c);
    GC(1, 0x0F);
    for (k = 0; k < 3; k++) {
        if (b[k] == 0)
            continue;
        GC(8, b[k]);
        latch = p[k];
        p[k] = 0xFF;
    }
    (void)latch;
    defaults();
}

static void ega_xor_fill(int x, int y, int w, int h)
{
    GC(3, 0x18); /* function: XOR with the latches */
    GC(0, 0x0F);
    GC(1, 0x0F);
    for (; h > 0; h--, y++)
        row_mask(ROW(y), x, w, 0xFF, 0);
    defaults();
}

static int row_bytes(int x, int w)
{
    return ((x + w - 1) >> 3) - (x >> 3) + 1;
}

static long ega_save_size(int x, int y, int w, int h)
{
    (void)y;
    return (long)row_bytes(x, w) * h * 4;
}

static void ega_save(int x, int y, int w, int h, uint8_t __far *buf)
{
    int nb = row_bytes(x, w), plane, yy, hh;

    for (plane = 0; plane < 4; plane++) {
        GC(4, plane);
        for (yy = y, hh = h; hh > 0; hh--, yy++, buf += nb)
            _fmemcpy(buf, ROW(yy) + (x >> 3), nb);
    }
    defaults();
}

static void ega_restore(int x, int y, int w, int h, const uint8_t __far *buf)
{
    int nb = row_bytes(x, w), plane, yy, hh;

    defaults();
    for (plane = 0; plane < 4; plane++) {
        SEQ(2, 1 << plane);
        for (yy = y, hh = h; hh > 0; hh--, yy++, buf += nb)
            _fmemcpy(ROW(yy) + (x >> 3), buf, nb);
    }
    defaults();
}

static int ega_dump(FILE *f)
{
    uint8_t tmp[250];
    unsigned o;
    int plane, ok = 1;

    for (plane = 0; plane < 4 && ok; plane++) {
        GC(4, plane);
        for (o = 0; o < PLANE_BYTES; o += sizeof tmp) {
            _fmemcpy(tmp, MK_FP(0xA000, o), sizeof tmp);
            if (fwrite(tmp, 1, sizeof tmp, f) != sizeof tmp) {
                ok = 0;
                break;
            }
        }
    }
    defaults();
    return ok;
}

const Driver drv_ega = {
    "Ink (EGA)",
    ega_init, ega_fill, ega_pattern, ega_span, ega_xor_fill,
    ega_save_size, ega_save, ega_restore, ega_dump
};
