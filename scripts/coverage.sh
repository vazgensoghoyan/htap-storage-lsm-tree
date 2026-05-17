#!/usr/bin/env bash

set -e

BUILD_DIR=build-coverage

echo "==> Configure"

CC=clang CXX=clang++ cmake \
    -B $BUILD_DIR \
    -DENABLE_COVERAGE=ON

echo "==> Build"

cmake --build $BUILD_DIR

cd $BUILD_DIR

echo "==> Run tests"

LLVM_PROFILE_FILE="coverage-%p.profraw" \
ctest --output-on-failure

echo "==> Merge coverage data"

llvm-profdata merge \
    -sparse coverage-*.profraw \
    -o coverage.profdata

echo "==> Coverage summary"

llvm-cov report \
    ./htap_storage_tests \
    -instr-profile=coverage.profdata

echo "==> Generate HTML report"

mkdir -p coverage-report

llvm-cov show \
    ./htap_storage_tests \
    -instr-profile=coverage.profdata \
    -format=html \
    -output-dir=coverage-report

echo "==> Coverage report generated"

echo "Open:"
echo "$BUILD_DIR/coverage-report/index.html"
