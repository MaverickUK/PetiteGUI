/* The keyboard, read through the BIOS (INT 16h) rather than DOS. DOS's console calls (which
 * kbhit and getch use) check for Ctrl+C and would end the program on one; the editor uses
 * Ctrl+C to copy. */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KB_SHIFT 0x03 /* either Shift key */
#define KB_CTRL 0x04

/* Is a key waiting? */
int kb_hit(void);
/* The next key, waiting for one: its character, or 0 and its extended scan code in *ext
 * (the arrows, F-keys and so on; *ext is 0 for a character). Both are 0 for Ctrl+Break. */
int kb_read(int *ext);
/* Which shift keys are down: KB_SHIFT, KB_CTRL. */
int kb_shift(void);
/* Throw away any keys waiting. */
void kb_flush(void);

#endif
