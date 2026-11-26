/* Running a game from the list as GameMinder would: its !GM_INFO.TXT read afresh, batch
 * files through COMMAND.COM, before: and after: in one batch file with the game, the play
 * logged to !GM_LOG.TXT, and PetiteGUI swapped out while it runs. */
#ifndef LAUNCH_H
#define LAUNCH_H

#include "gminfo.h"

#define LAUNCH_NO_INFO 0xFF10u   /* !GM_INFO.TXT couldn't be read */
#define LAUNCH_NO_EXEC 0xFF11u   /* it has no exec: (or setup:, for setup) */

/* Read dir's !GM_INFO.TXT. Zero if it can't be read. */
int launch_read_info(const char *dir, GameInfo *g);

/* The program a launch would run, without arguments: to check it exists. */
void launch_program(const char *dir, const GameInfo *g, int setup, char *out, int size);
int launch_exists(const char *path);

/* Run the game (or its setup program) in text mode. Prints what it's doing, as GameMinder
 * does, and returns 0, a DOS error code, or one of the SWAP_ERR_ codes. */
unsigned launch_run(const char *dir, const GameInfo *g, int setup);

/* Run a program in dir, swapped out, in text mode: prog with args (prog a full path), or with
 * shell set, the command line args through COMMAND.COM (/C), which finds programs on the
 * PATH and runs DOS's own commands; or COMMAND.COM itself, for a DOS prompt, if args is "".
 * banner is printed first. Returns as launch_run does. */
unsigned launch_command(const char *dir, const char *prog, const char *args, int shell, const char *banner);
const char *launch_comspec(void);

/* A sentence for a failed launch. */
const char *launch_error_text(unsigned status);

#endif
