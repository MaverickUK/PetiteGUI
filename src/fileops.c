#include <direct.h>
#include <dos.h>
#include <errno.h>
#include <i86.h>
#include <stdio.h>
#include <string.h>
#include "fileops.h"
#include "util.h"

#define PATH_SIZE 96
#define COPY_BUF 32768u
#define ALL_FILES (_A_SUBDIR | _A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_ARCH)

static long done_count;
static FoProgress progress_fn;
static char __far *buf;
static unsigned buf_seg;

int fo_is_dir(const char *path)
{
    unsigned attr;

    if (strlen(path) == 3 && path[1] == ':') /* a drive's root */
        return 1;
    return _dos_getfileattr(path, &attr) == 0 && (attr & _A_SUBDIR);
}

int fo_exists(const char *path)
{
    unsigned attr;

    return (strlen(path) == 3 && path[1] == ':') || _dos_getfileattr(path, &attr) == 0;
}

/* "dir\*.*" for a search, dir not ending in a backslash (or a root, which does) */
static void spec_of(char *spec, const char *dir)
{
    size_t n = strlen(dir);

    sprintf(spec, "%s%s*.*", dir, n > 0 && dir[n - 1] == '\\' ? "" : "\\");
}

static void child_of(char *out, const char *dir, const char *name)
{
    size_t n = strlen(dir);

    sprintf(out, "%s%s%s", dir, n > 0 && dir[n - 1] == '\\' ? "" : "\\", name);
}

void fo_count(const char *path, long *files, long *bytes)
{
    char spec[PATH_SIZE], child[PATH_SIZE];
    struct find_t f;

    if (!fo_is_dir(path)) {
        if (_dos_findfirst(path, ALL_FILES, &f) == 0) {
            (*files)++;
            *bytes += (long)f.size;
        }
        return;
    }
    spec_of(spec, path);
    if (_dos_findfirst(spec, ALL_FILES, &f) != 0)
        return;
    do {
        if (f.name[0] == '.')
            continue;
        child_of(child, path, f.name);
        if (f.attrib & _A_SUBDIR) {
            fo_count(child, files, bytes);
        } else {
            (*files)++;
            *bytes += (long)f.size;
        }
    } while (_dos_findnext(&f) == 0);
}

static int tick(const char *name)
{
    if (progress_fn != NULL && !progress_fn(name, done_count))
        return 0;
    done_count++;
    return 1;
}

/* ---- Deleting ---- */

static int delete_file(const char *path)
{
    if (remove(path) == 0)
        return FO_OK;
    _dos_setfileattr(path, _A_NORMAL); /* read-only, hidden or system: it was asked for */
    return remove(path) == 0 ? FO_OK : FO_CANT_DELETE;
}

static int delete_tree(const char *path)
{
    char spec[PATH_SIZE], child[PATH_SIZE];
    struct find_t f;
    int rc;

    if (!fo_is_dir(path)) {
        if (!tick(path))
            return FO_STOPPED;
        return delete_file(path);
    }
    spec_of(spec, path);
    /* Delete one entry, then search again from the start: removing entries while a search is
     * open can make DOS skip some. */
    for (;;) {
        if (_dos_findfirst(spec, ALL_FILES, &f) != 0)
            break;
        while (f.name[0] == '.')
            if (_dos_findnext(&f) != 0)
                goto empty;
        child_of(child, path, f.name);
        rc = delete_tree(child);
        if (rc != FO_OK)
            return rc;
    }
empty:
    if (strlen(path) == 3) /* a drive's root stays */
        return FO_OK;
    _dos_setfileattr(path, _A_SUBDIR);
    return rmdir(path) == 0 ? FO_OK : FO_CANT_DELETE;
}

int fo_delete(const char *path, FoProgress progress)
{
    progress_fn = progress;
    done_count = 0;
    if (!fo_exists(path))
        return FO_NOT_FOUND;
    return delete_tree(path);
}

/* ---- Copying ---- */

