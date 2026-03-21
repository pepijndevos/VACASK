#!/usr/bin/env bash
# Build OpenVAF-reloaded and install the binary to a cache directory.
# Skips the build if the binary already exists in the output directory.
#
# Usage: ci/build-openvaf.sh [output_dir]
#   output_dir: where to place openvaf-r binary (default: ./openvaf-cache)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE_DIR="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="${1:-$SOURCE_DIR/openvaf-cache}"
mkdir -p "$OUTPUT_DIR"

OS="$(uname -s)"

# Skip if already cached
case "$OS" in
  MINGW*|MSYS*) [ -f "$OUTPUT_DIR/openvaf-r.exe" ] && { echo "==> OpenVAF already cached"; exit 0; } ;;
  *)            [ -f "$OUTPUT_DIR/openvaf-r" ]     && { echo "==> OpenVAF already cached"; exit 0; } ;;
esac

echo "==> Building OpenVAF-reloaded"

# Install Rust if not present (Unix only; Windows MSYS2 has it via pacman)
if ! command -v cargo &>/dev/null; then
  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal
  # shellcheck source=/dev/null
  source ~/.cargo/env
fi

OPENVAF_SRC="/tmp/openvaf-src"
rm -rf "$OPENVAF_SRC"
git clone https://github.com/arpadbuermen/OpenVAF.git "$OPENVAF_SRC"
cd "$OPENVAF_SRC"

case "$OS" in
  Darwin)
    BREW_PREFIX="$(brew --prefix)"
    export PATH="$BREW_PREFIX/opt/llvm@18/bin:$PATH"
    export LLVM_SYS_180_PREFIX="$BREW_PREFIX/opt/llvm@18"
    ./configure
    ./build.sh --release
    ;;
  MINGW*|MSYS*)
    LLVM_VERSION=$(llvm-config --version | cut -d. -f1)
    echo "Detected LLVM version: $LLVM_VERSION"
    cargo build --release --features "llvm${LLVM_VERSION}" --bin openvaf-r
    ;;
  *)
    ./configure
    ./build.sh --release
    ;;
esac

# Install binary
case "$OS" in
  MINGW*|MSYS*)
    cp target/release/openvaf-r.exe "$OUTPUT_DIR/"
    ;;
  *)
    if [ -f target/release/openvaf-r ]; then
      cp target/release/openvaf-r "$OUTPUT_DIR/"
    elif [ -f target/release/openvaf ]; then
      cp target/release/openvaf "$OUTPUT_DIR/openvaf-r"
    else
      echo "ERROR: Could not find OpenVAF binary" >&2
      ls -la target/release/openvaf* 2>/dev/null || true
      exit 1
    fi
    chmod +x "$OUTPUT_DIR/openvaf-r"
    ;;
esac

echo "==> OpenVAF ready in $OUTPUT_DIR"
