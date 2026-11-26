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
extern const Theme theme_ink;

#endif
