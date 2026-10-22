/* check.h - a tiny test helper that works in both C and C++ */
#ifndef CHECK_H
#define CHECK_H
#include <stdio.h>

static int g_pass = 0, g_fail = 0;

#define CHECK(cond) do { if (cond) { g_pass++; } else { g_fail++; \
    fprintf(stderr, "  FAIL  %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define RUN(fn) do { int before_ = g_fail; fn(); \
    printf("  %-36s %s\n", #fn, g_fail == before_ ? "ok" : "FAILED"); } while (0)

#define SUMMARY() (printf("SUMMARY: %d passed, %d failed\n", g_pass, g_fail), g_fail ? 1 : 0)
#endif
