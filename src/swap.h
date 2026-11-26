/* Running a program with PetiteGUI swapped out of memory (TurboPower's ExecWithSwap, as
 * GameMinder uses it): everything but a ~1 KB stub goes to EMS, or to a swap file if there's
 * no EMS, so the program gets nearly all of conventional memory. See swapcore.asm. */
#ifndef SWAP_H
#define SWAP_H

#include <stdint.h>

/* Returned for failures before DOS EXEC is reached. */
#define SWAP_ERR_NO_SPACE 0xFF01u   /* neither EMS nor room for the swap file */
#define SWAP_ERR_VECTOR 0xFF02u     /* an interrupt points into PetiteGUI's memory */
#define SWAP_ERR_DIR 0xFF03u        /* couldn't change to the program's directory */
#define SWAP_ERR_OUT 1u             /* the swap out failed */

typedef struct SwapOptions {
    int use_ems;          /* EMS if there is some, else the file */
    const char *temp_dir; /* where SWAP.$$$ goes, ending in a backslash */
} SwapOptions;

typedef struct SwapReport {
    long bytes;      /* swapped out */
    int to_ems;      /* 1 EMS, 0 file */
    int vector;      /* for SWAP_ERR_VECTOR, the interrupt */
} SwapReport;

/* Run prog (a full path) with args, in dir (its drive and directory become current). */
unsigned swap_run(const char *prog, const char *args, const char *dir, const SwapOptions *o, SwapReport *r);

/* What a swap would move, and about how much conventional memory a program would get. */
long swap_bytes(void);
long swap_resident_bytes(void);
long swap_free_kb(void);
int swap_ems_present(void);
long swap_ems_free_kb(void);

#endif
