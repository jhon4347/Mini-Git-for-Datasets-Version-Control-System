// Patch.hpp - OOP wrapper (RAII) around the C patch (mg_patch): the list of INSERT / DELETE / KEEP edits.
#pragma once
#include <cstddef>
#include <string>
#include "mg_diff.h"

enum class EditType { INSERT = MG_EDIT_INSERT, DELETE = MG_EDIT_DELETE, KEEP = MG_EDIT_KEEP };

class Patch {
public:
    Patch();
    Patch(const Patch& other);
    Patch(Patch&& other) noexcept;
    Patch& operator=(const Patch& other);
    Patch& operator=(Patch&& other) noexcept;
    ~Patch();

    std::size_t size() const noexcept { return patch_.size; }
    EditType typeAt(std::size_t index) const;          // throws std::out_of_range
    std::string contentAt(std::size_t index) const;    // "" for KEEP; throws std::out_of_range
    std::size_t count(EditType type) const noexcept;
    void push_back(EditType type, const std::string& content = "");
    void swap(Patch& other) noexcept;

    const mg_patch& raw() const noexcept { return patch_; }
    mg_patch& raw() noexcept { return patch_; }

private:
    mg_patch patch_;
};
