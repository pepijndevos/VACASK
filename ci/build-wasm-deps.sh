#!/usr/bin/env bash
# Build the native VACASK dependencies as static, position-independent
# WebAssembly libraries for the Pyodide / Emscripten platform.
#
# Pyodide's own recipes (libsuitesparse, libopenblas, libboost) are shared
# libraries that exist only inside the Pyodide distribution, so a wheel
# uploaded to PyPI cannot depend on them (PEP 783: dependencies must be linked
# statically or bundled in the wheel). libsuitesparse also drops KLU and BTF.
# So the libraries are built here, from source, and linked into the extension.
#
# Boost is not needed: it is only used by lib/processutils.cpp (process
# spawning), which does not exist on wasm.
#
# Requires an activated emsdk (emcc on PATH).
# Usage: ci/build-wasm-deps.sh <prefix> [work-dir]
set -euo pipefail

PREFIX="$(mkdir -p "${1:?usage: $0 <prefix> [work-dir]}" && cd "$1" && pwd)"
WORK="${2:-$(mktemp -d)}"
mkdir -p "$WORK"
WORK="$(cd "$WORK" && pwd)"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu)}"

SUITESPARSE_VERSION=5.13.0
FFTW_VERSION=3.3.10

command -v emcc >/dev/null || { echo "emcc not found: activate emsdk first" >&2; exit 1; }

# -fPIC because everything ends up inside a shared (side) module.
CFLAGS_WASM="-O2 -fPIC"

fetch() { # url file
  [ -f "$WORK/$2" ] || curl -fsSL -o "$WORK/$2" "$1"
}

mkdir -p "$PREFIX/lib" "$PREFIX/include"

