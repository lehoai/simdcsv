include(FetchContent)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

add_executable(csv_tests
        test_csv_reader.cpp
)

target_include_directories(csv_tests PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(csv_tests
        GTest::gtest_main
        Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(csv_tests)