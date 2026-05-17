#!/usr/bin/env bash
set -e

BUILD_DIR=build-coverage

echo "==> Configure (coverage)"

CC=clang CXX=clang++ cmake \
    -B $BUILD_DIR \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTS=ON \
    -DENABLE_COVERAGE=ON

echo "==> Build"

cmake --build $BUILD_DIR -j$(nproc)

cd $BUILD_DIR

echo "==> Run tests with coverage"

LLVM_PROFILE_FILE="coverage-%p.profraw" \
ctest --output-on-failure

echo "==> Merge coverage"

llvm-profdata merge \
    -sparse coverage-*.profraw \
    -o coverage.profdata

echo "==> Report"

llvm-cov report \
    ./htap_storage_tests \
    -instr-profile=coverage.profdata \
    -ignore-filename-regex="googletest|/usr/"

echo "==> HTML"

mkdir -p coverage-report

llvm-cov show \
    ./htap_storage_tests \
    -instr-profile=coverage.profdata \
    -format=html \
    -output-dir=coverage-report

echo "==> Open:"
echo "$BUILD_DIR/coverage-report/index.html"
