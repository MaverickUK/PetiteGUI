#include <i86.h>
#include "gfx.h"
#include "mouse.h"

static int present;

/* The arrow, every other row: in a 200-line mode each pixel is 2.4 times taller than it is
 * wide on a 4:3 screen, so a full-height arrow would look stretched. x is black, o white. */
static const char *const ARROW[] = {
    "x...........",
    "xox.........",
    "xooox.......",
    "xooooox.....",
    "xooooooox...",
    "xooooooooox.",
    "xoooxoox....",
    "xox..xoox...",
    "x.....xoox..",
    ".......xx...",
};
#define ARROW_ROWS (sizeof ARROW / sizeof ARROW[0])

/* INT 33h function 9 takes 16 rows of screen mask (AND) then 16 of cursor mask (XOR). */
static uint16_t shape[32];

static void build_shape(void)
{
    unsigned r;
    int q;
    uint16_t bit;

    for (r = 0; r < 16; r++) {
        shape[r] = 0xFFFF;
        shape[16 + r] = 0x0000;
        if (r >= ARROW_ROWS)
            continue;
        for (q = 0; ARROW[r][q]; q++) {
            bit = (uint16_t)(0x8000u >> q);
            if (ARROW[r][q] == '.')
                continue;
            shape[r] &= (uint16_t)~bit;
            if (ARROW[r][q] == 'o')
                shape[16 + r] |= bit;
        }
    }
}

int mouse_init(void)
{
    union REGS r;
    struct SREGS s;
    void __far *vec = *(void __far *__far *)MK_FP(0, 0x33 * 4);
    void __far *fp;

    present = 0;
    if (vec == 0) /* no driver: INT 33h would jump to 0000:0000 on DOS 2 */
        return 0;
    r.x.ax = 0;
    int86(0x33, &r, &r);
    if (r.x.ax != 0xFFFF)
        return 0;
    present = 1;

    r.x.ax = 7;
    r.x.cx = 0;
    r.x.dx = SCREEN_W - 1;
    int86(0x33, &r, &r);
    r.x.ax = 8;
    r.x.cx = 0;
    r.x.dx = SCREEN_H - 1;
    int86(0x33, &r, &r);

    build_shape();
    fp = shape;
    segread(&s);
    r.x.ax = 9;
    r.x.bx = 0; /* hot spot: the tip */
    r.x.cx = 0;
    s.es = FP_SEG(fp);
    r.x.dx = FP_OFF(fp);
    int86x(0x33, &r, &r, &s);

    r.x.ax = 4; /* start in the middle */
    r.x.cx = SCREEN_W / 2;
    r.x.dx = SCREEN_H / 2;
    int86(0x33, &r, &r);
    return 1;
}

int mouse_present(void)
{
    return present;
}

void mouse_show(void)
{
    union REGS r;

    if (!present)
        return;
    r.x.ax = 1;
    int86(0x33, &r, &r);
}

void mouse_hide(void)
{
    union REGS r;

    if (!present)
        return;
    r.x.ax = 2;
    int86(0x33, &r, &r);
}

void mouse_read(int *x, int *y, int *buttons)
{
    union REGS r;

    if (!present) {
        *x = *y = *buttons = 0;
        return;
    }
    r.x.ax = 3;
    int86(0x33, &r, &r);
    *x = (int)r.x.cx;
    *y = (int)r.x.dx;
    *buttons = (int)r.x.bx;
}
