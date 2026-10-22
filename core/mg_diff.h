/* mg_diff.h - row-level diff (LCS dynamic programming) and patch apply.
 *
 * DSA concepts:
 *   - Dynamic programming: LCS table of size (m+1) x (n+1)   O(m*n) time, O(m*n) memory
 *   - Backtracking through the table to recover the edit script       O(m+n)
 *   - Patch replay with a moving index into the base file             O(patch size)
 *
 * A patch is a list of edits, in file order:
 *   KEEP   - row is unchanged (nothing is stored)
 *   INSERT - row exists only in the NEW file  (its text is stored)
 *   DELETE - row exists only in the OLD file  (its text is stored)
 */
#ifndef MG_DIFF_H
#define MG_DIFF_H

#include <stddef.h>
#include "mg_status.h"
#include "mg_strvec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { MG_EDIT_INSERT = 0, MG_EDIT_DELETE = 1, MG_EDIT_KEEP = 2 } mg_edit_type;

typedef struct {
    mg_edit_type type;
    char        *content;   /* owned; NULL for KEEP */
} mg_diffrow;

typedef struct {
    mg_diffrow *rows;
    size_t      size;
    size_t      capacity;
} mg_patch;

void      mg_patch_init(mg_patch *p);
void      mg_patch_free(mg_patch *p);
/* content is copied; it must be non-NULL for INSERT/DELETE and is ignored for KEEP. */
mg_status mg_patch_push(mg_patch *p, mg_edit_type type, const char *content);
/* Deep copy. dst is replaced only on success. */
mg_status mg_patch_copy(mg_patch *dst, const mg_patch *src);

/* Compute the edit script that turns old_rows into new_rows.
 * out is replaced only on success. */
mg_status mg_diff_lines(const mg_strvec *old_rows, const mg_strvec *new_rows, mg_patch *out);

/* Rebuild the new file from base + patch. The patch is validated against the
 * base (no reading past the end, deleted text must match, every base row must be
 * consumed); a mismatch returns MG_ERR_BAD_PATCH. out is replaced only on success. */
mg_status mg_apply_patch(const mg_strvec *base, const mg_patch *patch, mg_strvec *out);

#ifdef __cplusplus
}
#endif
#endif
