#!/usr/bin/env bash
# Configure, build, and test VACASK.
#
# Usage: ci/build-vacask.sh [--no-test] [--openvaf-dir DIR]
#   --no-test       Skip running tests
#   --openvaf-dir   Directory containing openvaf-r (default: ./openvaf-cache)
#
# Prerequisites: run ci/install-deps.sh and ci/build-openvaf.sh first.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$SOURCE_DIR/build"
OPENVAF_DIR="$SOURCE_DIR/openvaf-cache"
RUN_TESTS=true

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-test)     RUN_TESTS=false; shift ;;
    --openvaf-dir) OPENVAF_DIR="$2"; shift 2 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

OS="$(uname -s)"
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# ── Configure ────────────────────────────────────────────────────────────
echo "==> Configuring VACASK"
CMAKE_ARGS=(-G Ninja -S "$SOURCE_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release)
CMAKE_ARGS+=("-DOPENVAF_DIR=$OPENVAF_DIR")

case "$OS" in
  Linux)
    if [ -d /tmp/boost_1_88_0/stage ]; then
      CMAKE_ARGS+=("-DBoost_ROOT=/tmp/boost_1_88_0/stage")
    fi
    CMAKE_ARGS+=("-DSuiteSparse_DIR=/usr")
    CMAKE_ARGS+=("-DTOMLPP_DIR=/usr/local")
    ;;
  Darwin)
    BREW_PREFIX="$(brew --prefix)"
    CMAKE_ARGS+=("-DFLEX_INCLUDE_DIR=$BREW_PREFIX/opt/flex/include")
    CMAKE_ARGS+=("-DBoost_USE_STATIC_LIBS=ON")
    ;;
  MINGW*|MSYS*)
    CMAKE_ARGS+=(
      "-DSuiteSparse_DIR=/mingw64"
      "-DTOMLPP_DIR=/mingw64"
      "-DBoost_ROOT=/mingw64"
      "-DBISON_EXECUTABLE=/usr/bin/bison"
      "-DFLEX_EXECUTABLE=/usr/bin/flex"
      "-DFLEX_INCLUDE_DIR=/usr/include"
    )
    ;;
  *)
    echo "ERROR: Unsupported platform: $OS" >&2
    exit 1
    ;;
esac

echo "cmake ${CMAKE_ARGS[*]}"
cmake "${CMAKE_ARGS[@]}"

# ── Build ────────────────────────────────────────────────────────────────
echo "==> Building VACASK (${NPROC} jobs)"
cmake --build "$BUILD_DIR" -j"$NPROC"

# ── Test ─────────────────────────────────────────────────────────────────
if [ "$RUN_TESTS" = true ]; then
  echo "==> Running tests"
  cd "$BUILD_DIR"
  ctest --output-on-failure --timeout 60
fi

echo "==> Build complete"
