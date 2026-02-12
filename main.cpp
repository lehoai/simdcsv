#include <iostream>
#include <chrono>
#include "csv_reader.h"
// #include "csv.hpp"

constexpr int NUM_RUNS = 5;
constexpr auto CSV_FILE = "/home/lehoai/Desktop/tmp/vehicles.csv";

long long parse_simd() {
    const auto start = std::chrono::high_resolution_clock::now();
    
    size_t totalLines = 0;
    constexpr csv::format format;
    csv::CsvReader reader(CSV_FILE, format);
    reader.parse([&](const std::string_view* row) {
        // totalBytes += row[0].size();
        totalLines++;
    });

    std::cout << totalLines << std::endl;
    
    const auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

int main() {
    const auto time = parse_simd();

    std::cout << "| avg: " << time << "ms" << std::endl;

    return 0;
}
