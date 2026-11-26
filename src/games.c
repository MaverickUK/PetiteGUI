#include <dos.h>
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "games.h"
#include "gamecols.h"
#include "gminfo.h"
#include "saver.h"

#define CONFIG_FILE "PETITE.CFG"
#define INDEX_FILE "PETITE.IDX"
#define INDEX_VERSION "2" /* 2: every field and the play stats */

Settings S;

/* The list is two DOS blocks: a record per game, and all the games' text one string after
 * another, which the records point into (fixed-size fields for 500 games wouldn't fit in a
 * 64 KB segment). Offset 0 is an empty string. */
enum { F_NAME, F_YEAR, F_GENRE, F_DEVELOPER, F_TYPE, F_VIDEO, F_SOUND, F_DIR, F_LAST, F_STRINGS };

typedef struct Rec {
    unsigned s[F_STRINGS];
    unsigned memory_kb, plays;
    unsigned long minutes;
} Rec;

static unsigned rec_seg, pool_seg; /* 0: not allocated */
static Rec __far *recs;
static char __far *pool;
static unsigned pool_used;
static unsigned order[MAX_GAMES];  /* sorted: indexes into recs */
static int count, loaded, capped, sort_col = COL_NAME;

/* ---- Settings ---- */

void settings_load(const char *argv0)
{
    char path[128], line[200], key[16], value[128];
    const char *slash, *temp;
    FILE *f;

    strcpy(S.scan_path, "C:\\");
    S.use_ems = 1;
    S.play_stats = 1;
    S.saver = SAVER_RANDOM;
    S.saver_delay = 5;
    S.game_cols = 1u << COL_YEAR;
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
        else if (strcmp(key, "SAVER") == 0 && saver_by_key(value) != SAVER_UNKNOWN)
            S.saver = saver_by_key(value);
        else if (strcmp(key, "SAVERDELAY") == 0)
            S.saver_delay = atoi(value);
        else if (strcmp(key, "GAMECOLUMNS") == 0)
            S.game_cols = gc_parse(value);
    }
    fclose(f);
}

int settings_set(const char *key, const char *value)
{
    static char text[2048];
    char path[128], line[200], k[16], v[128];
    size_t used = 0, n;
    int found = 0;
    FILE *f;

    sprintf(path, "%s%s", S.exe_dir, CONFIG_FILE);
    f = fopen(path, "r");
    if (f != NULL) {
        while (fgets(line, sizeof line, f) != NULL) {
            if (gm_split(line, k, sizeof k, v, sizeof v) && stricmp(k, key) == 0) {
                if (found)
                    continue; /* only one of each */
                sprintf(line, "%s:%s\n", key, value);
                found = 1;
            }
            n = strlen(line);
            if (used + n + 1 >= sizeof text)
                break;
            memcpy(text + used, line, n);
            used += n;
        }
        fclose(f);
    }
    if (!found) {
        if (used > 0 && text[used - 1] != '\n')
            text[used++] = '\n';
        used += (size_t)sprintf(text + used, "%s:%s\n", key, value);
    }
    f = fopen(path, "w");
    if (f == NULL)
        return 0;
    n = fwrite(text, 1, used, f);
    return fclose(f) == 0 && n == used;
}

/* ---- The list ---- */

static int alloc_list(void)
{
    if (rec_seg == 0) {
        if (_dos_allocmem((unsigned)(((long)MAX_GAMES * sizeof(Rec) + 15) / 16), &rec_seg) != 0) {
            rec_seg = 0;
            return 0;
        }
        if (_dos_allocmem((GAMES_POOL + 15) / 16, &pool_seg) != 0) {
            _dos_freemem(rec_seg);
            rec_seg = pool_seg = 0;
            return 0;
        }
        recs = (Rec __far *)MK_FP(rec_seg, 0);
        pool = (char __far *)MK_FP(pool_seg, 0);
    }
    pool[0] = '\0';
    pool_used = 1;
    count = 0;
    capped = 0;
    return 1;
}

