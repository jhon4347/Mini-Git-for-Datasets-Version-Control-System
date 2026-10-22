#include "mg_status.h"

const char *mg_status_str(mg_status s) {
    switch (s) {
        case MG_OK:            return "ok";
        case MG_ERR_NOMEM:     return "out of memory";
        case MG_ERR_INVALID:   return "invalid argument";
        case MG_ERR_NOT_FOUND: return "not found";
        case MG_ERR_DUPLICATE: return "already exists";
        case MG_ERR_BAD_PATCH: return "patch does not match the data";
    }
    return "unknown error";
}
