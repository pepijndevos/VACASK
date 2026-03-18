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

If no filename is given VACASK prints a hint and exits.

## Startup Sequence

When VACASK is launched it performs the following steps in order:

1. Parse command line flags.
2. Apply `SIM_MODULE_PATH`, `SIM_INCLUDE_PATH`, and `SIM_OPENVAF` environment variables if set.
3. Read [TOML configuration files](startup-paths.md#toml-configuration-files) in order. Later files override earlier ones.
4. Parse the input file.
5. Extract embedded files to the current working directory (unless `-se` is given).
6. Create the circuit object, compiling any Verilog-A files referenced by `load` directives.
7. Execute the control block.
