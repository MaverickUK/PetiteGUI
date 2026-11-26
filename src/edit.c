#include <dos.h>
#include <fcntl.h>
#include <i86.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include "edit.h"

Editor E;

static struct {
    int active;
    char path[80];
    unsigned cur, top_line;
    int rows, cols;
} suspended;

/* ---- Files ---- */

static void set_name(void)
{
    const char *slash = strrchr(E.path, '\\');

    strncpy(E.name, slash != NULL ? slash + 1 : E.path, sizeof E.name - 1);
    E.name[sizeof E.name - 1] = '\0';
}

int edit_open(const char *path)
{
    int h;
    long size;
    unsigned cap, seg, got;
    char __far *buf;

    h = open(path, O_RDONLY | O_BINARY);
    if (h < 0)
        return EDIT_CANT_READ;
    size = filelength(h);
    if (size < 0 || size > (long)EDIT_MAX_FILE) {
        close(h);
        return size < 0 ? EDIT_CANT_READ : EDIT_TOO_BIG;
    }
    cap = (unsigned)size + 16384u > 65520u ? 65520u : (unsigned)size + 16384u;
    if (_dos_allocmem((cap + 15) / 16, &seg) != 0) {
        close(h);
        return EDIT_NO_MEMORY;
    }
    buf = (char __far *)MK_FP(seg, 0);
    /* Read to the end of the block, then load forward from there, CR LF to \n */
    if (_dos_read(h, buf + cap - (unsigned)size, (unsigned)size, &got) != 0 || got != (unsigned)size) {
        close(h);
        _dos_freemem(seg);
        return EDIT_CANT_READ;
    }
    close(h);

    memset(&E, 0, sizeof E);
    E.open = 1;
    E.seg = seg;
    strncpy(E.path, path, sizeof E.path - 1);
    set_name();
    tb_init(&E.t, buf, cap);
    tb_load(&E.t, buf + cap - (unsigned)size, (unsigned)size, &E.crlf);
    E.lines = tb_lines(&E.t);
    E.rows = 1;
    E.cols = 1;
    return EDIT_OK;
}

void edit_close(void)
{
    if (E.open && E.seg != 0)
        _dos_freemem(E.seg);
    memset(&E, 0, sizeof E);
}

/* Write to PETITE.$$$ beside the file, then put it in the file's place, so a failed write
 * (a full disk) leaves the file as it was. */
int edit_save(void)
{
    char tmp[96], chunk[512];
    const char *slash;
    unsigned i, len = tb_len(&E.t), n = 0;
    int h, ok = 1;
    char c;

    slash = strrchr(E.path, '\\');
    sprintf(tmp, "%.*sPETITE.$$$", slash != NULL ? (int)(slash - E.path + 1) : 0, E.path);
    h = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (h < 0)
        return EDIT_CANT_WRITE;
    for (i = 0; i < len && ok; i++) {
        c = tb_at(&E.t, i);
        if (c == '\n' && E.crlf)
            chunk[n++] = '\r';
        chunk[n++] = c;
        if (n >= sizeof chunk - 2) {
            ok = write(h, chunk, n) == (int)n;
            n = 0;
        }
    }
    if (ok && n > 0)
        ok = write(h, chunk, n) == (int)n;
    if (close(h) != 0)
        ok = 0;
    if (!ok) {
        remove(tmp);
        return EDIT_CANT_WRITE;
    }
    remove(E.path);
    if (rename(tmp, E.path) != 0)
        return EDIT_CANT_WRITE;
    E.modified = 0;
    return EDIT_OK;
}

/* ---- The view ---- */

static void keep_caret_in_view(void)
{
    unsigned col;

    while (E.cur_line < E.top_line) {
        E.top = tb_prev_line(&E.t, E.top);
        E.top_line--;
    }
    while (E.cur_line >= E.top_line + (unsigned)E.rows) {
        E.top = tb_next_line(&E.t, E.top);
        E.top_line++;
    }
    col = tb_col(&E.t, E.cur);
    if (col < E.left)
        E.left = col - col % 8;
    else if (col >= E.left + (unsigned)E.cols)
        E.left = col - (unsigned)E.cols + 8;
}

void edit_set_view(int rows, int cols)
{
    E.rows = rows > 0 ? rows : 1;
    E.cols = cols > 8 ? cols : 8;
    keep_caret_in_view();
}

void edit_scroll(int lines)
{
    for (; lines < 0 && E.top_line > 0; lines++) {
        E.top = tb_prev_line(&E.t, E.top);
        E.top_line--;
    }
    for (; lines > 0 && E.top_line + (unsigned)E.rows < E.lines; lines--) {
        E.top = tb_next_line(&E.t, E.top);
        E.top_line++;
    }
}

/* The start of the view row's line, or -1 (as unsigned) past the last line. */
static unsigned row_start(int row)
{
    unsigned p = E.top, next;

    while (row-- > 0) {
        next = tb_next_line(&E.t, p);
        if (next == p)
            return (unsigned)-1;
        p = next;
    }
    return p;
}

int edit_row_text(int row, char *out)
{
    unsigned p = row_start(row), end, n = 0;
    char c;

    if (p == (unsigned)-1) {
        out[0] = '\0';
        return -1;
    }
    end = tb_line_end(&E.t, p);
    for (p += E.left; p < end && n < (unsigned)E.cols; p++) {
        c = tb_at(&E.t, p);
        out[n++] = (unsigned char)c < 32 ? ' ' : c;
    }
    out[n] = '\0';
    return (int)n;
}

