#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=build-coverage

echo "=============================="
echo "  HTAP LSM COVERAGE BUILD"
echo "=============================="

echo "[1/5] Configuring project..."

rm -rf "$BUILD_DIR"

CC=clang CXX=clang++ cmake \
    -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTS=ON \
    -DENABLE_COVERAGE=ON

echo "[2/5] Building..."

cmake --build "$BUILD_DIR" -j"$(nproc)"

cd "$BUILD_DIR"

echo "[3/5] Running tests..."

rm -f coverage-*.profraw || true

LLVM_PROFILE_FILE="coverage-%p.profraw" \
ctest --output-on-failure

echo "[4/5] Merging coverage data..."

llvm-profdata merge \
    -sparse coverage-*.profraw \
    -o coverage.profdata

echo "[5/6] CLI report..."

llvm-cov report \
    ./htap_storage_tests \
    -instr-profile=coverage.profdata \
    -ignore-filename-regex=".*tests/.*|.*googletest.*|.*_deps/.*"

echo "[6/6] Generating HTML report..."

mkdir -p coverage-report

llvm-cov show \
    ./htap_storage_tests \
    -instr-profile=coverage.profdata \
    -format=html \
    -output-dir=coverage-report \
    -ignore-filename-regex=".*tests/.*|.*googletest.*|.*_deps/.*"

echo ""
echo "=============================="
echo "Coverage report ready!"
echo "Open:"
echo "$BUILD_DIR/coverage-report/index.html"
echo "=============================="
