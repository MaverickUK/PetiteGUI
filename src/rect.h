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
/* What's left of a when b is taken out of it: up to 4 rectangles in out, not overlapping;
 * returns how many. */
int rect_subtract(const Rect *a, const Rect *b, Rect *out);
/* r with its bottom-right corner dragged to make it w x h, keeping its top-left: no smaller
 * than min_w x min_h, no further than max_x, max_y (exclusive), and w a multiple of 8. */
Rect rect_resize(const Rect *r, int w, int h, int min_w, int min_h, int max_x, int max_y);

#endif
