cmake_minimum_required(VERSION 3.18)

project(sim)

set(CMAKE_CXX_STANDARD 20)
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

enable_testing()

# --- Rust netlist parser (optional; netlist-cxx via Corrosion) --------------
# SPICE/Spectre netlist support is optional. When OFF (default), no Rust
# toolchain is needed and foreign-format includes report a clean error; when
# ON, the Rust parser is fetched and built with cargo and wired into the
# vacask binary. Flip the default below to change the out-of-the-box behavior.
option(CADNIP_PARSERS "Build with Cadnip parsers, requires a Rust toolchain)" OFF)

if(CADNIP_PARSERS)
    find_program(CARGO_EXECUTABLE cargo)
    if(NOT CARGO_EXECUTABLE)
        message(FATAL_ERROR
            "CADNIP_PARSERS=ON requires a Rust toolchain (cargo) on PATH. "
            "Install Rust (https://rustup.rs) or reconfigure with -DCADNIP_PARSERS=OFF.")
    endif()

    include(FetchContent)
    FetchContent_Declare(
        Corrosion
        GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
        GIT_TAG v0.5.1
    )
    FetchContent_MakeAvailable(Corrosion)

    # By default the parser is fetched at a known-compatible revision. Pass
    # -DNETLIST_RS_DIR=/path/to/NetlistParse.rs to use a local checkout.
    if(NETLIST_RS_DIR)
        set(_netlistrs_src "${NETLIST_RS_DIR}")
        message(STATUS "netlist parser: using local checkout ${_netlistrs_src}")
    else()
        FetchContent_Declare(
            NetlistParseRs
            GIT_REPOSITORY https://github.com/NyanCAD/NetlistParse.rs.git
            GIT_TAG 78c1737a7f4a9239ab445ed491aedc622c0fab53
        )
        FetchContent_MakeAvailable(NetlistParseRs)
        set(_netlistrs_src "${netlistparsers_SOURCE_DIR}")
        message(STATUS "netlist parser: fetched ${_netlistrs_src}")
    endif()
    if(NOT EXISTS "${_netlistrs_src}/crates/netlist-cxx/Cargo.toml")
        message(FATAL_ERROR "netlist-cxx not found under ${_netlistrs_src} "
                            "(expected crates/netlist-cxx/Cargo.toml).")
    endif()

    corrosion_import_crate(MANIFEST_PATH "${_netlistrs_src}/crates/netlist-cxx/Cargo.toml")
    corrosion_add_cxxbridge(netlist_cxx_bridge
        CRATE netlist_cxx
        FILES lib.rs
    )
    message(STATUS "Building with Cadnip parsers support (Spectre/SPICE netlist parsers)")
else()
    message(STATUS "CADNIP_PARSERS=OFF: building without Cadnip parsers support (no Rust toolchain needed)")
endif()

message(STATUS "CMake version ${CMAKE_VERSION}")
set(PROGRAM_NAME "vacask")
set(PROGRAM_COPYRIGHT "2023-2026")
set(PROGRAM_VENDOR "EDA Lab FE Uni-Lj")
set(PROGRAM_HOMEPAGE "https://codeberg.org/arpadbuermen/VACASK")

