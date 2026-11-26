/* The games list: found by scanning ScanPath for GameMinder's !GM_INFO.TXT files the first
 * time it's opened, then kept in PETITE.IDX next to PETITE.EXE so later opens are quick.
 * Rescan rebuilds it.
 *
 * The list lives in a DOS memory block of its own, freed before a game runs (anything
 * allocated outside PetiteGUI's own block would stay in memory under the game) and read
 * back from the index after. */
#ifndef GAMES_H
#define GAMES_H

#define MAX_GAMES 500

typedef struct Game {
    char name[33];
    char year[5];
    char genre[17];
    char dir[67]; /* ends with a backslash */
} Game;

typedef struct Settings {
    char scan_path[128]; /* ScanPath, directories separated by ; */
    int use_ems;
    int play_stats;      /* log plays to !GM_LOG.TXT, as GameMinder does */
    char exe_dir[80];    /* where PETITE.EXE, PETITE.CFG and PETITE.IDX are */
    char temp_dir[80];   /* %TEMP%, else exe_dir: the swap file and launch batch file */
} Settings;

extern Settings S;

/* Read PETITE.CFG. argv0 locates it. */
void settings_load(const char *argv0);

int games_loaded(void);
int games_count(void);
int games_capped(void);  /* the last scan stopped at MAX_GAMES */
/* Read PETITE.IDX; zero if there isn't one, or it's for another ScanPath. */
int games_load_index(void);
/* Scan ScanPath and write PETITE.IDX. progress is called for each directory (may be NULL).
 * Returns the number of games, or -1 if there's no memory for the list. */
int games_scan(void (*progress)(const char *dir, int found));
void games_get(int k, Game *out);
/* Free the list's memory (before running a game). */
void games_free(void);

#endif