void games_free(void)
{
    if (rec_seg != 0)
        _dos_freemem(rec_seg);
    if (pool_seg != 0)
        _dos_freemem(pool_seg);
    rec_seg = pool_seg = 0;
    recs = NULL;
    pool = NULL;
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

/* Add a string to the pool; 0 (the empty string) for "", or when the pool is full. */
static unsigned pool_add(const char *str)
{
    unsigned n = (unsigned)strlen(str), at;

    if (n == 0)
        return 0;
    if (pool_used + n + 1 > GAMES_POOL) {
        capped = 1;
        return 0;
    }
    at = pool_used;
    _fmemcpy(pool + at, str, n + 1);
    pool_used += n + 1;
    return at;
}

static void get_str(char *dst, int size, unsigned at)
{
    const char __far *src = pool + at;
    int n = 0;

    while (n < size - 1 && src[n])
        dst[n] = src[n], n++;
    dst[n] = '\0';
}

static Rec __far *new_game(const char *dir)
{
    Rec __far *r;

    if (count >= MAX_GAMES) {
        capped = 1;
        return NULL;
    }
    r = &recs[count];
    _fmemset(r, 0, sizeof(Rec));
    r->s[F_DIR] = pool_add(dir);
    order[count] = (unsigned)count;
    count++;
    return r;
}

/* What a column compares, for sorting */
static int col_field(int col)
{
    switch (col) {
    case COL_YEAR: return F_YEAR;
    case COL_GENRE: return F_GENRE;
    case COL_DEVELOPER: return F_DEVELOPER;
    case COL_TYPE: return F_TYPE;
    case COL_VIDEO: return F_VIDEO;
    case COL_SOUND: return F_SOUND;
    case COL_LAST: return F_LAST;
    }
    return F_NAME;
}

/* Names A to Z; text columns A to Z (the year oldest first), with blanks last; numbers and
 * the last played, most first. Ties go by name. */
static int compare_games(const void *a, const void *b)
{
    const Rec __far *x = &recs[*(const unsigned *)a], __far *y = &recs[*(const unsigned *)b];
    int f = col_field(sort_col), c = 0;
    unsigned long vx = 0, vy = 0;

    if (sort_col == COL_PLAYS || sort_col == COL_TIME || sort_col == COL_MEMORY) {
        vx = sort_col == COL_PLAYS ? x->plays : sort_col == COL_TIME ? x->minutes : x->memory_kb;
        vy = sort_col == COL_PLAYS ? y->plays : sort_col == COL_TIME ? y->minutes : y->memory_kb;
        c = vx > vy ? -1 : vx < vy ? 1 : 0;
    } else if (f != F_NAME) {
        if (x->s[f] == 0 || y->s[f] == 0)
            c = (x->s[f] == 0) - (y->s[f] == 0);
        else
            c = _fstricmp(pool + x->s[f], pool + y->s[f]) * (sort_col == COL_LAST ? -1 : 1);
    }
    return c != 0 ? c : _fstricmp(pool + x->s[F_NAME], pool + y->s[F_NAME]);
}

void games_sort(int col)
{
    sort_col = col;
    qsort(order, (size_t)count, sizeof order[0], compare_games);
}

int games_sort_column(void)
{
    return sort_col;
}

void games_get(int k, Game *out)
{
    const Rec __far *r;

    memset(out, 0, sizeof *out);
    if (k < 0 || k >= count || recs == NULL)
        return;
    r = &recs[order[k]];
    get_str(out->name, sizeof out->name, r->s[F_NAME]);
    get_str(out->year, sizeof out->year, r->s[F_YEAR]);
    get_str(out->genre, sizeof out->genre, r->s[F_GENRE]);
    get_str(out->developer, sizeof out->developer, r->s[F_DEVELOPER]);
    get_str(out->type, sizeof out->type, r->s[F_TYPE]);
    get_str(out->video, sizeof out->video, r->s[F_VIDEO]);
    get_str(out->sound, sizeof out->sound, r->s[F_SOUND]);
    get_str(out->dir, sizeof out->dir, r->s[F_DIR]);
    get_str(out->last, sizeof out->last, r->s[F_LAST]);
    out->memory_kb = r->memory_kb;
    out->plays = r->plays;
    out->minutes = r->minutes;
}

/* A game's play stats, from its !GM_LOG.TXT */
static void read_plays(Rec __far *r, const char *dir)
{
    char path[GM_PATH_MAX + 16], line[64];
    Game g;
    FILE *f;

    memset(&g, 0, sizeof g);
    sprintf(path, "%s%s", dir, GM_LOG_FILE);
    f = fopen(path, "r");
    if (f != NULL) {
        while (fgets(line, sizeof line, f) != NULL)
            gc_log_line(&g, line);
        fclose(f);
    }
    r->plays = g.plays;
    r->minutes = g.minutes;
    r->s[F_LAST] = pool_add(g.last);
}

/* ---- Index ---- */

static void index_path(char *path)
{
    sprintf(path, "%s%s", S.exe_dir, INDEX_FILE);
}

static void write_index(void)
{
    static const char *const keys[F_STRINGS] = {
        "name", "year", "genre", "developer", "type", "video", "sound", "path", "last"
    };
    char path[128], text[80];
    const Rec __far *r;
    FILE *f;
    int k, i;

    index_path(path);
    f = fopen(path, "w");
    if (f == NULL)
        return;
    fprintf(f, "; PetiteGUI games index: Games > Rescan rebuilds it\n");
    fprintf(f, "index:%s\nscanpath:%s\n", INDEX_VERSION, S.scan_path);
    for (k = 0; k < count; k++) {
        r = &recs[k];
        get_str(text, sizeof text, r->s[F_DIR]);
        fprintf(f, "\npath:%s\n", text);
        for (i = 0; i < F_STRINGS; i++) {
            if (i == F_DIR || r->s[i] == 0)
                continue;
            get_str(text, sizeof text, r->s[i]);
            fprintf(f, "%s:%s\n", keys[i], text);
        }
        if (r->memory_kb)
            fprintf(f, "memory:%u\n", r->memory_kb);
        if (r->plays)
            fprintf(f, "plays:%u\n", r->plays);
        if (r->minutes)
            fprintf(f, "minutes:%lu\n", r->minutes);
    }
    fclose(f);
}

int games_load_index(void)
{
    char path[128], line[200], key[16], value[128];
    Rec __far *r = NULL;
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
            r = new_game(value);
        else if (r == NULL)
            continue;
        else if (strcmp(key, "NAME") == 0)
            r->s[F_NAME] = pool_add(value);
        else if (strcmp(key, "YEAR") == 0)
            r->s[F_YEAR] = pool_add(value);
        else if (strcmp(key, "GENRE") == 0)
            r->s[F_GENRE] = pool_add(value);
        else if (strcmp(key, "DEVELOPER") == 0)
            r->s[F_DEVELOPER] = pool_add(value);
        else if (strcmp(key, "TYPE") == 0)
            r->s[F_TYPE] = pool_add(value);
        else if (strcmp(key, "VIDEO") == 0)
            r->s[F_VIDEO] = pool_add(value);
        else if (strcmp(key, "SOUND") == 0)
            r->s[F_SOUND] = pool_add(value);
        else if (strcmp(key, "LAST") == 0)
            r->s[F_LAST] = pool_add(value);
        else if (strcmp(key, "MEMORY") == 0)
            r->memory_kb = (unsigned)atoi(value);
        else if (strcmp(key, "PLAYS") == 0)
            r->plays = (unsigned)atoi(value);
        else if (strcmp(key, "MINUTES") == 0)
            r->minutes = strtoul(value, NULL, 10);
    }
    fclose(f);
    if (!valid) {
        games_free();
        return 0;
    }
    games_sort(sort_col);
    loaded = 1;
    return 1;
}

