//
// Created by lehoai on 2/4/26.
//

#ifndef SIMDCSV_CSV_READER_H
#define SIMDCSV_CSV_READER_H

#include <string_view>
#include <immintrin.h>
#include <vector>
#include <thread>
#include <charconv>
#include <optional>
#include <mutex>
#include <condition_variable>

#include "mmap.h"

constexpr size_t BUFFER_SIZE = 128 * 1024;
constexpr size_t PREFETCH_CHUNK = 64 * 1024 * 1024;  // 64MB prefetch ahead
constexpr size_t PAGE_SIZE = 4096;
namespace csv {
    struct format {
        char delimiter = ',';
        char new_line = '\n';
        std::optional<char> quote;
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
        if (!format.quote.has_value()) {
            return sv;
        }
        if (sv.size() >= 2 && sv.front() == format.quote.value() && sv.back() == format.quote.value()) {
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


    class CsvReader {
    private:
        const char* file_path = nullptr;
        csv::format format;
        std::unique_ptr<csv::file::FMmap> f_map;
        const char* end = nullptr;
        int col_num = 0;
        const char* data_start = nullptr;
        std::vector<std::string> headers;
        inline void parse_header_row(const char* data);
    public:
        CsvReader(const char* file_path, csv::format format);
        template <typename RowCallback>
        void parse(const RowCallback &callback);

        std::vector<std::string> getHeaders() {
            return headers;
        }
    };
}

inline csv::CsvReader::CsvReader(const char *file_path, const csv::format format) {
    this->file_path = file_path;
    this->format = format;

    f_map = std::make_unique<csv::file::FMmap>(file_path);

    const char *data = f_map->data();
    const size_t size = f_map->size();
    this->end = data + size;

    // Auto-detect header and column count
    parse_header_row(data);
}

template <typename RowCallback>
void csv::CsvReader::parse(const RowCallback &callback) {
    const char* ptr = data_start; // start after header row

    // PREFETCH THREAD
    // Track parser progress so prefetcher stays ahead
    std::mutex prefetch_mtx;
    std::condition_variable prefetch_cv;
    bool advance_signal = false; // guarded by prefetch_mtx
    bool done = false; // guarded by prefetch_mtx

    std::thread prefetcher([&]() {
        volatile char sink = 0;  // prevent optimization
        const char* prefetch_ptr = data_start;
        const char* local_target = std::min(data_start + PREFETCH_CHUNK, end);

        while (true) {
            // Touch pages to trigger page faults ahead of parser
            while (prefetch_ptr < local_target) {
                sink += *prefetch_ptr;  // page fault
                prefetch_ptr += PAGE_SIZE;
            }

            if (prefetch_ptr >= end) break;

            // sleep
            {
                std::unique_lock<std::mutex> lock(prefetch_mtx);
                prefetch_cv.wait(lock, [&] {
                    return advance_signal || done;
                });

                if (done) break;
                advance_signal = false;
            }

            local_target = std::min(prefetch_ptr + PREFETCH_CHUNK, end);
        }
        (void)sink;  // suppress unused warning
    });

    const __m256i v_comma = _mm256_set1_epi8(format.delimiter);
    const __m256i v_newline = _mm256_set1_epi8(format.new_line);
    const __m256i v_quote = format.quote.has_value() ? _mm256_set1_epi8(format.quote.value()) : _mm256_setzero_si256();

    // std::vector<std::string_view> current_row;
    // current_row.reserve(col_num);
    auto current_row = std::make_unique<std::string_view[]>(col_num);
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

        uint32_t quote_mask = 0;
        uint32_t quote_solid_mask = 0;

        if (format.quote.has_value()) {
            const __m256i cmp_quote = _mm256_cmpeq_epi8(chunk, v_quote);
            quote_mask = _mm256_movemask_epi8(cmp_quote);

            // if in_quote = 1 -> (0 - 1) = -1 = 0xFFFFFFFF -> XOR result is NOT mask
            // if in_quote = 0 -> (0 - 0) = 0  = 0x00000000 -> XOR result is mask (keep)
            quote_solid_mask = prefix_xor(quote_mask);
            quote_solid_mask ^= (0 - in_quote);
            // calculate in_quote for next loop
            // if number of quote is odd, XOR with 1
            // else XOR with 0
            in_quote ^= (_mm_popcnt_u32(quote_mask) & 1);
        }

        // convert result to bitmask 32bit
        const uint32_t comma_mask = _mm256_movemask_epi8(cmp_comma);
        const uint32_t newline_mask = _mm256_movemask_epi8(cmp_newline);

        const uint32_t valid_comma_mask = comma_mask & ~quote_solid_mask;
        const uint32_t valid_newline_mask = newline_mask & ~quote_solid_mask;

        // get comma outside solid mask (outside quotation)
        uint32_t valid_sep_mask = valid_comma_mask | valid_newline_mask;

        while (valid_sep_mask != 0) {
            const int offset = __builtin_ctz(valid_sep_mask);
            const char* found_pos = ptr +offset;
            
            if (col_idx < col_num) {
                current_row[col_idx] = trim_quotes(std::string_view(field_start, found_pos - field_start), format);
            }
            col_idx++;

            // check current char is newline
            if ((valid_newline_mask >> offset) & 1) {
                // Lazy clear: only clear unfilled fields if row has fewer columns
                if (col_idx < col_num) {
                    for (int i = col_idx; i < col_num; i++) {
                        current_row[i] = std::string_view();
                    }
                }
                callback(current_row.get());
                col_idx = 0;
            }
            field_start = found_pos + 1;

            // mark processed pos to 0
            valid_sep_mask &= ~(1u << offset);
        }

        ptr += 32;

        // Update parser position for prefetcher (every 64KB to reduce overhead)
        if ((reinterpret_cast<uintptr_t>(ptr) & 0xFFFF) == 0) {
            {
                std::lock_guard<std::mutex> lock(prefetch_mtx);
                advance_signal = true;
            }
            prefetch_cv.notify_one(); // wakeup prefetcher
        }
    }

