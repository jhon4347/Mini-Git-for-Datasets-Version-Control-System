#include "DiffEngine.hpp"
#include "MiniGitError.hpp"

Patch DiffEngine::calculateLCS(const Dataset& old_data, const Dataset& new_data) {
    Patch patch;
    throwIfError(mg_diff_lines(&old_data.raw(), &new_data.raw(), &patch.raw()), "diff");
    return patch;
}

Dataset DiffEngine::applyPatch(const Dataset& base_data, const Patch& patch) {
    Dataset result;
    throwIfError(mg_apply_patch(&base_data.raw(), &patch.raw(), &result.raw()), "apply patch");
    return result;
}
