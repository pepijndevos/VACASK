# Input File Overview

VACASK input files describe circuits using a Spectre-like netlist syntax. The primary purposes of an input file are to define the circuit topology, configure simulations, and specify what results to save.

## Basic Structure

An input file contains the following elements:

- **Title**: the name of the circuit
- **Comments**: blocks of text that are ignored by the simulator
- **Devices**: device model specifications loaded via the `load` directive
- **Global and ground nodes**: lists of global and ground nodes (`global` and `ground` directives)
- **Models**: models specified with the `model` directive
- **Subcircuits**: reusable blocks of instances and models enclosed in `subckt` and `ends` keywords
- **Parametrization**: parameters defined with the `parameters` keyword
- **Circuit definition**: instances and their connections
- **Control block**: simulation setup and analysis configuration enclosed in `control` and `endc` keywords
- **Embedded files**: auxiliary data included directly in the netlist

The input file is case-sensitive. 

## Whitespace and newlines

VACASK uses a statement-based format where each statement ends with a newline. Whitespace is ignored except within strings. Newlines within parenthesis (`()`), square brackets (`[]`), or curly brackets (`{}`) are ignored in the sense that anything following such a newline is not considered to be the begining of a new line. If a line ends with a backslash `\` the charaters that follow in the next line are considered as part of the line with the backslash. Such a backslash itself is ignored. 

## Comments 

Comments begin with `//` and extend to end of line. Another way to specify comments is as blocks that start with `/*` and end with `*/`. 

## Example

```
My first circuit

ground 0              // Define ground node

load "resistor.osdi"  // Load resistor device

model r resistor      // Declare a resistor model
model v vsource       // Declare a voltage source model

v1 (1 0) v dc=10      // Voltage source: v1 between nodes 1 and 0
r1 (1 2) r r=1k       // Resistor: r1 between nodes 1 and 2

control                           // Analysis setup block
  options rawfile="binary"   
  analysis op1 op                 // Operating point analysis
  postprocess(PYTHON, "runme.py") // Run the embedded Python script
endc

// Embedded Python script (dumped before simulation starts)
embed "runme.py" <<<FILE
from rawfile import rawread

# Print operating point
op1 = rawread('op1.raw').get()
for name in op1.names:
  print(name, op1[name])
```

VACASK input files use the `.sim` extension by convention, though this is not required.
