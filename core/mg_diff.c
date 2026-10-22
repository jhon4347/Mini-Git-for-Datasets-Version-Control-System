#include "mg_diff.h"
#include "mg_alloc.h"
#include <stdint.h>
#include <string.h>

/* ---------------------------------------------------------------- patch */

void mg_patch_init(mg_patch *p) {
    p->rows = NULL;
    p->size = 0;
    p->capacity = 0;
}

void mg_patch_free(mg_patch *p) {
    if (!p) return;
    for (size_t i = 0; i < p->size; ++i) mg_free(p->rows[i].content);
    mg_free(p->rows);
    mg_patch_init(p);
}

static mg_status patch_grow(mg_patch *p) {
    size_t newcap = p->capacity ? p->capacity * 2 : 8;
    if (p->capacity > SIZE_MAX / 2 || newcap > SIZE_MAX / sizeof(mg_diffrow)) return MG_ERR_NOMEM;
    mg_diffrow *r = (mg_diffrow *)mg_realloc(p->rows, newcap * sizeof(mg_diffrow));
    if (!r) return MG_ERR_NOMEM;
    p->rows = r;
    p->capacity = newcap;
    return MG_OK;
}

mg_status mg_patch_push(mg_patch *p, mg_edit_type type, const char *content) {
    if (!p) return MG_ERR_INVALID;
    if (type != MG_EDIT_INSERT && type != MG_EDIT_DELETE && type != MG_EDIT_KEEP) return MG_ERR_INVALID;
    if (type != MG_EDIT_KEEP && !content) return MG_ERR_INVALID;

    char *copy = NULL;
    if (type != MG_EDIT_KEEP) {
        copy = mg_strdup(content);
        if (!copy) return MG_ERR_NOMEM;
    }
    if (p->size == p->capacity) {
        mg_status st = patch_grow(p);
        if (st != MG_OK) { mg_free(copy); return st; }
    }
    p->rows[p->size].type = type;
    p->rows[p->size].content = copy;
    p->size++;
    return MG_OK;
}

mg_status mg_patch_copy(mg_patch *dst, const mg_patch *src) {
    if (!dst || !src) return MG_ERR_INVALID;
    mg_patch tmp;
    mg_patch_init(&tmp);
    for (size_t i = 0; i < src->size; ++i) {
        mg_status st = mg_patch_push(&tmp, src->rows[i].type, src->rows[i].content);
        if (st != MG_OK) { mg_patch_free(&tmp); return st; }
    }
    mg_patch_free(dst);
    *dst = tmp;
    return MG_OK;
}

/* ------------------------------------------------------------ LCS diff */

mg_status mg_diff_lines(const mg_strvec *old_rows, const mg_strvec *new_rows, mg_patch *out) {
    if (!old_rows || !new_rows || !out) return MG_ERR_INVALID;

    const size_t m = old_rows->size;
    const size_t n = new_rows->size;
    if (m >= UINT32_MAX || n >= UINT32_MAX) return MG_ERR_NOMEM;

    /* One contiguous (m+1) x (n+1) table; dp[i][j] = LCS length of the first i
     * old rows and the first j new rows. Refuse sizes that would overflow. */
    const size_t cols = n + 1;
    if ((m + 1) > SIZE_MAX / sizeof(uint32_t) / cols) return MG_ERR_NOMEM;
    uint32_t *dp = (uint32_t *)mg_malloc((m + 1) * cols * sizeof(uint32_t));
    if (!dp) return MG_ERR_NOMEM;

    for (size_t j = 0; j <= n; ++j) dp[j] = 0;               /* row 0    */
    for (size_t i = 0; i <= m; ++i) dp[i * cols] = 0;        /* column 0 */

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            if (strcmp(old_rows->items[i - 1], new_rows->items[j - 1]) == 0) {
                dp[i * cols + j] = dp[(i - 1) * cols + (j - 1)] + 1;
            } else {
                uint32_t up   = dp[(i - 1) * cols + j];
                uint32_t left = dp[i * cols + (j - 1)];
                dp[i * cols + j] = up > left ? up : left;
            }
        }
    }

    /* The edit script has exactly m + n - LCS entries (KEEPs count once), so we can
     * allocate it once and fill it from the back while backtracking: no reversing. */
    const size_t lcs   = dp[m * cols + n];
    const size_t total = m + n - lcs;

    mg_diffrow *rows = NULL;
    if (total > 0) {
        if (total > SIZE_MAX / sizeof(mg_diffrow)) { mg_free(dp); return MG_ERR_NOMEM; }
        rows = (mg_diffrow *)mg_malloc(total * sizeof(mg_diffrow));
        if (!rows) { mg_free(dp); return MG_ERR_NOMEM; }
    }

    size_t idx = total;
    size_t i = m, j = n;
    while (i > 0 || j > 0) {
        mg_edit_type type;
        const char  *text = NULL;

        if (i > 0 && j > 0 && strcmp(old_rows->items[i - 1], new_rows->items[j - 1]) == 0) {
            type = MG_EDIT_KEEP;                       /* rows match: keep, move diagonally  */
            i--; j--;
        } else if (j > 0 && (i == 0 || dp[i * cols + (j - 1)] >= dp[(i - 1) * cols + j])) {
            type = MG_EDIT_INSERT;                     /* row only in the new file           */
            text = new_rows->items[j - 1];
            j--;
        } else {
            type = MG_EDIT_DELETE;                     /* row only in the old file           */
            text = old_rows->items[i - 1];
            i--;
        }

        if (idx == 0) {                                /* cannot happen; defensive guard     */
            for (size_t k = 0; k < total; ++k) mg_free(rows[k].content);
            mg_free(rows); mg_free(dp);
            return MG_ERR_INVALID;
        }
        idx--;
        rows[idx].type = type;
        rows[idx].content = NULL;
        if (text) {
            rows[idx].content = mg_strdup(text);
            if (!rows[idx].content) {
                for (size_t k = idx + 1; k < total; ++k) mg_free(rows[k].content);
                mg_free(rows); mg_free(dp);
                return MG_ERR_NOMEM;
            }
        }
    }
    mg_free(dp);

    if (idx != 0) {                                    /* cannot happen; defensive guard     */
        for (size_t k = 0; k < total; ++k) mg_free(rows[k].content);
        mg_free(rows);
        return MG_ERR_INVALID;
    }

    mg_patch_free(out);
    out->rows = rows;
    out->size = total;
    out->capacity = total;
    return MG_OK;
}

