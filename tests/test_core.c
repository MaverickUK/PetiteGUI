/* Host tests for the code with no DOS in it: rectangles, number formatting and GameMinder's
 * metadata and launch rules. */
#include <stdio.h>
#include <string.h>
#include "../src/rect.h"
#include "../src/util.h"
#include "../src/gminfo.h"
#include "../src/textbuf.h"
#include "../src/saverlib.h"
#include "../src/gamecols.h"
#include "../src/field.h"
#include <math.h>
#include <stdlib.h>

static int failures, checks;

#define CHECK(cond)                                                       \
    do {                                                                  \
        checks++;                                                         \
        if (!(cond)) {                                                    \
            failures++;                                                   \
            printf("%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond);     \
        }                                                                 \
    } while (0)

static int same(Rect r, int x, int y, int w, int h)
{
    return r.x == x && r.y == y && r.w == w && r.h == h;
}

static void test_intersect(void)
{
    Rect a = rect_make(0, 0, 100, 50), b = rect_make(60, 40, 100, 100), out;

    CHECK(rect_intersect(&out, &a, &b));
    CHECK(same(out, 60, 40, 40, 10));

    b = rect_make(100, 0, 10, 10); /* touching edges don't overlap */
    CHECK(!rect_intersect(&out, &a, &b));
    CHECK(rect_empty(&out));

    b = rect_make(-20, -20, 30, 30); /* partly off screen */
    CHECK(rect_intersect(&a, &a, &b)); /* out may be an input */
    CHECK(same(a, 0, 0, 10, 10));
}

static void test_union_contains(void)
{
    Rect a = rect_make(10, 10, 10, 10), b = rect_make(30, 5, 5, 5), e = rect_make(0, 0, 0, 0), u;

    u = rect_union(&a, &b);
    CHECK(same(u, 10, 5, 25, 15));
    u = rect_union(&e, &b);
    CHECK(same(u, 30, 5, 5, 5));
    CHECK(rect_contains(&a, 10, 10));
    CHECK(!rect_contains(&a, 20, 10));
    CHECK(!rect_contains(&a, 9, 15));
}

static void test_resize(void)
{
    Rect r = rect_make(176, 41, 336, 122), out;

    out = rect_resize(&r, 403, 99, 176, 72, 640, 199);
    CHECK(same(out, 176, 41, 400, 99));      /* the width snaps to whole bytes */
    out = rect_resize(&r, 50, 10, 176, 72, 640, 199);
    CHECK(same(out, 176, 41, 176, 72));      /* no smaller than the minimum */
    out = rect_resize(&r, 900, 900, 176, 72, 640, 199);
    CHECK(same(out, 176, 41, 464, 158));     /* no further than the screen */
}

static void test_dates(void)
{
    char buf[16];

    fmt_date((unsigned)((14 << 9) | (3 << 5) | 12), buf); /* 1994-03-12 */
    CHECK(strcmp(buf, "1994/03/12") == 0);
    fmt_date((unsigned)((127u << 9) | (12 << 5) | 31), buf); /* the last day DOS has */
    CHECK(strcmp(buf, "2107/12/31") == 0);
    fmt_date(0, buf);
    CHECK(buf[0] == '\0');
    fmt_time((unsigned)((10 << 11) | (42 << 5) | 7), buf);
    CHECK(strcmp(buf, "10:42") == 0);
    fmt_time((unsigned)((23u << 11) | (59 << 5)), buf);
    CHECK(strcmp(buf, "23:59") == 0);
}

static void test_thousands(void)
{
    char buf[16];

    fmt_thousands(0, buf);
    CHECK(strcmp(buf, "0") == 0);
    fmt_thousands(999, buf);
    CHECK(strcmp(buf, "999") == 0);
    fmt_thousands(1000, buf);
    CHECK(strcmp(buf, "1,000") == 0);
    fmt_thousands(54645L, buf);
    CHECK(strcmp(buf, "54,645") == 0);
    fmt_thousands(1234567L, buf);
    CHECK(strcmp(buf, "1,234,567") == 0);
    fmt_thousands(-2048, buf);
    CHECK(strcmp(buf, "-2,048") == 0);
}

static void test_snap(void)
{
    CHECK(SNAP8(176) == 176);
    CHECK(SNAP8(179) == 176);
    CHECK(SNAP8(180) == 184);
    CHECK(SNAP8(-13) == -16);
    CHECK(SNAP8_UP(161) == 168);
    CHECK(SNAP8_UP(168) == 168);
}

static void test_gm_split_apply(void)
{
    char key[16], value[128];
    GameInfo g;

    CHECK(gm_split("name:Commander Keen 1", key, sizeof key, value, sizeof value));
    CHECK(strcmp(key, "NAME") == 0 && strcmp(value, "Commander Keen 1") == 0);
    CHECK(gm_split("  Exec : C:\\GAMES\\KEEN\\KEEN1.EXE /NOJOY\r\n", key, sizeof key, value, sizeof value));
    CHECK(strcmp(key, "EXEC") == 0 && strcmp(value, "C:\\GAMES\\KEEN\\KEEN1.EXE /NOJOY") == 0);
    CHECK(!gm_split("; a comment: with a colon", key, sizeof key, value, sizeof value));
    CHECK(!gm_split("no colon here", key, sizeof key, value, sizeof value));
    CHECK(!gm_split("   ", key, sizeof key, value, sizeof value));

    memset(&g, 0, sizeof g);
    gm_apply(&g, "NAME", "A name far too long to fit in the thirty-two characters");
    CHECK(strlen(g.name) == 32);
    gm_apply(&g, "MEMORY", "580");
    CHECK(g.memory_kb == 580);
    gm_apply(&g, "YEAR", "1991");
    CHECK(strcmp(g.year, "1991") == 0);
    gm_apply(&g, "SOUND", "PC Speaker"); /* known to GameMinder, not needed here */
}

static void test_gm_paths(void)
{
    char out[64], dir[64];

    gm_dir_name("C:\\GAMES\\KEEN1\\", out, sizeof out);
    CHECK(strcmp(out, "KEEN1") == 0);
    gm_dir_name("D:\\DOOM", out, sizeof out);
    CHECK(strcmp(out, "DOOM") == 0);

    strcpy(dir, "C:\\GAMES");
    gm_normalize_dir(dir, sizeof dir);
    CHECK(strcmp(dir, "C:\\GAMES\\") == 0);
    gm_normalize_dir(dir, sizeof dir);
    CHECK(strcmp(dir, "C:\\GAMES\\") == 0);

    CHECK(gm_path_count("C:\\GAMES;D:\\GAMES") == 2);
    CHECK(gm_path_count(" C:\\GAMES ; ;D:\\X;") == 2);
    gm_path_item(" C:\\GAMES ; ;D:\\X;", 1, out, sizeof out);
    CHECK(strcmp(out, "D:\\X") == 0);
    gm_path_item("C:\\", 0, out, sizeof out);
    CHECK(strcmp(out, "C:\\") == 0);
    CHECK(gm_path_count("") == 0);
}

static void test_gm_command(void)
{
    char prog[GM_PATH_MAX], args[GM_PATH_MAX];
    char lines[GM_SCRIPT_LINES][GM_PATH_MAX + 8];
    int n;

    gm_command("C:\\GAMES\\DOOM\\", "DOOM.EXE -nomonsters", "", prog, sizeof prog, args, sizeof args);
    CHECK(strcmp(prog, "C:\\GAMES\\DOOM\\DOOM.EXE") == 0);
    CHECK(strcmp(args, "-nomonsters") == 0);

    gm_command("C:\\GAMES\\KEEN\\", "start.bat 2", "C:\\DOS\\COMMAND.COM", prog, sizeof prog, args, sizeof args);
    CHECK(strcmp(prog, "C:\\DOS\\COMMAND.COM") == 0);
    CHECK(strcmp(args, "/C C:\\GAMES\\KEEN\\start.bat 2") == 0);

    gm_command("C:\\GAMES\\X\\", "D:\\CD\\RUN.EXE", NULL, prog, sizeof prog, args, sizeof args);
    CHECK(strcmp(prog, "D:\\CD\\RUN.EXE") == 0 && args[0] == '\0');

    gm_command("C:\\G\\", "GO.BAT", NULL, prog, sizeof prog, args, sizeof args);
    CHECK(strcmp(prog, GM_DEFAULT_COMSPEC) == 0);

    CHECK(gm_is_batch("X.BAT") && gm_is_batch("x.bat") && !gm_is_batch("X.BATX") && !gm_is_batch("BAT"));

    n = gm_script("C:\\G\\", "GO.BAT -x", "C:\\MOUSE\\MOUSE.COM", "C:\\MOUSE\\MOUSE OFF", lines);
    CHECK(n == 4);
    CHECK(strcmp(lines[0], "@ECHO OFF") == 0);
    CHECK(strcmp(lines[1], "C:\\MOUSE\\MOUSE.COM") == 0);
    CHECK(strcmp(lines[2], "CALL C:\\G\\GO.BAT -x") == 0);
    CHECK(strcmp(lines[3], "C:\\MOUSE\\MOUSE OFF") == 0);
    n = gm_script("C:\\G\\", "GAME.EXE", "", "  ", lines);
    CHECK(n == 2 && strcmp(lines[1], "C:\\G\\GAME.EXE") == 0);
}

static void test_gm_time(void)
{
    long a = gm_day_number(2026, 11, 26), b = gm_day_number(2026, 11, 27);

    CHECK(b - a == 1);
    CHECK(gm_day_number(2025, 3, 1) - gm_day_number(2025, 2, 28) == 1);
    CHECK(gm_day_number(2024, 3, 1) - gm_day_number(2024, 2, 28) == 2); /* leap year */
    CHECK(gm_day_number(2027, 1, 1) - gm_day_number(2026, 12, 31) == 1);
    CHECK(gm_elapsed_minutes(a, 23 * 60 + 50, b, 10) == 20); /* across midnight */
    CHECK(gm_elapsed_minutes(a, 600, a, 635) == 35);
    CHECK(gm_elapsed_minutes(b, 10, a, 10) == 0); /* the clock went back */
}

static void test_paths(void)
{
    char p[64], child[16];

    strcpy(p, "C:\\GAMES\\KEEN\\");
    CHECK(path_parent(p, child, sizeof child));
    CHECK(strcmp(p, "C:\\GAMES\\") == 0 && strcmp(child, "KEEN") == 0);
    CHECK(path_parent(p, child, sizeof child));
    CHECK(strcmp(p, "C:\\") == 0 && strcmp(child, "GAMES") == 0);
    CHECK(!path_parent(p, child, sizeof child));
    CHECK(strcmp(p, "C:\\") == 0);

    CHECK(path_is_text("README.TXT") && path_is_text("setup.ini") && path_is_text("A.Cfg"));
    CHECK(!path_is_text("GAME.EXE") && !path_is_text("TXT") && !path_is_text("X.TXTX") && !path_is_text("X.TX"));
    CHECK(path_is_text("AUTOEXEC.BAT") && path_is_text("config.sys") && path_is_text("C:\\CONFIG.SYS"));
    CHECK(!path_is_text("AUTOEXEC.BA") && !path_is_text("GO.BAT") && !path_is_text("HIMEM.SYS"));
    CHECK(path_is_program("AUTOEXEC.BAT")); /* Run... still runs it */
    CHECK(strcmp(path_ext("A.B.C"), "C") == 0 && strcmp(path_ext("NOEXT"), "") == 0);
    CHECK(path_is_program("DOOM.EXE") && path_is_program("go.bat") && path_is_program("Command.Com"));

    CHECK(path_join(p, sizeof p, "C:\\GAMES\\", "DOOM") && strcmp(p, "C:\\GAMES\\DOOM") == 0);
    CHECK(!path_join(p, 8, "C:\\GAMES\\", "DOOM"));
    CHECK(path_inside("C:\\GAMES\\DOOM", "C:\\GAMES\\") && path_inside("c:\\games", "C:\\GAMES\\"));
    CHECK(path_inside("C:\\GAMES\\DOOM\\", "C:\\GAMES\\DOOM") && path_inside("C:\\ANY", "C:\\"));
    CHECK(!path_inside("C:\\GAMESX", "C:\\GAMES\\") && !path_inside("C:\\GAME", "C:\\GAMES") && !path_inside("D:\\GAMES", "C:\\"));
    CHECK(path_same_drive("C:\\A", "c:\\B") && !path_same_drive("C:\\A", "D:\\A"));
    CHECK(path_valid_name("README.TXT") && path_valid_name("A") && path_valid_name("NEWFOLDR") && path_valid_name("A.B"));
    CHECK(!path_valid_name("") && !path_valid_name("TOOLONGNAME") && !path_valid_name("A.TEXT") && !path_valid_name("A B"));
    CHECK(!path_valid_name("*.TXT") && !path_valid_name("A.") && !path_valid_name(".TXT") && !path_valid_name("A:B") && !path_valid_name("A.B.C"));
    CHECK(!path_is_program("README.TXT") && !path_is_program("EXE") && !path_is_program("X.EXEC"));
}

/* The text as a C string, for comparing. */
static void tb_str(const TextBuf *t, char *out)
{
    unsigned i;

    for (i = 0; i < tb_len(t); i++)
        out[i] = tb_at(t, i);
    out[i] = '\0';
}

static void test_textbuf(void)
{
    char mem[64], out[80];
    TextBuf t;
    int crlf;
    const char *file = "one\r\ntwo\r\n\r\nfour\x1a";
    unsigned n = (unsigned)strlen(file), i;

    /* load from the end of the buffer's own block, as the editor does */
    tb_init(&t, mem, sizeof mem);
    memcpy(mem + sizeof mem - n, file, n);
    tb_load(&t, mem + sizeof mem - n, n, &crlf);
    tb_str(&t, out);
    CHECK(crlf && strcmp(out, "one\ntwo\n\nfour") == 0);
    CHECK(tb_lines(&t) == 4);
    CHECK(tb_line_of(&t, 4) == 1 && tb_line_of(&t, 0) == 0 && tb_line_of(&t, tb_len(&t)) == 3);

    CHECK(tb_line_start(&t, 6) == 4 && tb_line_end(&t, 4) == 7 && tb_col(&t, 6) == 2);
    CHECK(tb_next_line(&t, 4) == 8 && tb_next_line(&t, 8) == 9 && tb_next_line(&t, 9) == 9);
    CHECK(tb_prev_line(&t, 9) == 8 && tb_prev_line(&t, 4) == 0 && tb_prev_line(&t, 0) == 0);
    CHECK(tb_at_col(&t, 8, 3) == 8);          /* the empty line is shorter */
    CHECK(tb_at_col(&t, 0, 2) == 2);

    /* edits in the middle, then at the ends: the gap moves both ways */
    CHECK(tb_insert(&t, 3, '!'));
    CHECK(tb_insert(&t, 0, '>'));
    CHECK(tb_delete(&t, 5));                  /* the first \n */
    tb_str(&t, out);
    CHECK(strcmp(out, ">one!two\n\nfour") == 0);
    CHECK(tb_insert(&t, tb_len(&t), '\n'));
    CHECK(!tb_delete(&t, tb_len(&t)));
    tb_str(&t, out);
    CHECK(strcmp(out, ">one!two\n\nfour\n") == 0 && tb_lines(&t) == 4);

    /* a full block refuses more */
    for (i = 0; tb_insert(&t, 0, 'x'); i++)
        ;
    CHECK(tb_len(&t) == sizeof mem && i == sizeof mem - 15);

    /* LF-only files stay that way */
    tb_init(&t, mem, sizeof mem);
    memcpy(mem, "a\nb", 3);
    tb_load(&t, mem, 3, &crlf);
    CHECK(!crlf && tb_lines(&t) == 2);
}

/* Ranges, for selections and the clipboard, and finding. */
static void test_textbuf_ranges(void)
{
    char mem[32], out[40], clip[40];
    TextBuf t;
    int crlf;
    unsigned i;

    tb_init(&t, mem, sizeof mem);
    memcpy(mem, "Hello\nwide\nworld", 16);
    tb_load(&t, mem, 16, &crlf);
    CHECK(tb_count_lines(&t, 0, 16) == 2 && tb_count_lines(&t, 6, 11) == 1 && tb_count_lines(&t, 7, 7) == 0);
    CHECK(tb_room(&t) == sizeof mem - 16);

    /* copy out across the gap, wherever it is */
    tb_insert(&t, 8, '-');                    /* the gap now sits after "Hello\nwi-" */
    tb_copy_out(&t, 3, 10, clip);
    clip[10] = '\0';
    CHECK(strcmp(clip, "lo\nwi-de\nw") == 0);
    tb_copy_out(&t, 10, 4, clip);             /* all after the gap */
    clip[4] = '\0';
    CHECK(strcmp(clip, "e\nwo") == 0);
    tb_copy_out(&t, 0, 3, clip);              /* all before it */
    clip[3] = '\0';
    CHECK(strcmp(clip, "Hel") == 0);

    /* delete a range, insert a block */
    CHECK(tb_delete_range(&t, 5, 6));         /* "\nwi-de" */
    tb_str(&t, out);
    CHECK(strcmp(out, "Hello\nworld") == 0);
    CHECK(!tb_delete_range(&t, 8, 4));        /* past the end: nothing goes */
    CHECK(tb_len(&t) == 11);
    CHECK(tb_delete_range(&t, 11, 0));        /* nothing, at the end, is fine */
    CHECK(tb_insert_n(&t, 0, "Oh, ", 4));
    tb_str(&t, out);
    CHECK(strcmp(out, "Oh, Hello\nworld") == 0);
    CHECK(!tb_insert_n(&t, 3, "0123456789012345678901234567890", 31)); /* too big: none of it */
    CHECK(tb_len(&t) == 15);
    CHECK(!tb_insert_n(&t, 16, "x", 1));      /* past the end */

    /* finding, ignoring case */
    CHECK(tb_find(&t, 0, "hello", 5) == 4);
    CHECK(tb_find(&t, 5, "hello", 5) == TB_NONE);
    CHECK(tb_find(&t, 0, "O\nW", 3) == 8);
    CHECK(tb_find(&t, 0, "WORLD", 5) == 10);  /* right at the end */
    CHECK(tb_find(&t, 11, "WORLD", 5) == TB_NONE);
    CHECK(tb_find(&t, 0, "", 0) == TB_NONE);
    CHECK(tb_find(&t, 0, "worlds", 6) == TB_NONE);
    for (i = 0; i < 17; i++)
        tb_insert(&t, 0, 'x');                /* fill it: a find still works */
    CHECK(tb_room(&t) == 0 && tb_find(&t, 0, "world", 5) == 27);
}

static void test_sin(void)
{
    int i, worst = 0, err;

    sl_make_sin();
    for (i = 0; i < 256; i++) {
        err = abs(sl_sin[i] - (int)lround(16384.0 * sin(i * 2.0 * 3.14159265358979 / 256.0)));
        if (err > worst)
            worst = err;
    }
    CHECK(worst <= 8); /* within 0.05% of a full swing */
    CHECK(sl_sin[0] == 0 && sl_sin[64] == 16384 && sl_sin[128] == 0 && sl_sin[192] == -16384);
}

static void test_rand(void)
{
    unsigned i, a, seen[8] = { 0 };

    sl_seed(42);
    a = sl_rand();
    sl_seed(42);
    CHECK(sl_rand() == a);
    for (i = 0; i < 800; i++)
        seen[sl_rand_n(8)]++;
    for (i = 0; i < 8; i++)
        CHECK(seen[i] > 60 && seen[i] < 140);
}

static void test_life(void)
{
    unsigned char a[36], b[36];

    /* a blinker on a 6x6 grid: period 2 */
    memset(a, 0, sizeof a);
    a[2 * 6 + 1] = a[2 * 6 + 2] = a[2 * 6 + 3] = 1;
    CHECK(sl_life_step(a, b, 6, 6) == 4);
    CHECK(b[1 * 6 + 2] && b[2 * 6 + 2] && b[3 * 6 + 2] && !b[2 * 6 + 1]);
    CHECK(sl_life_step(b, a, 6, 6) == 4);
    CHECK(a[2 * 6 + 1] && a[2 * 6 + 3]);

    /* a block is still; it wraps across the edges */
    memset(a, 0, sizeof a);
    a[0] = a[5] = a[30] = a[35] = 1;
    CHECK(sl_life_step(a, b, 6, 6) == 0);
}

static void test_maze(void)
{
    enum { CW = 19, CH = 13 };
    unsigned char blocks[(CW * 2 + 1) * (CH * 2 + 1)];
    SlMaze m;
    int bx[2], by[2], n, steps = 0, open_walls = 0, x, y, built = 0, path = 0;

    sl_seed(7);
    sl_maze_start(&m, blocks, CW, CH);
    while ((n = sl_maze_step(&m, bx, by)) >= 0 && steps < 20000) {
        steps++;
        if (m.solving && !built) {
            built = 1;
            /* every cell open, and cells - 1 passages: a tree, so one way between any two */
            for (y = 0; y < CH; y++)
                for (x = 0; x < CW; x++)
                    CHECK(blocks[(y * 2 + 1) * m.bw + x * 2 + 1] != MZ_WALL);
            for (y = 1; y < m.bh - 1; y++)
                for (x = 1; x < m.bw - 1; x++)
                    if ((x + y) % 2 == 1 && blocks[y * m.bw + x] != MZ_WALL)
                        open_walls++;
            CHECK(open_walls == CW * CH - 1);
        }
    }
    CHECK(built && m.done && steps < 20000);
    CHECK(blocks[(CH * 2 - 1) * m.bw + CW * 2 - 1] == MZ_PATH); /* reached the last cell */
    CHECK(blocks[1 * m.bw + 1] == MZ_PATH);
    for (y = 0; y < m.bh; y++)
        for (x = 0; x < m.bw; x++)
            path += blocks[y * m.bw + x] == MZ_PATH;
    CHECK(path >= CW + CH - 1); /* at least the straight-line distance, in cells and passages */
    for (x = 0; x < m.bw; x++)
        CHECK(blocks[x] == MZ_WALL && blocks[(m.bh - 1) * m.bw + x] == MZ_WALL); /* a closed border */
}

static void test_segments(void)
{
    CHECK(sl_segments[8] == 0x7F && sl_segments[1] == 0x06 && sl_segments[0] == 0x3F);
}

static void test_gamecols(void)
{
    char out[80];
    Game g;

    CHECK(gc_parse("year,genre") == ((1u << COL_YEAR) | (1u << COL_GENRE)));
    CHECK(gc_parse(" Plays , TIME,nonsense,last") == ((1u << COL_PLAYS) | (1u << COL_TIME) | (1u << COL_LAST)));
    CHECK(gc_parse("none") == 0 && gc_parse("") == 0);
    gc_format((1u << COL_TIME) | (1u << COL_YEAR), out);
    CHECK(strcmp(out, "year,time") == 0);
    gc_format(0, out);
    CHECK(strcmp(out, "none") == 0);
    CHECK(gc_parse("year,genre,developer,type,video,sound,memory,plays,last,time") == (1u << NUM_COLS) - 1);

    gc_minutes(0, out);
    CHECK(out[0] == '\0');
    gc_minutes(45, out);
    CHECK(strcmp(out, "45m") == 0);
    gc_minutes(185, out);
    CHECK(strcmp(out, "3h 05m") == 0);

    memset(&g, 0, sizeof g);
    gc_log_line(&g, ">2026/11/20 14:05");
    gc_log_line(&g, "<35\r\n");
    gc_log_line(&g, ">2026/11/24 09:00\n");
    gc_log_line(&g, "<150");
    gc_log_line(&g, "garbage");
    CHECK(g.plays == 2 && g.minutes == 185 && strcmp(g.last, "2026/11/24") == 0);

    strcpy(g.year, "1991");
    g.memory_kb = 580;
    gc_text(&g, COL_YEAR, out);
    CHECK(strcmp(out, "1991") == 0);
    gc_text(&g, COL_PLAYS, out);
    CHECK(strcmp(out, "2") == 0);
    gc_text(&g, COL_TIME, out);
    CHECK(strcmp(out, "3h 05m") == 0);
    gc_text(&g, COL_MEMORY, out);
    CHECK(strcmp(out, "580K") == 0);
    gc_text(&g, COL_GENRE, out);
    CHECK(out[0] == '\0');
    g.plays = 0;
    gc_text(&g, COL_PLAYS, out);
    CHECK(out[0] == '\0'); /* blank, not 0, for never played */
}

static void test_field(void)
{
    Field f;
    int i;

    field_set(&f, "PKUNZIP", 10);
    CHECK(f.caret == 7 && f.len == 7 && f.scroll == 0);
    field_key(&f, ' ', 0);
    field_key(&f, 'A', 0);
    CHECK(strcmp(f.text, "PKUNZIP A") == 0);
    field_key(&f, 0, 71);                     /* Home */
    field_key(&f, 'X', 0);
    CHECK(strcmp(f.text, "XPKUNZIP A") == 0 && f.caret == 1);
    field_key(&f, 0, 83);                     /* Delete: the P */
    CHECK(strcmp(f.text, "XKUNZIP A") == 0);
    field_key(&f, 8, 0);                      /* Backspace: the X */
    CHECK(strcmp(f.text, "KUNZIP A") == 0 && f.caret == 0);
    CHECK(field_key(&f, 8, 0) == 0);          /* nothing before the caret */
    CHECK(field_key(&f, 0, 75) == 0);         /* nor left of it */

    /* typing past the width scrolls, keeping the caret showing */
    field_set(&f, "", 10);
    for (i = 0; i < 25; i++)
        field_key(&f, 'a' + i, 0);
    CHECK(f.caret == 25 && f.scroll == 16);
    field_key(&f, 0, 71);
    CHECK(f.scroll == 0);
    field_key(&f, 0, 79);
    CHECK(f.scroll == 16);

    /* full: no more */
    field_set(&f, "", 10);
    for (i = 0; i < 100; i++)
        field_key(&f, 'x', 0);
    CHECK(f.len == FIELD_MAX && (int)strlen(f.text) == FIELD_MAX);
    CHECK(field_key(&f, 13, 0) == 0);         /* Enter isn't for the field */
}

int main(void)
{
    test_intersect();
    test_union_contains();
    test_thousands();
    test_dates();
    test_resize();
    test_snap();
    test_gm_split_apply();
    test_gm_paths();
    test_gm_command();
    test_gm_time();
    test_paths();
    test_textbuf();
    test_textbuf_ranges();
    test_sin();
    test_rand();
    test_life();
    test_maze();
    test_segments();
    test_gamecols();
    test_field();
    printf("%d checks, %d failed\n", checks, failures);
    return failures != 0;
}
