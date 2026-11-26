#include "theme.h"

/* CGA mode 06h: 0 black, 1 white. */
const Theme theme_paper = {
    "Paper",
    1, 0,       /* desktop: 50% grey */
    1, 0,       /* menu */
    0, 1, 0,    /* frame, window, text */
    0, 1,       /* selection: inverted */
    0,          /* shadow */
    0, 0,       /* active title: stripes, black text */
    1, 0,
    1,
    0, 1, 1,    /* scroll track grey, thumb white */
    0, 1, 0, 1,
    -1, -1,     /* disabled text and separators dithered */
    1, 0,
    0, 1, 1, 1, 1, 0, 0,
    1, 0, 1
};

/* EGA mode 0Eh, the CGA colours: 0 black, 1 blue, 3 cyan, 7 light grey, 8 dark grey,
 * 10 light green, 14 yellow, 15 white. */
const Theme theme_ink = {
    "Ink",
    3, 1,       /* desktop: cyan and blue checker */
    15, 0,
    0, 15, 0,
    1, 15,      /* selection: blue */
    0,
    1, 15,      /* active title: blue */
    15, 8,      /* inactive title: white, dark grey text */
    0,
    7, 15, 15,
    0, 14, 0, 15, /* yellow folders */
    7, 7,
    15, 0,
    0, 15, 1, 7, 15, 8, 10,
    15, 14, 0
};
