# Command Line Options and Startup Sequence

## Command Line Options

| Option | Long form | Effect |
|--------|-----------|--------|
| `-h` | `--help` | Print help and exit. |
| `-dp` | `--dump-paths` | Print the locations of all simulator components (binary, module path, include path, OpenVAF, Python). |
| `-df` | `--debug-files` | Print each file's path as it is loaded, compiled, or written. |
| `-se` | `--skip-embed` | Do not extract embedded files from the input file. |
| `-sp` | `--skip-postprocess` | Do not run `postprocess` steps defined in the control block. |
| `-qp` | `--quiet-progress` | Suppress progress messages. |
| `--no-output` | | Suppress writing of result files. |
| `-n <n>` | `--ncpu <n>` | Number of threads used for device evaluation and by the multithreaded SuperLU solver. Default `1`. `n <= 0` autodetects: it honors `OMP_NUM_THREADS` if set, otherwise uses all available CPUs. |
| `-b <n>` | `--blas-ncpu <n>` | Number of threads OpenBLAS may use for dense operations. Default `1`. `n <= 0` lets OpenBLAS autodetect. |

If no filename is given VACASK prints a hint and exits.

## Parallelism

VACASK uses OpenMP threads in three places: device evaluation, the SuperLU
sparse solver, and OpenBLAS dense operations. Everything else, including
assembly of the Jacobian and the `klu` solver, runs on one thread.

Builds without OpenMP have no parallel device evaluation or multithreaded
SuperLU. There `--ncpu` values above `1` are clamped to `1` with a startup
warning.

There are two independent thread counts:

- `--ncpu` (`-n`): threads used for device evaluation and by the SuperLU
  factorization. Default `1`.
- `--blas-ncpu` (`-b`): threads OpenBLAS uses for dense operations, most of which
  occur in the small-signal analyses. Default `1`.

### Device evaluation

With `--ncpu` greater than `1` the device instances are split into batches
(ten per thread by default) that are evaluated in parallel. The number of
batches and the way they are scheduled among threads are set by the
[parallel evaluation options](cmd-options-omp.md). The results are then
loaded into the Jacobian and the residual on a single thread. Device evaluation
runs in every analysis, regardless of the selected solver.

### SuperLU solver

The SuperLU factorization runs on `--ncpu` threads. SuperLU is used by the
analyses that select it with the [solver options](cmd-options-solver.md). When the
simulator is built with the SuperLU backend, it is the default for
[harmonic balance](cmd-analysis-hb.md) and the
[(quasi)periodic small-signal analyses](cmd-analysis-hbac.md). The `klu` solver
ignores `--ncpu`.

### OpenBLAS

`--blas-ncpu` applies only to OpenBLAS calls made outside the parallel regions
above. An OpenBLAS call made from inside a parallel region runs on one thread, so
the two counts do not multiply and no extra thread pool needs to be reserved.

Leave `--blas-ncpu` at `1` unless the circuit is very large. The dense operations
it parallelizes are a small share of the total work for typical circuits, so
raising it only adds thread-management overhead. It pays off only when the
small-signal dense blocks are large enough to amortize that cost.

### Choosing the thread count

The thread counts are passed to OpenMP explicitly, so `OMP_NUM_THREADS` only
matters when `--ncpu` or `--blas-ncpu` is `<= 0`. A typical parallel run is

```text
vacask -n 4 circuit.sim
```

More threads is not always faster: for small circuits evaluation and
factorization are cheap and thread synchronization dominates, so the serial
default often wins. Do not set `--ncpu` above the number of physical cores.
Measure with `print stats`.

### OpenMP environment

For best performance set the following OpenMP environment variables before
starting VACASK:

| Variable | Value | Effect |
|----------|-------|--------|
| `OMP_PROC_BIND` | `close` | Pins each thread to a fixed place, with places assigned next to each other. Threads do not migrate between cores. |
| `OMP_PLACES` | `cores` | One place per physical core, so two threads never share the hyperthreads of one core. |

```text
OMP_PROC_BIND=close OMP_PLACES=cores vacask -n 4 circuit.sim
```

By default idle threads spin for a while before they sleep. When the serial
work between parallel regions is long, or other processes compete for the
cores, setting `OMP_WAIT_POLICY=passive` may help. Idle threads then sleep
immediately instead of burning CPU time, at the cost of a few microseconds per
parallel region to wake them. Measure both settings with `print stats`.

Builds without OpenMP run everything on one thread, and both options have no
effect.

## Startup Sequence

When VACASK is launched it performs the following steps in order:

1. Parse command line flags.
2. Resolve the thread counts from `--ncpu` / `--blas-ncpu` (and `OMP_NUM_THREADS` when a count is `<= 0`).
3. Apply `SIM_MODULE_PATH`, `SIM_INCLUDE_PATH`, and `SIM_OPENVAF` environment variables if set.
4. Read [TOML configuration files](startup-paths.md#toml-configuration-files) in order. Later files override earlier ones.
5. Parse the input file.
6. Extract embedded files to the current working directory (unless `-se` is given).
7. Create the circuit object, compiling any Verilog-A files referenced by `load` directives.
8. Execute the control block.
