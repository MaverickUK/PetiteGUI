/* The mouse, through the DOS mouse driver (INT 33h), which also draws the pointer. Hide it
 * before drawing anything under it, and show it again after: the calls nest. */
#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

/* Reset the driver for the current video mode and set the pointer. Zero if there's no driver. */
int mouse_init(void);
int mouse_present(void);
/* Reset the driver for text mode and stop using it (before running a program). */
void mouse_shutdown(void);
void mouse_show(void);
void mouse_hide(void);
/* buttons: bit 0 left, bit 1 right. */
void mouse_read(int *x, int *y, int *buttons);

#endif
