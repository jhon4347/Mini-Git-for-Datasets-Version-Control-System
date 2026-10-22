// MiniGitError.hpp - one exception type for every failure reported by the C core.
#pragma once
#include <stdexcept>
#include <string>
#include "mg_status.h"

class MiniGitError : public std::runtime_error {
public:
    MiniGitError(mg_status status, const std::string& context)
        : std::runtime_error(context + ": " + mg_status_str(status)), status_(status) {}

    mg_status status() const noexcept { return status_; }

private:
    mg_status status_;
};

// Turns a C status code into an exception (the bridge between C error codes and C++).
inline void throwIfError(mg_status status, const std::string& context) {
    if (status != MG_OK) throw MiniGitError(status, context);
}
