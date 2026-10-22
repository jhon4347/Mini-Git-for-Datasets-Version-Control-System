/* mg_strvec.h - DYNAMIC ARRAY of owned strings (one string = one CSV row).
 *
 * DSA concept: dynamic array with capacity doubling.
 *   push  : amortised O(1)      at : O(1)      copy : O(n)
 * The vector owns its strings: it stores private copies and frees them.
 */
#ifndef MG_STRVEC_H
#define MG_STRVEC_H

#include <stddef.h>
#include "mg_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char  **items;
    size_t  size;
    size_t  capacity;
} mg_strvec;

void        mg_strvec_init(mg_strvec *v);               /* empty vector            */
void        mg_strvec_free(mg_strvec *v);               /* frees everything, then re-inits */
mg_status   mg_strvec_push(mg_strvec *v, const char *s);/* stores a copy of s      */
const char *mg_strvec_at(const mg_strvec *v, size_t i); /* NULL when i >= size     */

/* Deep copy. dst is replaced only on success (dst may equal src). */
mg_status   mg_strvec_copy(mg_strvec *dst, const mg_strvec *src);

#ifdef __cplusplus
}
#endif
#endif