# Get version (versions in git are marked with tags of the form _<version number>)
# set(PROGRAM_VERSION "0.2")
execute_process(
    COMMAND git describe --tags --dirty --broken --match "_*"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
    RESULT_VARIABLE GIT_DESCRIBE_ERROR_CODE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (NOT GIT_DESCRIBE_ERROR_CODE)
    # Zero return code (OK), strip leading underscore from tag
    string(SUBSTRING "${GIT_DESCRIBE_VERSION}" 1 -1 PROGRAM_VERSION)
else()
    # No matching tag found, fall back to short git hash
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_SHORT_HASH
        RESULT_VARIABLE GIT_HASH_ERROR_CODE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (NOT GIT_HASH_ERROR_CODE)
        set(PROGRAM_VERSION "${GIT_SHORT_HASH}")
    else()
        message(STATUS "Failed to retrieve version from git")
        set(PROGRAM_VERSION "unknown")
    endif()
endif()
message(STATUS "Program version: ${PROGRAM_VERSION}")

# Platform
set(Boost_USE_STATIC_LIBS ON)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    # Windows
    set(SIM_PLATFORM "Windows")
    set(SIM_ARCH "x86_64")
    add_compile_definitions("SIMWINDOWS")
    set(PROGRAM_LIB_DIR "lib")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    # macOS with Homebrew\
    set(SIM_PLATFORM "Darwin")
    set(SIM_ARCH "arm64")
    add_compile_definitions("SIMMACOS")
    execute_process(COMMAND brew --prefix OUTPUT_VARIABLE BREW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(FLEX_EXECUTABLE "${BREW_PREFIX}/opt/flex/bin/flex")
    set(FLEX_INCLUDE_DIRS "${BREW_PREFIX}/opt/flex/include")
    set(BISON_EXECUTABLE "${BREW_PREFIX}/opt/bison/bin/bison")
    set(YACC_EXECUTABLE "${BREW_PREFIX}/opt/bison/bin/yacc")
    message(STATUS "Using BISON ${BISON_EXECUTABLE}")
    set(LLVM_PREFIX "${BREW_PREFIX}/opt/llvm@18")
    set(ENV{LLVM_SYS_181_PREFIX} "${LLVM_PREFIX}")
    set(SuiteSparse_DIR "${BREW_PREFIX}/opt/suite-sparse")
    set(TOMLPP_DIR "${BREW_PREFIX}/opt/tomlplusplus")
    set(Boost_ROOT "${BREW_PREFIX}/opt/boost")
    set(PROGRAM_LIB_DIR "lib/${PROGRAM_NAME}")
    find_package(Boost REQUIRED COMPONENTS filesystem)
    include_directories("${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost include: ${Boost_INCLUDE_DIRS}")
else()
    # Linux
    set(SIM_PLATFORM "Linux")
    set(SIM_ARCH "x86_64")
    set(PROGRAM_LIB_DIR "lib/${PROGRAM_NAME}")
    add_compile_definitions("SIMLINUX")
endif()
message(STATUS "Platform: ${SIM_PLATFORM}")

# Compiler
if (DEFINED MSVC_VERSION)
	# Microsoft C, Windows
	message(STATUS "Using MSVC toolchain")
	add_compile_definitions("SIMMSC")
	# Floating point exceptions are disabled by default in MSVC
	# Need this for MSVC to correctly define __cplusplus,
	# needed by Flex lexers and Bison parsers
	add_compile_options("/Zc:__cplusplus")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    # Apple Clang (macOS)
    message(STATUS "Using AppleClang toolchain")
    add_compile_options("-fno-signaling-nans" "-fno-trapping-math")
else()
	# GCC/MINGW
	message(STATUS "Using GCC/MINGW toolchain")
	add_compile_options("-fno-signaling-nans" "-fno-trapping-math" "-fcoroutines")
endif()

# OpenMP: probed once, optional. OPENMP_ENABLED is defined project-wide so any
# TU can test whether OpenMP is present, but the -fopenmp compile flag is NOT
# applied project-wide - building the whole project with it only slowed it down.
# The flag is scoped to just the TUs that need it: lib/libplatform.cpp and the
# SuperLU_MT backend TUs lib/solsuperlu_{real,complex}.cpp (see the
# set_source_files_properties() calls in lib/CMakeLists.txt). The OpenMP runtime
# is added to every executable link further down via
# $<LINK_ONLY:OpenMP::OpenMP_CXX> (link requirement, no compile flag).
# (The SuperLU_MT backend additionally needs -D__OPENMP on its own TUs - see
# below - and requires OpenMP to be present.)
find_package(OpenMP COMPONENTS CXX)
if (OpenMP_CXX_FOUND)
    message(STATUS "OpenMP: found (${OpenMP_CXX_FLAGS}); OPENMP_ENABLED project-wide, -fopenmp scoped to libplatform + SuperLU TUs")
    add_compile_definitions(OPENMP_ENABLED)
else()
    message(STATUS "OpenMP: not found, building without it")
endif()

set(STAGED_LIBRARY_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${PROGRAM_LIB_DIR}")
message(STATUS "Staged library directory: ${STAGED_LIBRARY_DIRECTORY}")

# Find Python (needed for tests)
find_package(Python 3 COMPONENTS Interpreter)

# Look for openvaf Verilog-A compiler
message(CHECK_START "Looking for OpenVAF-Reloaded Verilog-A compiler")
if (DEFINED OPENVAF_DIR)
  find_program(OPENVAF_COMPILER openvaf-r PATHS "${OPENVAF_DIR}" NO_DEFAULT_PATH)
else()
  find_program(OPENVAF_COMPILER openvaf-r)
endif()
if (OPENVAF_COMPILER STREQUAL "OPENVAF_COMPILER-NOTFOUND")
    message(FATAL_ERROR "  not found")
else()
    message(STATUS "  found ${OPENVAF_COMPILER}") 
endif()

#find_package(SWIG 4.0 COMPONENTS python)
#if(SWIG_FOUND)
#  include(UseSWIG)
#  message("SWIG found: ${SWIG_EXECUTABLE}")
#endif()

# find_package (Python3 COMPONENTS Interpreter Development)
# if(Python3_FOUND)
#   message("Python found: ${Python3_INCLUDE_DIRS}")
# endif()

if (${SIM_PLATFORM} STREQUAL "Windows")
    find_package(Boost REQUIRED COMPONENTS filesystem process)
    include_directories("${Boost_INCLUDE_DIRS}")
    link_directories("${Boost_LIBRARY_DIRS}")
    message(STATUS "Boost include: ${Boost_INCLUDE_DIRS}")
    include_directories("${TOMLPP_DIR}/include")
    message(STATUS "TOMLPP include: ${TOMLPP_DIR}/include")
elseif (${SIM_PLATFORM} STREQUAL "Darwin")
    # macOS with Homebrew
    find_package(Boost 1.85 REQUIRED COMPONENTS filesystem process)
    include_directories("${Boost_INCLUDE_DIRS}")
    link_directories("${Boost_LIBRARY_DIRS}")
    message(STATUS "Boost include: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost library: ${Boost_LIBRARY_DIRS}")
    include_directories("${TOMLPP_DIR}/include")
    message(STATUS "TOMLPP include: ${TOMLPP_DIR}/include")
else()
    # For 1.83
    # set(CMAKE_PREFIX_PATH "/usr/lib/x86_64-linux-gnu/cmake/Boost-1.88.0")
    # find_package(Boost CONFIG REQUIRED filesystem)

    # For 1.88+ installed manually by user
    set(Boost_NO_SYSTEM_PATHS TRUE)
    find_package(Boost 1.88 REQUIRED COMPONENTS filesystem process)
    include_directories("${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost version: ${Boost_VERSION}")
    message(STATUS "Boost include: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")

    # toml++ for Linux (passed via -DTOMLPP_DIR=...)
    if (DEFINED TOMLPP_DIR)
        include_directories("${TOMLPP_DIR}/include")
        message(STATUS "TOMLPP include: ${TOMLPP_DIR}/include")
    endif()

    # For 1.88 system-installed boost
    # find_package(Boost 1.88 REQUIRED COMPONENTS filesystem process)
    # include_directories("${Boost_INCLUDE_DIRS}")
    # link_directories("${Boost_LIBRARY_DIRS}")
    # message(STATUS "Boost include: ${Boost_INCLUDE_DIRS} lib ${Boost_EXTRA_LINK_DIR}")
endif()

#find_package(SuiteSparse CONFIG REQUIRED)
#include_directories("${SuiteSparse_INCLUDE_DIRS}")
include_directories("${SuiteSparse_DIR}/include")
if (${SIM_PLATFORM} STREQUAL "Windows")
    # cblas.h ships under an openblas/ subdirectory in the OpenBLAS install;
    # lib/libplatform.cpp includes it unqualified as <cblas.h>.
    include_directories("${OPENBLAS_DIR}/include/openblas")
endif()

# Bison >= 3.3 is required (matches %require "3.3" in lib/dflparser.y).
# Flex  >= 2.6.4 is required for the (?:...) non-capturing group syntax used in
# lib/dfllexer.l (extended patterns landed in the 2.6 series; 2.6.4 is verified).
find_package(BISON 3.3 REQUIRED)
find_package(FLEX 2.6.4 REQUIRED)

# This is for circumventing a bug in mingw cmake 3.28 that causes it to incorrectly set two variables
if (${SIM_PLATFORM} STREQUAL "Windows")
	get_filename_component(tmp "${FLEX_EXECUTABLE}" DIRECTORY)
	set(FLEX_INCLUDE_DIRS "${tmp}/../include" CACHE STRING "Flex include dir" FORCE)
	set(FL_LIBRARY "${tmp}/../lib/libfl.a" CACHE STRING "Flex library" FORCE)
endif()

#
# Optional SuperLU_MT sparse solver backend (lib/solsuperlu_{real,complex}.cpp,
# lib/solsuperlu_common.h, include/solsuperlu.h).
#
# Enabled with -DSuperluMT_DIR=<path> pointing at a built SuperLU_MT 4.x tree:
# headers in <dir>/SRC, the static library in <dir>/lib
# (libsuperlu_mt_OPENMP.a or _PTHREAD.a). This build expects the OpenMP variant
# linked against OpenBLAS. When the headers, the library and OpenMP are all
# found, SIM_HAVE_SUPERLU is defined project-wide; that macro gates both the
# backend TUs and the registration of the "superlu" solver in lib/simulator.cpp.
# Without -DSuperluMT_DIR the build proceeds without it.
#
set(SIM_HAVE_SUPERLU FALSE)

if (DEFINED SuperluMT_DIR)
    find_path(SIM_SUPERLU_INCLUDE slu_mt_ddefs.h
        HINTS "${SuperluMT_DIR}/SRC" "${SuperluMT_DIR}/include" "${SuperluMT_DIR}"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    find_library(SIM_SUPERLU_LIBRARY
        NAMES superlu_mt_OPENMP superlu_mt_PTHREAD superlu_mt
        HINTS "${SuperluMT_DIR}/lib"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    # SuperLU_MT here was built against OpenBLAS; its static library carries
    # unresolved BLAS symbols that this resolves at the final link.
    find_library(SIM_SUPERLU_BLAS NAMES openblas
        HINTS "${OPENBLAS_DIR}/lib" NO_CMAKE_FIND_ROOT_PATH)

    # OpenMP is probed unconditionally further up; the _OPENMP variant of the
    # SuperLU_MT library needs it.
    if (SIM_SUPERLU_INCLUDE AND SIM_SUPERLU_LIBRARY AND OpenMP_CXX_FOUND)
        set(SIM_HAVE_SUPERLU TRUE)
        message(STATUS "SuperLU_MT backend: enabled")
        message(STATUS "  headers:  ${SIM_SUPERLU_INCLUDE}")
        message(STATUS "  library:  ${SIM_SUPERLU_LIBRARY}")
        message(STATUS "  OpenBLAS: ${SIM_SUPERLU_BLAS}")
    else()
        message(WARNING "SuperLU_MT backend: -DSuperluMT_DIR set but incomplete, "
            "building without it (headers=${SIM_SUPERLU_INCLUDE} "
            "library=${SIM_SUPERLU_LIBRARY} openmp=${OpenMP_CXX_FOUND})")
    endif()
else()
    message(STATUS "SuperLU_MT backend: disabled (set -DSuperluMT_DIR to enable)")
endif()

#
# Custom build types
# 

# Profiling debug version -pg -fprofile-arcs -ftest-coverage --coverage
# Also does line coverage
# Function profile
#   gprof <binary> gmon.out
# Line-by-line profile
#   gprof -l <binary> gmon.out
set(CMAKE_CXX_FLAGS_DEBUGPROFILE "${CMAKE_CXX_FLAGS_DEBUG} -pg -fprofile-arcs -ftest-coverage --coverage" CACHE STRING
    "Flags used by the C++ compiler during debug profile builds." FORCE)
set(CMAKE_C_FLAGS_DEBUGPROFILE "${CMAKE_C_FLAGS_DEBUG} -pg -fprofile-arcs -ftest-coverage --coverage" CACHE STRING
    "Flags used by the C compiler during debug profile builds." FORCE)
set(CMAKE_EXE_LINKER_FLAGS_DEBUGPROFILE "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -pg" CACHE STRING
    "Flags used for linking binaries during debug profile builds." FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_DEBUGPROFILE "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -pg" CACHE STRING
    "Flags used by the shared libraries linker during debug profile builds." FORCE)

# Profiling release version
set(CMAKE_CXX_FLAGS_RELEASEPROFILE "${CMAKE_CXX_FLAGS_RELEASE} -pg -fprofile-arcs -ftest-coverage --coverage" CACHE STRING
"Flags used by the C++ compiler during release profile builds." FORCE)
set(CMAKE_C_FLAGS_RELEASEPROFILE "${CMAKE_C_FLAGS_RELEASE} -pg -fprofile-arcs -ftest-coverage --coverage" CACHE STRING
"Flags used by the C compiler during release profile builds." FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELEASEPROFILE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -pg" CACHE STRING
"Flags used for linking binaries during release profile builds." FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_RELEASEPROFILE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -pg" CACHE STRING
"Flags used by the shared libraries linker during release profile builds." FORCE)

# Address sanitizer debug version
set(CMAKE_CXX_FLAGS_ASANDEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address" CACHE STRING
"Flags used by the C++ compiler during address sanitizer debug builds." FORCE)
set(CMAKE_C_FLAGS_ASANDEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address" CACHE STRING
"Flags used by the C compiler during address sanitizer debug builds." FORCE)
set(CMAKE_EXE_LINKER_FLAGS_ASANDEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -static-libasan" CACHE STRING
"Flags used for linking binaries during address sanitizer debug builds." FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_ASANDEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -static-libasan" CACHE STRING
"Flags used by the shared libraries linker during address sanitizer debug builds." FORCE)

MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_DEBUGPROFILE
    CMAKE_C_FLAGS_DEBUGPROFILE
    CMAKE_EXE_LINKER_FLAGS_DEBUGPROFILE
    CMAKE_SHARED_LINKER_FLAGS_DEBUGPROFILE

    CMAKE_CXX_FLAGS_RELEASEPROFILE
    CMAKE_C_FLAGS_RELEASEPROFILE
    CMAKE_EXE_LINKER_FLAGS_RELEASEPROFILE
    CMAKE_SHARED_LINKER_FLAGS_RELEASEPROFILE

    CMAKE_CXX_FLAGS_ASANDEBUG
    CMAKE_C_FLAGS_ASANDEBUG
    CMAKE_EXE_LINKER_FLAGS_ASANDEBUG
    CMAKE_SHARED_LINKER_FLAGS_ASANDEBUG
)

IF(NOT CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE Debug
       CACHE STRING "Choose the type of build : None Debug Release DebugProfile ReleaseProfile AsanDebug."
       FORCE
    )
ENDIF(NOT CMAKE_BUILD_TYPE)
message("* Current build type is : ${CMAKE_BUILD_TYPE}")

#
# End of custom build types
# 


# Define SIMDEBUG preprocessor macro when build type is Debug
add_compile_definitions("$<$<CONFIG:Debug>:SIMDEBUG>")
add_compile_definitions("$<$<CONFIG:DebugProfile>:SIMDEBUG>")
add_compile_definitions("$<$<CONFIG:ReleaseProfile>:SIMPROFILE>")
add_compile_definitions("$<$<CONFIG:DebugProfile>:SIMPROFILE>")
add_compile_definitions("$<$<CONFIG:AsanDebug>:SIMDEBUG>")

# Location of include files
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

# BLAS/LAPACK for VectorView/DenseMatrix (Fortran routines called directly, no C wrapper)
# and for lib/libplatform.cpp, which calls openblas_set_num_threads() / includes
# <cblas.h> - so OpenBLAS must be what find_package() resolves to here, not the
# reference Netlib BLAS/LAPACK. On Linux that means the libblas.so.3 /
# liblapack.so.3 alternatives must point at the OpenBLAS (OpenMP) build.
# Windows gets it via the bundled libopenblas.a below; macOS gets Homebrew
# OpenBLAS the same way it already gets Homebrew SuiteSparse.
if (${SIM_PLATFORM} STREQUAL "Linux" OR ${SIM_PLATFORM} STREQUAL "Darwin")
    # libplatform.cpp needs OpenBLAS (<cblas.h>, openblas_set_num_threads)
    set(BLA_VENDOR OpenBLAS)
    find_package(BLAS REQUIRED)
    find_package(LAPACK REQUIRED)
endif()

# What to link with the object files to produce an executable
if (${SIM_PLATFORM} STREQUAL "Windows")
    link_directories(${SuiteSparse_DIR}/lib)
    link_directories(${OPENBLAS_DIR}/lib)
    set(SIM_EXE_LIBRARIES
            Boost::filesystem
            Boost::process
            Boost::system
            ${SuiteSparse_DIR}/lib/libklu.a
            ${SuiteSparse_DIR}/lib/libamd.a
            ${SuiteSparse_DIR}/lib/libcamd.a
            ${SuiteSparse_DIR}/lib/libcolamd.a
            ${SuiteSparse_DIR}/lib/libccolamd.a
            ${SuiteSparse_DIR}/lib/libbtf.a
            ${SuiteSparse_DIR}/lib/libcholmod.a
            ${SuiteSparse_DIR}/lib/libklu_cholmod.a
            ${SuiteSparse_DIR}/lib/libsuitesparseconfig.a
            ${OPENBLAS_DIR}/lib/libopenblas.a
            gomp dl wsock32
        CACHE STRING "External libraries for creating an executable."
        FORCE
    )
elseif (${SIM_PLATFORM} STREQUAL "Darwin")
    # macOS with Homebrew
    link_directories(${SuiteSparse_DIR}/lib)
    set(SIM_EXE_LIBRARIES
            Boost::filesystem
            Boost::process
            klu amd camd colamd ccolamd btf cholmod suitesparseconfig
            ${LAPACK_LIBRARIES} ${BLAS_LIBRARIES}
        CACHE STRING "External libraries for creating an executable."
        FORCE
    )
else()
    set(SIM_EXE_LIBRARIES
            dl m klu amd camd colamd ccolamd btf cholmod suitesparseconfig
            ${LAPACK_LIBRARIES} ${BLAS_LIBRARIES}
            ${Boost_LIBRARIES}
        CACHE STRING "External libraries for creating an executable."
        FORCE
    )
    # -Wl,-Bstatic
endif()

# Wire the SuperLU_MT backend in once SIM_EXE_LIBRARIES exists. Its headers clash
# with blaslapack.h, so the include dir, the -fopenmp flag and its -D__OPENMP are
# all scoped to just the backend TUs lib/solsuperlu_{real,complex}.cpp (in
# lib/CMakeLists.txt). Here we only define the gate macro and add the SuperLU
# libraries.
if (SIM_HAVE_SUPERLU)
    add_compile_definitions(SIM_HAVE_SUPERLU)
    set(SIM_EXE_LIBRARIES ${SIM_EXE_LIBRARIES}
            ${SIM_SUPERLU_LIBRARY}
            $<$<BOOL:${SIM_SUPERLU_BLAS}>:${SIM_SUPERLU_BLAS}>
        CACHE STRING "External libraries for creating an executable."
        FORCE
    )
endif()

# OpenMP runtime: linked into every executable (lib/libplatform.cpp and, when
# built, the SuperLU_MT TUs pull in libgomp) WITHOUT the -fopenmp compile flag -
# that stays scoped to those TUs (lib/CMakeLists.txt). $<LINK_ONLY:...> drops the
# imported target's INTERFACE_COMPILE_OPTIONS so nothing else gets -fopenmp.
if (OpenMP_CXX_FOUND)
    set(SIM_EXE_LIBRARIES ${SIM_EXE_LIBRARIES}
            $<LINK_ONLY:OpenMP::OpenMP_CXX>
        CACHE STRING "External libraries for creating an executable."
        FORCE
    )
endif()


add_subdirectory(lib)
add_subdirectory(simulator)
add_subdirectory(devices)
add_subdirectory(inc)
add_subdirectory(python)
add_subdirectory(test)

add_subdirectory(demo/api)

include("packaging.cmake")
