// FileHandler.hpp - reads/writes CSV files as a Dataset (one row per line).
#pragma once
#include <string>
#include "Dataset.hpp"

class FileHandler {
public:
    // Reads a file line by line. Strips a trailing '\r' so Windows and Linux files behave the same.
    // If the file cannot be opened, prints an error and returns an empty Dataset.
    static Dataset readFile(const std::string& filepath);

    // Writes every row followed by '\n'. Returns false (after printing an error) on failure.
    static bool writeFile(const std::string& filepath, const Dataset& rows);

    static bool fileExists(const std::string& filepath);
};
