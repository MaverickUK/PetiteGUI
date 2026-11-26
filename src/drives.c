#include <dos.h>
#include <i86.h>
#include <string.h>
#include "drives.h"

/* DOS's own filename parser (INT 21h function 29h) says whether a drive letter is valid,
 * from its tables alone. */
static int drive_exists(char letter)
{
    char name[4], fcb[37];
    union REGS r;
    struct SREGS s;
    void __far *pn = name, __far *pf = fcb;

    name[0] = letter;
    name[1] = ':';
    name[2] = '\0';
    segread(&s);
    r.x.ax = 0x2900;
    s.ds = FP_SEG(pn);
    r.x.si = FP_OFF(pn);
    s.es = FP_SEG(pf);
    r.x.di = FP_OFF(pf);
    int86x(0x21, &r, &r, &s);
    return r.h.al != 0xFF;
}

/* MSCDEX (INT 2Fh AX=150Bh) knows its CD-ROM drives. */
static int is_cdrom(char letter)
{
    union REGS r;

    r.x.ax = 0x150B;
    r.x.bx = 0;
    r.x.cx = (unsigned)(letter - 'A');
    int86(0x2F, &r, &r);
    return r.x.bx == 0xADAD && r.x.ax != 0;
}

/* IOCTL: is a drive remote (a network drive), and is it removable? */
static int is_remote(char letter)
{
    union REGS r;

    r.x.ax = 0x4409;
    r.h.bl = (unsigned char)(letter - 'A' + 1);
    int86(0x21, &r, &r);
    return !r.x.cflag && (r.x.dx & 0x1000) != 0;
}

static int is_removable(char letter)
{
    union REGS r;

    r.x.ax = 0x4408;
    r.h.bl = (unsigned char)(letter - 'A' + 1);
    int86(0x21, &r, &r);
    return !r.x.cflag && r.x.ax == 0;
}

/* The BIOS's equipment word: bit 0 set if there are floppy drives, bits 6-7 how many less
 * one. With one, DOS still answers for B: (the same drive, asking for disks to be swapped). */
static int floppy_drives(void)
{
    union REGS r;

    int86(0x11, &r, &r);
    return (r.x.ax & 1) ? ((r.x.ax >> 6) & 3) + 1 : 0;
}

int drives_find(Drive *drives)
{
    int n = 0, floppies = floppy_drives();
    char c;

    for (c = 'A'; c <= 'Z'; c++) {
        if (c == 'B' && floppies < 2)
            continue;
        if (!drive_exists(c))
            continue;
        drives[n].letter = c;
        if (c <= 'B')
            drives[n].kind = DRIVE_FLOPPY;
        else if (is_cdrom(c))
            drives[n].kind = DRIVE_CDROM;
        else if (is_remote(c))
            drives[n].kind = DRIVE_REMOTE;
        else if (is_removable(c))
            drives[n].kind = DRIVE_FLOPPY;
        else
            drives[n].kind = DRIVE_FIXED;
        n++;
    }
    return n;
}

const char *drive_kind_name(int kind)
{
    switch (kind) {
    case DRIVE_FLOPPY: return "floppy";
    case DRIVE_CDROM: return "CD-ROM";
    case DRIVE_REMOTE: return "network";
    }
    return "hard disk";
}
