#!/bin/bash
# Lynne C++ dependencies build script
# Usage: ./build-deps.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEPS_DIR="$SCRIPT_DIR/third_party"
DIST_DIR="$SCRIPT_DIR/dist"
BUILD_DIR="$DEPS_DIR/build"

echo "=== Lynne C++ Dependencies Build ==="
echo "Source: $DEPS_DIR"
echo "Output: $DIST_DIR"
echo ""

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure and build
echo "[1/3] Running CMake..."
cmake "$DEPS_DIR" -DCMAKE_BUILD_TYPE=Release

echo "[2/3] Building dependencies..."
cmake --build . -j$(nproc)

echo "[3/3] Installing to dist/..."
cmake --install .

# Cleanup build dir (optional, keep for incremental builds)
# rm -rf "$BUILD_DIR"

echo ""
echo "=== Done! ==="
echo "  include -> $DIST_DIR/include"
echo "  lib     -> $DIST_DIR/lib"
echo "  bin     -> $DIST_DIR/bin (if any)"
echo ""
echo "To compile the project:"
echo "  cd src && mkdir -p build && cd build"
echo "  cmake .. -DCMAKE_PREFIX_PATH=$DIST_DIR"
echo "  cmake --build ."