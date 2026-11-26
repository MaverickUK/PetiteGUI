/* A stand-in game for tools/dostest.sh: appends to the file named first how much conventional
 * memory DOS has free while it runs (the largest block, in KB), then its other arguments. */
#include <dos.h>
#include <i86.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    union REGS r;
    FILE *f;
    int i;

    if (argc < 2)
        return 1;
    r.h.ah = 0x48;
    r.x.bx = 0xFFFF;
    int86(0x21, &r, &r);
    f = fopen(argv[1], "a");
    if (f == NULL)
        return 1;
    fprintf(f, "free=%u", (unsigned)((unsigned long)r.x.bx * 16UL / 1024UL));
    for (i = 2; i < argc; i++)
        fprintf(f, " %s", argv[i]);
    fprintf(f, "\n");
    fclose(f);
    return 0;
}
