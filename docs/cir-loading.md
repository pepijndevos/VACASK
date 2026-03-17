# Loading Devices

The `load` directive makes device modules available to the netlist. VACASK loads compiled device files in the OSDI format (Open Source Device Interface). Verilog-A source files are also accepted and compiled to OSDI automatically before loading.

## Syntax

```text
load "path" [map=["from1", "to1", ...]]
```

`load` is only allowed at the top level. It cannot appear inside a subcircuit definition.

## File search

For an absolute `path`, VACASK opens that file directly. For a relative path it searches:

1. The directory containing the file where the `load` directive appears.
2. The current working directory.
3. The module search path configured in the simulator settings.

## Compiling Verilog-A modules on the fly

When a `.va` file is loaded, VACASK compiles it to OSDI using the OpenVAF-reloaded compiler (`openvaf-r` by default, `openvaf-r.exe` under Windows) before loading. The compiled `.osdi` file is written to the current working directory with the same base name as the source file. Compilation is skipped if an `.osdi` file with that name already exists in the current directory and is newer than the `.va` source.

By default VACASK searches for the OpenVAF-reloaded compiler in the directory where the simulator is installed, followed by the system path. The path to the OpenVAF-reloaded binary and additional compiler arguments can be configured in the simulator config file:

```toml
[Binaries]
openvaf = "/path/to/openvaf-r"
openvaf_args = ["--extra-arg", "value"]
```

## Module naming

An OSDI file can contain multiple device modules. By default VACASK loads all of them under their original names. The optional `map` parameter controls which modules are loaded and under what names. It takes a string vector of `["from", "to", ...]` pairs, where `from` is a module name or `*` (wildcard), and `to` is the target name (`*` in `to` is replaced by the original module name).

| `map` value | Effect |
|-------------|--------|
| not given | Load all modules under their original names. |
| `["*", "*"]` | Same as not given. |
| `["*", "pfx_*"]` | Load all modules, prepend `pfx_` to each name. |
| `["foo", "bar"]` | Load only module `foo` and rename it to `bar`. |
| `[]` | Load nothing. |

## Case of imported names

VACASK converts all parameter names, terminal names, and names of noise contributions found in an OSDI file to lowercase. 

## Builtin devices

A small set of device models is built into VACASK. These devices comprise independent sources (`vsource` and `isource`), linear controlled sources (`vcvs`, `vccs`, `ccvs`, and `cccs`), and inductive coupling (`mutual`). 

## Examples

```text
load "resistor.osdi"
load "spice/diode.osdi"
load "mybip.osdi" map=["mybip", "bjt"]
load "alldevices.osdi" map=["*", "lib_*"]
```
