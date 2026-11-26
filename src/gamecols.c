#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gamecols.h"

const GameColumn game_columns[NUM_COLS] = {
    { "year", "Year", "Year", 4, 0 },
    { "genre", "Genre", "Genre", 12, 0 },
    { "developer", "Developer", "Developer", 14, 0 },
    { "type", "Type", "Type", 9, 0 },
    { "video", "Video", "Video", 6, 0 },
    { "sound", "Sound", "Sound", 11, 0 },
    { "memory", "Memory", "Memory needed", 6, 1 },
    { "plays", "Played", "Times played", 6, 1 },
    { "last", "Last played", "Last played", 11, 0 },
    { "time", "Time", "Time played", 7, 1 },
};

unsigned gc_parse(const char *list)
{
    unsigned mask = 0;
    char word[16];
    int c, n;

    while (*list) {
        while (*list == ',' || *list == ' ')
            list++;
        for (n = 0; *list && *list != ',' && *list != ' '; list++)
            if (n < (int)sizeof word - 1)
                word[n++] = *list;
        word[n] = '\0';
        for (c = 0; c < NUM_COLS; c++)
            if (n > 0 && strlen(word) == strlen(game_columns[c].key)) {
                int i;

                for (i = 0; word[i] && (word[i] | 0x20) == game_columns[c].key[i]; i++)
                    ;
                if (word[i] == '\0')
                    mask |= 1u << c;
            }
    }
    return mask;
}

void gc_format(unsigned mask, char *out)
{
    int c;

    out[0] = '\0';
    for (c = 0; c < NUM_COLS; c++)
        if (mask & (1u << c)) {
            if (out[0])
                strcat(out, ",");
            strcat(out, game_columns[c].key);
        }
    if (out[0] == '\0')
        strcpy(out, "none");
}

void gc_minutes(unsigned long minutes, char *out)
{
    if (minutes == 0)
        out[0] = '\0';
    else if (minutes < 60)
        sprintf(out, "%lum", minutes);
    else
        sprintf(out, "%luh %02lum", minutes / 60, minutes % 60);
}

void gc_text(const Game *g, int col, char *out)
{
    out[0] = '\0';
    switch (col) {
    case COL_YEAR: strcpy(out, g->year); break;
    case COL_GENRE: strcpy(out, g->genre); break;
    case COL_DEVELOPER: strcpy(out, g->developer); break;
    case COL_TYPE: strcpy(out, g->type); break;
    case COL_VIDEO: strcpy(out, g->video); break;
    case COL_SOUND: strcpy(out, g->sound); break;
    case COL_MEMORY:
        if (g->memory_kb)
            sprintf(out, "%uK", g->memory_kb);
        break;
    case COL_PLAYS:
        if (g->plays)
            sprintf(out, "%u", g->plays);
        break;
    case COL_LAST: strcpy(out, g->last); break;
    case COL_TIME: gc_minutes(g->minutes, out); break;
    }
}

void gc_log_line(Game *g, const char *line)
{
    size_t n;

    while (*line == ' ' || *line == '\t')
        line++;
    if (line[0] == '>') {
        g->plays++;
        strncpy(g->last, line + 1, sizeof g->last - 1); /* the date part */
        g->last[sizeof g->last - 1] = '\0';
        n = strcspn(g->last, " \r\n");
        g->last[n] = '\0';
    } else if (line[0] == '<') {
        g->minutes += strtoul(line + 1, NULL, 10);
    }
}
