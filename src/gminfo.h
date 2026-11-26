/* GameMinder's metadata and launch rules, as pure C (no DOS), so the host tests build it.
 *
 * Each game's directory holds a !GM_INFO.TXT of key:value lines (GameMinder's QUICKSTART.md):
 * exec (required), setup, name, developer, genre, year, video, sound, type, memory, before,
 * after, notes. Lines starting with ; are comments. Plays are logged to !GM_LOG.TXT beside
 * it, in GameMinder's format, so both programs count them. */
#ifndef GMINFO_H
#define GMINFO_H

#define GM_INFO_FILE "!GM_INFO.TXT"
#define GM_LOG_FILE "!GM_LOG.TXT"
#define GM_DEFAULT_COMSPEC "C:\\COMMAND.COM"
#define GM_PATH_MAX 128
#define GM_SCRIPT_LINES 4

typedef struct GameInfo {
    char name[33];
    char exec[80];
    char setup[80];
    char before[80];
    char after[80];
    char year[5];
    char genre[17];
    char developer[33];
    char type[17];
    char video[17];
    char sound[17];
    unsigned memory_kb; /* 0 if not given */
} GameInfo;

/* Split "key:value" into an upper-case key and a trimmed value. Zero for blank lines,
 * comments and lines with no colon. */
int gm_split(const char *line, char *key, int key_size, char *value, int value_size);

/* Apply one key (upper case) to g; unknown keys are ignored. */
void gm_apply(GameInfo *g, const char *key, const char *value);

/* The last part of a directory, for a game with no name: "C:\GAMES\KEEN1\" -> "KEEN1". */
void gm_dir_name(const char *dir, char *out, int size);

/* Add a trailing backslash if there isn't one. */
void gm_normalize_dir(char *dir, int size);

/* Split an exec: value into the program's full path and its arguments. A program without a
 * drive or leading backslash is in the game's directory (dir ends with a backslash). */
void gm_resolve(const char *dir, const char *exec, char *prog, int prog_size, char *args, int args_size);

int gm_is_batch(const char *prog);

/* The program to EXEC and its command line: batch files go through the command
 * interpreter (DOS can't EXEC them), with "/C". comspec may be empty. */
void gm_command(const char *dir, const char *exec, const char *comspec,
                char *prog, int prog_size, char *args, int args_size);

/* A batch file running before:, the game, then after:, in one command interpreter.
 * Returns the number of lines. */
int gm_script(const char *dir, const char *exec, const char *before, const char *after,
              char lines[GM_SCRIPT_LINES][GM_PATH_MAX + 8]);

/* ScanPath: directories separated by ';'. */
int gm_path_count(const char *paths);
void gm_path_item(const char *paths, int index, char *out, int size);

/* Days since a fixed date, for play time across midnight. */
long gm_day_number(int year, int month, int day);
long gm_elapsed_minutes(long start_day, int start_minute, long end_day, int end_minute);

#endif
