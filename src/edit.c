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

/* The clipboard: its own DOS block, so it lasts when the file is closed or another opened. */
static unsigned clip_seg, clip_len;

#define NEW_FILE_ROOM 16384u /* what a file can grow by */

/* ---- Files ---- */

static void set_name(void)
{
    const char *slash = strrchr(E.path, '\\');

    if (E.path[0] == '\0') {
        strcpy(E.name, "Untitled");
        return;
    }
    strncpy(E.name, slash != NULL ? slash + 1 : E.path, sizeof E.name - 1);
    E.name[sizeof E.name - 1] = '\0';
}

/* A block for size bytes of file and room to grow; NULL if there's no memory. */
static char __far *alloc_text(unsigned size, unsigned *cap, unsigned *seg)
{
    *cap = size + NEW_FILE_ROOM > 65520u ? 65520u : size + NEW_FILE_ROOM;
    if (_dos_allocmem((*cap + 15) / 16, seg) != 0)
        return NULL;
    return (char __far *)MK_FP(*seg, 0);
}

static void start(const char *path, unsigned seg, char __far *buf, unsigned cap)
{
    memset(&E, 0, sizeof E);
    E.open = 1;
    E.seg = seg;
    strncpy(E.path, path, sizeof E.path - 1);
    set_name();
    tb_init(&E.t, buf, cap);
    E.lines = 1;
    E.rows = 1;
    E.cols = 1;
}

int edit_open(const char *path)
{
    int h, crlf;
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
    buf = alloc_text((unsigned)size, &cap, &seg);
    if (buf == NULL) {
        close(h);
        return EDIT_NO_MEMORY;
    }
    /* Read to the end of the block, then load forward from there, CR LF to \n */
    if (_dos_read(h, buf + cap - (unsigned)size, (unsigned)size, &got) != 0 || got != (unsigned)size) {
        close(h);
        _dos_freemem(seg);
        return EDIT_CANT_READ;
    }
    close(h);

    start(path, seg, buf, cap);
    tb_load(&E.t, buf + cap - (unsigned)size, (unsigned)size, &crlf);
    E.crlf = crlf;
    E.lines = tb_lines(&E.t);
    return EDIT_OK;
}

int edit_new(void)
{
    unsigned cap, seg;
    char __far *buf = alloc_text(0, &cap, &seg);

    if (buf == NULL)
        return EDIT_NO_MEMORY;
    start("", seg, buf, cap);
    E.crlf = 1; /* DOS's line ends */
    return EDIT_OK;
}

void edit_close(void)
{
    if (E.open && E.seg != 0)
        _dos_freemem(E.seg);
    memset(&E, 0, sizeof E);
}

/* Write to PETITE.$$$ beside path, then put it in path's place, so a failed write (a full
 * disk) leaves the file as it was. */
static int write_file(const char *path)
{
    char tmp[96], chunk[512];
    const char *slash;
    unsigned i, len = tb_len(&E.t), n = 0;
    int h, ok = 1;
    char c;

    slash = strrchr(path, '\\');
    sprintf(tmp, "%.*sPETITE.$$$", slash != NULL ? (int)(slash - path + 1) : 0, path);
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
    remove(path);
    if (rename(tmp, path) != 0)
        return EDIT_CANT_WRITE;
    return EDIT_OK;
}

int edit_save(void)
{
    if (E.path[0] == '\0' || write_file(E.path) != EDIT_OK)
        return EDIT_CANT_WRITE;
    E.modified = 0;
    return EDIT_OK;
}

