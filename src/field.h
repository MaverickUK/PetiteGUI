/* A one-line text field's editing: the text, the caret, and which part of it shows. Pure C
 * (the host tests build it); draw.c draws it, and main.c's text-entry box feeds it keys. */
#ifndef FIELD_H
#define FIELD_H

#define FIELD_MAX 79

typedef struct Field {
    char text[FIELD_MAX + 1];
    int len, caret;
    int scroll; /* the first character showing */
    int width;  /* characters that show */
} Field;

void field_set(Field *f, const char *text, int width);
/* A key: ch, or 0 and an extended scan code in ext. Returns 1 if the text or caret changed. */
int field_key(Field *f, int ch, int ext);

#endif
