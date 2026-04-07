# AC Small-Signal S-Parameter Analysis

AC S-parameter analysis computes the small-signal scattering parameters ($S$-matrix) of a multi-port circuit as a function of frequency. It sweeps a range of frequencies and solves for the $S$-matrix at each frequency point.

## Syntax

```text
analysis name acsp [parameters]
```

## How it works

1. VACASK first performs an operating point (OP) analysis to find the DC solution $x_0$.
2. It linearizes the circuit by computing the resistive Jacobian $J_r$ and the reactive Jacobian $J_c$ at $x_0$.
3. For each frequency $f$ in the sweep it assembles the complex system matrix

   $$(J_r + j \omega J_c)$$

   where $\omega = 2\pi f$.
4. For each port $i$, the voltage source at that port is excited while all others are set to zero. The system is solved for the resulting node voltages and source currents, giving incident ($a$) and reflected ($b$) waves at each port.
5. The $S$-matrix satisfies $B = S A$, where $A$ and $B$ collect incident and reflected wave amplitudes. It is recovered by solving this linear system from the per-port solutions.

## Port definition

Each port is defined by a pair of elements:

- A **voltage source** — provides the excitation and carries the port current.
- A **series resistor** — its resistance (divided by `$mfactor`) defines the characteristic impedance $Z_0$ of the port.

The positive terminal of the voltage source must be connected to one terminal of the series resistor. The remaining resistor terminal is the positive interface-plane node; the negative terminal of the voltage source is the negative interface-plane node.

Port impedances may differ between ports.

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `ports` | string list | `""` | Alternating list of voltage source and series resistor names defining the ports: `["vsrc1", "rser1", "vsrc2", "rser2", …]`. |
| `nodeset` | string or list | `""` | Initial guess for the operating point. See [Operating Point Analysis](cmd-analysis-op.md). |
| `store` | string | `""` | Save the computed operating point under the given name. See [Operating Point Analysis](cmd-analysis-op.md). |
| `from` | real | `0` | Start frequency (Hz) for stepped or mode-based sweeps. |
| `to` | real | `0` | Stop frequency (Hz) for stepped or mode-based sweeps. |
| `step` | real | `0` | Frequency step size (Hz) for a linear stepped sweep. |
| `mode` | string | - | Sweep mode: `"lin"` (linear), `"dec"` (logarithmic per decade), or `"oct"` (logarithmic per octave). |
| `points` | integer | `0` | Number of points (total for `"lin"`, per decade for `"dec"`, per octave for `"oct"`). |
| `values` | real vector | - | Explicit vector of frequencies (Hz). Overrides `from`/`to`/`step`/`mode`/`points`. |
| `write` | boolean | `1` | Write the analysis results to a file. |
| `writeop` | boolean | `0` | Also write the operating point results to `<analysis>.op.*`. |

### Sweep modes

See [AC Small-Signal Analysis](cmd-analysis-ac.md) for a description of sweep modes.

## Save directives

`acsp` runs an operating point core internally, so operating point save directives (`v(node)`, `i(instance)`, `p(instance,outvar)`) are also accepted. They apply to the operating point results and are written to `<analysis>.op.*` when `writeop=1`.

## Output

A file `<analysis>.*` containing the complex $S$-parameters at each frequency point.

| Variable | Description |
|----------|-------------|
| `frequency` | Frequency sweep variable (Hz). Always present. |
| `s(i,j)` | Complex $S$-parameter $S_{ij}$: reflected wave at port $i$ due to excitation at port $j$. |

All $S_{ij}$ entries for the defined ports are always written. If `writeop=1`, an additional `<analysis>.op.*` file is written with the operating point results.

## Example

Two-port S-parameter analysis with 50 Ω and 75 Ω port impedances, decade sweep from 1 Hz to 100 MHz:

```text
S-parameter analysis

ground 0

load "resistor.osdi"
load "capacitor.osdi"

model r resistor
model c capacitor
model vsrc vsource

r1 (1 0) r r=1k
r2 (2 0) r r=2k
c1 (1 2) c c=1u

// Port 1: 50 Ω, interface plane at nodes 1, 0
vp1 (a1 0) vsrc dc=0
rp1 (a1 1) r r=50

// Port 2: 75 Ω, interface plane at nodes 2, 0
vp2 (a2 0) vsrc dc=0
rp2 (a2 2) r r=75

control
  analysis sp1 acsp ports=["vp1", "rp1", "vp2", "rp2"] from=1 to=100M mode="dec" points=10
endc
```

The output file `sp1.raw` contains `frequency`, `s(1,1)`, `s(1,2)`, `s(2,1)`, and `s(2,2)`.

## Options

- [Small-Signal Analysis Options](cmd-options-smsig.md)
- [Newton-Raphson Solver](cmd-options-nr.md)
- [Homotopy Algorithms](cmd-options-homotopy.md)
