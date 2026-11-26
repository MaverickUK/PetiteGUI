#include <ctype.h>
#include <string.h>
#include "util.h"

void fmt_thousands(long v, char *buf)
{
    char tmp[16];
    int n = 0, i, out = 0;
    unsigned long u;

    if (v < 0) {
        buf[out++] = '-';
        u = (unsigned long)(-v);
    } else {
        u = (unsigned long)v;
    }
    do {
        if (n == 3 || n == 7 || n == 11)
            tmp[n++] = ',';
        tmp[n++] = (char)('0' + (int)(u % 10));
        u /= 10;
    } while (u != 0);
    for (i = n - 1; i >= 0; i--)
        buf[out++] = tmp[i];
    buf[out] = '\0';
}

int path_parent(char *path, char *child, int child_size)
{
    int end = (int)strlen(path), start;

    if (end > 0 && path[end - 1] == '\\')
        end--;
    start = end;
    while (start > 0 && path[start - 1] != '\\')
        start--;
    if (start == 0 || start == end || path[start - 1] != '\\' || (start == 3 && path[1] == ':' && end == 2))
        return 0;
    if (start <= 2) /* "C:" itself */
        return 0;
    if (child != NULL && child_size > 0) {
        int n = end - start < child_size - 1 ? end - start : child_size - 1;

        memcpy(child, path + start, (size_t)n);
        child[n] = '\0';
    }
    path[start] = '\0';
    return 1;
}

const char *path_ext(const char *name)
{
    const char *dot = strrchr(name, '.');

    return dot != NULL && dot != name ? dot + 1 : name + strlen(name);
}

int path_is_text(const char *name)
{
    static const char *const exts[] = { "TXT", "INI", "CFG" };
    const char *e = path_ext(name);
    int i, k;

    for (i = 0; i < 3; i++) {
        for (k = 0; k < 3 && e[k] && toupper((unsigned char)e[k]) == exts[i][k]; k++)
            ;
        if (k == 3 && e[3] == '\0')
            return 1;
    }
    return 0;
}
