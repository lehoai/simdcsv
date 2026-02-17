# Fetch GoogleTest
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Create test executable
add_executable(csv_tests test_csv_reader.cpp)

# Link with simdcsv library and GoogleTest
target_link_libraries(csv_tests
    PRIVATE
    simdcsv::simdcsv
    GTest::gtest_main
)

# Add optimization flags for tests
if(MSVC)
    target_compile_options(csv_tests PRIVATE /O2)
else()
    target_compile_options(csv_tests PRIVATE -O3)
endif()

# Discover and register tests
include(GoogleTest)
gtest_discover_tests(csv_tests)