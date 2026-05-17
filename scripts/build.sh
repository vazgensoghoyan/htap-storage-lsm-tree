#!/usr/bin/env bash
set -e

BUILD_DIR=${BUILD_DIR:-build}

echo "==> Configuring project"

cmake -B $BUILD_DIR \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTS=ON

echo "==> Building"

cmake --build $BUILD_DIR -j$(nproc)

echo "==> Done"
