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
/* dir (ending in a backslash) and name: "C:\GAMES\" + "DOOM" -> "C:\GAMES\DOOM". Zero if it
 * wouldn't fit in size. */
int path_join(char *out, int size, const char *dir, const char *name);
/* Whether path is dir or inside it (a folder can't be copied into itself). Case doesn't
 * matter; a trailing backslash on either doesn't either. */
int path_inside(const char *path, const char *dir);
int path_same_drive(const char *a, const char *b);
/* A DOS 8.3 name: 1-8 characters, then optionally a dot and 1-3, none of them spaces or
 * any of . " / \ [ ] : | < > + = ; , * ? (wildcards aren't names). */
int path_valid_name(const char *name);

/* Programs DOS runs: .EXE, .COM and .BAT. */
int path_is_program(const char *name);
/* "TXT" for "README.TXT", "" for "README". */
const char *path_ext(const char *name);

#endif