    // remain bytes
    while (ptr < end) {
        char c = *ptr;
        if (format.quote.has_value() && c == format.quote.value()) {
            in_quote = !in_quote;
        } else if (!in_quote) {
            if (c == format.delimiter || c == format.new_line) {
                if (col_idx < col_num) {
                    current_row[col_idx] = trim_quotes(std::string_view(field_start, ptr - field_start), format);
                }
                col_idx++;
                if (c == format.new_line) {
                    // Lazy clear: only clear unfilled fields if row has fewer columns
                    if (col_idx < col_num) {
                        for (int i = col_idx; i < col_num; i++) {
                            current_row[i] = std::string_view();
                        }
                    }
                    callback(current_row.get());
                    col_idx = 0;
                }
                field_start = ptr + 1;
            }
        }
        ptr++;
    }

    // Flush last line (if file doesn't end with newline)
    if (field_start < end) {
        if (col_idx < col_num) {
            current_row[col_idx] = trim_quotes(std::string_view(field_start, end - field_start), format);
        }
        col_idx++;
    }
    if (col_idx > 0) {
        // Lazy clear: only clear unfilled fields if last row has fewer columns
        if (col_idx < col_num) {
            for (int i = col_idx; i < col_num; i++) {
                current_row[i] = std::string_view();
            }
        }
        callback(current_row.get());
    }

    // Stop prefetcher thread
    {
        std::lock_guard lock(prefetch_mtx);
        done = true;
    }
    prefetch_cv.notify_one();
    prefetcher.join();
}

// Parse header row and return: (col_count, headers, pointer after header line)
void csv::CsvReader::parse_header_row(const char* data) {
    const char* ptr = data;
    const char* field_start = data;
    bool in_quote = false;
    int header_row_idx = 0;


    while (ptr < end) {
        const char c = *ptr;
        if (format.quote.has_value() && c == format.quote.value()) {
            in_quote = !in_quote;
        } else if (!in_quote) {
            if (c == format.delimiter || c == format.new_line) {
                if (header_row_idx == format.header_row) {
                    std::string header {trim_quotes(std::string_view(field_start, ptr - field_start), format)};
                    headers.push_back(header);
                }
                field_start = ptr + 1;
                if (c == format.new_line) {
                    if (header_row_idx == format.header_row) {
                        this->col_num = static_cast<int>(headers.size());
                        this->data_start = ptr + 1;
                        return;
                    }
                    header_row_idx++;
                }
            }
        }
        ptr++;
    }

    // Handle last field if no trailing newline
    if (field_start < end) {
        std::string header {trim_quotes(std::string_view(field_start, end - field_start), format)};
        headers.push_back(header);
    }
    this->col_num = static_cast<int>(headers.size());
    this->data_start = end;
}

#endif //SIMDCSV_CSV_READER_H