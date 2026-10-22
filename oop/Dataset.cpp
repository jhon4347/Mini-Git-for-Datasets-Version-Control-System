#include "Dataset.hpp"
#include <cstring>
#include <stdexcept>
#include <utility>
#include "MiniGitError.hpp"

Dataset::Dataset() { mg_strvec_init(&rows_); }

Dataset::Dataset(const std::vector<std::string>& rows) : Dataset() {
    for (const auto& row : rows) push_back(row);
}

Dataset::Dataset(std::initializer_list<std::string> rows) : Dataset() {
    for (const auto& row : rows) push_back(row);
}

Dataset::Dataset(const Dataset& other) : Dataset() {
    throwIfError(mg_strvec_copy(&rows_, &other.rows_), "Dataset copy");
}

Dataset::Dataset(Dataset&& other) noexcept : rows_(other.rows_) {
    mg_strvec_init(&other.rows_);
}

Dataset& Dataset::operator=(const Dataset& other) {
    if (this != &other) {
        Dataset copy(other);       // copy first: if it throws, *this is untouched
        swap(copy);
    }
    return *this;
}

Dataset& Dataset::operator=(Dataset&& other) noexcept {
    if (this != &other) {
        mg_strvec_free(&rows_);
        rows_ = other.rows_;
        mg_strvec_init(&other.rows_);
    }
    return *this;
}

Dataset::~Dataset() { mg_strvec_free(&rows_); }

std::string Dataset::at(std::size_t index) const {
    const char* row = mg_strvec_at(&rows_, index);
    if (!row) throw std::out_of_range("Dataset::at: row index out of range");
    return row;
}

void Dataset::push_back(const std::string& row) {
    if (row.find('\0') != std::string::npos) {
        throw std::invalid_argument("Dataset::push_back: a row may not contain a NUL character");
    }
    throwIfError(mg_strvec_push(&rows_, row.c_str()), "Dataset::push_back");
}

std::vector<std::string> Dataset::toVector() const {
    std::vector<std::string> out;
    out.reserve(rows_.size);
    for (std::size_t i = 0; i < rows_.size; ++i) out.emplace_back(rows_.items[i]);
    return out;
}

void Dataset::swap(Dataset& other) noexcept { std::swap(rows_, other.rows_); }

bool Dataset::operator==(const Dataset& other) const {
    if (rows_.size != other.rows_.size) return false;
    for (std::size_t i = 0; i < rows_.size; ++i) {
        if (std::strcmp(rows_.items[i], other.rows_.items[i]) != 0) return false;
    }
    return true;
}
