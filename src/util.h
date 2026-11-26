/* Small helpers with no DOS in them, so the host tests build them. */
#ifndef UTIL_H
#define UTIL_H

/* Round to the nearest multiple of 8, or up to one: window edges sit on whole bytes. */
#define SNAP8(v) (((v) + 4) & ~7)
#define SNAP8_UP(v) (((v) + 7) & ~7)

/* 54645 -> "54,645". buf needs 14 characters. */
void fmt_thousands(long v, char *buf);

/* Directory paths end with a backslash: "C:\GAMES\KEEN\". */
/* Go up one level, putting the name left in child (if not NULL). Zero at a drive's root. */
int path_parent(char *path, char *child, int child_size);
/* Files the editor opens on a double-click: .TXT, .INI and .CFG. */
int path_is_text(const char *name);
/* Programs DOS runs: .EXE, .COM and .BAT. */
int path_is_program(const char *name);
/* "TXT" for "README.TXT", "" for "README". */
const char *path_ext(const char *name);

#endif
