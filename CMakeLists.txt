cmake_minimum_required(VERSION 3.14)
project(simdcsv)

set(CMAKE_CXX_STANDARD 17)

# Optimization flags
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")
else()
    # Use -march=native to auto-detect all CPU features (includes AVX2, BMI, etc.)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# Enable Link-Time Optimization for Release builds
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")

    # Nếu vẫn lỗi, hãy thử dùng cờ mạnh tay hơn:
    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

add_executable(simdcsv main.cpp
        mmap.h
        csv_reader.h)

# Link pthread for std::thread
find_package(Threads REQUIRED)
target_link_libraries(simdcsv Threads::Threads)

enable_testing()
add_subdirectory(test)