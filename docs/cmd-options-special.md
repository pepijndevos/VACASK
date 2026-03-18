# Options with Special Behavior

Most options take effect simply by being applied before the next analysis. A subset of options trigger additional internal work when they change. This page describes those categories.

## Mapping-affecting options

The following options are passed to device models via the OSDI interface and may influence whether a device collapses internal nodes:

`tnom`, `temp`, `gmin`, `gdev`, `minr`, `scale`, `reltol`, `vntol`, `abstol`, `chgtol`, `fluxtol`

When any of these changes, the simulator re-evaluates node collapsing for all devices before the next analysis. If the collapsing pattern differs from the previous elaboration the system of equations is rebuilt.

## Parametrization-affecting options

The options `temp` and `scale` are exposed as the special identifiers `$temp` and `$scale` respectively (see [Special Identifiers](expr-special.md)). Any instance or model parameter expression that references `$temp` or `$scale` is re-evaluated when the corresponding option changes. This propagates through the entire instance hierarchy.

```text
options temp=85   // $temp becomes 85 everywhere in the netlist
```

## Tolerance-affecting options

The following options control how tolerances are assigned to circuit unknowns:

`tolmode`, `abstol`, `chgtol`, `vntol`, `fluxtol`

When any of these changes, tolerance values are re-assigned to all unknowns. The `tolmode` option selects the assignment strategy (`spice`, `va`, or `mixed`); the others set the corresponding absolute tolerance floors for the `spice` and the `mixed` strategy.

## Hierarchy-affecting options

No options in the current version affect circuit topology. This category is reserved for future use. If a hierarchy-affecting option were changed it would trigger a full re-elaboration of the affected subcircuit instances.
