/* Screensavers. After PETITE.CFG's saverdelay minutes with no key, click or mouse movement
 * (and no menu or alert open), the chosen saver runs until any of those, then the desktop is
 * redrawn. Each saver draws on the screen as it is: two colours in Paper, 16 in Ink. */
#ifndef SAVER_H
#define SAVER_H

#define SAVER_OFF (-1)
#define SAVER_RANDOM (-2)
#define SAVER_UNKNOWN (-3)

typedef struct Saver {
    const char *key;   /* PETITE.CFG's saver: value */
    const char *label; /* in the Saver menu */
    void (*start)(void);
    void (*step)(void); /* a frame: quick, never waiting for long */
    void (*stop)(void);
} Saver;

extern const Saver savers[];
extern const int num_savers;

/* The index of a saver key, SAVER_OFF ("off"), SAVER_RANDOM ("random") or SAVER_UNKNOWN. */
int saver_by_key(const char *key);
const char *saver_key(int index);

/* Run a saver (SAVER_RANDOM picks one) until a key, a click or the mouse moves, or for steps
 * steps if steps > 0 (the tests). redraw puts the desktop back, for the savers that start
 * from it again. The caller redraws the desktop afterwards. */
void saver_run(int index, void (*redraw)(void), long steps);

#endif
