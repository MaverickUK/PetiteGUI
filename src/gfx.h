/* Drawing. A Driver knows one video mode's memory layout; everything above it calls only
 * the gfx_ functions, which clip to the current clip rectangle and then call the driver.
 *
 * Both drivers so far are 640x200, so the layout is shared: Paper (CGA mode 06h, 1 bit a
 * pixel) and Ink (EGA mode 0Eh, 16 colours in four planes). */
#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <stdio.h>
#include "rect.h"

#define SCREEN_W 640
#define SCREEN_H 200

typedef struct Driver {
    const char *name;
    /* Set the mode. */
    void (*init)(void);
    /* All coordinates below are already clipped to the screen. */
    void (*fill)(int x, int y, int w, int h, uint8_t c);
    /* pat is two rows of 8 pixels (bit 7 leftmost), chosen by y & 1 and aligned to the screen. */
    void (*pattern)(int x, int y, int w, int h, const uint8_t *pat, uint8_t on, uint8_t off);
    /* Up to 16 pixels from bits (bit 15 leftmost); only set bits are drawn. */
    void (*span)(int x, int y, uint16_t bits, int n, uint8_t c);
    /* Invert: drawn twice, it leaves the screen as it was (the outline while dragging). */
    void (*xor_fill)(int x, int y, int w, int h);
    /* Copy a rectangle (in whole bytes) to memory and back: the pixels under a menu. */
    long (*save_size)(int x, int y, int w, int h);
    void (*save)(int x, int y, int w, int h, uint8_t __far *buf);
    void (*restore)(int x, int y, int w, int h, const uint8_t __far *buf);
    /* Write video memory as it is laid out (for screenshots). Non-zero on success. */
    int (*dump)(FILE *f);
} Driver;

extern const Driver drv_cga;
extern const Driver drv_ega;

extern const uint8_t PAT_CHECKER[2]; /* 50% grey */
extern const uint8_t PAT_DOTS[2];    /* every other pixel, same on both rows */

void gfx_start(const Driver *d);
const Driver *gfx_driver(void);

/* NULL clips to the whole screen. */
void gfx_set_clip(const Rect *r);
Rect gfx_get_clip(void);

void gfx_fill(int x, int y, int w, int h, uint8_t c);
void gfx_pattern(int x, int y, int w, int h, const uint8_t *pat, uint8_t on, uint8_t off);
void gfx_frame(int x, int y, int w, int h, uint8_t c);
void gfx_bits(int x, int y, uint16_t bits, int n, uint8_t c);
/* rows[0..h-1], each n pixels wide from bit 15. */
void gfx_mask(int x, int y, const uint16_t *rows, int h, int n, uint8_t c);
/* 8x8 text from the BIOS font; dither draws every other pixel (disabled items in Paper). */
void gfx_text(int x, int y, const char *s, uint8_t c, int dither);
int gfx_text_w(const char *s);
/* A 2-pixel-wide inverted outline, for dragging. */
void gfx_xor_frame(const Rect *r);

/* Save what's under r (clipped to the screen); NULL if there's no memory. */
uint8_t __far *gfx_save(const Rect *r);
/* Put it back and free it. */
void gfx_restore(const Rect *r, uint8_t __far *buf);

#endif
