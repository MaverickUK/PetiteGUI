#include <string.h>
#include "textbuf.h"

#ifdef __WATCOMC__
#define TB_MOVE _fmemmove
#else
#define TB_MOVE memmove
#endif

void tb_init(TextBuf *t, char TB_FAR *mem, unsigned cap)
{
    t->buf = mem;
    t->cap = cap;
    t->gs = 0;
    t->ge = cap;
}

void tb_load(TextBuf *t, const char TB_FAR *src, unsigned n, int *crlf)
{
    unsigned i, out = 0;
    char c;

    *crlf = 0;
    if (n > 0 && src[n - 1] == 0x1A) /* the end-of-file mark some editors leave */
        n--;
    for (i = 0; i < n && out < t->cap; i++) {
        c = src[i]; /* src is at or after buf + out: copying forward is safe */
        if (c == '\r' && i + 1 < n && src[i + 1] == '\n') {
            *crlf = 1;
            continue;
        }
        t->buf[out++] = c;
    }
    t->gs = out;
    t->ge = t->cap;
}

unsigned tb_len(const TextBuf *t)
{
    return t->cap - (t->ge - t->gs);
}

char tb_at(const TextBuf *t, unsigned pos)
{
    return pos < t->gs ? t->buf[pos] : t->buf[pos + (t->ge - t->gs)];
}

/* Move the gap so it starts at pos. */
static void move_gap(TextBuf *t, unsigned pos)
{
    unsigned n;

    if (pos < t->gs) {
        n = t->gs - pos;
        TB_MOVE(t->buf + t->ge - n, t->buf + pos, n);
        t->gs -= n;
        t->ge -= n;
    } else if (pos > t->gs) {
        n = pos - t->gs;
        TB_MOVE(t->buf + t->gs, t->buf + t->ge, n);
        t->gs += n;
        t->ge += n;
    }
}

int tb_insert(TextBuf *t, unsigned pos, char c)
{
    if (t->gs == t->ge || pos > tb_len(t))
        return 0;
    move_gap(t, pos);
    t->buf[t->gs++] = c;
    return 1;
}

int tb_delete(TextBuf *t, unsigned pos)
{
    if (pos >= tb_len(t))
        return 0;
    move_gap(t, pos);
    t->ge++;
    return 1;
}

unsigned tb_line_start(const TextBuf *t, unsigned pos)
{
    while (pos > 0 && tb_at(t, pos - 1) != '\n')
        pos--;
    return pos;
}

unsigned tb_line_end(const TextBuf *t, unsigned pos)
{
    unsigned len = tb_len(t);

    while (pos < len && tb_at(t, pos) != '\n')
        pos++;
    return pos;
}

unsigned tb_col(const TextBuf *t, unsigned pos)
{
    return pos - tb_line_start(t, pos);
}

unsigned tb_at_col(const TextBuf *t, unsigned start, unsigned col)
{
    unsigned end = tb_line_end(t, start);

    return start + col < end ? start + col : end;
}

unsigned tb_next_line(const TextBuf *t, unsigned start)
{
    unsigned end = tb_line_end(t, start);

    return end < tb_len(t) ? end + 1 : start;
}

unsigned tb_prev_line(const TextBuf *t, unsigned start)
{
    return start == 0 ? 0 : tb_line_start(t, start - 1);
}

unsigned tb_lines(const TextBuf *t)
{
    return tb_line_of(t, tb_len(t)) + 1;
}

unsigned tb_line_of(const TextBuf *t, unsigned pos)
{
    unsigned i, n = 0;

    for (i = 0; i < pos; i++)
        if (tb_at(t, i) == '\n')
            n++;
    return n;
}
