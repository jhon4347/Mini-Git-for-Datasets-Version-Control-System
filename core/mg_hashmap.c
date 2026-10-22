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

static mg_hm_node *find_node(const mg_hashmap *m, const char *key, uint64_t h) {
    if (m->nbuckets == 0) return NULL;
    for (mg_hm_node *n = m->buckets[h & (m->nbuckets - 1)]; n; n = n->next) {
        if (n->hash == h && strcmp(n->key, key) == 0) return n;
    }
    return NULL;
}

/* Best effort: if memory is short the table simply keeps its current size
 * (chains get longer, but everything still works). */
static void resize(mg_hashmap *m) {
    size_t newn = m->nbuckets * 2;
    if (newn / 2 != m->nbuckets || newn > SIZE_MAX / sizeof(mg_hm_node *)) return;
    mg_hm_node **nb = (mg_hm_node **)mg_malloc(newn * sizeof(mg_hm_node *));
    if (!nb) return;
    for (size_t i = 0; i < newn; ++i) nb[i] = NULL;
    for (size_t b = 0; b < m->nbuckets; ++b) {
        mg_hm_node *n = m->buckets[b];
        while (n) {                                  /* re-link nodes, no new allocation */
            mg_hm_node *next = n->next;
            size_t idx = (size_t)(n->hash & (newn - 1));
            n->next = nb[idx];
            nb[idx] = n;
            n = next;
        }
    }
    mg_free(m->buckets);
    m->buckets = nb;
    m->nbuckets = newn;
}

mg_status mg_hashmap_put(mg_hashmap *m, const char *key, void *value) {
    if (!m || !key || m->nbuckets == 0) return MG_ERR_INVALID;
    uint64_t h = hash_str(key);
    if (find_node(m, key, h)) return MG_ERR_DUPLICATE;

    if ((m->size + 1) * 4 > m->nbuckets * 3) resize(m);    /* load factor > 0.75 */

    mg_hm_node *n = (mg_hm_node *)mg_malloc(sizeof *n);
    if (!n) return MG_ERR_NOMEM;
    n->key = mg_strdup(key);
    if (!n->key) { mg_free(n); return MG_ERR_NOMEM; }
    n->value = value;
    n->hash = h;

    size_t idx = (size_t)(h & (m->nbuckets - 1));
    n->next = m->buckets[idx];                              /* insert at the head of the chain */
    m->buckets[idx] = n;
    m->size++;
    return MG_OK;
}

void *mg_hashmap_get(const mg_hashmap *m, const char *key) {
    if (!m || !key) return NULL;
    mg_hm_node *n = find_node(m, key, hash_str(key));
    return n ? n->value : NULL;
}

int mg_hashmap_contains(const mg_hashmap *m, const char *key) {
    if (!m || !key) return 0;
    return find_node(m, key, hash_str(key)) != NULL;
}

mg_status mg_hashmap_remove(mg_hashmap *m, const char *key) {
    if (!m || !key) return MG_ERR_INVALID;
    if (m->nbuckets == 0) return MG_ERR_NOT_FOUND;
    uint64_t h = hash_str(key);
    mg_hm_node **link = &m->buckets[h & (m->nbuckets - 1)];
    while (*link) {
        mg_hm_node *n = *link;
        if (n->hash == h && strcmp(n->key, key) == 0) {
            *link = n->next;                                /* unlink, then free */
            free_node(m, n);
            m->size--;
            return MG_OK;
        }
        link = &n->next;
    }
    return MG_ERR_NOT_FOUND;
}

size_t mg_hashmap_size(const mg_hashmap *m) { return m ? m->size : 0; }
