/* The text editor's buffer: a gap buffer, so typing in the middle of a file moves only the
 * text between the last edit and this one, not the whole file.
 *
 * The text sits at the two ends of one block, with the gap between them; positions count
 * the text only (0 to tb_len). Lines end with \n: CR LF becomes \n when a file is loaded,
 * and goes back to CR LF when it's saved.
 *
 * Pure C, so the host tests build it. On DOS the block is far memory (TB_FAR). */
#ifndef TEXTBUF_H
#define TEXTBUF_H

#ifdef __WATCOMC__
#define TB_FAR __far
#else
#define TB_FAR
#endif

typedef struct TextBuf {
    char TB_FAR *buf;
    unsigned cap;
    unsigned gs, ge; /* the gap: [gs, ge) */
} TextBuf;

void tb_init(TextBuf *t, char TB_FAR *mem, unsigned cap);
/* Fill from n bytes at src, turning CR LF into \n and dropping a final ^Z. src may be the
 * end of t's own block. *crlf says whether there were any CR LFs. */
void tb_load(TextBuf *t, const char TB_FAR *src, unsigned n, int *crlf);

unsigned tb_len(const TextBuf *t);
char tb_at(const TextBuf *t, unsigned pos);
/* Zero if the block is full. */
int tb_insert(TextBuf *t, unsigned pos, char c);
/* Delete the character at pos; zero if there isn't one. */
int tb_delete(TextBuf *t, unsigned pos);

/* The start of pos's line, the end of it (its \n, or the end of the text), and its column. */
unsigned tb_line_start(const TextBuf *t, unsigned pos);
unsigned tb_line_end(const TextBuf *t, unsigned pos);
unsigned tb_col(const TextBuf *t, unsigned pos);
/* The position in the line starting at start, at col or its end if it's shorter. */
unsigned tb_at_col(const TextBuf *t, unsigned start, unsigned col);
/* The start of the line after (or before) the one starting at start; start itself at the
 * last (or first) line. */
unsigned tb_next_line(const TextBuf *t, unsigned start);
unsigned tb_prev_line(const TextBuf *t, unsigned start);
/* Lines in the text (1 for empty text), and the line pos is on (from 0). */
unsigned tb_lines(const TextBuf *t);
unsigned tb_line_of(const TextBuf *t, unsigned pos);

#endif
