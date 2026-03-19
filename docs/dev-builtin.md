# Builtin Devices

Builtin devices are compiled into the simulator. They do not need a `load` directive. A `model` declaration is still required to make a device type available under a chosen name in the netlist.

```text
model vs vsource
model is isource
```

VACASK provides seven builtin device types.

**`vsource`** and **`isource`** are independent voltage and current sources. Both have two terminals (`p`, `n`) and share the same set of parameters. The `type` parameter selects the waveform: `dc`, `sine`, `pulse`, `exp`, `pwl`, `am`, or `fm`. The `mag` and `phase` parameters set the small-signal excitation used by AC and noise analyses regardless of the transient waveform type.

**`vccs`** (voltage-controlled current source) and **`vcvs`** (voltage-controlled voltage source) each have four terminals. The first two (`p`, `n`) are the output terminals; the second two (`cp`, `cn`) are the controlling voltage terminals. The `gain` parameter is the transconductance for `vccs` (in siemens) and the voltage gain for `vcvs` (dimensionless).

**`cccs`** (current-controlled current source) and **`ccvs`** (current-controlled voltage source) each have two terminals (`p`, `n`). The controlling current is taken from another instance in the circuit, identified by the `ctlinst` parameter. By default the branch current of that instance is used; `ctlnode` selects a different internal node for the controlling current if needed. The `gain` parameter is the current gain for `cccs` (dimensionless) and the transresistance for `ccvs` (in ohms).

**`mutual`** models inductive coupling between two inductors. It takes no terminals. The `ind1` and `ind2` parameters name the coupled inductor instances and `k` is the coupling coefficient. The inductors themselves must be loaded Verilog-A devices; `mutual` only adds the off-diagonal terms to the reactive stamp.

All builtin devices support `$mfactor` for parallel replication. See the subsections below for the complete parameter lists and usage examples.
