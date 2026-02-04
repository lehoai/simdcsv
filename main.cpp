#include <iostream>
#include <chrono>
#include "csv_reader.h"
#include "csv.hpp"

constexpr int NUM_RUNS = 5;
constexpr const char* CSV_FILE = "/home/lehoai/Desktop/tmp/34.csv";

long long parse_simd() {
    const auto start = std::chrono::high_resolution_clock::now();
    
    size_t totalLines = 0;
    size_t totalBytes = 0;
    csv::CsvReader::parse(CSV_FILE, [&](const std::string_view* row) {
        totalBytes += row[0].size();
        totalLines++;
    });
    
    const auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

long long parse_csv_parser() {
    const auto start = std::chrono::high_resolution_clock::now();
    
    size_t totalLines = 0;
    size_t totalBytes = 0;
    csv::CSVFormat format;
    format.header_row(0);
    format.delimiter(',');
    format.quote('"');
    csv::CSVReader reader(CSV_FILE, format);
    
    for (auto& row : reader) {
        totalBytes += row[0].get<std::string_view>().size();
        totalLines++;
    }
    
    const auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

int main() {
    long long simd_total = 0;
    long long csv_total = 0;

    std::cout << "SIMD: ";
    for (int i = 0; i < NUM_RUNS; i++) {
        long long t = parse_simd();
        simd_total += t;
        std::cout << t << "ms ";
    }
    std::cout << "| avg: " << simd_total / NUM_RUNS << "ms" << std::endl;

    std::cout << "csv-parser: ";
    for (int i = 0; i < NUM_RUNS; i++) {
        long long t = parse_csv_parser();
        csv_total += t;
        std::cout << t << "ms ";
    }
    std::cout << "| avg: " << csv_total / NUM_RUNS << "ms" << std::endl;

    return 0;
}
