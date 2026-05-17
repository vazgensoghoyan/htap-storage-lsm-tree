#!/usr/bin/env bash
set -e

BUILD_DIR=build

echo "==> Dev build"

cmake -B $BUILD_DIR \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTS=ON

cmake --build $BUILD_DIR -j$(nproc)

echo "==> Running tests"

cd $BUILD_DIR
ctest --output-on-failure
