#include <direct.h>
#include <dos.h>
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "swap.h"

#pragma pack(push, 1)
typedef struct SwapInfo {      /* must match the Info offsets in swapcore.asm */
    long bytes;
    uint8_t ems_allocated;
    uint8_t file_allocated;
    uint16_t ems_handle;
    uint16_t frame_seg;
    uint16_t file_handle;
    uint16_t psp;
} SwapInfo;
#pragma pack(pop)

/* Far functions, as everything is in the medium model. */
extern unsigned __cdecl swap_core(const char __far *path, const char __far *cmdline, SwapInfo __far *info);
extern unsigned __cdecl swap_first_seg(void);

#define EMS_PAGE 16384L
#define SWAP_FILE "SWAP.$$$"

/* The memory control block before our PSP says how many paragraphs we own. */
static unsigned block_paras(void)
{
    return *(unsigned __far *)MK_FP(_psp - 1, 3);
}

long swap_bytes(void)
{
    return ((long)_psp + block_paras() - swap_first_seg()) * 16L;
}

long swap_resident_bytes(void)
{
    return ((long)swap_first_seg() - _psp) * 16L;
}

static unsigned largest_free(void)
{
    union REGS r;

    r.h.ah = 0x48;
    r.x.bx = 0xFFFF; /* more than there is: DOS says how much there is */
    int86(0x21, &r, &r);
    return r.x.bx;
}

long swap_free_kb(void)
{
    unsigned next = _psp + block_paras(), keep = swap_first_seg() - _psp;
    long paras = largest_free();
    long after;

    /* Once we shrink, our freed memory joins the free block after us, if there is one */
    if (*(unsigned __far *)MK_FP(next, 1) == 0) {
        after = (long)block_paras() - keep + *(unsigned __far *)MK_FP(next, 3);
        if (after > paras)
            paras = after;
    }
    return paras * 16L / 1024L;
}

int swap_ems_present(void)
{
    static const char name[] = "EMMXXXX0";
    void __far *vec = *(void __far *__far *)MK_FP(0, 0x67 * 4);
    const char __far *dev;
    int i;

    if (vec == 0)
        return 0;
    dev = (const char __far *)MK_FP(FP_SEG(vec), 0x0A); /* the driver's device name */
    for (i = 0; i < 8; i++)
        if (dev[i] != name[i])
            return 0;
    return 1;
}

long swap_ems_free_kb(void)
{
    union REGS r;

    if (!swap_ems_present())
        return 0;
    r.h.ah = 0x42; /* unallocated pages */
    int86(0x67, &r, &r);
    return r.h.ah == 0 ? (long)r.x.bx * 16L : 0;
}

/* An interrupt handler inside the memory we swap out would crash the machine. */
static int vector_into_us(void)
{
    unsigned long lo = (unsigned long)swap_first_seg() * 16UL;
    unsigned long hi = ((unsigned long)_psp + block_paras()) * 16UL;
    unsigned __far *ivt = (unsigned __far *)MK_FP(0, 0);
    unsigned long a;
    int i;

    for (i = 0; i < 256; i++) {
        if (i == 0x30 || i == 0x31)
            continue; /* not vectors: DOS keeps a far JMP there for CP/M-style calls */
        a = (unsigned long)ivt[i * 2 + 1] * 16UL + ivt[i * 2];
        if (a >= lo && a < hi)
            return i;
    }
    return -1;
}

static int ems_alloc(SwapInfo *info, long bytes)
{
    union REGS r;

    r.h.ah = 0x43;
    r.x.bx = (unsigned)((bytes + EMS_PAGE - 1) / EMS_PAGE);
    int86(0x67, &r, &r);
    if (r.h.ah != 0)
        return 0;
    info->ems_handle = r.x.dx;
    r.h.ah = 0x41; /* page frame */
    int86(0x67, &r, &r);
    if (r.h.ah != 0 || r.x.bx == 0) {
        r.h.ah = 0x45;
        r.x.dx = info->ems_handle;
        int86(0x67, &r, &r);
        return 0;
    }
    info->frame_seg = r.x.bx;
    info->ems_allocated = 1;
    return 1;
}

static void ems_free(SwapInfo *info)
{
    union REGS r;

    r.h.ah = 0x45;
    r.x.dx = info->ems_handle;
    int86(0x67, &r, &r);
    info->ems_allocated = 0;
}

static int file_alloc(SwapInfo *info, const char *path, long bytes)
{
    struct diskfree_t df;
    unsigned drive;
    int handle;

    drive = path[1] == ':' ? (unsigned)((path[0] & ~0x20) - 'A' + 1) : 0;
    if (_dos_getdiskfree(drive, &df) != 0)
        return 0;
    if ((long)df.avail_clusters * df.sectors_per_cluster * df.bytes_per_sector <= bytes)
        return 0;
    if (_dos_creat(path, _A_HIDDEN | _A_SYSTEM, &handle) != 0)
        return 0;
    info->file_handle = (uint16_t)handle;
    info->file_allocated = 1;
    return 1;
}

static void file_free(SwapInfo *info, const char *path)
{
    _dos_close(info->file_handle);
    _dos_setfileattr(path, _A_NORMAL);
    remove(path);
    info->file_allocated = 0;
}

/* Make dir ("C:\GAMES\KEEN\") the current drive and directory. */
static int change_dir(const char *dir)
{
    char d[80];
    unsigned n;
    size_t len;

    strncpy(d, dir, sizeof d - 1);
    d[sizeof d - 1] = '\0';
    len = strlen(d);
    if (len > 3 && d[len - 1] == '\\')
        d[len - 1] = '\0'; /* "C:\GAMES" but "C:\" */
    if (d[1] == ':')
        _dos_setdrive((unsigned)((d[0] & ~0x20) - 'A' + 1), &n);
    return chdir(d) == 0;
}

unsigned swap_run(const char *prog, const char *args, const char *dir, const SwapOptions *o, SwapReport *r)
{
    SwapInfo info;
    char swap_path[96], tail[130], old_dir[80];
    unsigned old_drive, n, status;
    int v;

    memset(&info, 0, sizeof info);
    memset(r, 0, sizeof *r);
    r->vector = -1;

    v = vector_into_us();
    if (v >= 0) {
        r->vector = v;
        return SWAP_ERR_VECTOR;
    }

    _dos_getdrive(&old_drive);
    if (getcwd(old_dir, sizeof old_dir) == NULL)
        old_dir[0] = '\0';

    info.bytes = swap_bytes();
    info.psp = _psp;
    r->bytes = info.bytes;
    sprintf(swap_path, "%s%s", o->temp_dir, SWAP_FILE);
    if (!(o->use_ems && swap_ems_present() && ems_alloc(&info, info.bytes)) &&
        !file_alloc(&info, swap_path, info.bytes))
        return SWAP_ERR_NO_SPACE;
    r->to_ems = info.ems_allocated;

    if (!change_dir(dir)) {
        status = SWAP_ERR_DIR;
    } else {
        /* DOS passes the tail as typed: after the program's name, so with a space first */
        tail[0] = '\0';
        if (args[0]) {
            tail[0] = ' ';
            strncpy(tail + 1, args, sizeof tail - 2);
            tail[sizeof tail - 1] = '\0';
        }
        status = swap_core(prog, tail, &info);
    }

    if (info.ems_allocated)
        ems_free(&info);
    if (info.file_allocated)
        file_free(&info, swap_path);
    _dos_setdrive(old_drive, &n);
    if (old_dir[0])
        chdir(old_dir);
    return status;
}
