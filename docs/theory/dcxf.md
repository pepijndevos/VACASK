# DC Transfer Function Analysis

DC transfer function analysis computes the linear relationship between independent sources and a chosen output variable (typically a node voltage) at a DC operating point.

## Overview

Given a DC operating point $x^*$ and an output variable of interest (a node voltage $v_k$ or a more general linear function of the incremental solution), DC transfer function analysis computes the transfer functions to an output from all independent sources. Instead of solving $n$ separate incremental DC problems (one for each source), the adjoint system is used to compute all transfer functions in a single linear solve.

## Problem Statement

The goal is to compute the transfer function vector $\mathbf{H}$, where each component is:

$$H_\ell = \frac{\partial v_{\text{out}}}{\partial u_\ell}$$

that is, the transfer function to output $v_{\text{out}}$ from the $\ell$-th independent source $u_\ell$. The output is defined as a linear combination of the components of the incremental solution

$$v_{\text{out}}=a^{\text{T}} \Delta x.$$

The incremental solution $\Delta x$ is obtained by solving 

$$J_r(x^*, t_0)\,\Delta x = u_\ell.$$

where $u_\ell$ is the unity incremental excitation originating from the $\ell$-th independent source. 
## Naive Approach: Multiple Incremental DC Solves

To compute each $H_\ell$ naively, one would solve:

$$J_r(x^*, t_0)\,\Delta x_\ell = u_\ell$$

for each source contribution $u_\ell$ (where $u_\ell$ is the $\ell$-th standard basis vector), then extract:

$$H_\ell = a^T \Delta x_\ell.$$

This requires $m$ linear solves (one per source).

## The Adjoint System

Instead, transpose the system and solve the **adjoint system** once:

$$J_r(x^*, t_0)^T\,\lambda = a,$$

where $\lambda$ is the adjoint variable. Once $\lambda$ is computed, all transfer functions are obtained directly:

$$H_\ell = a^T \Delta x_\ell = a^T J_r(x^*, t_0)^{-1}\,u_\ell = (J_r(x^*, t_0)^{-T}\,a)^T u_\ell = \lambda^T u_\ell.$$

For circuits with many sources and few outputs of interest, the adjoint method is dramatically faster. The trade-off is that each output requires one adjoint solve; if many different outputs are needed, the advantage diminishes.

## Source scaling and $mfactor

The transfer function reported for source $\ell$ is the gain from its `mag` parameter to the output,

$$H_\ell \;=\; \frac{\partial v_{\text{out}}}{\partial\,\mathrm{mag}_\ell}.$$

For a voltage source `mag` is the imposed terminal voltage, independent of `$mfactor`. For a current source `mag` is the per-instance current; with `$mfactor` $= m$ the $m$ parallel instances together inject $m\,\mathrm{mag}$ amperes, so $H_\ell$ absorbs the `$mfactor` scaling and is thus proportional to `$mfactor`.

The input impedance $Z_{\text{in},\ell}$ and admittance $Y_{\text{in},\ell}$ are evaluated **at the source's terminals**. $Z_{\text{in}}$ and $Y_{\text{in}}$ are intrinsic properties of the linear circuit at the port and are **independent of `$mfactor`**. This convention matches the one used by AC transfer function analysis (see [acxf.md](acxf.md)) - effectively, incremental DC is the $\omega = 0$ limit of the AC transfer function formulation, with the reactive Jacobian dropped. 

## Why the naive approach is used in practice

The adjoint formulation is optimal when all transfer functions share a **single, fixed output functional** $a$: one adjoint solve $J_r^{\text{T}} \lambda = a$ yields every $H_\ell$ by an inner product with $u_\ell$.

That assumption is broken by $Y_{\text{in},\ell}$ and $Z_{\text{in},\ell}$. Each of those reads the branch response $\Delta x_{\ell,r_1} - \Delta x_{\ell,r_2}$ at the **same source** that supplied the excitation; the output functional therefore changes from source to source ($a^{(\ell)} = e_{r_1^{(\ell)}} - e_{r_2^{(\ell)}}$). Recovering the per-source response unknowns through the adjoint would require one separate adjoint solve per source, so the adjoint cost grows with the source count just as the naive forward cost does - and the forward solve has the advantage of producing $H_\ell$ and the branch response in a single pass through the factored matrix.

For this reason VACASK's implementation uses the **naive forward approach**: one factorization of $J_r(x^*, t_0)$ followed by one solve per source. The adjoint formulation above is retained as a reference because it remains the appropriate strategy whenever only transfer functions to a fixed output are needed and input admittance/impedance are not requested.
