#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "files.h"

static int sort_by_size;

static int compare_rows(const void *a, const void *b)
{
    const Row *x = (const Row *)a, *y = (const Row *)b;

    if (strcmp(x->name, "..") == 0)
        return -1;
    if (strcmp(y->name, "..") == 0)
        return 1;
    if (x->dir != y->dir)
        return x->dir ? -1 : 1;
    if (sort_by_size && x->size != y->size)
        return x->size > y->size ? -1 : 1;
    return strcmp(x->name, y->name);
}

void files_sort(Win *w, int by_size)
{
    sort_by_size = by_size;
    qsort(w->rows, (size_t)w->nrows, sizeof(Row), compare_rows);
}

void files_load(Win *w, int by_size)
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
        r->dir = (uint8_t)((f.attrib & _A_SUBDIR) != 0);
    }
    if (!w->not_ready && _dos_getdiskfree((unsigned)((w->path[0] & ~0x20) - 'A' + 1), &df) == 0)
        w->free_kb = (long)df.avail_clusters * df.sectors_per_cluster * df.bytes_per_sector / 1024L;
    files_sort(w, by_size);
}
