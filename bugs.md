# Bug audit — `origin/main..HEAD` (SuperLU_MT backend + CPU/thread-count plumbing)

11 commits reviewed: SuperLU_MT solver backend, OpenMP/OpenBLAS thread-count
plumbing, block-dense-aware solvers. Ordered most to least severe.

- [x] **1. `std::stoi` on `--ncpu` / `--bncpu` aborts the process on bad input**
  `simulator/main.cpp:141` (and `:148`).
  `ncpu = std::stoi(argv[i]);` with no try/catch anywhere in `main`.
  `vacask -n foo`, `vacask -n 99999999999`, or a trailing `-n` before a filename
  → `std::invalid_argument` / `std::out_of_range` → `std::terminate`/abort.
  Every other bad argument in this loop prints a message and `return 1`.
  Fix: wrap in try/catch or use `strtol` with validation.

- [x] **2. Help text advertises a flag the parser does not accept**
  `simulator/main.cpp:39` vs `simulator/main.cpp:142`.
  Help said `-b <n>, --blas-ncpu <n>`; parser matched `-b` / `--bncpu`; docs
  use `--blas-ncpu`. Copying `--blas-ncpu` from `--help` → `Unrecognized
  argument` + exit 1.
  Fixed: parser now accepts `-b` / `--blas-ncpu`, matching the help text and docs.

- [x] **3. Library API default (`ncpu=0`) contradicts CLI default (`1`) and docs**
  `include/simulator.h:20-27`, `lib/simulator.cpp:89-99`.
  `Simulator::setup(int ncpu=0, ...)`; `ncpu<=0` means "autodetect = all cores",
  and the serial-OpenMP cap only fires when `ncpu==1 && nBlasCpu==1`. Any
  embedder calling `Simulator::setup()` (see `demo/api/`) silently gets a
  full-width SuperLU/OpenMP thread team, while `simulator/main.cpp` (`int ncpu =
  1`) and `docs/startup-options.md` state the default is serial.
  Fix: make the API default `1`.

- [x] **4. Unconditional `<cblas.h>` / `openblas_*_num_threads` breaks the build on non-OpenBLAS BLAS**
  `lib/libplatform.cpp:15`, `:164-170`.
  `libplatform.cpp` (previously no BLAS dependency) now unconditionally includes
  `<cblas.h>` and calls `openblas_set_num_threads` / `openblas_get_num_threads`.
  `CMakeLists.txt:346` is just `find_package(BLAS REQUIRED)` with no
  `BLA_VENDOR`. A box whose `libblas.so.3` alternative points at reference Netlib
  configures fine, then this TU fails to compile/link (Netlib `cblas.h` does not
  declare those symbols). The CMake comment at `:338-344` admits OpenBLAS is
  required but nothing enforces it.
  Fix: enforce `BLA_VENDOR=OpenBLAS` / check for the symbol, or guard the calls.

- [x] **5. `setBlockSize` shrinks SuperLU's max supernode size ~7x below default**
  `lib/solsuperlu.cpp:23`.
  `maxSize = 2*blockSize; // lxm, >=default=200` — no floor. For a typical HB run
  (`nharm=7` → block ~15 cols) `maxSize` becomes ~30 vs `sp_ienv(3)`'s default of
  200; the comment only holds for `blockSize>=100`. Supernodes fragment → slower
  factorization for every HB/HBAC solve. Plausibly why `test_hbac1` tolerances
  were loosened 1e-14 → 1e-12 in this same branch (`test/test_hbac1.sim`).
  Fix: `std::max(200, 2*blockSize)` etc.

- [x] **6. All SuperLU_MT instances of a value type share one global `IEnvData` (and SuperLU's static `GlobalLU_t`)**
  Deliberate limitation (one live factorization per value type, no concurrency).
  Safeguarded: `SolverImpl::gluOwner` throws if a dispossessed instance attempts
  a `refact=YES` factorization.
  `lib/solsuperlu.cpp:27-31`, `include/solsuperlu.h`.
  `IEnvData::activeData` is a single global pointer; SuperLU_MT's
  `pdgstrf_thread_init` keeps a `static GlobalLU_t Glu`. Two live
  `SuperLURealSparseSolver` instances used interleaved — one analysis holding its
  `linearSolver_` while another SuperLU analysis factors, then the first is
  `refactor()`'d with `refact=YES` — reuse the other matrix's symbolic structure
  / block-size hints → wrong factors or heap corruption. The code's TODO comment
  flags only the multi-threaded case, not this single-threaded interleaving.
  Fix: document the limitation where solvers are selected; a real fix needs
  per-instance state / TLS.
