# Incremental DC Analysis

Incremental DC analysis computes the linear response to small changes in independent sources (voltage and current excitations) around a DC operating point.

## Overview

Given a DC operating point solution $x^*$ (computed by solving $f(x, t_0) - u_0 = 0$), incremental DC analysis applies a small perturbation $\Delta u$ to the independent sources and solves the linearized circuit equations to obtain the resulting changes $\Delta x$ in node potentials and branch currents.

## Mathematical Formulation

At the operating point $x^*$, the resistive equation is satisfied, where $f(x, t_0)$ contains all terms except independent sources, and $u_0$ represents their contribution:

$$f(x^*, t_0) - u_0 = 0.$$

When independent sources are perturbed by $\Delta u$, the residual becomes:

$$f(x^* + \Delta x, t_0) - (u_0 + \Delta u) = 0.$$

Linearizing around the operating point:

$$f(x^*, t_0) + J_r(x^*, t_0)\,\Delta x - u_0 - \Delta u = 0,$$

where $J_r(x^*, t_0) = \frac{\partial f}{\partial x}$ is the resistive Jacobian at the operating point (see equations.md).

Since $f(x^*, t_0) - u_0 = 0$ at the operating point, the incremental system simplifies to:

$$J_r(x^*, t_0)\,\Delta x = \Delta u.$$

Solving this linear system yields the incremental response $\Delta x$.

## Source contributions and $mfactor

The right-hand side $\Delta u$ is assembled from the increment contributions of each independent source. The increment of source $\ell$ is its `mag` parameter (the same field that, in AC analysis, gives the source's magnitude). The source's `$mfactor` $m$ enters differently for the two source types:

- **Voltage source:** the increment is loaded into the source's branch-equation row only - the constraint $v_+ - v_- = \mathrm{mag}_\ell$ is imposed regardless of `$mfactor`. All $m$ parallel instances impose the same voltage, so $m$ does not appear in $\Delta u$:

  $$\Delta u \;\mathrel{+}=\; \mathrm{mag}_\ell\,\big(e_{e_1^{(\ell)}} - e_{e_2^{(\ell)}}\big).$$

- **Current source:** the increment is summed into the KCL equations of the two terminal nodes, scaled by `$mfactor` because $m$ parallel sources each push $\mathrm{mag}_\ell$ amperes, for a total of $m\,\mathrm{mag}_\ell$:

  $$\Delta u \;\mathrel{+}=\; -m\,\mathrm{mag}_\ell\,\big(e_{p^{(\ell)}} - e_{n^{(\ell)}}\big),$$

  with $(p, n)$ the source's "current-pulled-from" and "current-pushed-into" terminal unknowns (so the KCL RHS sees a negative contribution at $p$).

For interpretation: a voltage source with $\mathrm{mag} = 1$ always represents a 1 V increment at the port. A current source with $\mathrm{mag} = 1$ and `$mfactor` $= m$ represents an $m$ A total increment, so the response at any node is $m$ times what a single 1 A increment would produce. This convention matches the one used by AC analysis (see [ac.md](ac.md)) - effectively, incremental DC is the $\omega = 0$ limit of the AC formulation, with the reactive Jacobian dropped.

## Example: Current Source Contribution

Consider an independent current source with `mag` $= \Delta I_s$ and `$mfactor` $= m$, pulling from node $i$ and pushing into node $j$. In the residual formulation with $f(x) - u_0 = 0$, the contributions to $u_0$ are:

- To the $i$-th KCL equation (node $i$): $u_{0,i} = -m\,I_s$
- To the $j$-th KCL equation (node $j$): $u_{0,j} = +m\,I_s$

If the current source is perturbed by $\Delta I_s$, then $\Delta u_i = -m\,\Delta I_s$ and $\Delta u_j = +m\,\Delta I_s$. Solving $J_r(x^*, t_0)\,\Delta x = \Delta u$ yields the changes in all node voltages and currents resulting from this perturbation. For $m = 1$ this reduces to the per-instance contribution.
