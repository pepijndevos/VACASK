# Printing

The `print` command writes information to standard output. It is mainly used for inspection and debugging during a simulation run. It triggers elaboration of any pending changes before executing.

## Printing expression values

```text
print (expr1 [, expr2 ...])
```

Evaluates and prints one or more expressions. Strings are printed without quotes; numbers and vectors are followed by a space. A newline is appended at the end.

```text
print ("vdd =", 1.8)
print ("gain =", 10*log10(gain^2), "dB")
print ("nodes:", n1, n2, n3)
print ([1,2,3])
```

## Inspecting circuit state

The keyword forms report on the elaborated circuit. All require the circuit to be elaborated; if it is not, default elaboration runs first.

### Instance and model parameters

```text
print instance("name" [, "name2" ...])
print model("name" [, "name2" ...])
```

Dumps the current parameter values of the named instance or model. Multiple names can be given in the expression list. 

For instances also prints terminals and output variables. 

```text
print instance("m1")
print model("nmos")
```

After `alter`, parameters that were changed are shown with their new values.

### Device information

```text
print device("name" [, "name2" ...])
```

Prints the list of nodes (terminals and internal nodes), Verilog-A tolerances of nodes, model inputs, Jacobian entries, number of internal states, lists of model and instance parameters, and output variables, and the data structure sizes. 

```text
print device("bsim4")
```

### Model and device inventory

```text
print models
print devices
print device_files
print device_file("pattern" [, ...])
```

`print models` lists all elaborated models and the corresponding devices. `print devices` lists all device types available in the elaborated circuit. `print device_files` lists all loaded OSDI files. `print device_file("pattern")` shows detailed information (natures, disciplines, and modules) for each loaded OSDI file whose name contains the given substring.

```text
print device_file("bsim4")
```

### Hierarchy

```text
print hierarchy
```

Prints the elaborated instance hierarchy with instance names, master names, and device names. 

### Variables, options, and saves

```text
print variables
print options
print options_state
print saves
```

`print variables` shows the current values of all circuit variables set with `var`.

`print options` shows the option assignments that have been given in `options` statements so far in the control block.

`print options_state` shows the effective simulator option values currently in use, including defaults for anything not explicitly set.

`print saves` shows the active save directives.

### Circuit complexity

```text
print nodes
print unknowns
print tolerances
print sparsity
print counts
```

`print nodes` and `print unknowns` list the circuit nodes and the MNA unknowns with their indices. `print tolerances` shows the absolute tolerances assigned to each unknown and residual. `print sparsity` dumps the sparsity pattern of the Jacobian. `print counts` gives a summary of instance, master, and node counts. 

### Performance counters

```text
print stats
```

`print stats` reports performance counters and elapsed time from all analyses run so far.

## Example

```text
Resistor divider

ground 0
load "resistor.osdi"
load "vsource.osdi"

model resistor resistor
model vsource vsource

v1 (vdd 0) vsource dc=10
r1 (vdd mid) resistor r=6k
r2 (mid 0) resistor r=4k

control
  abort always
  print devices
  print instance("r1")
  analysis op1 op
  print ("V(mid) expected:", 10*4/(6+4))
  print instance("r2")
endc
```
