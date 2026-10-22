// Dataset.hpp - OOP wrapper (RAII) around the C dynamic array mg_strvec.
// One Dataset = the rows of one CSV file.
//
// OOP concepts: encapsulation (the C struct is private), RAII (the destructor frees the C
// memory), and the "rule of five" (copy/move constructors, copy/move assignment, destructor).
#pragma once
#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>
#include "mg_strvec.h"

class Dataset {
public:
    Dataset();
    explicit Dataset(const std::vector<std::string>& rows);
    Dataset(std::initializer_list<std::string> rows);

    Dataset(const Dataset& other);                 // deep copy
    Dataset(Dataset&& other) noexcept;             // steals the C array, leaves other empty
    Dataset& operator=(const Dataset& other);
    Dataset& operator=(Dataset&& other) noexcept;
    ~Dataset();

    std::size_t size() const noexcept { return rows_.size; }
    bool empty() const noexcept { return rows_.size == 0; }
    std::string at(std::size_t index) const;       // throws std::out_of_range
    void push_back(const std::string& row);        // throws std::invalid_argument if row contains '\0'
    std::vector<std::string> toVector() const;
    void swap(Dataset& other) noexcept;

    bool operator==(const Dataset& other) const;
    bool operator!=(const Dataset& other) const { return !(*this == other); }

    // Access for the C functions (kept public on purpose: the other wrapper classes need it).
    const mg_strvec& raw() const noexcept { return rows_; }
    mg_strvec& raw() noexcept { return rows_; }

private:
    mg_strvec rows_;
};
