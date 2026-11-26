#include <ctype.h>
#include <stdio.h>
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

void fmt_date(unsigned date, char *buf)
{
    if (date == 0) {
        buf[0] = '\0';
        return;
    }
    sprintf(buf, "%04u/%02u/%02u", 1980u + ((date >> 9) & 0x7F), (date >> 5) & 0x0F, date & 0x1F);
}

void fmt_time(unsigned time, char *buf)
{
    sprintf(buf, "%02u:%02u", (time >> 11) & 0x1F, (time >> 5) & 0x3F);
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

int stricmp_ascii(const char *a, const char *b)
{
    int ca, cb;

    do {
        ca = toupper((unsigned char)*a++);
        cb = toupper((unsigned char)*b++);
    } while (ca == cb && ca != 0);
    return ca - cb;
}

static int ext_in(const char *name, const char *const *exts, int n)
{
    const char *e = path_ext(name);
    int i, k;

    for (i = 0; i < n; i++) {
        for (k = 0; k < 3 && e[k] && toupper((unsigned char)e[k]) == exts[i][k]; k++)
            ;
        if (k == 3 && e[3] == '\0')
            return 1;
    }
    return 0;
}

int path_is_text(const char *name)
{
    static const char *const exts[] = { "TXT", "INI", "CFG" };
    const char *slash = strrchr(name, '\\');

    if (slash != NULL)
        name = slash + 1;
    /* DOS's own two: a batch file and a .SYS, but text to edit, not to run */
    return ext_in(name, exts, 3) || stricmp_ascii(name, "AUTOEXEC.BAT") == 0 ||
           stricmp_ascii(name, "CONFIG.SYS") == 0;
}

int path_is_program(const char *name)
{
    static const char *const exts[] = { "EXE", "COM", "BAT" };

    return ext_in(name, exts, 3);
}

int path_join(char *out, int size, const char *dir, const char *name)
{
    size_t d = strlen(dir), n = strlen(name);

    if ((int)(d + n + 1) > size)
        return 0;
    memcpy(out, dir, d);
    memcpy(out + d, name, n + 1);
    return 1;
}

static int path_len_trimmed(const char *p)
{
    int n = (int)strlen(p);

    if (n > 3 && p[n - 1] == '\\') /* "C:\GAMES\" but not "C:\" */
        n--;
    return n;
}

int path_inside(const char *path, const char *dir)
{
    int pn = path_len_trimmed(path), dn = path_len_trimmed(dir), i;

    if (pn < dn)
        return 0;
    for (i = 0; i < dn; i++)
        if (toupper((unsigned char)path[i]) != toupper((unsigned char)dir[i]))
            return 0;
    return pn == dn || path[dn] == '\\' || dir[dn - 1] == '\\';
}

int path_same_drive(const char *a, const char *b)
{
    return a[0] && b[0] && a[1] == ':' && b[1] == ':' && toupper((unsigned char)a[0]) == toupper((unsigned char)b[0]);
}

int path_valid_name(const char *name)
{
    static const char bad[] = " .\"/\\[]:|<>+=;,*?";
    int base = 0, ext = -1;
    const char *p;

    for (p = name; *p; p++) {
        if (*p == '.' && ext < 0) {
            ext = 0;
            continue;
        }
        if ((unsigned char)*p < 32 || strchr(bad, *p) != NULL)
            return 0;
        if (ext < 0)
            base++;
        else
            ext++;
    }
    return base >= 1 && base <= 8 && (ext < 0 || (ext >= 1 && ext <= 3));
}
