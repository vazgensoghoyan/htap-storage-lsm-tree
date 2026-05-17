#!/usr/bin/env bash
set -e

echo "==> Cleaning build directories"

rm -rf build
rm -rf build-coverage
rm -rf CMakeCache.txt
rm -rf CMakeFiles

echo "==> Clean done"
