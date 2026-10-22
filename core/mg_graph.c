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

