#include "FileHandler.hpp"
#include <fstream>
#include <iostream>

Dataset FileHandler::readFile(const std::string& filepath) {
    Dataset rows;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[FileHandler Error] Failed to open file: " << filepath << "\n";
        return rows;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        rows.push_back(line);
    }
    return rows;
}

bool FileHandler::writeFile(const std::string& filepath, const Dataset& rows) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[FileHandler Error] Failed to write to file: " << filepath << "\n";
        return false;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) file << rows.at(i) << "\n";
    file.flush();
    if (!file) {
        std::cerr << "[FileHandler Error] Failed while writing: " << filepath << "\n";
        return false;
    }
    return true;
}

bool FileHandler::fileExists(const std::string& filepath) {
    std::ifstream file(filepath);
    return file.good();
}
