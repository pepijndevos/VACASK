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
| `-n <n>` | `--ncpu <n>` | Number of threads the multithreaded linear solver may use. `n <= 0` (default) autodetects: it honors `OMP_NUM_THREADS` if set, otherwise uses all available CPUs. |
| `-b <n>` | `--bncpu <n>` | Number of threads OpenBLAS may use. Default `1`. `n <= 0` lets OpenBLAS autodetect. |

If no filename is given VACASK prints a hint and exits.

## Parallelism

VACASK is single-threaded except for the sparse linear solver used by
[harmonic balance](cmd-analysis-hb.md) and the harmonic-balance-based
[(quasi)periodic small-signal analysis](cmd-analysis-hbac.md). When the simulator
is built with the multithreaded SuperLU backend, that solver is the default for
these analyses and runs on `--ncpu` threads. The other analyses and their default
`klu` solver ignore this setting.

More threads is not always faster: for small circuits the factorization is
cheap and thread startup dominates, so `--ncpu 1` or `2` can beat the autodetect
default. Measure with `print stats`.

`--ncpu` sets only the solver thread count; it does not cap the process-wide
OpenMP pool. For a single consistent limit, set `OMP_NUM_THREADS` in the
environment - it is picked up by the `--ncpu` autodetect, by OpenBLAS, and by the
solver's own parallel regions:

```text
OMP_NUM_THREADS=4 vacask circuit.sim
```

Dense operations (small, mostly in the small-signal analyses) go through OpenBLAS,
which defaults to a single thread here; raise it with `--bncpu` only if profiling
shows it helps.

Builds without the SuperLU backend or without OpenMP run everything on one thread,
and both options have no effect.

## Startup Sequence

When VACASK is launched it performs the following steps in order:

1. Parse command line flags.
2. Resolve the thread counts from `--ncpu` / `--bncpu` (and `OMP_NUM_THREADS`).
3. Apply `SIM_MODULE_PATH`, `SIM_INCLUDE_PATH`, and `SIM_OPENVAF` environment variables if set.
4. Read [TOML configuration files](startup-paths.md#toml-configuration-files) in order. Later files override earlier ones.
5. Parse the input file.
6. Extract embedded files to the current working directory (unless `-se` is given).
7. Create the circuit object, compiling any Verilog-A files referenced by `load` directives.
8. Execute the control block.
