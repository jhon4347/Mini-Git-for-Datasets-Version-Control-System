#include "mg_graph.h"
#include "mg_alloc.h"
#include <stdint.h>

static void commit_free(void *p) {
    mg_commit *c = (mg_commit *)p;
    if (!c) return;
    mg_free(c->id);
    for (size_t i = 0; i < c->parent_count; ++i) mg_free(c->parent_ids[i]);
    mg_free(c->parent_ids);
    mg_patch_free(&c->patch);
    mg_free(c->timestamp);
    mg_free(c->message);
    mg_free(c);
}

mg_status mg_graph_init(mg_graph *g) {
    if (!g) return MG_ERR_INVALID;
    return mg_hashmap_init(&g->commits, commit_free);
}

void mg_graph_destroy(mg_graph *g) {
    if (g) mg_hashmap_destroy(&g->commits);
}

void mg_graph_clear(mg_graph *g) {
    if (g) mg_hashmap_clear(&g->commits);
}

size_t mg_graph_count(const mg_graph *g) {
    return g ? mg_hashmap_size(&g->commits) : 0;
}

const mg_commit *mg_graph_get(const mg_graph *g, const char *id) {
    if (!g || !id) return NULL;
    return (const mg_commit *)mg_hashmap_get(&g->commits, id);
}

mg_status mg_graph_add_commit(mg_graph *g, const char *id,
                              const char *const *parent_ids, size_t parent_count,
                              const char *message, const char *timestamp,
                              const mg_patch *patch) {
    if (!g || !id || !*id || !patch) return MG_ERR_INVALID;
    if (parent_count > 0 && !parent_ids) return MG_ERR_INVALID;
    if (parent_count > SIZE_MAX / sizeof(char *)) return MG_ERR_INVALID;
    if (mg_hashmap_contains(&g->commits, id)) return MG_ERR_DUPLICATE;
    for (size_t i = 0; i < parent_count; ++i) {
        if (!parent_ids[i] || !mg_hashmap_contains(&g->commits, parent_ids[i])) return MG_ERR_NOT_FOUND;
    }

    mg_commit *c = (mg_commit *)mg_malloc(sizeof *c);
    if (!c) return MG_ERR_NOMEM;
    c->id = NULL; c->parent_ids = NULL; c->parent_count = 0;
    mg_patch_init(&c->patch);
    c->timestamp = NULL; c->message = NULL;

    mg_status st = MG_OK;
    c->id        = mg_strdup(id);
    c->message   = mg_strdup(message ? message : "");
    c->timestamp = mg_strdup(timestamp ? timestamp : "");
    if (!c->id || !c->message || !c->timestamp) st = MG_ERR_NOMEM;

    if (st == MG_OK && parent_count > 0) {
        c->parent_ids = (char **)mg_malloc(parent_count * sizeof(char *));
        if (!c->parent_ids) st = MG_ERR_NOMEM;
        for (size_t i = 0; st == MG_OK && i < parent_count; ++i) {
            c->parent_ids[i] = mg_strdup(parent_ids[i]);
            if (!c->parent_ids[i]) st = MG_ERR_NOMEM;
            else c->parent_count++;                  /* count only what exists, so commit_free is safe */
        }
    }
    if (st == MG_OK) st = mg_patch_copy(&c->patch, patch);
    if (st == MG_OK) st = mg_hashmap_put(&g->commits, id, c);

    if (st != MG_OK) commit_free(c);                 /* put failed => we still own c */
    return st;
}

