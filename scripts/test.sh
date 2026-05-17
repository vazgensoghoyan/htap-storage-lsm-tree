#!/usr/bin/env bash
set -e

BUILD_DIR=${BUILD_DIR:-build}

echo "==> Running tests"

cd $BUILD_DIR
ctest --output-on-failure
