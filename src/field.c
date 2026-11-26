#include <string.h>
#include "field.h"

static void keep_caret_showing(Field *f)
{
    if (f->caret < f->scroll)
        f->scroll = f->caret;
    if (f->caret > f->scroll + f->width - 1)
        f->scroll = f->caret - f->width + 1;
    if (f->scroll < 0)
        f->scroll = 0;
}

void field_set(Field *f, const char *text, int width)
{
    strncpy(f->text, text, FIELD_MAX);
    f->text[FIELD_MAX] = '\0';
    f->len = (int)strlen(f->text);
    f->caret = f->len;
    f->width = width > 1 ? width : 1;
    f->scroll = 0;
    keep_caret_showing(f);
}

int field_key(Field *f, int ch, int ext)
{
    if (ch == 0) {
        switch (ext) {
        case 75: if (f->caret == 0) return 0; f->caret--; break;          /* left */
        case 77: if (f->caret == f->len) return 0; f->caret++; break;     /* right */
        case 71: f->caret = 0; break;                                     /* Home */
        case 79: f->caret = f->len; break;                                /* End */
        case 83:                                                          /* Delete */
            if (f->caret == f->len)
                return 0;
            memmove(f->text + f->caret, f->text + f->caret + 1, (size_t)(f->len - f->caret));
            f->len--;
            break;
        default:
            return 0;
        }
    } else if (ch == 8) {                                                 /* Backspace */
        if (f->caret == 0)
            return 0;
        memmove(f->text + f->caret - 1, f->text + f->caret, (size_t)(f->len - f->caret + 1));
        f->caret--;
        f->len--;
    } else if (ch >= 32 && ch < 127) {
        if (f->len >= FIELD_MAX)
            return 0;
        memmove(f->text + f->caret + 1, f->text + f->caret, (size_t)(f->len - f->caret + 1));
        f->text[f->caret++] = (char)ch;
        f->len++;
    } else {
        return 0;
    }
    keep_caret_showing(f);
    return 1;
}
