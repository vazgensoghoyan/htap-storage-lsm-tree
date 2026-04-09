cmake_minimum_required(VERSION 3.20)

project(htap_storage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_library(htap_storage
    src/storage/model/schema.cpp
    src/storage/model/row.cpp

    src/storage/mock/mock_cursor.cpp
    src/storage/mock/mock_storage_engine.cpp
)

target_include_directories(htap_storage
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

add_executable(htap_storage_tests
    tests/storage/model/schema_tests.cpp
    tests/storage/model/row_tests.cpp

    tests/storage/mock/mock_cursor_tests.cpp
    tests/storage/mock/mock_storage_engine_tests.cpp
)

target_link_libraries(htap_storage_tests
    PRIVATE
        htap_storage
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(htap_storage_tests)
