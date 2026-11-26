#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "files.h"
#include "util.h"

static int sort_by;

static int compare_rows(const void *a, const void *b)
{
    const Row *x = (const Row *)a, *y = (const Row *)b;

    if (strcmp(x->name, "..") == 0)
        return -1;
    if (strcmp(y->name, "..") == 0)
        return 1;
    if (x->dir != y->dir)
        return x->dir ? -1 : 1;
    if (sort_by == SORT_SIZE && x->size != y->size)
        return x->size > y->size ? -1 : 1; /* biggest first */
    if (sort_by == SORT_DATE && (x->date != y->date || x->time != y->time)) /* newest first */
        return x->date != y->date ? (x->date > y->date ? -1 : 1) : (x->time > y->time ? -1 : 1);
    return strcmp(x->name, y->name);
}

void files_sort(Win *w, int by)
{
    sort_by = by;
    qsort(w->rows, (size_t)w->nrows, sizeof(Row), compare_rows);
}

void files_load(Win *w, int by)
{
    char spec[96];
    struct find_t f;
    struct diskfree_t df;
    unsigned rc;
    Row *r;

    w->nrows = 0;
    w->truncated = 0;
    w->not_ready = 0;
    w->free_kb = 0;
    w->anchor = -1;
    sprintf(spec, "%s*.*", w->path);
    rc = _dos_findfirst(spec, _A_SUBDIR | _A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_ARCH, &f);
    if (rc != 0 && rc != 0x12) /* 12h: no files, an empty directory */
        w->not_ready = 1;
    for (; rc == 0; rc = _dos_findnext(&f)) {
        if (strcmp(f.name, ".") == 0 || (strcmp(f.name, "..") == 0 && strlen(w->path) <= 3))
            continue; /* DOSBox lists ".." even at a drive's root */
        if (w->nrows >= w->cap) {
            w->truncated = 1;
            break;
        }
        r = &w->rows[w->nrows++];
        strncpy(r->name, f.name, sizeof r->name - 1);
        r->name[sizeof r->name - 1] = '\0';
        r->size = (long)f.size;
        r->attrib = f.attrib;
        r->dir = (uint8_t)((f.attrib & _A_SUBDIR) != 0);
        r->date = f.wr_date;
        r->time = f.wr_time;
        r->picked = 0;
    }
    w->name_w = w->size_w = 0;
    for (r = w->rows; r < w->rows + w->nrows; r++) {
        char size[16];

        if ((int)strlen(r->name) * 8 > w->name_w)
            w->name_w = (int)strlen(r->name) * 8;
        if (!r->dir) {
            fmt_thousands(r->size, size);
            if ((int)strlen(size) * 8 > w->size_w)
                w->size_w = (int)strlen(size) * 8;
        }
    }
    if (!w->not_ready && _dos_getdiskfree((unsigned)((w->path[0] & ~0x20) - 'A' + 1), &df) == 0)
        w->free_kb = (long)df.avail_clusters * df.sectors_per_cluster * df.bytes_per_sector / 1024L;
    files_sort(w, by);
}
