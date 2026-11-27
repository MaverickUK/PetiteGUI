/* A theme is the colour table the widgets draw with. Paper and Ink share every layout and
 * every widget; only this table (and the driver) differs. Colours are the mode's own: 0-1 in
 * Paper, the 16 CGA colours in Ink. */
#ifndef THEME_H
#define THEME_H

#include <stdint.h>

typedef struct Theme {
    const char *name;
    uint8_t desk_on, desk_off;          /* desktop: a checker of two colours */
    uint8_t menu_bg, menu_text;
    uint8_t frame, win_bg, text;
    uint8_t sel, sel_text;               /* selected rows, menu items, icon labels */
    uint8_t shadow;
    uint8_t title_on, title_on_text;     /* the active window's title bar */
    uint8_t title_off, title_off_text;
    uint8_t stripes;                     /* active title bar striped (Paper) or solid (Ink) */
    uint8_t track_on, track_off, thumb;  /* scroll bar */
    uint8_t folder_line, folder_fill, file_line, file_fill;
    int disabled;                        /* text colour, or -1 to dither */
    int separator;                       /* menu separator colour, or -1 to dither */
    int label_bg;                        /* desktop icon labels, or -1 for none */
    uint8_t label_text;
    uint8_t icon_line, icon_fill, icon_floppy, icon_shutter, icon_label, icon_detail, icon_led;
    uint8_t dialog_bg, alert_bg, alert_text;
} Theme;

extern const Theme theme_paper;
/* Not const: its colour scheme can change (ink_scheme_apply). */
extern Theme theme_ink;

/* Ink's colour schemes, chosen in the Options menu and kept in PETITE.CFG as colours:key.
 * Each sets only the desktop's checker and an accent for the active title bar and the
 * selection, so every scheme still looks like PetiteGUI. */
typedef struct InkScheme {
    const char *name, *key;
    uint8_t desk_on, desk_off; /* the same twice for a solid desktop */
    uint8_t accent, accent_text;
} InkScheme;

#define INK_SCHEMES 8
extern const InkScheme ink_schemes[INK_SCHEMES];

/* A scheme's index from its key (any case), or -1. */
int ink_scheme_by_key(const char *key);
/* Set theme_ink's colours to scheme i's. */
void ink_scheme_apply(int i);

#endif
