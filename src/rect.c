#include "rect.h"

Rect rect_make(int x, int y, int w, int h)
{
    Rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

int rect_empty(const Rect *r)
{
    return r->w <= 0 || r->h <= 0;
}

int rect_contains(const Rect *r, int x, int y)
{
    return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

int rect_intersect(Rect *out, const Rect *a, const Rect *b)
{
    int x0 = a->x > b->x ? a->x : b->x;
    int y0 = a->y > b->y ? a->y : b->y;
    int x1 = a->x + a->w < b->x + b->w ? a->x + a->w : b->x + b->w;
    int y1 = a->y + a->h < b->y + b->h ? a->y + a->h : b->y + b->h;

    out->x = x0;
    out->y = y0;
    out->w = x1 - x0;
    out->h = y1 - y0;
    if (out->w <= 0 || out->h <= 0) {
        out->w = 0;
        out->h = 0;
        return 0;
    }
    return 1;
}

Rect rect_union(const Rect *a, const Rect *b)
{
    int x0, y0, x1, y1;

    if (rect_empty(a))
        return *b;
    if (rect_empty(b))
        return *a;
    x0 = a->x < b->x ? a->x : b->x;
    y0 = a->y < b->y ? a->y : b->y;
    x1 = a->x + a->w > b->x + b->w ? a->x + a->w : b->x + b->w;
    y1 = a->y + a->h > b->y + b->h ? a->y + a->h : b->y + b->h;
    return rect_make(x0, y0, x1 - x0, y1 - y0);
}

Rect rect_resize(const Rect *r, int w, int h, int min_w, int min_h, int max_x, int max_y)
{
    Rect out = *r;

    if (w > max_x - r->x)
        w = max_x - r->x;
    if (h > max_y - r->y)
        h = max_y - r->y;
    w &= ~7;
    if (w < min_w)
        w = min_w;
    if (h < min_h)
        h = min_h;
    out.w = w;
    out.h = h;
    return out;
}
