#include <iostream>
#include <chrono>
#include "csv_reader.h"
// #include "csv.hpp"

constexpr int NUM_RUNS = 5;
constexpr const char* CSV_FILE = "/home/lehoai/Desktop/tmp/34.csv";

long long parse_simd() {
    const auto start = std::chrono::high_resolution_clock::now();
    
    size_t totalLines = 0;
    size_t totalBytes = 0;
    csv::format format;
    csv::CsvReader::parse(CSV_FILE, format, [&](const std::string_view* row) {
        // totalBytes += row[0].size();
        totalLines++;
    });

    std::cout << totalLines << std::endl;
    std::cout << totalBytes << std::endl;
    
    const auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

int main() {

    auto time = parse_simd();

    std::cout << "| avg: " << time << "ms" << std::endl;

    return 0;
}
