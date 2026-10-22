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

