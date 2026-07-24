#pragma once
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>
#include <iostream>

namespace mydb {

    using Row = std::unordered_map<std::string, std::string>;

    inline void printRow(const Row& row, const std::vector<std::string>& cols) {
        for (const auto& col : cols) {
            auto it = row.find(col);
            std::cout << col << ": "
                      << (it != row.end() ? it->second : "NULL")
                      << "  ";
        }
        std::cout << "\n";
    }

}
