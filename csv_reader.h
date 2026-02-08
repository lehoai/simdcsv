//
// Created by lehoai on 2/4/26.
//

#ifndef SIMDCSV_CSV_READER_H
#define SIMDCSV_CSV_READER_H

#include <cstdint>
#include <string_view>
#include <immintrin.h>
#include <vector>
#include <tuple>
#include <thread>
#include <atomic>
#include <charconv>

#include "mmap.h"

constexpr size_t BUFFER_SIZE = 128 * 1024;
constexpr size_t PREFETCH_CHUNK = 16 * 1024 * 1024;  // 16MB prefetch ahead
constexpr size_t PAGE_SIZE = 4096;
namespace csv {

    struct format {
        char delimiter = ',';
        char quote = '"';
        int header_row =0;
    };

    // Prefix XOR
    // ex: 00100100 -> 00111100
    inline uint32_t prefix_xor(uint32_t mask) {
        mask ^= (mask << 1);
        mask ^= (mask << 2);
        mask ^= (mask << 4);
        mask ^= (mask << 8);
        mask ^= (mask << 16);
        return mask;
    }

    // trim quote
    inline std::string_view trim_quotes(const std::string_view sv, const csv::format& format) {
        if (sv.size() >= 2 && sv.front() == format.quote && sv.back() == format.quote) {
            return sv.substr(1, sv.size() - 2);
        }
        return sv;
    }

    // helper convert string_view to data
    // std::from_chars for best performance
    template<typename T>
    inline T get(std::string_view sv) {
        T value{};
        std::from_chars(sv.data(), sv.data() + sv.size(), value);
        return value;
    }

    // Parse header row and return: (col_count, headers, pointer after header line)
    inline std::tuple<int, std::vector<std::string_view>, const char*>
    parse_header_row(const char* data, const char* end, format format) {
        std::vector<std::string_view> headers;
        const char* ptr = data;
        const char* field_start = data;
        bool in_quote = false;
        int header_row_idx = 0;


        while (ptr < end) {
            const char c = *ptr;
            if (c == format.quote) {
                in_quote = !in_quote;
            } else if (!in_quote) {
                if (c == format.delimiter || c == '\n') {
                    if (header_row_idx == format.header_row) {
                        headers.push_back(trim_quotes(std::string_view(field_start, ptr - field_start), format));
                    }
                    field_start = ptr + 1;
                    if (c == '\n') {
                        if (header_row_idx == format.header_row) {
                            return {static_cast<int>(headers.size()), headers, ptr + 1};
                        }
                        header_row_idx++;
                    }
                }
            }
            ptr++;
        }

        // Handle last field if no trailing newline
        if (field_start < end) {
            headers.push_back(trim_quotes(std::string_view(field_start, end - field_start), format));
        }
        return {static_cast<int>(headers.size()), headers, end};
    }


    class CsvReader {
    public:
        // using RowCallback = std::function<void(const std::vector<std::string_view>&)>;

        template <typename RowCallback>
        static void parse(const char* file_path, format format, const RowCallback &callback);
    };
}

