/* Rectangles: the unit of clipping, hit testing and redrawing. Pure C, so the host tests build it. */
#ifndef RECT_H
#define RECT_H

typedef struct Rect {
    int x, y, w, h;
} Rect;

Rect rect_make(int x, int y, int w, int h);
int rect_empty(const Rect *r);
int rect_contains(const Rect *r, int x, int y);
/* The overlap of a and b in *out (which may be a or b); non-zero if it isn't empty. */
int rect_intersect(Rect *out, const Rect *a, const Rect *b);
/* The smallest rectangle holding both; an empty one is ignored. */
Rect rect_union(const Rect *a, const Rect *b);

#endif
