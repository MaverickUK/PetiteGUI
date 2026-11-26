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
