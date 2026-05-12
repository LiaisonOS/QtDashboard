#!/bin/bash
#
# Author  : Sylvain Deguire (VA2OPS)
# Date    : May 2026
# Purpose : Out-of-source build for QtDashboard
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/../QtDashboard-build"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
qmake "$SCRIPT_DIR/QtDashboard.pro"
make -j$(nproc)

echo "Binary: $BUILD_DIR/QtDashboard"
