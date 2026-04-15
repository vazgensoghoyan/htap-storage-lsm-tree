cmake_minimum_required(VERSION 3.20)

project(htap_storage LANGUAGES CXX)

# -----------------------------
# C++ standard
# -----------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------
# Options
# -----------------------------
option(ENABLE_WARNINGS "Enable compiler warnings" ON)
option(BUILD_TESTS "Build tests" ON)

# -----------------------------
# Collect sources automatically
# -----------------------------
file(GLOB_RECURSE HTAP_STORAGE_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(htap_storage ${HTAP_STORAGE_SOURCES})

target_include_directories(htap_storage
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# -----------------------------
# Compiler warnings
# -----------------------------
if (ENABLE_WARNINGS)
    if (MSVC)
        target_compile_options(htap_storage PRIVATE /W4 /permissive-)
    else()
        target_compile_options(htap_storage PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# -----------------------------
# Tests
# -----------------------------
if (BUILD_TESTS)
    enable_testing()

    include(FetchContent)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    file(GLOB_RECURSE TEST_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    add_executable(htap_storage_tests ${TEST_SOURCES})

    target_link_libraries(htap_storage_tests
        PRIVATE
            htap_storage
            GTest::gtest_main
    )

    if (ENABLE_WARNINGS)
        if (MSVC)
            target_compile_options(htap_storage_tests PRIVATE /W4 /permissive-)
        else()
            target_compile_options(htap_storage_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endif()

    include(GoogleTest)
    gtest_discover_tests(htap_storage_tests)
endif()
