#include "mg_alloc.h"
#include <stdlib.h>
#include <string.h>

static long g_budget = -1;   /* < 0: no simulated failures */

static int should_fail(void) {
    if (g_budget < 0)  return 0;
    if (g_budget == 0) return 1;
    g_budget--;
    return 0;
}

void *mg_malloc(size_t n) {
    if (should_fail()) return NULL;
    return malloc(n ? n : 1);
}

void *mg_realloc(void *p, size_t n) {
    if (should_fail()) return NULL;
    return realloc(p, n ? n : 1);
}

void mg_free(void *p) { free(p); }

char *mg_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = (char *)mg_malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

void mg_alloc_fail_after(long n) { g_budget = n; }
