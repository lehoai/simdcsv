//
// Created by lehoai on 2/4/26.
//

#ifndef SIMDCSV_MMAP_H
#define SIMDCSV_MMAP_H

// mmap for linux
// use native C api to avoid double buffer
// destructor use for release resource, ensure munmap and close are called
//

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdexcept>
#include <unistd.h>

namespace csv::file {

    class FMmap {
    private:
        char* _data = nullptr;
        size_t _size = 0;
        int fd = -1;
    public:
        explicit FMmap(const char* file_path);
        ~FMmap();
        [[nodiscard]] const char* data() const { return _data; }
        [[nodiscard]] size_t size() const { return _size; }
    };
}

inline csv::file::FMmap::FMmap(const char *file_path) {
    // open file
    fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("Cannot open file");
    }

    // get filesize
    struct stat st{};
    if (fstat(fd, &st) == -1) {
        close(fd);
        throw std::runtime_error("Cannot get filesize");
    }
    _size = st.st_size;

    // native mmap
    _data = static_cast<char *>(mmap(nullptr, _size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (_data == MAP_FAILED) {
        close(fd);
        throw std::runtime_error("Cannot map file");
    }

    // suggest kernel read contiguous, prefetch
    madvise(_data, _size, MADV_SEQUENTIAL | MADV_HUGEPAGE); // TODO: MADV_WILLNEED?
}

inline csv::file::FMmap::~FMmap() {
    if (_data && _data!= MAP_FAILED) munmap(_data, _size);
    if (fd != -1) close(fd);
}

#endif //SIMDCSV_MMAP_H