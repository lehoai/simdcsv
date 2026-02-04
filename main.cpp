#include <iostream>
#include <chrono>
// #include "csv_reader.h"
#include "csv.hpp"
void parse_old() {

    const auto start = std::chrono::high_resolution_clock::now();

    size_t totalLines = 0;
    size_t totalBytes = 0;

    csv::CSVFormat format;
    format.header_row(0);
    format.delimiter(',');
    format.quote('"');
    csv::CSVReader reader("/home/lehoai/Desktop/100column34Tr100KRecord.csv", format);

    for (auto& row: reader) {
        totalBytes += row[0].get<std::string_view>().size();
        totalLines++;
    }

    std::cout << totalBytes << std::endl;
    std::cout << totalLines << std::endl;
    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << duration.count() << "ms" << std::endl;

}

// void parse() {
//
//     const auto start = std::chrono::high_resolution_clock::now();
//
//     size_t totalLines = 0;
//     size_t totalBytes = 0;
//     csv::CsvReader::parse("/home/lehoai/Desktop/100column34Tr100KRecord.csv", [&](const std::string_view* row) {
//         totalBytes+= row[0].size();
//         totalLines++;
//     });
//
//     std::cout << totalBytes << std::endl;
//     std::cout << totalLines << std::endl;
//     const auto end = std::chrono::high_resolution_clock::now();
//     const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
//     std::cout << duration.count() << "ms" << std::endl;
// }

int main() {
    parse_old();
    // parse();
    return 0;
}