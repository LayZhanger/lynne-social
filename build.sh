#!/bin/bash
# Lynne C++ build script
# Usage:
#   ./build.sh            # configure + build
#   ./build.sh --deps     # rebuild dependencies first, then configure + build
#   ./build.sh --test     # configure + build + run tests
#   ./build.sh --clean    # clean build dir + dist outputs, then configure + build
#   ./build.sh --all      # deps + clean + configure + build + test (full from scratch)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

# -------------------------------------------------------------------
# Parse flags
# -------------------------------------------------------------------
DO_DEPS=false
DO_TEST=false
DO_CLEAN=false

for arg in "$@"; do
    case "$arg" in
        --deps) DO_DEPS=true ;;
        --test) DO_TEST=true ;;
        --clean) DO_CLEAN=true ;;
        --all)
            DO_DEPS=true
            DO_TEST=true
            DO_CLEAN=true
            ;;
    esac
done

# -------------------------------------------------------------------
# Count total steps
# -------------------------------------------------------------------
TOTAL=2                           # configure + build (always)
$DO_DEPS  && TOTAL=$((TOTAL + 1)) # + deps
$DO_CLEAN && TOTAL=$((TOTAL + 1)) # + clean
$DO_TEST  && TOTAL=$((TOTAL + 1)) # + test

STEP=0

next_step() {
    STEP=$((STEP + 1))
    echo "=== [$STEP/$TOTAL] $* ==="
}

# -------------------------------------------------------------------
# 1. Dependencies (optional)
# -------------------------------------------------------------------
if $DO_DEPS; then
    next_step "Rebuilding dependencies (third_party → dist)"
    bash "$SCRIPT_DIR/build-deps.sh"
    echo ""
fi

# -------------------------------------------------------------------
# 2. Clean (optional)
# -------------------------------------------------------------------
if $DO_CLEAN; then
    next_step "Cleaning build directory and dist outputs"
    rm -rf "$BUILD_DIR"
    rm -f "$SCRIPT_DIR/dist/lib/liblynne_"*.a
    rm -f "$SCRIPT_DIR/dist/lib/libgtest"*.a "$SCRIPT_DIR/dist/lib/libgmock"*.a
    rm -f "$SCRIPT_DIR/dist/bin/test_"*
fi

# -------------------------------------------------------------------
# 3. Configure
# -------------------------------------------------------------------
next_step "Configuring CMake"
mkdir -p "$BUILD_DIR"
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR"
echo ""

# -------------------------------------------------------------------
# 4. Build
# -------------------------------------------------------------------
next_step "Building targets"
cmake --build "$BUILD_DIR" -j"$(nproc)"
echo ""

# -------------------------------------------------------------------
# 5. Test (optional)
# -------------------------------------------------------------------
if $DO_TEST; then
    next_step "Running tests"
    ctest --test-dir "$BUILD_DIR" --output-on-failure -j"$(nproc)"
    echo ""
fi

echo "=== Done ==="
