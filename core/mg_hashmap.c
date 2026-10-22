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

mg_status mg_hashmap_init(mg_hashmap *m, mg_hm_free_fn free_value) {
    if (!m) return MG_ERR_INVALID;
    m->buckets = NULL;
    m->nbuckets = 0;
    m->size = 0;
    m->free_value = free_value;
    m->buckets = (mg_hm_node **)mg_malloc(MG_HM_INITIAL_BUCKETS * sizeof(mg_hm_node *));
    if (!m->buckets) return MG_ERR_NOMEM;
    for (size_t i = 0; i < MG_HM_INITIAL_BUCKETS; ++i) m->buckets[i] = NULL;
    m->nbuckets = MG_HM_INITIAL_BUCKETS;
    return MG_OK;
}

static void free_node(mg_hashmap *m, mg_hm_node *n) {
    if (m->free_value) m->free_value(n->value);
    mg_free(n->key);
    mg_free(n);
}

void mg_hashmap_clear(mg_hashmap *m) {
    if (!m) return;
    for (size_t b = 0; b < m->nbuckets; ++b) {
        mg_hm_node *n = m->buckets[b];
        while (n) {
            mg_hm_node *next = n->next;
            free_node(m, n);
            n = next;
        }
        m->buckets[b] = NULL;
    }
    m->size = 0;
}

void mg_hashmap_destroy(mg_hashmap *m) {
    if (!m) return;
    mg_hashmap_clear(m);
    mg_free(m->buckets);
    m->buckets = NULL;
    m->nbuckets = 0;
}

