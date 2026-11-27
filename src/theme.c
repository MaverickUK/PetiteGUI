#include "theme.h"
#include "util.h"

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
Theme theme_ink = {
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

/* Classic is Ink as it was first drawn; the desktop pairs are close in brightness, or solid,
 * because two very different colours checkered look stripy on tall pixels. */
const InkScheme ink_schemes[INK_SCHEMES] = {
    { "Classic", "classic", 3, 1, 1, 15 },  /* cyan and blue, blue title bars */
    { "Slate", "slate", 7, 8, 8, 15 },      /* light and dark grey */
    { "Ocean", "ocean", 9, 1, 3, 0 },       /* light blue and blue, cyan title bars */
    { "Forest", "forest", 2, 2, 2, 15 },    /* solid green */
    { "Plum", "plum", 5, 1, 5, 15 },        /* magenta and blue */
    { "Midnight", "midnight", 0, 1, 1, 15 },/* black and blue */
    { "Rust", "rust", 6, 4, 4, 15 },        /* brown and red */
    { "Teal", "teal", 3, 3, 1, 15 },        /* solid cyan */
};

int ink_scheme_by_key(const char *key)
{
    int i;

    for (i = 0; i < INK_SCHEMES; i++)
        if (stricmp_ascii(key, ink_schemes[i].key) == 0)
            return i;
    return -1;
}

void ink_scheme_apply(int i)
{
    const InkScheme *s = &ink_schemes[i];

    theme_ink.desk_on = s->desk_on;
    theme_ink.desk_off = s->desk_off;
    theme_ink.title_on = s->accent;
    theme_ink.title_on_text = s->accent_text;
    theme_ink.sel = s->accent;
    theme_ink.sel_text = s->accent_text;
}
