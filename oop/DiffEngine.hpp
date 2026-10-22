// DiffEngine.hpp - OOP face of the diff algorithm. The algorithm itself (LCS dynamic programming)
// lives in the C core (core/mg_diff.c); this class only wraps it.
#pragma once
#include "Dataset.hpp"
#include "Patch.hpp"

class DiffEngine {
public:
    // Row-level diff: the edits that turn old_data into new_data. Throws MiniGitError on failure.
    static Patch calculateLCS(const Dataset& old_data, const Dataset& new_data);

    // Rebuild a file: base_data + patch. Throws MiniGitError(MG_ERR_BAD_PATCH) if they do not fit.
    static Dataset applyPatch(const Dataset& base_data, const Patch& patch);
};
