/* The drives DOS has, found without reading any of them (an empty floppy drive would make
 * DOS wait, then fail). */
#ifndef DRIVES_H
#define DRIVES_H

#define MAX_DRIVES 26

enum { DRIVE_FLOPPY, DRIVE_FIXED, DRIVE_CDROM, DRIVE_REMOTE };

typedef struct Drive {
    char letter;
    int kind;
} Drive;

/* Fill drives (up to MAX_DRIVES) in letter order; returns how many. */
int drives_find(Drive *drives);
const char *drive_kind_name(int kind);

#endif
