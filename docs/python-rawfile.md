# rawfile.py — Reading Raw Files

`rawfile` reads SPICE binary raw files produced by VACASK. The only public entry point is `rawread`.

```python
from rawfile import rawread
```

## rawread

```python
rawread(fname) → RawData
```

Opens the raw file at path `fname` and reads all plots it contains. A single file can hold multiple plots when several analyses write to the same output file (this is not the case with VACASK). Returns a `RawData` object. Currently it supports only padded binary rawfiles. 

## RawData

```python
RawData.get(ndx=0, sweeps=0) → RawFile
```

Returns the plot at index `ndx` (zero-based). `sweeps` tells the reader how many leading columns in the data matrix are sweep-parameter columns rather than signal columns. For a simple analysis with no outer sweep, omit `sweeps` or pass 0. For 1 1-dimensional DC sweep pass 0 because you want the sweep to define the scale for the results. For 2-dimensional DC sweeps pass 1 because the outer sweep should split the resulting vectors in traces, one per outer sweep point. If you pass 2 the resulting vectors will have length 1 (i.e. the results will be treated as a 2-dimensional sweep of operating points). 

```python
plot = rawread('op1.raw').get()           # single analysis, no sweep
plot = rawread('op1.raw').get(sweeps=1)   # one swept parameter
```

## RawFile

A `RawFile` holds all data for one plot.

### Attributes

| Attribute | Description |
|-----------|-------------|
| `title` | Circuit title string from the raw file header |
| `date` | Date string from the raw file header |
| `plotname` | Analysis type string, e.g. `"Operating Point"` |
| `flags` | Raw file flags string, e.g. `"real"` or `"complex"` |
| `names` | List of variable names in column order |
| `sweepGroups` | Nomber of sweep groups (1 when `sweeps=0`) |

There are as many sweep groups as there are points in the outer sweeps specified by the `sweeps` argument to `get()`. 

### Indexing

```python
plot["varname"]              # all data points for variable, as a NumPy array
plot[sweepGroup, "varname"]  # data points for variable within one sweep group
plot[sweepGroup, index]      # same, by column index instead of name
```

With `sweeps=0` there is one sweep group (index 0) and `plot["v"]` and `plot[0, "v"]` return the same array.

Variable names follow VACASK output naming: node voltages by node name (`"out"`), branch currents as `"instname:flow(br)"`, and output variables as `"instname.varname"`.

### sweepData

```python
RawFile.sweepData(sweepGroup) → dict
```

Returns a dictionary of `{name: value}` for the sweep parameters at the given sweep group. Useful for labelling plots.

## Example

```python
from rawfile import rawread
import matplotlib.pyplot as plt
import numpy as np

# Single operating-point analysis
op = rawread('op1.raw').get()
print(op.names)
vout = op['out']

# AC analysis with complex data
ac = rawread('ac1.raw').get()
freq = ac['frequency']
gain = 20*np.log10(np.abs(ac['out']))

plt.semilogx(freq, gain)
plt.xlabel('Frequency [Hz]')
plt.ylabel('Gain [dB]')
plt.show()

# Swept DC analysis: one outer sweep variable
dc = rawread('op1.raw').get(sweeps=1)
for g in range(dc.sweepGroups):
    params = dc.sweepData(g)
    vin = dc[g, 'in']
    vout = dc[g, 'out']
    plt.plot(vin, vout, label=str(params))
```