int edit_save_as(const char *path)
{
    if (strlen(path) >= sizeof E.path || write_file(path) != EDIT_OK)
        return EDIT_CANT_WRITE;
    strcpy(E.path, path);
    set_name();
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

unsigned edit_view_start(void)
{
    return E.top;
}

unsigned edit_next_line(unsigned p)
{
    unsigned next = tb_next_line(&E.t, p);

    return next == p ? (unsigned)-1 : next;
}

int edit_row_text(int row, char *out)
{
    return edit_line_text(row_start(row), out);
}

int edit_line_text(unsigned p, char *out)
{
    unsigned end, n = 0;
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

int edit_row_selection(int row, int *c0, int *c1)
{
    return edit_line_selection(row_start(row), c0, c1);
}

int edit_line_selection(unsigned p, int *c0, int *c1)
{
    unsigned end, s, e, a, b;
    long from, to;

    if (!edit_selection(&s, &e))
        return 0;
    if (p == (unsigned)-1)
        return 0;
    end = tb_line_end(&E.t, p);
    a = s > p ? s : p;
    b = e < end + 1 ? e : end + 1; /* the line break counts as a column */
    if (a >= b)
        return 0;
    from = (long)(a - p) - (long)E.left;
    to = (long)(b - p) - (long)E.left;
    if (to <= 0 || from >= E.cols)
        return 0;
    *c0 = from < 0 ? 0 : (int)from;
    *c1 = to > E.cols ? E.cols : (int)to;
    return 1;
}

int edit_caret(int *row, int *col)
{
    unsigned c = tb_col(&E.t, E.cur);

    *row = (int)(E.cur_line - E.top_line);
    *col = (int)(c - E.left);
    return E.cur_line >= E.top_line && *row < E.rows && c >= E.left && *col < E.cols;
}

/* ---- The selection ---- */

static int has_selection(void)
{
    return E.anchor != E.cur;
}

/* Nothing selected: the anchor joins the caret. */
static void collapse(void)
{
    E.anchor = E.cur;
    E.anchor_line = E.cur_line;
}

int edit_selection(unsigned *start, unsigned *end)
{
    *start = E.anchor < E.cur ? E.anchor : E.cur;
    *end = E.anchor < E.cur ? E.cur : E.anchor;
    return has_selection();
}

void edit_select_all(void)
{
    E.anchor = 0;
    E.anchor_line = 0;
    E.cur = tb_len(&E.t);
    E.cur_line = E.lines - 1;
    E.want_col = tb_col(&E.t, E.cur); /* the view stays where it is */
}

/* The line pos is on, counted from the caret's (which is known) rather than the top. */
static unsigned line_at(unsigned pos)
{
    return pos >= E.cur ? E.cur_line + tb_count_lines(&E.t, E.cur, pos)
                        : E.cur_line - tb_count_lines(&E.t, pos, E.cur);
}

/* Delete [s, e), s on line s_line; the caret goes to s, with nothing selected. */
static void delete_range(unsigned s, unsigned e, unsigned s_line)
{
    unsigned removed = tb_count_lines(&E.t, s, e);

    if (e <= s || !tb_delete_range(&E.t, s, e - s))
        return;
    E.lines -= removed;
    E.cur = s;
    E.cur_line = s_line;
    collapse();
    /* The view's first line: after the deletion, it moves up with the text; inside it (or
     * just after a deleted line break), it becomes the line the caret is on. */
    if (E.top > e) {
        E.top -= e - s;
        E.top_line -= removed;
    } else if (E.top > s) {
        E.top = tb_line_start(&E.t, s);
        E.top_line = s_line;
    }
    E.modified = 1;
}

static void delete_selection(void)
{
    if (E.anchor < E.cur)
        delete_range(E.anchor, E.cur, E.anchor_line);
    else if (E.cur < E.anchor)
        delete_range(E.cur, E.anchor, E.cur_line);
}

/* Select [s, e) (the caret at e). */
static void select_range(unsigned s, unsigned e)
{
    unsigned s_line = line_at(s), e_line = s_line + tb_count_lines(&E.t, s, e);

    E.anchor = s;
    E.anchor_line = s_line;
    E.cur = e;
    E.cur_line = e_line;
    E.want_col = tb_col(&E.t, E.cur);
}

/* ---- The clipboard ---- */

void edit_clipboard_free(void)
{
    if (clip_seg != 0)
        _dos_freemem(clip_seg);
    clip_seg = 0;
    clip_len = 0;
}

int edit_can_paste(void)
{
    return clip_len > 0;
}

int edit_copy(void)
{
    unsigned s, e, seg;

    if (!edit_selection(&s, &e))
        return EDIT_OK;
    if (_dos_allocmem((e - s + 15) / 16, &seg) != 0)
        return EDIT_NO_MEMORY;
    edit_clipboard_free();
    clip_seg = seg;
    clip_len = e - s;
    tb_copy_out(&E.t, s, clip_len, (char __far *)MK_FP(clip_seg, 0));
    return EDIT_OK;
}

int edit_cut(void)
{
    int rc = edit_copy();

    if (rc == EDIT_OK)
        delete_selection();
    keep_caret_in_view();
    return rc;
}

int edit_paste(void)
{
    const char __far *clip = (const char __far *)MK_FP(clip_seg, 0);
    unsigned s, e, i, breaks = 0;

    if (clip_len == 0)
        return EDIT_OK;
    edit_selection(&s, &e);
    if (tb_room(&E.t) + (e - s) < clip_len)
        return EDIT_FULL;
    delete_selection();
    if (!tb_insert_n(&E.t, E.cur, clip, clip_len))
        return EDIT_FULL;
    for (i = 0; i < clip_len; i++)
        if (clip[i] == '\n')
            breaks++;
    E.cur += clip_len;
    E.cur_line += breaks;
    E.lines += breaks;
    collapse();
    E.want_col = tb_col(&E.t, E.cur);
    E.modified = 1;
    keep_caret_in_view();
    return EDIT_OK;
}

/* ---- Finding ---- */

int edit_find(const char *s)
{
    unsigned n = (unsigned)strlen(s), at = tb_find(&E.t, E.cur, s, n);

    if (at == TB_NONE)
        at = tb_find(&E.t, 0, s, n); /* round to the top */
    if (at == TB_NONE)
        return 0;
    select_range(at, at + n);
    keep_caret_in_view();
    return 1;
}

/* ---- The mouse ---- */

void edit_click(int row, int col, int extend)
{
    unsigned p = E.top, next, line = E.top_line;

    if (row < 0 && E.top_line > 0) { /* above the view */
        p = tb_prev_line(&E.t, E.top);
        line--;
    }
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
    if (!extend)
        collapse();
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

/* Left and right with a selection and no Shift go to its start or end. */
static void to_selection_edge(int end)
{
    if ((E.anchor < E.cur) != (end != 0)) {
        E.cur = E.anchor;
        E.cur_line = E.anchor_line;
    }
}

static int insert(char c)
{
    delete_selection();
    if (!tb_insert(&E.t, E.cur, c))
        return EDIT_FULL;
    E.cur++;
    if (c == '\n') {
        E.cur_line++;
        E.lines++;
    }
    collapse();
    E.modified = 1;
    return EDIT_OK;
}

/* Keys that move the caret (and with Shift, select); the rest edit. */
static int moves(int ext)
{
    return ext == 72 || ext == 80 || ext == 73 || ext == 81 || ext == 75 || ext == 77 ||
           ext == 71 || ext == 79 || ext == 119 || ext == 117;
}

int edit_key(int ch, int ext, int shift)
{
    int vertical = 0, i, result = EDIT_OK;
    char c;

    keep_caret_in_view(); /* edits happen where the caret shows */
    if (ch == 0 && moves(ext)) {
        int selected = has_selection();

        switch (ext) {
        case 72: move_up(); vertical = 1; break;                  /* up */
        case 80: move_down(); vertical = 1; break;                /* down */
        case 73: for (i = 1; i < E.rows; i++) move_up();          /* Page Up */
                 vertical = 1; break;
        case 81: for (i = 1; i < E.rows; i++) move_down();        /* Page Down */
                 vertical = 1; break;
        case 75:                                                  /* left */
            if (selected && !shift)
                to_selection_edge(0);
            else if (E.cur > 0) {
                E.cur--;
                if (tb_at(&E.t, E.cur) == '\n')
                    E.cur_line--;
            }
            break;
        case 77:                                                  /* right */
            if (selected && !shift)
                to_selection_edge(1);
            else if (E.cur < tb_len(&E.t)) {
                if (tb_at(&E.t, E.cur) == '\n')
                    E.cur_line++;
                E.cur++;
            }
            break;
        case 71: E.cur = tb_line_start(&E.t, E.cur); break;       /* Home */
        case 79: E.cur = tb_line_end(&E.t, E.cur); break;         /* End */
        case 119: E.cur = 0; E.cur_line = 0; break;               /* Ctrl+Home */
        case 117: E.cur = tb_len(&E.t); E.cur_line = E.lines - 1; break; /* Ctrl+End */
        }
        if (!shift)
            collapse();
    } else if (ch == 0 && ext == 83) {                            /* Delete */
        if (has_selection())
            delete_selection();
        else if (E.cur < tb_len(&E.t))
            delete_range(E.cur, E.cur + 1, E.cur_line);
    } else if (ch == 8) {                                         /* Backspace */
        if (has_selection()) {
            delete_selection();
        } else if (E.cur > 0) {
            c = tb_at(&E.t, E.cur - 1);
            delete_range(E.cur - 1, E.cur, c == '\n' ? E.cur_line - 1 : E.cur_line);
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
    edit_clipboard_free(); /* nothing of PetiteGUI's may stay allocated under a program */
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
    if ((suspended.path[0] ? edit_open(suspended.path) : edit_new()) != EDIT_OK)
        return 0;
    E.rows = rows > 0 ? rows : 1;
    E.cols = cols > 8 ? cols : 8;
    E.cur = suspended.cur < tb_len(&E.t) ? suspended.cur : tb_len(&E.t);
    E.cur_line = tb_line_of(&E.t, E.cur);
    collapse();
    E.want_col = tb_col(&E.t, E.cur);
    for (i = 0; i < suspended.top_line && E.top_line + 1 < E.lines; i++) {
        E.top = tb_next_line(&E.t, E.top);
        E.top_line++;
    }
    keep_caret_in_view();
    return 1;
}
