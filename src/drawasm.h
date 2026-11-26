/* The drivers' text loop in assembly (drawasm.asm): one row of glyphs across a run of
 * characters. Only drv_cga.c and drv_ega.c use these. */
#ifndef DRAWASM_H
#define DRAWASM_H

extern void __cdecl text_row_cga(unsigned off, const char *s, int n, unsigned font_off, unsigned font_seg,
                                 int shift, int ink);
extern void __cdecl text_row_ega(unsigned off, const char *s, int n, unsigned font_off, unsigned font_seg,
                                 int shift);

#endif
