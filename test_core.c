/* test_core.c - tests for the C core (dynamic array, hash table, LCS diff, commit DAG). Pure C. */
#include "check.h"
#include "mg_alloc.h"
#include "mg_diff.h"
#include "mg_graph.h"
#include "mg_hashmap.h"
#include "mg_strvec.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- helpers ---------- */

static uint64_t rng_state = 88172645463325252ULL;           /* xorshift64: deterministic */
static uint32_t rnd(void) {
    rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17;
    return (uint32_t)(rng_state >> 11);
}

/* build a vector from "a|b|c" ("" gives no rows) */
static mg_strvec rows_of(const char *spec) {
    mg_strvec v;
    mg_strvec_init(&v);
    if (!*spec) return v;
    char buf[256];
    strncpy(buf, spec, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
    for (char *tok = strtok(buf, "|"); tok; tok = strtok(NULL, "|")) mg_strvec_push(&v, tok);
    return v;
}

static int rows_equal(const mg_strvec *a, const mg_strvec *b) {
    if (a->size != b->size) return 0;
    for (size_t i = 0; i < a->size; ++i) if (strcmp(a->items[i], b->items[i]) != 0) return 0;
    return 1;
}

/* independent oracle: LCS length with a rolling 1-D array (different code from mg_diff.c) */
static size_t oracle_lcs(const mg_strvec *a, const mg_strvec *b) {
    size_t *row = (size_t *)calloc(b->size + 1, sizeof *row);
    for (size_t i = 1; i <= a->size; ++i) {
        size_t diag = 0;
        for (size_t j = 1; j <= b->size; ++j) {
            size_t saved = row[j];
            if (strcmp(a->items[i - 1], b->items[j - 1]) == 0) row[j] = diag + 1;
            else if (row[j - 1] > row[j]) row[j] = row[j - 1];
            diag = saved;
        }
    }
    size_t r = row[b->size];
    free(row);
    return r;
}

static size_t count_type(const mg_patch *p, mg_edit_type t) {
    size_t n = 0;
    for (size_t i = 0; i < p->size; ++i) if (p->rows[i].type == t) n++;
    return n;
}

/* ---------- dynamic array ---------- */

static void test_strvec(void) {
    mg_strvec v; mg_strvec_init(&v);
    char buf[32];
    for (int i = 0; i < 1000; ++i) { snprintf(buf, sizeof buf, "row-%d", i); CHECK(mg_strvec_push(&v, buf) == MG_OK); }
    CHECK(v.size == 1000 && v.capacity >= 1000);
    CHECK(strcmp(mg_strvec_at(&v, 0), "row-0") == 0 && strcmp(mg_strvec_at(&v, 999), "row-999") == 0);
    CHECK(mg_strvec_at(&v, 1000) == NULL);
    CHECK(mg_strvec_push(&v, NULL) == MG_ERR_INVALID);
    CHECK(mg_strvec_push(&v, "") == MG_OK && strcmp(mg_strvec_at(&v, 1000), "") == 0);   /* empty row is legal */

    mg_strvec c; mg_strvec_init(&c);
    CHECK(mg_strvec_copy(&c, &v) == MG_OK && rows_equal(&c, &v));
    CHECK(c.items[0] != v.items[0]);                                 /* deep copy, not shared */
    CHECK(mg_strvec_copy(&v, &v) == MG_OK && v.size == 1001);        /* copy onto itself is safe */
    mg_strvec_free(&c); CHECK(c.size == 0 && c.items == NULL);
    mg_strvec_free(&v);
}

/* ---------- hash table ---------- */

static int g_freed = 0;
static void counting_free(void *p) { g_freed++; free(p); }

static void test_hashmap(void) {
    mg_hashmap m;
    CHECK(mg_hashmap_init(&m, counting_free) == MG_OK && m.nbuckets == 16);
    char key[32];
    for (int i = 0; i < 5000; ++i) {
        snprintf(key, sizeof key, "key-%d", i);
        int *val = (int *)malloc(sizeof *val); *val = i * 7;
        CHECK(mg_hashmap_put(&m, key, val) == MG_OK);
    }
    CHECK(mg_hashmap_size(&m) == 5000);
    CHECK(m.nbuckets > 16 && m.size * 4 <= m.nbuckets * 3);          /* it grew and stayed under load 0.75 */
    int all_found = 1;
    for (int i = 0; i < 5000; ++i) {
        snprintf(key, sizeof key, "key-%d", i);
        int *got = (int *)mg_hashmap_get(&m, key);
        if (!got || *got != i * 7) all_found = 0;
    }
    CHECK(all_found);
    CHECK(mg_hashmap_get(&m, "nope") == NULL && !mg_hashmap_contains(&m, "nope"));

    int *dup = (int *)malloc(sizeof *dup);
    CHECK(mg_hashmap_put(&m, "key-1", dup) == MG_ERR_DUPLICATE);    /* caller keeps ownership */
    free(dup);
    CHECK(mg_hashmap_size(&m) == 5000);

    g_freed = 0;
    for (int i = 0; i < 5000; i += 2) { snprintf(key, sizeof key, "key-%d", i); CHECK(mg_hashmap_remove(&m, key) == MG_OK); }
    CHECK(g_freed == 2500 && mg_hashmap_size(&m) == 2500);
    CHECK(mg_hashmap_remove(&m, "key-0") == MG_ERR_NOT_FOUND);
    CHECK(mg_hashmap_get(&m, "key-1") != NULL && mg_hashmap_get(&m, "key-2") == NULL);

    int *e = (int *)malloc(sizeof *e); *e = 1;
    CHECK(mg_hashmap_put(&m, "", e) == MG_OK && mg_hashmap_get(&m, "") == e);   /* empty key works */

    g_freed = 0;
    mg_hashmap_clear(&m);
    CHECK(mg_hashmap_size(&m) == 0 && g_freed == 2501);
    mg_hashmap_destroy(&m);
    CHECK(m.buckets == NULL && mg_hashmap_get(&m, "key-1") == NULL);
}

/* ---------- diff ---------- */

static void roundtrip(const char *oldspec, const char *newspec) {
    mg_strvec a = rows_of(oldspec), b = rows_of(newspec), r; mg_patch p;
    mg_strvec_init(&r); mg_patch_init(&p);
    CHECK(mg_diff_lines(&a, &b, &p) == MG_OK);
    CHECK(mg_apply_patch(&a, &p, &r) == MG_OK && rows_equal(&r, &b));
    CHECK(count_type(&p, MG_EDIT_KEEP) == oracle_lcs(&a, &b));
    CHECK(p.size == a.size + b.size - oracle_lcs(&a, &b));
    mg_strvec_free(&a); mg_strvec_free(&b); mg_strvec_free(&r); mg_patch_free(&p);
}

static void test_diff_cases(void) {
    roundtrip("", "");
    roundtrip("", "x|y");
    roundtrip("x|y", "");
    roundtrip("a|b|c", "a|b|c");
    roundtrip("a|b|c", "a|c");
    roundtrip("a|c", "a|b|c");
    roundtrip("a|b|c", "x|y|z");
    roundtrip("a|a|a", "a|a");
    roundtrip("a|b|a|b", "b|a|b|a");
    roundtrip("h,1|h,2|h,3", "h,3|h,2|h,1");

    /* exact shape (documents the tie-break: DELETE comes before INSERT) */
    mg_strvec a = rows_of("a|b|c"), b = rows_of("a|x|c"); mg_patch p; mg_patch_init(&p);
    CHECK(mg_diff_lines(&a, &b, &p) == MG_OK && p.size == 4);
    CHECK(p.rows[0].type == MG_EDIT_KEEP   && p.rows[0].content == NULL);
    CHECK(p.rows[1].type == MG_EDIT_DELETE && strcmp(p.rows[1].content, "b") == 0);
    CHECK(p.rows[2].type == MG_EDIT_INSERT && strcmp(p.rows[2].content, "x") == 0);
    CHECK(p.rows[3].type == MG_EDIT_KEEP);
    mg_patch_free(&p); mg_strvec_free(&a); mg_strvec_free(&b);

    /* identical files store no text at all (every edit is a text-less KEEP) */
    a = rows_of("r1|r2|r3"); b = rows_of("r1|r2|r3"); mg_patch_init(&p);
    CHECK(mg_diff_lines(&a, &b, &p) == MG_OK && count_type(&p, MG_EDIT_KEEP) == 3);
    int no_text = 1; for (size_t i = 0; i < p.size; ++i) if (p.rows[i].content) no_text = 0;
    CHECK(no_text);
    mg_patch_free(&p); mg_strvec_free(&a); mg_strvec_free(&b);

    /* argument checks */
    CHECK(mg_diff_lines(NULL, &b, &p) == MG_ERR_INVALID);
}

static void test_apply_errors(void) {
    mg_strvec base = rows_of("a|b"), out = rows_of("keep-me"); mg_patch p; mg_patch_init(&p);

    mg_patch_push(&p, MG_EDIT_KEEP, NULL); mg_patch_push(&p, MG_EDIT_KEEP, NULL); mg_patch_push(&p, MG_EDIT_KEEP, NULL);
    CHECK(mg_apply_patch(&base, &p, &out) == MG_ERR_BAD_PATCH);     /* KEEP past the end of base */
    CHECK(out.size == 1 && strcmp(out.items[0], "keep-me") == 0);   /* out untouched on failure  */
    mg_patch_free(&p);

    mg_patch_push(&p, MG_EDIT_DELETE, "WRONG"); mg_patch_push(&p, MG_EDIT_KEEP, NULL);
    CHECK(mg_apply_patch(&base, &p, &out) == MG_ERR_BAD_PATCH);     /* deleted text differs from base */
    mg_patch_free(&p);

    mg_patch_push(&p, MG_EDIT_KEEP, NULL);
    CHECK(mg_apply_patch(&base, &p, &out) == MG_ERR_BAD_PATCH);     /* base not fully consumed */
    mg_patch_free(&p);

    CHECK(mg_patch_push(&p, MG_EDIT_INSERT, NULL) == MG_ERR_INVALID);   /* INSERT needs text */
    mg_strvec_free(&base); mg_strvec_free(&out);
}

static void test_random_roundtrip(void) {
    int bad = 0;
    for (int trial = 0; trial < 3000; ++trial) {
        unsigned alphabet = 1 + rnd() % 6;
        mg_strvec a, b, r; mg_patch p;
        mg_strvec_init(&a); mg_strvec_init(&b); mg_strvec_init(&r); mg_patch_init(&p);
        size_t la = rnd() % 41, lb = rnd() % 41;
        char buf[16];
        for (size_t i = 0; i < la; ++i) { snprintf(buf, sizeof buf, "r%u", rnd() % alphabet); mg_strvec_push(&a, buf); }
        for (size_t i = 0; i < lb; ++i) { snprintf(buf, sizeof buf, "r%u", rnd() % alphabet); mg_strvec_push(&b, buf); }
        if (mg_diff_lines(&a, &b, &p) != MG_OK) bad++;
        else {
            if (mg_apply_patch(&a, &p, &r) != MG_OK || !rows_equal(&r, &b)) bad++;
            if (count_type(&p, MG_EDIT_KEEP) != oracle_lcs(&a, &b)) bad++;
        }
        mg_strvec_free(&a); mg_strvec_free(&b); mg_strvec_free(&r); mg_patch_free(&p);
    }
    CHECK(bad == 0);
}

/* ---------- commit DAG ---------- */

static void add_version(mg_graph *g, const char *id, const char *parent, const mg_strvec *prev, const mg_strvec *next) {
    mg_patch p; mg_patch_init(&p);
    CHECK(mg_diff_lines(prev, next, &p) == MG_OK);
    const char *parents[1] = { parent };
    CHECK(mg_graph_add_commit(g, id, parent ? parents : NULL, parent ? 1 : 0, "msg", "2026-01-01 00:00:00", &p) == MG_OK);
    mg_patch_free(&p);
}

static void test_graph(void) {
    mg_graph g; CHECK(mg_graph_init(&g) == MG_OK);
    mg_strvec empty = rows_of(""), v1 = rows_of("h|1|2"), v2 = rows_of("h|1|2|3"), v3 = rows_of("h|2|3|4"), v4 = rows_of("h|2|4"), out;
    mg_strvec_init(&out);

    add_version(&g, "c1", NULL, &empty, &v1);
    add_version(&g, "c2", "c1", &v1, &v2);
    add_version(&g, "c3", "c2", &v2, &v3);
    CHECK(mg_graph_count(&g) == 3);
    CHECK(mg_graph_rebuild(&g, "c1", &out) == MG_OK && rows_equal(&out, &v1));
    CHECK(mg_graph_rebuild(&g, "c2", &out) == MG_OK && rows_equal(&out, &v2));
    CHECK(mg_graph_rebuild(&g, "c3", &out) == MG_OK && rows_equal(&out, &v3));

    const mg_commit *c2 = mg_graph_get(&g, "c2");
    CHECK(c2 && c2->parent_count == 1 && strcmp(c2->parent_ids[0], "c1") == 0 && strcmp(c2->message, "msg") == 0);
    CHECK(mg_graph_get(&g, "c1")->parent_count == 0);
    CHECK(mg_graph_get(&g, "zzz") == NULL);

    /* a commit with two parents (merge-shaped): rebuild follows the first parent */
    mg_patch p; mg_patch_init(&p);
    const char *two[2] = { "c3", "c1" };
    CHECK(mg_diff_lines(&v3, &v4, &p) == MG_OK);
    CHECK(mg_graph_add_commit(&g, "c4", two, 2, "merge-ish", "t", &p) == MG_OK);
    CHECK(mg_graph_get(&g, "c4")->parent_count == 2);
    CHECK(mg_graph_rebuild(&g, "c4", &out) == MG_OK && rows_equal(&out, &v4));

    /* invariants that keep the graph acyclic */
    const char *ghost[1] = { "no-such-parent" };
    CHECK(mg_graph_add_commit(&g, "c5", ghost, 1, "m", "t", &p) == MG_ERR_NOT_FOUND);
    CHECK(mg_graph_count(&g) == 4);                                         /* nothing was added */
    CHECK(mg_graph_add_commit(&g, "c1", NULL, 0, "m", "t", &p) == MG_ERR_DUPLICATE);
    CHECK(mg_graph_add_commit(&g, "", NULL, 0, "m", "t", &p) == MG_ERR_INVALID);
    CHECK(mg_graph_rebuild(&g, "unknown", &out) == MG_ERR_NOT_FOUND && rows_equal(&out, &v4));   /* out untouched */

    /* tamper on purpose: make c1 point at c3 => a cycle. rebuild must notice, not loop forever. */
    mg_commit *c1 = (mg_commit *)mg_graph_get(&g, "c1");
    c1->parent_ids = (char **)mg_malloc(sizeof(char *));
    c1->parent_ids[0] = mg_strdup("c3");
    c1->parent_count = 1;
    CHECK(mg_graph_rebuild(&g, "c3", &out) == MG_ERR_INVALID);

    mg_graph_clear(&g);
    CHECK(mg_graph_count(&g) == 0 && mg_graph_get(&g, "c1") == NULL);
    mg_patch_free(&p); mg_strvec_free(&empty); mg_strvec_free(&v1); mg_strvec_free(&v2);
    mg_strvec_free(&v3); mg_strvec_free(&v4); mg_strvec_free(&out);
    mg_graph_destroy(&g);
}

static void test_large_chain(void) {
    /* 2000 commits in a row: one hash-table with many entries + a long DAG walk */
    mg_graph g; CHECK(mg_graph_init(&g) == MG_OK);
    mg_strvec prev, next, out; mg_strvec_init(&prev); mg_strvec_init(&next); mg_strvec_init(&out);
    char id[32], prev_id[32] = "", row[32];
    for (int i = 0; i < 2000; ++i) {
        mg_strvec_free(&next);
        for (int k = 0; k <= i % 50; ++k) { snprintf(row, sizeof row, "row-%d", (i + k) % 97); mg_strvec_push(&next, row); }
        snprintf(id, sizeof id, "commit-%d", i);
        add_version(&g, id, i ? prev_id : NULL, &prev, &next);
        mg_strvec_copy(&prev, &next);
        strcpy(prev_id, id);
    }
    CHECK(mg_graph_count(&g) == 2000);
    CHECK(mg_graph_rebuild(&g, "commit-1999", &out) == MG_OK && rows_equal(&out, &prev));
    mg_strvec_free(&prev); mg_strvec_free(&next); mg_strvec_free(&out); mg_graph_destroy(&g);
}

/* ---------- out-of-memory behaviour ---------- */

/* Runs a whole mini-workflow. Every allocation may fail; whatever happens, everything is freed.
 * On success it also checks that the rebuilt file is exactly right. */
static mg_status scenario(void) {
    mg_strvec empty, a, b, r; mg_patch p0, p1; mg_graph g;
    mg_strvec_init(&empty); mg_strvec_init(&a); mg_strvec_init(&b); mg_strvec_init(&r);
    mg_patch_init(&p0); mg_patch_init(&p1);
    mg_status st = mg_graph_init(&g);                       /* a failed init leaves g safe to destroy */
    const char *va[] = { "h", "1", "2", "3", "4", "5" };
    const char *vb[] = { "h", "2", "x", "4", "5", "6", "7" };
    for (size_t i = 0; st == MG_OK && i < 6; ++i) st = mg_strvec_push(&a, va[i]);
    for (size_t i = 0; st == MG_OK && i < 7; ++i) st = mg_strvec_push(&b, vb[i]);
    if (st == MG_OK) st = mg_diff_lines(&empty, &a, &p0);   /* commit one: empty -> a */
    if (st == MG_OK) st = mg_diff_lines(&a, &b, &p1);       /* commit two: a -> b     */
    if (st == MG_OK) st = mg_graph_add_commit(&g, "one", NULL, 0, "first", "t", &p0);
    if (st == MG_OK) { const char *par[1] = { "one" }; st = mg_graph_add_commit(&g, "two", par, 1, "second", "t", &p1); }
    if (st == MG_OK) st = mg_graph_rebuild(&g, "two", &r);
    if (st == MG_OK && !rows_equal(&r, &b)) st = MG_ERR_INVALID;          /* wrong content = test failure */
    mg_strvec_free(&empty); mg_strvec_free(&a); mg_strvec_free(&b); mg_strvec_free(&r);
    mg_patch_free(&p0); mg_patch_free(&p1); mg_graph_destroy(&g);
    return st;
}

static void test_out_of_memory(void) {
    int nomem = 0, other = 0, ok_at = -1;
    for (int n = 0; n < 5000; ++n) {
        mg_alloc_fail_after(n);                              /* the first n allocations work, then all fail */
        mg_status st = scenario();
        mg_alloc_fail_after(-1);
        if (st == MG_OK) { ok_at = n; break; }
        if (st == MG_ERR_NOMEM) nomem++; else other++;
    }
    CHECK(other == 0);              /* a failure is always reported as "out of memory", never anything else */
    CHECK(nomem > 50);              /* the simulation really did hit many different allocation points        */
    CHECK(ok_at > 0);               /* and with enough memory the scenario completes                         */
    printf("      (simulated allocation failures survived: %d)\n", nomem);
}

int main(void) {
    printf("C core tests\n");
    RUN(test_strvec);
    RUN(test_hashmap);
    RUN(test_diff_cases);
    RUN(test_apply_errors);
    RUN(test_random_roundtrip);
    RUN(test_graph);
    RUN(test_large_chain);
    RUN(test_out_of_memory);
    return SUMMARY();
}
