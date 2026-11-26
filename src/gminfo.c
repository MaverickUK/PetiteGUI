#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "gminfo.h"

/* Copy at most size-1 characters of s[0..len), trimmed of spaces, tabs and line ends. */
static void trim_copy(char *dst, int size, const char *s, int len)
{
    while (len > 0 && (*s == ' ' || *s == '\t')) {
        s++;
        len--;
    }
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n'))
        len--;
    if (len > size - 1)
        len = size - 1;
    memcpy(dst, s, (size_t)len);
    dst[len] = '\0';
}

static void copy_str(char *dst, int size, const char *s)
{
    trim_copy(dst, size, s, (int)strlen(s));
}

int gm_split(const char *line, char *key, int key_size, char *value, int value_size)
{
    const char *colon;
    char *k;

    while (*line == ' ' || *line == '\t')
        line++;
    if (*line == ';' || *line == '\0')
        return 0;
    colon = strchr(line, ':');
    if (colon == NULL)
        return 0;
    trim_copy(key, key_size, line, (int)(colon - line));
    if (key[0] == '\0')
        return 0;
    for (k = key; *k; k++)
        *k = (char)toupper((unsigned char)*k);
    copy_str(value, value_size, colon + 1);
    return 1;
}

void gm_apply(GameInfo *g, const char *key, const char *value)
{
    if (strcmp(key, "NAME") == 0)
        copy_str(g->name, sizeof g->name, value);
    else if (strcmp(key, "EXEC") == 0)
        copy_str(g->exec, sizeof g->exec, value);
    else if (strcmp(key, "SETUP") == 0)
        copy_str(g->setup, sizeof g->setup, value);
    else if (strcmp(key, "BEFORE") == 0)
        copy_str(g->before, sizeof g->before, value);
    else if (strcmp(key, "AFTER") == 0)
        copy_str(g->after, sizeof g->after, value);
    else if (strcmp(key, "YEAR") == 0)
        copy_str(g->year, sizeof g->year, value);
    else if (strcmp(key, "GENRE") == 0)
        copy_str(g->genre, sizeof g->genre, value);
    else if (strcmp(key, "DEVELOPER") == 0)
        copy_str(g->developer, sizeof g->developer, value);
    else if (strcmp(key, "MEMORY") == 0)
        g->memory_kb = (unsigned)atoi(value);
}

void gm_dir_name(const char *dir, char *out, int size)
{
    int end = (int)strlen(dir), start;

    while (end > 0 && dir[end - 1] == '\\')
        end--;
    start = end;
    while (start > 0 && dir[start - 1] != '\\' && dir[start - 1] != ':')
        start--;
    trim_copy(out, size, dir + start, end - start);
}

void gm_normalize_dir(char *dir, int size)
{
    int n = (int)strlen(dir);

    if (n > 0 && dir[n - 1] != '\\' && n < size - 1) {
        dir[n] = '\\';
        dir[n + 1] = '\0';
    }
}

void gm_resolve(const char *dir, const char *exec, char *prog, int prog_size, char *args, int args_size)
{
    char e[GM_PATH_MAX];
    const char *space;
    char name[GM_PATH_MAX];

    copy_str(e, sizeof e, exec);
    space = strchr(e, ' ');
    if (space != NULL) {
        trim_copy(name, sizeof name, e, (int)(space - e));
        copy_str(args, args_size, space + 1);
    } else {
        copy_str(name, sizeof name, e);
        args[0] = '\0';
    }
    if (strchr(name, ':') == NULL && name[0] != '\\') {
        /* relative to the game's directory */
        if ((int)(strlen(dir) + strlen(name)) < prog_size) {
            strcpy(prog, dir);
            strcat(prog, name);
        } else {
            copy_str(prog, prog_size, name);
        }
    } else {
        copy_str(prog, prog_size, name);
    }
}

int gm_is_batch(const char *prog)
{
    const char *dot = strrchr(prog, '.');

    return dot != NULL && toupper((unsigned char)dot[1]) == 'B' && toupper((unsigned char)dot[2]) == 'A' &&
           toupper((unsigned char)dot[3]) == 'T' && dot[4] == '\0';
}

void gm_command(const char *dir, const char *exec, const char *comspec,
                char *prog, int prog_size, char *args, int args_size)
{
    char program[GM_PATH_MAX], extra[GM_PATH_MAX], line[GM_PATH_MAX * 2];

    gm_resolve(dir, exec, program, sizeof program, extra, sizeof extra);
    if (gm_is_batch(program)) {
        copy_str(prog, prog_size, comspec != NULL && comspec[0] ? comspec : GM_DEFAULT_COMSPEC);
        strcpy(line, "/C ");
        strcat(line, program);
        if (extra[0]) {
            strcat(line, " ");
            strcat(line, extra);
        }
        copy_str(args, args_size, line);
    } else {
        copy_str(prog, prog_size, program);
        copy_str(args, args_size, extra);
    }
}

int gm_script(const char *dir, const char *exec, const char *before, const char *after,
              char lines[GM_SCRIPT_LINES][GM_PATH_MAX + 8])
{
    char program[GM_PATH_MAX], extra[GM_PATH_MAX], b[GM_PATH_MAX];
    int n = 0;
    size_t size = GM_PATH_MAX + 8;

    strcpy(lines[n++], "@ECHO OFF");
    copy_str(b, sizeof b, before);
    if (b[0])
        strcpy(lines[n++], b);
    gm_resolve(dir, exec, program, sizeof program, extra, sizeof extra);
    lines[n][0] = '\0';
    if (gm_is_batch(program))
        strcpy(lines[n], "CALL "); /* without CALL, after: wouldn't run */
    strncat(lines[n], program, size - strlen(lines[n]) - 1);
    if (extra[0]) {
        strncat(lines[n], " ", size - strlen(lines[n]) - 1);
        strncat(lines[n], extra, size - strlen(lines[n]) - 1);
    }
    n++;
    copy_str(b, sizeof b, after);
    if (b[0])
        strcpy(lines[n++], b);
    return n;
}

int gm_path_count(const char *paths)
{
    int n = 0;
    char item[GM_PATH_MAX];

    while (1) {
        gm_path_item(paths, n, item, sizeof item);
        if (item[0] == '\0')
            break;
        n++;
    }
    return n;
}

void gm_path_item(const char *paths, int index, char *out, int size)
{
    const char *start = paths, *end;

    out[0] = '\0';
    while (1) {
        end = strchr(start, ';');
        if (end == NULL)
            end = start + strlen(start);
        if (end > start) {
            char tmp[GM_PATH_MAX];

            trim_copy(tmp, sizeof tmp, start, (int)(end - start));
            if (tmp[0]) {
                if (index == 0) {
                    copy_str(out, size, tmp);
                    return;
                }
                index--;
            }
        }
        if (*end == '\0')
            return;
        start = end + 1;
    }
}

long gm_day_number(int year, int month, int day)
{
    /* days from civil (Howard Hinnant), shifted so March starts the year */
    long y = (long)year - (month <= 2);
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153L * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

    return era * 146097L + doe;
}

long gm_elapsed_minutes(long start_day, int start_minute, long end_day, int end_minute)
{
    long m = (end_day - start_day) * 1440L + end_minute - start_minute;

    return m < 0 ? 0 : m;
}