# ---------------------------------------------------------------- SuiteSparse
# Only what VACASK links: config, AMD, CAMD, COLAMD, CCOLAMD, BTF, KLU, CHOLMOD
# (klu_cholmod needs CHOLMOD). CHOLMOD is built without the supernodal and
# partition modules, so it needs neither BLAS/LAPACK nor METIS.
if [ ! -f "$PREFIX/lib/libklu.a" ]; then
  echo "==> SuiteSparse $SUITESPARSE_VERSION"
  fetch "https://github.com/DrTimothyAldenDavis/SuiteSparse/archive/refs/tags/v${SUITESPARSE_VERSION}.tar.gz" suitesparse.tar.gz
  rm -rf "$WORK/SuiteSparse-$SUITESPARSE_VERSION"
  tar xzf "$WORK/suitesparse.tar.gz" -C "$WORK"
  cd "$WORK/SuiteSparse-$SUITESPARSE_VERSION"
  # No `ranlib`/`ar` from the host: use the emscripten wrappers.
  MKFLAGS=(
    CC=emcc CXX=em++ AR=emar RANLIB=emranlib
    CF="$CFLAGS_WASM" CFLAGS="$CFLAGS_WASM"
    CUDA=no GPU_CONFIG= BLAS= LAPACK=
    CHOLMOD_CONFIG="-DNPARTITION -DNSUPERNODAL"
    JOBS="$JOBS"
  )
  for lib in SuiteSparse_config AMD CAMD COLAMD CCOLAMD BTF CHOLMOD KLU; do
    make -C "$lib" "${MKFLAGS[@]}" static
  done
  cp SuiteSparse_config/*.a AMD/Lib/*.a CAMD/Lib/*.a COLAMD/Lib/*.a \
     CCOLAMD/Lib/*.a BTF/Lib/*.a CHOLMOD/Lib/*.a KLU/Lib/*.a "$PREFIX/lib/"
  # Under include/suitesparse/ like the distro packages (include/solklu.h
  # includes <suitesparse/klu.h>).
  mkdir -p "$PREFIX/include/suitesparse"
  cp SuiteSparse_config/SuiteSparse_config.h \
     AMD/Include/amd.h CAMD/Include/camd.h COLAMD/Include/colamd.h \
     CCOLAMD/Include/ccolamd.h BTF/Include/btf.h KLU/Include/klu.h \
     CHOLMOD/Include/cholmod*.h "$PREFIX/include/suitesparse/"
fi

# ----------------------------------------------------------------------- FFTW
# Double precision only (VACASK uses fftw3 double). No SIMD, no threads.
if [ ! -f "$PREFIX/lib/libfftw3.a" ]; then
  echo "==> FFTW $FFTW_VERSION"
  fetch "https://www.fftw.org/fftw-${FFTW_VERSION}.tar.gz" fftw.tar.gz
  rm -rf "$WORK/fftw-$FFTW_VERSION"
  tar xzf "$WORK/fftw.tar.gz" -C "$WORK"
  mkdir -p "$WORK/fftw-build" && cd "$WORK/fftw-build"
  rm -rf ./*
  emconfigure "$WORK/fftw-$FFTW_VERSION/configure" \
    --prefix="$PREFIX" \
    --enable-static --disable-shared --with-pic \
    --disable-fortran --disable-doc \
    CFLAGS="$CFLAGS_WASM"
  emmake make -j"$JOBS"
  emmake make install
fi

# ------------------------------------------------------------------- OpenBLAS
# Pure-C build (no Fortran compiler for wasm): generic C kernels
# (TARGET=RISCV64_GENERIC, the usual trick for wasm) and the f2c-translated
# LAPACK shipped in the OpenBLAS tarball (NOFORTRAN=1). Single-threaded.
# As in Pyodide's libopenblas recipe, all subroutines return int instead of
# void (see below); include/blaslapack.h declares them that way on Emscripten.
# Complex-valued functions such as zdotc_ still return by value.
OPENBLAS_VERSION=0.3.31
if [ ! -f "$PREFIX/lib/libopenblas.a" ]; then
  echo "==> OpenBLAS $OPENBLAS_VERSION"
  fetch "https://github.com/OpenMathLib/OpenBLAS/releases/download/v${OPENBLAS_VERSION}/OpenBLAS-${OPENBLAS_VERSION}.tar.gz" openblas.tar.gz
  rm -rf "$WORK/OpenBLAS-$OPENBLAS_VERSION"
  tar xzf "$WORK/openblas.tar.gz" -C "$WORK"
  cd "$WORK/OpenBLAS-$OPENBLAS_VERSION"
  # WebAssembly needs caller and callee signatures to agree exactly, and the
  # f2c-translated LAPACK in the tarball is inconsistent about `void` vs `int`
  # for its subroutines (callers use int prototypes). As in Pyodide's
  # libopenblas recipe, make every subroutine return int (callers then match,
  # and include/blaslapack.h declares them that way on Emscripten).
  sed -i.bak -E 's/void(\s+)BLASFUNC/int\1BLASFUNC/g' common_interface.h
  sed -i.bak -E 's/void(\s+)cblas_/int\1cblas_/g' cblas.h
  sed -i.bak -E 's/void(\s+)(C?NAME)/int\1\2/g' interface/*.c
  # (TESTING/MATGEN is part of libopenblas too and calls these subroutines.)
  sed -i.bak -E 's/((extern)?.+) void ([a-z0-9]+_)/\1\2 int \3/g' \
    lapack-netlib/SRC/*.c lapack-netlib/SRC/DEPRECATED/*.c lapack-netlib/TESTING/MATGEN/*.c
  sed -i.bak -E 's/^void (LAED4|LACPY|LASET)\(/int \1(/g' lapack/laed3/*.c
  # f2c complex-valued functions really return void (result via pointer).
  sed -i.bak -E 's@int ([cz](dotc|dotu|ladiv))@void \1@g' \
    lapack-netlib/SRC/*.c lapack-netlib/SRC/DEPRECATED/*.c lapack-netlib/TESTING/MATGEN/*.c
  # -march/-mabi added for RISCV64_GENERIC are rejected by emscripten.
  sed -i.bak 's@ifeq ($(CORE), RISCV64_GENERIC)@ifeq ($(CORE), NOT_RISCV64_GENERIC)@g' Makefile.riscv64
  sed -i.bak 's@ifeq ($(TARGET), RISCV64_GENERIC)@ifeq ($(TARGET), NOT_RISCV64_GENERIC)@g' Makefile.prebuild
  # `libs` is the BLAS/OpenBLAS core, `netlib` the f2c-translated LAPACK. Run
  # them as separate invocations: together under -j they append to the same
  # archive concurrently and members (laswp_plus, ...) go missing.
  OPENBLAS_FLAGS=(
    BINARY=32 CC="emcc -msimd128 -fPIC" AR=emar RANLIB=emranlib HOSTCC=gcc
    TARGET=RISCV64_GENERIC NOFORTRAN=1 NO_LAPACKE=1 USE_THREAD=0 NO_SHARED=1
    # The int-returning subroutines above do not return a value.
    COMMON_OPT="-O2 -Wno-return-type"
  )
  emmake make -j"$JOBS" libs "${OPENBLAS_FLAGS[@]}"
  emmake make -j"$JOBS" netlib "${OPENBLAS_FLAGS[@]}"
  emmake make install PREFIX="$PREFIX" "${OPENBLAS_FLAGS[@]}"
fi

# Slim archive with only the BLAS/LAPACK routines VACASK calls (the list must
# match include/blaslapack.h and the openblas_* calls in lib/libplatform.cpp).
# pyodide-build links extensions with -sSIDE_MODULE=1, which pulls in EVERY
# member of a static archive: the full libopenblas.a would add ~7 MB of unused
# LAPACK to the wheel and trip over signature inconsistencies in routines that
# are never called. A stub object takes the address of each routine, so that a
# relocatable link of stub + archive keeps exactly the objects they need.
if [ ! -f "$PREFIX/lib/libvacask_blas.a" ]; then
  echo "==> slim BLAS/LAPACK archive"
  BLAS_SYMBOLS="dcopy_ zcopy_ dnrm2_ dznrm2_ ddot_ zdotc_ daxpy_ zaxpy_ dswap_ zswap_
    dscal_ zscal_ zdscal_ idamax_ dlaset_ zlaset_ dlacpy_ zlacpy_ dlange_ zlange_
    dgetrf_ zgetrf_ dgetrs_ zgetrs_ openblas_set_num_threads openblas_get_num_threads"
  {
    for sym in $BLAS_SYMBOLS; do echo "extern void $sym(void);"; done
    echo "void* const vacask_blas_keep[] = {"
    for sym in $BLAS_SYMBOLS; do echo "  (void*)$sym,"; done
    echo "};"
  } > "$WORK/vacask_blas_keep.c"
  emcc $CFLAGS_WASM -c "$WORK/vacask_blas_keep.c" -o "$WORK/vacask_blas_keep.o"
  emcc -r -o "$WORK/vacask_blas.o" "$WORK/vacask_blas_keep.o" "$PREFIX/lib/libopenblas.a"
  rm -f "$PREFIX/lib/libvacask_blas.a"
  emar rcs "$PREFIX/lib/libvacask_blas.a" "$WORK/vacask_blas.o"
fi

# --------------------------------------------------------------------- toml++
# Header-only.
TOMLPP_VERSION=3.4.0
if [ ! -d "$PREFIX/include/toml++" ]; then
  echo "==> toml++ $TOMLPP_VERSION"
  fetch "https://github.com/marzer/tomlplusplus/archive/refs/tags/v${TOMLPP_VERSION}.tar.gz" tomlplusplus.tar.gz
  tar xzf "$WORK/tomlplusplus.tar.gz" -C "$WORK"
  cp -r "$WORK/tomlplusplus-$TOMLPP_VERSION/include/toml++" "$PREFIX/include/"
fi

echo "==> wasm dependencies installed in $PREFIX"
ls "$PREFIX/lib"
