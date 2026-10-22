/* mg_status.h - result codes shared by the whole C core */
#ifndef MG_STATUS_H
#define MG_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MG_OK = 0,
    MG_ERR_NOMEM,      /* malloc/realloc failed or a size would overflow      */
    MG_ERR_INVALID,    /* NULL pointer or otherwise unusable argument         */
    MG_ERR_NOT_FOUND,  /* commit id / hash-map key does not exist             */
    MG_ERR_DUPLICATE,  /* commit id / hash-map key already exists             */
    MG_ERR_BAD_PATCH   /* patch does not fit the data it is applied to        */
} mg_status;

/* Human-readable text for a status code (never NULL). */
const char *mg_status_str(mg_status s);

#ifdef __cplusplus
}
#endif
#endif
