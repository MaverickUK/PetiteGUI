/* Host tests for the code with no DOS in it: rectangles and number formatting. */
#include <stdio.h>
#include <string.h>
#include "../src/rect.h"
#include "../src/util.h"

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

int main(void)
{
    test_intersect();
    test_union_contains();
    test_thousands();
    test_snap();
    printf("%d checks, %d failed\n", checks, failures);
    return failures != 0;
}
