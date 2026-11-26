/* Small helpers with no DOS in them, so the host tests build them. */
#ifndef UTIL_H
#define UTIL_H

/* Round to the nearest multiple of 8, or up to one: window edges sit on whole bytes. */
#define SNAP8(v) (((v) + 4) & ~7)
#define SNAP8_UP(v) (((v) + 7) & ~7)

/* 54645 -> "54,645". buf needs 14 characters. */
void fmt_thousands(long v, char *buf);

#endif
