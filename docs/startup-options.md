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
| `-n <n>` | `--ncpu <n>` | Number of threads the multithreaded SuperLU solver spawns per factorization. Default `1`. `n <= 0` autodetects: it honors `OMP_NUM_THREADS` if set, otherwise uses all available CPUs. |
| `-b <n>` | `--bncpu <n>` | Number of threads OpenBLAS may use for dense operations. Default `1`. `n <= 0` lets OpenBLAS autodetect. |

If no filename is given VACASK prints a hint and exits.

## Parallelism

VACASK runs single-threaded except for the sparse linear solver used by
[harmonic balance](cmd-analysis-hb.md) and the harmonic-balance-based
[(quasi)periodic small-signal analysis](cmd-analysis-hbac.md). When the simulator
is built with the multithreaded SuperLU backend, that solver is the default for
these analyses. Every other analysis uses the `klu` solver, which ignores both
options below.

There are two independent thread counts:

- `--ncpu` (`-n`): threads the SuperLU factorization (`pdgstrf`) spawns.
  Default `1`.
- `--bncpu` (`-b`): threads OpenBLAS uses for dense operations, most of which
  occur in the small-signal analyses. Default `1`.

### Serial runs

When both `--ncpu` and `--bncpu` are `1` (the defaults), VACASK also pins the
OpenMP pool to a single thread. SuperLU's factorization loop has no explicit
thread-count clause, so without this cap it would still create a full-size team
of worker threads that busy-wait through every factorization and burn CPU for
nothing. With the cap, a default run is genuinely serial.

### Sizing the thread pool

As soon as either `--ncpu` or `--bncpu` exceeds `1`, VACASK stops managing the
OpenMP pool; its size then comes entirely from `OMP_NUM_THREADS`. Set it to match
the work requested.

The solver and OpenBLAS thread counts multiply: a factorization on `--ncpu`
threads, each of which may call into an `--bncpu`-threaded OpenBLAS, needs
`ncpu * bncpu` threads in the pool. With `--ncpu 4 --bncpu 3` size it for
`4 * 3 = 12`:

```text
OMP_NUM_THREADS=12 vacask -n 4 -b 3 circuit.sim
```

An undersized pool oversubscribes the cores and is usually slower than a smaller
thread count that schedules cleanly. Measure with `print stats`.

More threads is not always faster: for small circuits the factorization is cheap
and thread startup dominates, so the serial default often wins.

Builds without the SuperLU backend or without OpenMP run everything on one
thread, and both options have no effect.

## Startup Sequence

When VACASK is launched it performs the following steps in order:

1. Parse command line flags.
2. Resolve the thread counts from `--ncpu` / `--bncpu` (and `OMP_NUM_THREADS`); cap the OpenMP pool to one thread when both are `1`.
3. Apply `SIM_MODULE_PATH`, `SIM_INCLUDE_PATH`, and `SIM_OPENVAF` environment variables if set.
4. Read [TOML configuration files](startup-paths.md#toml-configuration-files) in order. Later files override earlier ones.
5. Parse the input file.
6. Extract embedded files to the current working directory (unless `-se` is given).
7. Create the circuit object, compiling any Verilog-A files referenced by `load` directives.
8. Execute the control block.
