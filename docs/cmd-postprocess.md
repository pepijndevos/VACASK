# Postprocessing

The `postprocess` command runs an external program after simulation. It is typically used to launch a Python script that reads simulation results and generates plots or reports.

## Syntax

```text
postprocess("program" [, "arg1", "arg2", ...])
```

All arguments are string expressions. The first argument is the path to the program to execute. Subsequent arguments are passed as command-line arguments to that program.

## The PYTHON variable

VACASK automatically defines the circuit variable `PYTHON` as the path to the Python interpreter found at startup. Use it to run Python scripts portably:

```text
postprocess(PYTHON, "plot.py")
```

The VACASK library directory's `python/` subdirectory is automatically prepended to `PYTHONPATH` when a postprocess command runs. This makes the bundled Python helpers — including `rawfile` — importable without any path configuration:

```text
from rawfile import rawread
```

## Example

The following netlist sweeps the diode forward voltage and plots the I-V curve:

```text
Diode I-V curve

load "spice/diode.osdi"

model vsource vsource
model d1 sp_diode

vd (a 0) vsource dc=0
d1 (a 0) d1

control
  sweep vd instance="vd" parameter="dc" from=0 to=0.8 step=0.01
    analysis op1 op
  postprocess(PYTHON, "plot.py")
endc

embed "plot.py" <<<FILE
import matplotlib.pyplot as plt
from rawfile import rawread

plot = rawread('op1.raw').get()

fig, ax = plt.subplots()
ax.plot(plot['vd'], -plot['vd:flow(br)']*1e3)
ax.set_xlabel('Vd [V]')
ax.set_ylabel('Id [mA]')
ax.grid(True)
plt.show()
>>>FILE
```

The script is embedded in the netlist using the `embed` directive (see [Embedded Files](input-embed.md)), so no separate file needs to be maintained.

## Skipping postprocessing

Pass the `-sp` / `--skip-postprocess` flag to the simulator to skip all `postprocess` commands. This is useful for batch runs where plotting is not needed:

```text
vacask -sp mynetlist.spi
```
