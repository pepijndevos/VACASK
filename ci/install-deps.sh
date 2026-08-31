#!/usr/bin/env bash
# Install VACASK build dependencies for the current platform.
# On Linux, also builds Boost 1.88 from source (distro packages lack the
# process library) and installs toml++ headers.
# Usage: ci/install-deps.sh
set -euo pipefail

OS="$(uname -s)"

# Use sudo only when not already root (Docker CI runs as root without sudo)
if [ "$(id -u)" -eq 0 ]; then SUDO=""; else SUDO="sudo"; fi

case "$OS" in
  Linux)
    if command -v dnf >/dev/null 2>&1; then
      # AlmaLinux 9 (manylinux_2_34): build portable binaries against glibc 2.34.
      # LLVM 21.1 is the default llvm-devel and matches openvaf-r's llvm21
      # feature; SuiteSparse 5.4.0 and bison 3.7 (>= the grammar's required 3.3)
      # come from the distro (CRB repo), so only Boost is built by hand.
      echo "==> Installing packages (Linux/dnf, AlmaLinux 9)"
      $SUDO dnf install -y epel-release
      $SUDO dnf config-manager --set-enabled crb
      $SUDO dnf install -y \
        curl git make \
        cmake ninja-build \
        bison flex flex-devel \
        suitesparse-devel \
        llvm-devel clang clang-devel
      # The simulator spawns "python3" from PATH at runtime (see libplatform.cpp)
      # to run test control blocks that import numpy/scipy. AlmaLinux's python3
      # has no pip in this image, so bootstrap it with ensurepip first.
      python3 -m ensurepip --upgrade
      python3 -m pip install --upgrade numpy scipy
      # Boost bootstrap uses gcc (the manylinux default compiler). clang is also
      # installed above, but only because openvaf-r's build requires it.
      BOOST_TOOLSET=gcc
    else
      echo "==> Installing packages (Linux/apt, Debian/Ubuntu)"
      $SUDO apt-get update
      $SUDO apt-get install -y \
        curl git \
        cmake ninja-build \
        bison flex libfl-dev \
        libsuitesparse-dev \
        llvm-dev libclang-dev clang \
        python3 python3-numpy python3-scipy
      BOOST_TOOLSET=clang
    fi

    # toml++ header-only library (not in distro repos)
    echo "==> Installing toml++"
    cd /tmp
    curl -L -o tomlplusplus.tar.gz \
      https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz
    tar xzf tomlplusplus.tar.gz
    $SUDO mkdir -p /usr/local/include
    $SUDO cp -r tomlplusplus-3.4.0/include/toml++ /usr/local/include/

    # Boost 1.88 from source (process library missing in distro packages)
    echo "==> Building Boost 1.88"
    cd /tmp
    curl -L -o boost_1_88_0.tar.gz \
      https://archives.boost.io/release/1.88.0/source/boost_1_88_0.tar.gz
    tar xzf boost_1_88_0.tar.gz
    cd boost_1_88_0
    cd tools/build
    ./bootstrap.sh "$BOOST_TOOLSET"
    cd ../..
    tools/build/b2 -j"$(nproc)" --with-filesystem --with-process --with-asio \
      link=static "toolset=$BOOST_TOOLSET"
    ;;

  Darwin)
    echo "==> Installing packages (macOS)"
    brew install \
      llvm@18 cmake ninja \
      bison flex \
      suite-sparse boost tomlplusplus \
      numpy scipy
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
