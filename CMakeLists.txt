cmake_minimum_required(VERSION 4.1)
project(simdcsv)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")

include_directories(/home/lehoai/Desktop/code/csv-parser-master/single_include)

add_executable(simdcsv main.cpp
        mmap.h
        csv_reader.h)

# Link pthread for std::thread
find_package(Threads REQUIRED)
target_link_libraries(simdcsv Threads::Threads)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
endif()