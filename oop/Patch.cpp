#include "Patch.hpp"
#include <stdexcept>
#include <utility>
#include "MiniGitError.hpp"

Patch::Patch() { mg_patch_init(&patch_); }

Patch::Patch(const Patch& other) : Patch() {
    throwIfError(mg_patch_copy(&patch_, &other.patch_), "Patch copy");
}

Patch::Patch(Patch&& other) noexcept : patch_(other.patch_) { mg_patch_init(&other.patch_); }

Patch& Patch::operator=(const Patch& other) {
    if (this != &other) {
        Patch copy(other);
        swap(copy);
    }
    return *this;
}

Patch& Patch::operator=(Patch&& other) noexcept {
    if (this != &other) {
        mg_patch_free(&patch_);
        patch_ = other.patch_;
        mg_patch_init(&other.patch_);
    }
    return *this;
}

Patch::~Patch() { mg_patch_free(&patch_); }

EditType Patch::typeAt(std::size_t index) const {
    if (index >= patch_.size) throw std::out_of_range("Patch::typeAt: index out of range");
    return static_cast<EditType>(patch_.rows[index].type);
}

std::string Patch::contentAt(std::size_t index) const {
    if (index >= patch_.size) throw std::out_of_range("Patch::contentAt: index out of range");
    const char* text = patch_.rows[index].content;
    return text ? text : "";
}

std::size_t Patch::count(EditType type) const noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < patch_.size; ++i) {
        if (static_cast<EditType>(patch_.rows[i].type) == type) ++n;
    }
    return n;
}

void Patch::push_back(EditType type, const std::string& content) {
    throwIfError(mg_patch_push(&patch_, static_cast<mg_edit_type>(type), content.c_str()), "Patch::push_back");
}

void Patch::swap(Patch& other) noexcept { std::swap(patch_, other.patch_); }