template <typename RowCallback>
void csv::CsvReader::parse(const char* file_path, const format format, const RowCallback &callback) {
    const auto f_map = new csv::file::FMmap(file_path);

    const char *data = f_map->data();
    const size_t size = f_map->size();
    const char* end = data + size;

    // Auto-detect header and column count
    auto [col_num, headers, data_start] = parse_header_row(data, end, format);

    const char* ptr = data_start; // start after header row

    // PREFETCH THREAD
    // Track parser progress so prefetcher stays ahead
    std::atomic<const char*> parser_pos{data_start};
    std::atomic<bool> done{false};

    std::thread prefetcher([&]() {
        volatile char sink = 0;  // prevent optimization
        const char* prefetch_ptr = data_start;

        while (!done.load(std::memory_order_relaxed)) {
            // Stay PREFETCH_CHUNK ahead of parser
            const char* current_parser = parser_pos.load(std::memory_order_relaxed);
            const char* target = std::min(current_parser + PREFETCH_CHUNK, end);

            // Touch pages to trigger page faults ahead of parser
            while (prefetch_ptr < target) {
                sink += *prefetch_ptr;  // page fault
                prefetch_ptr += PAGE_SIZE;
            }

            // If we've prefetched far enough, yield CPU
            if (prefetch_ptr >= target) {
                std::this_thread::yield();
            }
        }
        (void)sink;  // suppress unused warning
    });

    const __m256i v_comma = _mm256_set1_epi8(format.delimiter);
    const __m256i v_newline = _mm256_set1_epi8('\n');
    const __m256i v_quote = _mm256_set1_epi8(format.quote);

    // std::vector<std::string_view> current_row;
    // current_row.reserve(col_num);
    auto current_row = new std::string_view[col_num];
    int col_idx = 0;

    const char* field_start = ptr;
    uint32_t in_quote = 0;

    // loop with step 32 bytes
    while (ptr + 32 <= end) {
        // load 32 bytes into register
        const __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));

        // find comma, new line, quote parallel
        const __m256i cmp_comma = _mm256_cmpeq_epi8(chunk, v_comma);
        const __m256i cmp_newline = _mm256_cmpeq_epi8(chunk, v_newline);
        const __m256i cmp_quote = _mm256_cmpeq_epi8(chunk, v_quote);

        // convert result to bitmask 32bit
        const uint32_t quote_mask = _mm256_movemask_epi8(cmp_quote);
        const uint32_t comma_mask = _mm256_movemask_epi8(cmp_comma);
        const uint32_t newline_mask    = _mm256_movemask_epi8(cmp_newline);

        uint32_t quote_solid_mask = prefix_xor(quote_mask);

        // if in_quote = 1 -> (0 - 1) = -1 = 0xFFFFFFFF -> XOR result is NOT mask
        // if in_quote = 0 -> (0 - 0) = 0  = 0x00000000 -> XOR result is mask (keep)
        quote_solid_mask ^= (0 - in_quote);

        // calculate in_quote for next loop
        // if number of quote is odd, XOR with 1
        // else XOR with 0
        in_quote ^= (_mm_popcnt_u32(quote_mask) & 1);

        const uint32_t valid_comma_mask = comma_mask & ~quote_solid_mask;
        const uint32_t valid_newline_mask    = newline_mask    & ~quote_solid_mask;

        // get comma outside solid mask (outside quotation)
        uint32_t valid_sep_mask = valid_comma_mask | valid_newline_mask;

        while (valid_sep_mask != 0) {
            const int offset = __builtin_ctz(valid_sep_mask);
            const char* found_pos = ptr +offset;
            current_row[col_idx] = trim_quotes(std::string_view(field_start, found_pos - field_start), format);
            col_idx++;

            // check current char is newline
            if ((valid_newline_mask >> offset) & 1) {
                callback(current_row);
                col_idx = 0;
            }
            field_start = found_pos + 1;

            // mark processed pos to 0
            valid_sep_mask &= ~(1u << offset);
        }

        ptr += 32;

        // Update parser position for prefetcher (every 64KB to reduce overhead)
        if ((reinterpret_cast<uintptr_t>(ptr) & 0xFFFF) == 0) {
            parser_pos.store(ptr, std::memory_order_relaxed);
        }
    }

    // remain bytes
    while (ptr < end) {
        if (char c = *ptr; c == format.quote) {
            in_quote = !in_quote;
        } else if (!in_quote) {
            if (c == format.delimiter || c == '\n') {
                current_row[col_idx] = trim_quotes(std::string_view(field_start, ptr - field_start), format);
                col_idx++;
                if (c == '\n') {
                    callback(current_row);
                    col_idx = 0;
                }
                field_start = ptr + 1;
            }
        }
        ptr++;
    }

    // Flush last line (if file doesn't end with newline)
    if (field_start < end) {
        current_row[col_idx] = trim_quotes(std::string_view(field_start, end - field_start), format);
        col_idx++;
    }
    if (col_idx > 0) {
        callback(current_row);
    }

    // Stop prefetcher thread
    done.store(true, std::memory_order_relaxed);
    prefetcher.join();

    delete[] current_row;
    delete f_map;
}

#endif //SIMDCSV_CSV_READER_H