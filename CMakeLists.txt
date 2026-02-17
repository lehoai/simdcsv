cmake_minimum_required(VERSION 3.14)
project(simdcsv VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# Create INTERFACE library (header-only library)
# ============================================================================
add_library(simdcsv INTERFACE)
add_library(simdcsv::simdcsv ALIAS simdcsv)

# Include directories for the library
target_include_directories(simdcsv INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Optimization flags for users of the library
target_compile_options(simdcsv INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-march=native>
)

target_compile_definitions(simdcsv INTERFACE
    $<$<CONFIG:Release>:NDEBUG>
)

# Link Threads for the library
find_package(Threads REQUIRED)
target_link_libraries(simdcsv INTERFACE Threads::Threads)

# ============================================================================
# Build tests only when this is the main project
# (not when fetched by another project)
# ============================================================================
# Check if this is the top-level project or a subproject
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(SIMDCSV_IS_TOP_LEVEL ON)
else()
    set(SIMDCSV_IS_TOP_LEVEL OFF)
endif()

# Build tests by default only when top-level
option(SIMDCSV_BUILD_TESTS "Build tests" ${SIMDCSV_IS_TOP_LEVEL})

if(SIMDCSV_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
