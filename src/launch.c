#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "games.h"
#include "launch.h"
#include "swap.h"

#define SCRIPT_FILE "PETITE.BAT"

int launch_read_info(const char *dir, GameInfo *g)
{
    char path[GM_PATH_MAX + 16], line[200], key[16], value[128];
    FILE *f;

    memset(g, 0, sizeof *g);
    sprintf(path, "%s%s", dir, GM_INFO_FILE);
    f = fopen(path, "r");
    if (f == NULL)
        return 0;
    while (fgets(line, sizeof line, f) != NULL)
        if (gm_split(line, key, sizeof key, value, sizeof value))
            gm_apply(g, key, value);
    fclose(f);
    if (g->name[0] == '\0')
        gm_dir_name(dir, g->name, sizeof g->name);
    return 1;
}

void launch_program(const char *dir, const GameInfo *g, int setup, char *out, int size)
{
    char args[GM_PATH_MAX];

    gm_resolve(dir, setup ? g->setup : g->exec, out, size, args, sizeof args);
}

int launch_exists(const char *path)
{
    struct find_t f;

    return _dos_findfirst(path, _A_NORMAL | _A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_ARCH, &f) == 0;
}

const char *launch_comspec(void)
{
    const char *c = getenv("COMSPEC");

    return c != NULL && c[0] ? c : GM_DEFAULT_COMSPEC;
}

/* GameMinder's play log: ">2026/11/26 14:05" when a game starts, "<35" (minutes) after. */
static void log_play(const char *dir, const char *line)
{
    char path[GM_PATH_MAX + 16];
    FILE *f;

    sprintf(path, "%s%s", dir, GM_LOG_FILE);
    f = fopen(path, "a");
    if (f == NULL)
        return; /* read-only media, such as a CD-ROM */
    fprintf(f, "%s\n", line);
    fclose(f);
}

static void now(long *day, int *minute, char *stamp)
{
    struct dosdate_t d;
    struct dostime_t t;

    _dos_getdate(&d);
    _dos_gettime(&t);
    *day = gm_day_number(d.year, d.month, d.day);
    *minute = t.hour * 60 + t.minute;
    if (stamp != NULL)
        sprintf(stamp, ">%04d/%02d/%02d %02d:%02d", d.year, d.month, d.day, t.hour, t.minute);
}

unsigned launch_run(const char *dir, const GameInfo *g, int setup)
{
    char prog[GM_PATH_MAX], args[GM_PATH_MAX], script[GM_PATH_MAX], stamp[32];
    char lines[GM_SCRIPT_LINES][GM_PATH_MAX + 8];
    int use_script = !setup && (g->before[0] || g->after[0]);
    int i, n, start_minute, end_minute;
    long start_day, end_day;
    unsigned status;
    SwapOptions o;
    SwapReport r;
    FILE *f;

    if ((setup ? g->setup : g->exec)[0] == '\0')
        return LAUNCH_NO_EXEC;
    printf("Starting %s%s\n", g->name, setup ? " setup" : "");

    if (use_script) {
        sprintf(script, "%s%s", S.temp_dir, SCRIPT_FILE);
        n = gm_script(dir, g->exec, g->before, g->after, lines);
        f = fopen(script, "w");
        if (f != NULL) {
            for (i = 0; i < n; i++)
                fprintf(f, "%s\n", lines[i]);
            fclose(f);
        }
        strcpy(prog, launch_comspec());
        sprintf(args, "/C %s", script);
    } else {
        gm_command(dir, setup ? g->setup : g->exec, launch_comspec(), prog, sizeof prog, args, sizeof args);
    }

    now(&start_day, &start_minute, stamp);
    if (!setup && S.play_stats)
        log_play(dir, stamp);

    o.use_ems = S.use_ems;
    o.temp_dir = S.temp_dir;
    status = swap_run(prog, args, dir, &o, &r);
    if (status == SWAP_ERR_VECTOR)
        printf("Interrupt %02Xh points into PetiteGUI.\n", r.vector);

    if (use_script)
        remove(script);
    if (status == 0 && !setup && S.play_stats) {
        now(&end_day, &end_minute, NULL);
        sprintf(stamp, "<%ld", gm_elapsed_minutes(start_day, start_minute, end_day, end_minute));
        log_play(dir, stamp);
    }
    return status;
}

unsigned launch_command(const char *dir, const char *prog, const char *args, int shell, const char *banner)
{
    char tail[GM_PATH_MAX + 4];
    SwapOptions o;
    SwapReport r;

    if (banner != NULL && banner[0])
        printf("%s\n", banner);
    o.use_ems = S.use_ems;
    o.temp_dir = S.temp_dir;
    if (!shell)
        return swap_run(prog, args, dir, &o, &r);
    tail[0] = '\0';
    if (args[0])
        sprintf(tail, "/C %.*s", GM_PATH_MAX - 4, args);
    return swap_run(launch_comspec(), tail, dir, &o, &r);
}

const char *launch_error_text(unsigned status)
{
    switch (status) {
    case 2: return "The program wasn't found.";
    case 3: return "Its directory wasn't found.";
    case 5: return "DOS refused access to it.";
    case 8: return "There isn't enough memory.";
    case 10: return "The environment is too big.";
    case 11: return "It isn't a program DOS can run.";
    case SWAP_ERR_OUT: return "Swapping out failed.";
    case SWAP_ERR_NO_SPACE: return "No EMS or disk space to swap to.";
    case SWAP_ERR_VECTOR: return "An interrupt points into PetiteGUI.";
    case SWAP_ERR_DIR: return "Its directory couldn't be opened.";
    case LAUNCH_NO_INFO: return "Its !GM_INFO.TXT couldn't be read.";
    case LAUNCH_NO_EXEC: return "Its !GM_INFO.TXT has no program.";
    }
    return "DOS couldn't run it.";
}
