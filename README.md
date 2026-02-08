# SIMD CSV Reader

[![CI](https://github.com/lehoai/simdcsv/actions/workflows/ci.yml/badge.svg)](https://github.com/lehoai/simdcsv/actions/workflows/ci.yml)

A blazing-fast CSV parser using SIMD (AVX2) instructions. Up to **40% faster** than leading CSV libraries.

## Features

- **SIMD Parsing**: Uses AVX2 to process 32 bytes per iteration
- **Memory-Mapped I/O**: Zero-copy file reading with `mmap`
- **Prefetch Thread**: Overlapping I/O with parsing to minimize page fault latency
- **Header-only**: Just include and use

## Benchmark

Dataset: [Used Cars Dataset](https://www.kaggle.com/datasets/austinreese/craigslist-carstrucks-data) (1.2GB)

```
Time (mean ± σ):     654.6 ms ±  20.1 ms
Range (min … max):   632.3 ms … 689.5 ms    10 runs
```

**Throughput: ~1.8 GB/s**

## Usage

```cpp
#include "csv_reader.h"

int main() {
    csv::format format;
    format.delimiter = ',';
    format.quote = '"';
    
    csv::CsvReader::parse("data.csv", format, [](const std::string_view* row) {
        // row[0], row[1], ... are string_view of each column
        int id = csv::get<int>(row[0]);
        double value = csv::get<double>(row[1]);
    });
    
    return 0;
}
```

## Requirements

- C++17
- AVX2 support (Intel Haswell+ / AMD Zen+)
- Linux (uses mmap)

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## License

MIT
