#include "mg_hashmap.h"
#include "mg_alloc.h"
#include <string.h>

#define MG_HM_INITIAL_BUCKETS 16u

static uint64_t hash_str(const char *s) {            /* FNV-1a, 64 bit */
    uint64_t h = 14695981039346656037ULL;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ULL;
    }
    return h;
}

