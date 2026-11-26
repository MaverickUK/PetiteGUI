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

    /* For the screensavers. */
    /* n pixels of colour indexes from pix; x and n multiples of 8. */
    void (*put_row)(int x, int y, int n, const uint8_t *pix);
    /* Move rows y..y+h-1 of the byte-aligned columns x..x+w-1 down by dy. */
    void (*move_down)(int x, int y, int w, int h, int dy);
    /* Like span, but XORs colour c into the pixels. */
    void (*xor_span)(int x, int y, uint16_t bits, int n, uint8_t c);
    uint8_t (*peek)(int x, int y);
    /* Turn the picture off (1) or on (0); it stays in video memory. */
    void (*blank)(int off);
    /* Show colour index i as CGA colour map[i] (0-15); Paper shows only map[1], its ink. */
    void (*palette)(const uint8_t *map);

    /* n characters of 8 x 8 text, whole and on the screen: only the set pixels are drawn.
     * font is the BIOS font (8 bytes a character), s already 0-127. One call per run of
     * characters, so the card is set up once, not once per glyph row. */
    void (*text)(int x, int y, const uint8_t __far *font, const char *s, int n, uint8_t c);
    /* Copy h rows of the byte-aligned columns x..x+w-1 from y to to_y (the two may overlap:
     * scrolling a list moves what's already drawn). */
    void (*copy_rows)(int x, int y, int w, int h, int to_y);
    /* Drawing changes the card's registers (the EGA's graphics controller), so the mouse
     * driver mustn't draw the pointer meanwhile, anywhere: see mouse_hide_in. */
    int uses_registers;
} Driver;

extern const Driver drv_cga;
extern const Driver drv_ega;
/* Set by main: an EGA or VGA is fitted (so Paper's ink and blanking go through it). */
extern int gfx_has_ega;

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

/* For the screensavers: on-screen coordinates only (put_row and move_down aren't clipped). */
void gfx_put_row(int x, int y, int n, const uint8_t *pix);
void gfx_move_down(int x, int y, int w, int h, int dy);
void gfx_xor_bits(int x, int y, uint16_t bits, int n, uint8_t c);
uint8_t gfx_peek(int x, int y);
void gfx_blank(int off);
void gfx_palette(const uint8_t *map);
void gfx_palette_reset(void);
/* The BIOS font's 8 rows for a character. */
const uint8_t __far *gfx_glyph(unsigned char ch);

/* Move what's drawn in r (byte-aligned across, and wholly on the screen) by dy rows, down if
 * it's positive; the rows it leaves aren't touched. Zero if r isn't like that. */
int gfx_scroll(const Rect *r, int dy);

/* Save what's under r (clipped to the screen); NULL if there's no memory. */
uint8_t __far *gfx_save(const Rect *r);
/* Put it back and free it. */
void gfx_restore(const Rect *r, uint8_t __far *buf);

#endif
