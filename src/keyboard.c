#include <bios.h>
#include <i86.h>
#include "keyboard.h"

/* The BIOS data area at 0040:0000: the keyboard buffer's head and tail, and the shift keys. */
#define BIOS_WORD(off) (*(volatile unsigned __far *)MK_FP(0x40, (off)))
#define BIOS_BYTE(off) (*(volatile unsigned char __far *)MK_FP(0x40, (off)))

int kb_hit(void)
{
    /* Compared directly rather than with INT 16h function 1, which can't tell "no key" from
     * Ctrl+Break's key of 0 (that would sit in the buffer for good). */
    return BIOS_WORD(0x1A) != BIOS_WORD(0x1C);
}

int kb_shift(void)
{
    return BIOS_BYTE(0x17) & (KB_SHIFT | KB_CTRL);
}

int kb_read(int *ext)
{
    unsigned k = _bios_keybrd(_KEYBRD_READ);
    int ch = k & 0xFF, scan = k >> 8;

    *ext = 0;
    if (ch == 0 || (ch == 0xE0 && scan != 0)) { /* an extended key (0xE0: the grey keys) */
        *ext = scan;
        return 0;
    }
    /* On a keyboard without separate arrow keys (an XT's), Shift turns the keypad's arrows
     * into digits: take them as the arrows, so Shift+arrow selects text. */
    if (scan >= 0x47 && scan <= 0x53 && (kb_shift() & KB_SHIFT) &&
        ((ch >= '0' && ch <= '9') || ch == '.')) {
        *ext = scan;
        return 0;
    }
    return ch;
}

void kb_flush(void)
{
    int ext;

    while (kb_hit())
        kb_read(&ext);
}