int edit_caret(int *row, int *col)
{
    unsigned c = tb_col(&E.t, E.cur);

    *row = (int)(E.cur_line - E.top_line);
    *col = (int)(c - E.left);
    return E.cur_line >= E.top_line && *row < E.rows && c >= E.left && *col < E.cols;
}

void edit_click(int row, int col)
{
    unsigned p = E.top, next, line = E.top_line;

    while (row-- > 0) {
        next = tb_next_line(&E.t, p);
        if (next == p)
            break;
        p = next;
        line++;
    }
    E.cur = tb_at_col(&E.t, p, E.left + (unsigned)(col < 0 ? 0 : col));
    E.cur_line = line;
    E.want_col = tb_col(&E.t, E.cur);
    keep_caret_in_view();
}

/* ---- Keys ---- */

static void move_up(void)
{
    if (E.cur_line == 0)
        return;
    E.cur = tb_at_col(&E.t, tb_prev_line(&E.t, tb_line_start(&E.t, E.cur)), E.want_col);
    E.cur_line--;
}

static void move_down(void)
{
    unsigned start = tb_line_start(&E.t, E.cur), next = tb_next_line(&E.t, start);

    if (next == start)
        return;
    E.cur = tb_at_col(&E.t, next, E.want_col);
    E.cur_line++;
}

static int insert(char c)
{
    if (!tb_insert(&E.t, E.cur, c))
        return EDIT_FULL;
    E.cur++;
    if (c == '\n') {
        E.cur_line++;
        E.lines++;
    }
    E.modified = 1;
    return EDIT_OK;
}

int edit_key(int ch, int ext)
{
    int vertical = 0, i, result = EDIT_OK;
    char c;

    keep_caret_in_view(); /* edits happen where the caret shows */
    if (ch == 0) {
        switch (ext) {
        case 72: move_up(); vertical = 1; break;                  /* up */
        case 80: move_down(); vertical = 1; break;                /* down */
        case 73: for (i = 1; i < E.rows; i++) move_up();          /* Page Up */
                 vertical = 1; break;
        case 81: for (i = 1; i < E.rows; i++) move_down();        /* Page Down */
                 vertical = 1; break;
        case 75:                                                  /* left */
            if (E.cur > 0) {
                E.cur--;
                if (tb_at(&E.t, E.cur) == '\n')
                    E.cur_line--;
            }
            break;
        case 77:                                                  /* right */
            if (E.cur < tb_len(&E.t)) {
                if (tb_at(&E.t, E.cur) == '\n')
                    E.cur_line++;
                E.cur++;
            }
            break;
        case 71: E.cur = tb_line_start(&E.t, E.cur); break;       /* Home */
        case 79: E.cur = tb_line_end(&E.t, E.cur); break;         /* End */
        case 119: E.cur = 0; E.cur_line = 0; break;               /* Ctrl+Home */
        case 117: E.cur = tb_len(&E.t); E.cur_line = E.lines - 1; break; /* Ctrl+End */
        case 83:                                                  /* Delete */
            c = tb_at(&E.t, E.cur);
            if (tb_delete(&E.t, E.cur)) {
                if (c == '\n')
                    E.lines--;
                E.modified = 1;
            }
            break;
        default:
            return EDIT_OK;
        }
    } else if (ch == 8) {                                         /* Backspace */
        if (E.cur > 0) {
            c = tb_at(&E.t, E.cur - 1);
            tb_delete(&E.t, E.cur - 1);
            E.cur--;
            if (c == '\n') {
                E.cur_line--;
                E.lines--;
                if (E.cur < E.top) { /* joined the line above the view */
                    E.top = tb_line_start(&E.t, E.cur);
                    E.top_line = E.cur_line;
                }
            }
            E.modified = 1;
        }
    } else if (ch == 13) {
        result = insert('\n');
    } else if (ch == 9) {                                         /* Tab: spaces to a column of 4 */
        do
            result = insert(' ');
        while (result == EDIT_OK && tb_col(&E.t, E.cur) % 4 != 0);
    } else if (ch >= 32 && ch != 127) {
        result = insert((char)ch);
    } else {
        return EDIT_OK;
    }
    if (!vertical)
        E.want_col = tb_col(&E.t, E.cur);
    keep_caret_in_view();
    return result;
}

/* ---- Around a game ---- */

void edit_suspend(void)
{
    suspended.active = E.open;
    if (!E.open)
        return;
    strcpy(suspended.path, E.path);
    suspended.cur = E.cur;
    suspended.top_line = E.top_line;
    suspended.rows = E.rows;
    suspended.cols = E.cols;
    edit_close();
}

int edit_resume(void)
{
    int rows = suspended.rows, cols = suspended.cols;
    unsigned i;

    if (!suspended.active)
        return 0;
    suspended.active = 0;
    if (edit_open(suspended.path) != EDIT_OK)
        return 0;
    E.rows = rows > 0 ? rows : 1;
    E.cols = cols > 8 ? cols : 8;
    E.cur = suspended.cur < tb_len(&E.t) ? suspended.cur : tb_len(&E.t);
    E.cur_line = tb_line_of(&E.t, E.cur);
    E.want_col = tb_col(&E.t, E.cur);
    for (i = 0; i < suspended.top_line && E.top_line + 1 < E.lines; i++) {
        E.top = tb_next_line(&E.t, E.top);
        E.top_line++;
    }
    keep_caret_in_view();
    return 1;
}
