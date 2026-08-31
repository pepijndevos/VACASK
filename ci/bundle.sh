#!/usr/bin/env bash
# Bundle shared libraries so VACASK binaries are self-contained.
#
# Usage: ci/bundle.sh [--openvaf-dir DIR] [--build-dir DIR]
#   --openvaf-dir  Directory containing openvaf-r (default: ./openvaf-cache)
#   --build-dir    Build directory (default: ./build)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$SOURCE_DIR/build"
OPENVAF_DIR="$SOURCE_DIR/openvaf-cache"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --openvaf-dir) OPENVAF_DIR="$2"; shift 2 ;;
    --build-dir)   BUILD_DIR="$2"; shift 2 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

OS="$(uname -s)"
BINDIR="$BUILD_DIR/simulator"

# Use sudo only when not already root (Docker CI runs as root without sudo)
if [ "$(id -u)" -eq 0 ]; then SUDO=""; else SUDO="sudo"; fi

case "$OS" in
  Linux)
    echo "==> Bundling shared libraries (Linux)"
    if command -v dnf >/dev/null 2>&1; then
      $SUDO dnf install -y epel-release
      $SUDO dnf install -y patchelf
    else
      $SUDO apt-get install -y patchelf
    fi
    LIBDIR="$BINDIR/lib"
    mkdir -p "$LIBDIR"

    bundle_linux() {
      local bin="$1"
      ldd "$bin" | awk '/=> \// {print $3}' | while read -r lib; do
        # Skip the glibc components (tied to the host kernel/loader); we build
        # against glibc 2.34 so they are satisfied on the target. Everything
        # else — including libstdc++/libgcc and libLLVM — is bundled so the
        # binaries are self-contained.
        case "$(basename "$lib")" in
          libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*) continue ;;
          ld-linux*) continue ;;
        esac
        cp -Ln "$lib" "$LIBDIR/" 2>/dev/null || true
        patchelf --set-rpath '$ORIGIN' "$LIBDIR/$(basename "$lib")"
      done
      patchelf --set-rpath '$ORIGIN/lib' "$bin"
    }

    cp "$OPENVAF_DIR/openvaf-r" "$BINDIR/"
    bundle_linux "$BINDIR/vacask"
    bundle_linux "$BINDIR/openvaf-r"
    echo "Bundled libraries:"
    ls -la "$LIBDIR"
    ;;

  Darwin)
    echo "==> Bundling shared libraries (macOS)"
    brew install dylibbundler
    cp "$OPENVAF_DIR/openvaf-r" "$BINDIR/"
    dylibbundler -od -b -x "$BINDIR/vacask" \
      -d "$BINDIR/lib/" -p @executable_path/lib/
    dylibbundler -b -x "$BINDIR/openvaf-r" \
      -d "$BINDIR/lib/" -p @executable_path/lib/
    echo "Bundled libraries:"
    ls -la "$BINDIR/lib/"
    ;;

  MINGW*|MSYS*)
    echo "==> Bundling DLLs (Windows)"
    cp "$OPENVAF_DIR/openvaf-r.exe" "$BINDIR/"

    for bin in "$BINDIR"/vacask.exe "$BINDIR"/openvaf-r.exe; do
      ldd "$bin" | awk '/mingw64|clang|ucrt64/ {print $3}' | while read -r lib; do
        cp -n "$lib" "$BINDIR/" 2>/dev/null || true
      done
    done

    # openvaf-r delay-loads LLVM (not visible to ldd)
    LLVM_DLL=$(find /mingw64/bin -name "libLLVM*.dll" 2>/dev/null | head -1)
    if [ -n "$LLVM_DLL" ]; then
      cp "$LLVM_DLL" "$BINDIR/"
    fi

    echo "Bundled DLLs:"
    ls "$BINDIR"/*.dll 2>/dev/null || echo "(none — statically linked)"
    ;;

  *)
    echo "ERROR: Unsupported platform: $OS" >&2
    exit 1
    ;;
esac

echo "==> Bundling complete"
