#!/bin/bash
#
# Author  : Sylvain Deguire (VA2OPS)
# Date    : May 2026
# Purpose : Build QtDashboard.
#

set -e

cd "$(dirname "$0")"

echo "Running qmake..."
qmake

echo "Building..."
make -j$(nproc)

echo "Done. Binary: $(pwd)/QtDashboard"