void games_refresh(const char *dir)
{
    char d[GM_PATH_MAX];
    int k;

    for (k = 0; k < count; k++) {
        get_str(d, sizeof d, recs[k].s[F_DIR]);
        if (stricmp(d, dir) == 0) {
            read_plays(&recs[k], dir);
            games_sort(sort_col);
            write_index();
            return;
        }
    }
}

/* ---- Scanning ---- */

static char scan_dir_buf[GM_PATH_MAX];
static void (*on_progress)(const char *dir, int found);

static void add_from_info(void)
{
    char path[GM_PATH_MAX + 16], line[200], key[16], value[128];
    GameInfo info;
    Rec __far *r;
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
    r = new_game(scan_dir_buf);
    if (r == NULL)
        return;
    r->s[F_NAME] = pool_add(info.name);
    r->s[F_YEAR] = pool_add(info.year);
    r->s[F_GENRE] = pool_add(info.genre);
    r->s[F_DEVELOPER] = pool_add(info.developer);
    r->s[F_TYPE] = pool_add(info.type);
    r->s[F_VIDEO] = pool_add(info.video);
    r->s[F_SOUND] = pool_add(info.sound);
    r->memory_kb = info.memory_kb;
    read_plays(r, scan_dir_buf);
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
    games_sort(sort_col);
    loaded = 1;
    write_index();
    return count;
}
