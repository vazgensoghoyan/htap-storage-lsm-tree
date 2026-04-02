cmake_minimum_required(VERSION 3.20)

project(htap_storage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_library(htap_parser
    src/parser/lexer.cpp
)

target_include_directories(htap_parser
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

add_executable(htap_parser_tests
    tests/parser/lexer_tests.cpp
)

target_link_libraries(htap_parser_tests
    PRIVATE
        htap_parser
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(htap_parser_tests)