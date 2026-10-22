/* mg_hashmap.h - HASH TABLE with separate chaining (string key -> pointer value).
 *
 * DSA concepts:
 *   - hash function: 64-bit FNV-1a
 *   - collisions: each bucket is a singly linked list (chaining)
 *   - the bucket array doubles when the load factor passes 0.75, keeping chains short
 *   put / get / remove : O(1) on average, O(n) worst case
 *
 * The map owns its keys (it stores copies) and calls free_value on every value it
 * still holds when an entry is removed or the map is cleared/destroyed.
 */
#ifndef MG_HASHMAP_H
#define MG_HASHMAP_H

#include <stddef.h>
#include <stdint.h>
#include "mg_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mg_hm_free_fn)(void *value);

typedef struct mg_hm_node {
    char              *key;
    void              *value;
    uint64_t           hash;   /* cached so a resize never re-hashes the key */
    struct mg_hm_node *next;
} mg_hm_node;

typedef struct {
    mg_hm_node  **buckets;
    size_t        nbuckets;    /* always a power of two (or 0 if init failed) */
    size_t        size;
    mg_hm_free_fn free_value;  /* may be NULL */
} mg_hashmap;

/* On failure the map is left empty and safe to destroy. */
mg_status mg_hashmap_init(mg_hashmap *m, mg_hm_free_fn free_value);
void      mg_hashmap_destroy(mg_hashmap *m);
void      mg_hashmap_clear(mg_hashmap *m);                 /* removes all entries, keeps the buckets */

/* MG_ERR_DUPLICATE if key exists. On any failure the caller keeps ownership of value. */
mg_status mg_hashmap_put(mg_hashmap *m, const char *key, void *value);
/* NULL when the key is missing (so do not store NULL values). */
void     *mg_hashmap_get(const mg_hashmap *m, const char *key);
int       mg_hashmap_contains(const mg_hashmap *m, const char *key);
mg_status mg_hashmap_remove(mg_hashmap *m, const char *key);   /* MG_ERR_NOT_FOUND if missing */
size_t    mg_hashmap_size(const mg_hashmap *m);

#ifdef __cplusplus
}
#endif
#endif
