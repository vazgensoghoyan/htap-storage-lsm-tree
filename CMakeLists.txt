cmake_minimum_required(VERSION 3.20)

project(htap_storage LANGUAGES CXX)

# -----------------------------
# C++ standard
# -----------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------
# Options
# -----------------------------
option(ENABLE_WARNINGS "Enable compiler warnings" ON)
option(BUILD_TESTS "Build tests" ON)
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

# -----------------------------
# Collect sources automatically
# -----------------------------
file(GLOB_RECURSE HTAP_STORAGE_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# -----------------------------
# Main library
# -----------------------------
add_library(htap_storage OBJECT ${HTAP_STORAGE_SOURCES})

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
# Coverage
# -----------------------------
if (ENABLE_COVERAGE)
    message(STATUS "Coverage enabled")

    target_compile_options(htap_storage PRIVATE
        -O0
        -g
        -fprofile-instr-generate
        -fcoverage-mapping
    )

    target_link_options(htap_storage PRIVATE
        -fprofile-instr-generate
    )
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

    add_executable(htap_storage_tests
        ${TEST_SOURCES}
    )

    # Link object files from main library
    target_sources(htap_storage_tests PRIVATE
        $<TARGET_OBJECTS:htap_storage>
    )

    target_include_directories(htap_storage_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(htap_storage_tests
        PRIVATE
            GTest::gtest_main
    )

    # Warnings for tests
    if (ENABLE_WARNINGS)
        if (MSVC)
            target_compile_options(htap_storage_tests PRIVATE /W4 /permissive-)
        else()
            target_compile_options(htap_storage_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endif()

    # Coverage for tests
    if (ENABLE_COVERAGE)
        target_compile_options(htap_storage_tests PRIVATE
            -O0
            -g
            -fprofile-instr-generate
            -fcoverage-mapping
        )

        target_link_options(htap_storage_tests PRIVATE
            -fprofile-instr-generate
        )
    endif()

    include(GoogleTest)
    gtest_discover_tests(htap_storage_tests)
endif()
