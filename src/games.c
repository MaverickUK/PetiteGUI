#include <dos.h>
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "games.h"
#include "gminfo.h"

#define CONFIG_FILE "PETITE.CFG"
#define INDEX_FILE "PETITE.IDX"
#define INDEX_VERSION "1"

Settings S;

static unsigned list_seg;          /* 0: not allocated */
static Game __far *list;
static unsigned order[MAX_GAMES];  /* sorted by name: indexes into list */
static int count, loaded, capped;

/* ---- Settings ---- */

void settings_load(const char *argv0)
{
    char path[128], line[200], key[16], value[128];
    const char *slash, *temp;
    FILE *f;

    strcpy(S.scan_path, "C:\\");
    S.use_ems = 1;
    S.play_stats = 1;
    slash = strrchr(argv0, '\\');
    if (slash != NULL && slash - argv0 < (int)sizeof S.exe_dir - 1) {
        memcpy(S.exe_dir, argv0, (size_t)(slash - argv0 + 1));
        S.exe_dir[slash - argv0 + 1] = '\0';
    } else {
        S.exe_dir[0] = '\0';
    }
    temp = getenv("TEMP");
    if (temp != NULL && temp[0] && strlen(temp) < sizeof S.temp_dir - 2) {
        strcpy(S.temp_dir, temp);
        gm_normalize_dir(S.temp_dir, sizeof S.temp_dir);
    } else {
        strcpy(S.temp_dir, S.exe_dir);
    }

    sprintf(path, "%s%s", S.exe_dir, CONFIG_FILE);
    f = fopen(path, "r");
    if (f == NULL)
        return;
    while (fgets(line, sizeof line, f) != NULL) {
        if (!gm_split(line, key, sizeof key, value, sizeof value))
            continue;
        if (strcmp(key, "SCANPATH") == 0 && value[0])
            strcpy(S.scan_path, value);
        else if (strcmp(key, "USEEMS") == 0)
            S.use_ems = stricmp(value, "false") != 0;
        else if (strcmp(key, "PLAYSTATS") == 0)
            S.play_stats = stricmp(value, "false") != 0;
    }
    fclose(f);
}

/* ---- The list ---- */

static int alloc_list(void)
{
    unsigned paras = (unsigned)(((long)MAX_GAMES * sizeof(Game) + 15) / 16);

    if (list_seg == 0) {
        if (_dos_allocmem(paras, &list_seg) != 0) {
            list_seg = 0;
            return 0;
        }
        list = (Game __far *)MK_FP(list_seg, 0);
    }
    count = 0;
    capped = 0;
    return 1;
}

void games_free(void)
{
    if (list_seg != 0)
        _dos_freemem(list_seg);
    list_seg = 0;
    list = NULL;
    count = 0;
    loaded = 0;
}

int games_loaded(void)
{
    return loaded;
}

int games_count(void)
{
    return count;
}

int games_capped(void)
{
    return capped;
}

static void set_field(char __far *dst, int size, const char *src)
{
    int n = (int)strlen(src);

    if (n > size - 1)
        n = size - 1;
    _fmemcpy(dst, src, n);
    dst[n] = '\0';
}

static Game __far *new_game(const char *dir)
{
    Game __far *g;

    if (count >= MAX_GAMES) {
        capped = 1;
        return NULL;
    }
    g = &list[count];
    _fmemset(g, 0, sizeof(Game));
    set_field(g->dir, sizeof g->dir, dir);
    order[count] = (unsigned)count;
    count++;
    return g;
}

static int compare_names(const void *a, const void *b)
{
    return _fstricmp(list[*(const unsigned *)a].name, list[*(const unsigned *)b].name);
}

static void sort_list(void)
{
    qsort(order, (size_t)count, sizeof order[0], compare_names);
}

void games_get(int k, Game *out)
{
    if (k < 0 || k >= count || list == NULL) {
        memset(out, 0, sizeof *out);
        return;
    }
    _fmemcpy(out, &list[order[k]], sizeof *out);
}

/* ---- Index ---- */

static void index_path(char *path)
{
    sprintf(path, "%s%s", S.exe_dir, INDEX_FILE);
}

