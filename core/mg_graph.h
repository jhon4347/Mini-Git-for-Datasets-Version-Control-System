/* mg_graph.h - the COMMIT GRAPH (a DAG) stored in a hash table.
 *
 * DSA concepts:
 *   - Graph / DAG: every commit lists its parents (parent_ids). A commit can only be
 *     added after its parents exist, so a cycle can never be created.
 *   - Hash map: commit id -> commit, O(1) average lookup.
 *   - Graph traversal: mg_graph_rebuild walks from a commit back to the root along
 *     first parents, then replays the stored patches root -> commit.
 */
#ifndef MG_GRAPH_H
#define MG_GRAPH_H

#include <stddef.h>
#include "mg_status.h"
#include "mg_strvec.h"
#include "mg_diff.h"
#include "mg_hashmap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mg_commit {
    char     *id;
    char    **parent_ids;     /* array of parent commit ids (0 for the first commit) */
    size_t    parent_count;
    mg_patch  patch;          /* the delta from the first parent's content to this commit's content */
    char     *timestamp;
    char     *message;
} mg_commit;

typedef struct {
    mg_hashmap commits;       /* id -> mg_commit* */
} mg_graph;

/* On failure the graph is left empty and safe to destroy. */
mg_status        mg_graph_init(mg_graph *g);
void             mg_graph_destroy(mg_graph *g);
void             mg_graph_clear(mg_graph *g);

/* Adds a commit. Copies every argument. Errors: MG_ERR_DUPLICATE (id exists),
 * MG_ERR_NOT_FOUND (a parent does not exist), MG_ERR_INVALID, MG_ERR_NOMEM. */
mg_status        mg_graph_add_commit(mg_graph *g, const char *id,
                                     const char *const *parent_ids, size_t parent_count,
                                     const char *message, const char *timestamp,
                                     const mg_patch *patch);

const mg_commit *mg_graph_get(const mg_graph *g, const char *id);   /* NULL if missing */
size_t           mg_graph_count(const mg_graph *g);

/* Reconstructs the full file as it was at commit `id`. out is replaced only on success. */
mg_status        mg_graph_rebuild(const mg_graph *g, const char *id, mg_strvec *out);

#ifdef __cplusplus
}
#endif
#endif
