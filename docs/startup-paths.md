# Search Paths and Configuration Files

## Search Paths

VACASK uses two search paths.

**Module path** — searched when a `load` directive specifies a relative path to an `.osdi` or `.va` file. Default: `<library directory>/mod`. Override with the `SIM_MODULE_PATH` environment variable or the `[Paths]` section of the configuration file.

**Include path** — searched when an `include` directive specifies a relative path. Default: `<library directory>/inc`. Override with `SIM_INCLUDE_PATH` or the configuration file.

For relative paths VACASK always searches first in the directory of the file that issued the directive, then the current working directory, and finally the configured path. Absolute paths bypass the search entirely.

When set via environment variable, directories are separated by colons on Linux and macOS, and by semicolons on Windows.

## TOML Configuration Files

Configuration files are read in the following order. Later files override earlier ones.

| Location | Path |
|----------|------|
| System | `/etc/vacask/vacaskrc.toml` (Linux) or `<install>/lib/vacaskrc.toml` (Windows) |
| User | `~/.vacaskrc.toml` |
| Local (startup directory) | `.vacaskrc.toml` in the directory where VACASK was launched |
| Netlist directory | `.vacaskrc.toml` next to the top-level input file |

Each file is read at most once. 

## Configuration File Format

Environment variable values can be interpolated in strings with `$(VAR_NAME)`. A documented sample configuration is provided in `config/vacaskrc-sample.toml`.

**`[Paths]`**

| Setting | Type | Effect |
|---------|------|--------|
| `include_path_prefix` | list of strings | Directories prepended to the include path (searched before the builtin default). |
| `include_path_suffix` | list of strings | Directories appended to the include path (searched after the builtin default). |
| `module_path_prefix` | list of strings | Directories prepended to the module path (searched before the builtin default). |
| `module_path_suffix` | list of strings | Directories appended to the module path (searched after the builtin default). |

**`[Binaries]`**

| Setting | Type | Effect |
|---------|------|--------|
| `openvaf` | string | Full path to the OpenVAF-reloaded compiler binary. |
| `openvaf_args` | list of strings | Extra arguments passed to the OpenVAF compiler. |
| `python` | string | Full path to the Python 3 interpreter. |

### Example TOML configuration file

```toml
[Paths]
# Searched before the builtin module path
module_path_prefix = [ "/home/user/mymodels", "$(PROJECT_ROOT)/mod" ]
# Searched after the builtin include path
include_path_suffix = [ "/home/user/myinc" ]

[Binaries]
openvaf = "/usr/local/bin/openvaf-r"
openvaf_args = [ "--allow", "variant_const_simparam" ]
python = "/usr/bin/python3"
```
