#!/usr/bin/env bash
# Install VACASK build dependencies for the current platform.
# On Linux, also builds Boost 1.88 from source (distro packages lack the
# process library) and installs toml++ headers.
# Usage: ci/install-deps.sh
set -euo pipefail

OS="$(uname -s)"

case "$OS" in
  Linux)
    echo "==> Installing packages (Linux)"
    sudo apt-get update
    sudo apt-get install -y \
      cmake ninja-build \
      bison flex libfl-dev \
      libsuitesparse-dev \
      llvm-dev libclang-dev clang \
      python3 python3-numpy python3-scipy

    # toml++ header-only library (not in Ubuntu repos)
    echo "==> Installing toml++"
    cd /tmp
    curl -L -o tomlplusplus.tar.gz \
      https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz
    tar xzf tomlplusplus.tar.gz
    sudo mkdir -p /usr/local/include
    sudo cp -r tomlplusplus-3.4.0/include/toml++ /usr/local/include/

    # Boost 1.88 from source (process library missing in distro packages)
    echo "==> Building Boost 1.88"
    cd /tmp
    curl -L -o boost_1_88_0.tar.gz \
      https://archives.boost.io/release/1.88.0/source/boost_1_88_0.tar.gz
    tar xzf boost_1_88_0.tar.gz
    cd boost_1_88_0
    cd tools/build
    ./bootstrap.sh gcc
    cd ../..
    tools/build/b2 -j"$(nproc)" --with-filesystem --with-process --with-asio \
      link=static toolset=gcc
    ;;

  Darwin)
    echo "==> Installing packages (macOS)"
    brew install \
      llvm@18 cmake ninja \
      bison flex \
      suite-sparse boost tomlplusplus \
      python3 numpy scipy
    ;;

  MINGW*|MSYS*)
    echo "==> Installing packages (Windows/MSYS2)"
    pacman -S --noconfirm --needed \
      git bison flex \
      mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
      mingw-w64-x86_64-boost mingw-w64-x86_64-suitesparse \
      mingw-w64-x86_64-tomlplusplus mingw-w64-x86_64-llvm mingw-w64-x86_64-clang \
      mingw-w64-x86_64-lld mingw-w64-x86_64-rust \
      mingw-w64-x86_64-python mingw-w64-x86_64-python-numpy \
      mingw-w64-x86_64-python-scipy
    ;;

  *)
    echo "ERROR: Unsupported platform: $OS" >&2
    exit 1
    ;;
esac

echo "==> Dependencies installed"