static int copy_file(const char *src, const char *dst)
{
    int in, out, rc = FO_OK;
    unsigned got, put, date, time, attr;

    if (!tick(src))
        return FO_STOPPED;
    if (_dos_open(src, 0 /* read */, &in) != 0)
        return FO_CANT_READ;
    _dos_getftime(in, &date, &time);
    if (_dos_getfileattr(dst, &attr) == 0)
        _dos_setfileattr(dst, _A_NORMAL); /* a read-only file being replaced */
    if (_dos_creat(dst, _A_NORMAL, &out) != 0) {
        _dos_close(in);
        return FO_CANT_WRITE;
    }
    for (;;) {
        if (_dos_read(in, buf, COPY_BUF, &got) != 0) {
            rc = FO_CANT_READ;
            break;
        }
        if (got == 0)
            break;
        if (_dos_write(out, buf, got, &put) != 0 || put != got) {
            rc = FO_DISK_FULL;
            break;
        }
    }
    _dos_setftime(out, date, time); /* the copy keeps the file's date */
    _dos_close(in);
    _dos_close(out);
    if (rc != FO_OK) {
        remove(dst); /* no half copies */
        return rc;
    }
    if (_dos_getfileattr(src, &attr) == 0)
        _dos_setfileattr(dst, attr & (_A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_ARCH));
    return FO_OK;
}

static int copy_tree(const char *src, const char *dst)
{
    char spec[PATH_SIZE], s[PATH_SIZE], d[PATH_SIZE];
    struct find_t f;
    int rc;

    if (!fo_is_dir(src))
        return copy_file(src, dst);
    if (!fo_is_dir(dst) && mkdir(dst) != 0)
        return FO_CANT_MAKE;
    spec_of(spec, src);
    if (_dos_findfirst(spec, ALL_FILES, &f) != 0)
        return FO_OK;
    do {
        if (f.name[0] == '.')
            continue;
        child_of(s, src, f.name);
        child_of(d, dst, f.name);
        rc = copy_tree(s, d);
        if (rc != FO_OK)
            return rc;
    } while (_dos_findnext(&f) == 0);
    return FO_OK;
}

int fo_copy(const char *src, const char *dst, int replace, FoProgress progress)
{
    int rc;

    progress_fn = progress;
    done_count = 0;
    if (!fo_exists(src))
        return FO_NOT_FOUND;
    if (stricmp(src, dst) == 0 || (fo_is_dir(src) && path_inside(dst, src)))
        return FO_INTO_ITSELF;
    if (fo_exists(dst) && !replace)
        return FO_EXISTS;
    if (_dos_allocmem(COPY_BUF / 16, &buf_seg) != 0)
        return FO_NO_MEMORY;
    buf = (char __far *)MK_FP(buf_seg, 0);
    rc = copy_tree(src, dst);
    _dos_freemem(buf_seg);
    buf = NULL;
    return rc;
}

/* ---- Moving, renaming, making ---- */

int fo_rename(const char *src, const char *dst)
{
    if (!fo_exists(src))
        return FO_NOT_FOUND;
    if (fo_exists(dst))
        return FO_EXISTS;
    return rename(src, dst) == 0 ? FO_OK : FO_CANT_WRITE;
}

int fo_move(const char *src, const char *dst, int replace, FoProgress progress)
{
    int rc;

    if (stricmp(src, dst) == 0 || (fo_is_dir(src) && path_inside(dst, src)))
        return FO_INTO_ITSELF;
    if (fo_exists(dst)) {
        if (!replace)
            return FO_EXISTS;
        if (!fo_is_dir(src) && !fo_is_dir(dst)) {
            rc = fo_delete(dst, NULL); /* a file replaced by a file */
            if (rc != FO_OK)
                return rc;
        }
    }
    /* DOS renames a file into another folder on the same drive; a folder only in place */
    if (path_same_drive(src, dst) && !fo_exists(dst) && rename(src, dst) == 0)
        return FO_OK;
    rc = fo_copy(src, dst, replace, progress);
    if (rc != FO_OK)
        return rc;
    return fo_delete(src, NULL);
}

int fo_mkdir(const char *path)
{
    if (fo_exists(path))
        return FO_EXISTS;
    return mkdir(path) == 0 ? FO_OK : FO_CANT_MAKE;
}

const char *fo_error_text(int error)
{
    switch (error) {
    case FO_STOPPED: return "It was stopped.";
    case FO_NO_MEMORY: return "There isn't enough memory.";
    case FO_CANT_READ: return "A file couldn't be read.";
    case FO_CANT_WRITE: return "It couldn't be written there.";
    case FO_DISK_FULL: return "The disk is full.";
    case FO_EXISTS: return "There's one of that name there.";
    case FO_INTO_ITSELF: return "A folder can't go inside itself.";
    case FO_NOT_FOUND: return "It isn't there any more.";
    case FO_CANT_DELETE: return "Something couldn't be deleted.";
    case FO_CANT_MAKE: return "The folder couldn't be made.";
    }
    return "Done.";
}
