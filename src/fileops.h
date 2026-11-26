/* Copying, moving and deleting files and whole folders, and making folders. Paths are full
 * ("C:\GAMES\DOOM"), without a trailing backslash except a drive's root ("C:\").
 *
 * A progress function, if given, is called before each file with its name and how many have
 * been done; returning zero stops the operation (Esc). */
#ifndef FILEOPS_H
#define FILEOPS_H

enum {
    FO_OK, FO_STOPPED, FO_NO_MEMORY, FO_CANT_READ, FO_CANT_WRITE, FO_DISK_FULL, FO_EXISTS,
    FO_INTO_ITSELF, FO_NOT_FOUND, FO_CANT_DELETE, FO_CANT_MAKE
};

typedef int (*FoProgress)(const char *name, long done);

/* Is path a folder? Does it exist at all? */
int fo_is_dir(const char *path);
int fo_exists(const char *path);
/* Files under path (1 for a file), and their bytes. */
void fo_count(const char *path, long *files, long *bytes);

int fo_delete(const char *path, FoProgress progress);
/* Copy src (a file or a folder) to dst, its full new path. replace allows dst to exist
 * already (folders merge; files are overwritten). */
int fo_copy(const char *src, const char *dst, int replace, FoProgress progress);
/* Move: a rename on the same drive (instant) where DOS can, else a copy and a delete. */
int fo_move(const char *src, const char *dst, int replace, FoProgress progress);
int fo_rename(const char *src, const char *dst);
int fo_mkdir(const char *path);

const char *fo_error_text(int error);

#endif
