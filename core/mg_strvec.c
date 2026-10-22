#include "mg_strvec.h"
#include "mg_alloc.h"
#include <stdint.h>

void mg_strvec_init(mg_strvec *v) {
    v->items = NULL;
    v->size = 0;
    v->capacity = 0;
}

void mg_strvec_free(mg_strvec *v) {
    if (!v) return;
    for (size_t i = 0; i < v->size; ++i) mg_free(v->items[i]);
    mg_free(v->items);
    mg_strvec_init(v);
}

static mg_status grow(mg_strvec *v) {
    size_t newcap = v->capacity ? v->capacity * 2 : 8;
    if (v->capacity > SIZE_MAX / 2 || newcap > SIZE_MAX / sizeof(char *)) return MG_ERR_NOMEM;
    char **p = (char **)mg_realloc(v->items, newcap * sizeof(char *));
    if (!p) return MG_ERR_NOMEM;
    v->items = p;
    v->capacity = newcap;
    return MG_OK;
}

mg_status mg_strvec_push(mg_strvec *v, const char *s) {
    if (!v || !s) return MG_ERR_INVALID;
    if (v->size == v->capacity) {
        mg_status st = grow(v);
        if (st != MG_OK) return st;
    }
    char *copy = mg_strdup(s);
    if (!copy) return MG_ERR_NOMEM;
    v->items[v->size++] = copy;
    return MG_OK;
}

const char *mg_strvec_at(const mg_strvec *v, size_t i) {
    return (v && i < v->size) ? v->items[i] : NULL;
}

mg_status mg_strvec_copy(mg_strvec *dst, const mg_strvec *src) {
    if (!dst || !src) return MG_ERR_INVALID;
    mg_strvec tmp;
    mg_strvec_init(&tmp);
    for (size_t i = 0; i < src->size; ++i) {
        mg_status st = mg_strvec_push(&tmp, src->items[i]);
        if (st != MG_OK) { mg_strvec_free(&tmp); return st; }
    }
    mg_strvec_free(dst);   /* safe even if dst == src: tmp is already complete */
    *dst = tmp;
    return MG_OK;
}
