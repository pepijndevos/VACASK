# Nodes

Nodes represent the connection points between instances in the circuit. Each node corresponds to one or more unknowns in the circuit equations, depending on the disciplines of the connected terminals.

## Node names

Node names are identifiers or non-negative integers. The following are all valid: `vdd`, `net1`, `3`, `0`.

Node names are scoped to the subcircuit in which they appear. Nodes in a subcircuit definition are local to that subcircuit and do not conflict with same-named nodes in other subcircuits.

## Ground nodes

Node `0` is the reference (ground) node by default. Additional ground nodes can be declared with the `ground` directive:

```text
ground node1 node2 ...
ground (node1 node2 ...)
```

All nodes listed in a `ground` directive are tied together and to the circuit reference. The `ground` directive is only allowed at the top level.

## Global nodes

Global nodes are accessible from within subcircuit definitions without being passed as terminals. Declare them with the `global` directive:

```text
global node1 node2 ...
global (node1 node2 ...)
```

A subcircuit that references a global node uses the top-level node with that name. The `global` directive is only allowed at the top level. Ground nodes are global nodes. 

## Examples

```text
ground gnd agnd          // Tie gnd and agnd to reference (node 0)
global vdd vss           // Make vdd and vss visible inside subcircuits
```