static void write_index(void)
{
    char path[128];
    Game g;
    FILE *f;
    int k;

    index_path(path);
    f = fopen(path, "w");
    if (f == NULL)
        return;
    fprintf(f, "; PetiteGUI games index: Games > Rescan rebuilds it\n");
    fprintf(f, "index:%s\nscanpath:%s\n", INDEX_VERSION, S.scan_path);
    for (k = 0; k < count; k++) {
        games_get(k, &g);
        fprintf(f, "\npath:%s\nname:%s\n", g.dir, g.name);
        if (g.year[0])
            fprintf(f, "year:%s\n", g.year);
        if (g.genre[0])
            fprintf(f, "genre:%s\n", g.genre);
    }
    fclose(f);
}

int games_load_index(void)
{
    char path[128], line[200], key[16], value[128];
    Game __far *g = NULL;
    int valid = 0, header = 1;
    FILE *f;

    index_path(path);
    f = fopen(path, "r");
    if (f == NULL)
        return 0;
    if (!alloc_list()) {
        fclose(f);
        return 0;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        if (!gm_split(line, key, sizeof key, value, sizeof value))
            continue;
        if (header && strcmp(key, "PATH") != 0) {
            if (strcmp(key, "INDEX") == 0)
                valid = strcmp(value, INDEX_VERSION) == 0;
            else if (strcmp(key, "SCANPATH") == 0)
                valid = valid && stricmp(value, S.scan_path) == 0;
            continue;
        }
        if (!valid)
            break;
        header = 0;
        if (strcmp(key, "PATH") == 0)
            g = new_game(value);
        else if (g == NULL)
            continue;
        else if (strcmp(key, "NAME") == 0)
            set_field(g->name, sizeof g->name, value);
        else if (strcmp(key, "YEAR") == 0)
            set_field(g->year, sizeof g->year, value);
        else if (strcmp(key, "GENRE") == 0)
            set_field(g->genre, sizeof g->genre, value);
    }
    fclose(f);
    if (!valid) {
        games_free();
        return 0;
    }
    sort_list();
    loaded = 1;
    return 1;
}

/* ---- Scanning ---- */

static char scan_dir_buf[GM_PATH_MAX];
static void (*on_progress)(const char *dir, int found);

static void add_from_info(void)
{
    char path[GM_PATH_MAX + 16], line[200], key[16], value[128];
    GameInfo info;
    Game __far *g;
    FILE *f;

    sprintf(path, "%s%s", scan_dir_buf, GM_INFO_FILE);
    f = fopen(path, "r");
    if (f == NULL)
        return;
    memset(&info, 0, sizeof info);
    while (fgets(line, sizeof line, f) != NULL)
        if (gm_split(line, key, sizeof key, value, sizeof value))
            gm_apply(&info, key, value);
    fclose(f);
    if (info.name[0] == '\0')
        gm_dir_name(scan_dir_buf, info.name, sizeof info.name);
    g = new_game(scan_dir_buf);
    if (g == NULL)
        return;
    set_field(g->name, sizeof g->name, info.name);
    set_field(g->year, sizeof g->year, info.year);
    set_field(g->genre, sizeof g->genre, info.genre);
}

/* Search scan_dir_buf (ending in a backslash, len long) and everything under it. */
static void scan_dir(size_t len)
{
    struct find_t f;
    size_t n;

    if (on_progress != NULL)
        on_progress(scan_dir_buf, count);
    strcpy(scan_dir_buf + len, "*.*");
    if (_dos_findfirst(scan_dir_buf, _A_SUBDIR | _A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_ARCH, &f) == 0) {
        do {
            if (capped)
                break;
            if (f.name[0] == '.')
                continue;
            if (f.attrib & _A_SUBDIR) {
                n = strlen(f.name);
                if (len + n + 6 < sizeof scan_dir_buf) {
                    strcpy(scan_dir_buf + len, f.name);
                    strcat(scan_dir_buf, "\\");
                    scan_dir(len + n + 1);
                }
            } else if (stricmp(f.name, GM_INFO_FILE) == 0) {
                scan_dir_buf[len] = '\0';
                add_from_info();
            }
        } while (_dos_findnext(&f) == 0);
    }
    scan_dir_buf[len] = '\0';
}

int games_scan(void (*progress)(const char *dir, int found))
{
    int i, n = gm_path_count(S.scan_path);

    if (!alloc_list())
        return -1;
    on_progress = progress;
    for (i = 0; i < n && !capped; i++) {
        gm_path_item(S.scan_path, i, scan_dir_buf, sizeof scan_dir_buf - 16);
        gm_normalize_dir(scan_dir_buf, sizeof scan_dir_buf);
        scan_dir(strlen(scan_dir_buf));
    }
    on_progress = NULL;
    sort_list();
    loaded = 1;
    write_index();
    return count;
}
