/* mg_alloc.h - the only place the C core allocates memory.
 *
 * Wrapping malloc/realloc lets the tests simulate "out of memory" at any
 * point (mg_alloc_fail_after) and prove that every error path frees what it
 * allocated. Not thread-safe (the project is single-threaded).
 */
#ifndef MG_ALLOC_H
#define MG_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *mg_malloc(size_t n);
void *mg_realloc(void *p, size_t n);
void  mg_free(void *p);
char *mg_strdup(const char *s);          /* NULL on failure */

/* Testing hook: the next n allocations succeed, every later one fails.
 * Pass a negative number to switch the simulation off (the default). */
void  mg_alloc_fail_after(long n);

#ifdef __cplusplus
}
#endif
#endif
