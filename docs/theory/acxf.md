# AC Transfer Function Analysis

AC transfer function analysis computes the frequency-dependent linear relationship between independent sources and a chosen output variable in the frequency domain.

## Overview

Given a DC operating point $x^*$ and an output variable of interest (a node voltage or a more general linear function of the AC response), AC transfer function analysis computes the transfer functions to an output from all independent sources at each frequency $\omega$. Instead of solving $m$ separate AC problems (one for each source), the adjoint system is used to compute all transfer functions in a single solve per frequency.

## Problem Statement

The goal is to compute the frequency-dependent transfer function vector $\mathbf{H}(\omega)$, where each component is:

$$H_\ell(\omega) = \frac{\partial V_{\text{out}}(\omega)}{\partial U_\ell(\omega)}$$

that is, the transfer function of the output phasor $V_{\text{out}}(\omega)$ to the $\ell$-th independent source phasor $U_\ell(\omega)$. The output is defined as a linear combination of the phasor response:

$$V_{\text{out}}(\omega) = a^{\text{T}} \tilde X(\omega),$$

where $\tilde X(\omega)$ is the phasor response obtained by solving:

$$J_{AC}(\omega)\,\tilde X = U_\ell(\omega),$$

where $U_\ell(\omega)$ is the unity phasor excitation from the $\ell$-th independent source.

## Naive Approach: Multiple AC Solves

To compute each $H_\ell(\omega)$ naively at a given frequency $\omega$, one would solve:

$$J_{AC}(\omega)\,\tilde X_\ell = U_\ell(\omega)$$

for each source $\ell = 1, \ldots, m$, then extract:

$$H_\ell(\omega) = a^T \tilde X_\ell.$$

This requires $m$ complex linear solves per frequency.

## The Adjoint System

Instead, take the Hermitian transpose (conjugate transpose) of the system and solve the **adjoint system** once per frequency:

$$J_{AC}(\omega)^T\,\lambda = a,$$

where $\lambda$ is the adjoint phasor (complex vector). Once $\lambda$ is computed, all transfer functions are obtained directly:

$$H_\ell(\omega) = \lambda^T U_\ell(\omega).$$

## Why This Works

By duality:

$$H_\ell(\omega) = a^T \tilde X_\ell = a^T J_{AC}(\omega)^{-1}\,U_\ell(\omega) = (J_{AC}(\omega)^{-T}\,a)^T U_\ell(\omega) = \lambda(\omega)^T U_\ell(\omega),$$

where $J_{AC}(\omega)^{-T} = (J_{AC}(\omega)^T)^{-1} = (J_{AC}(\omega)^{-1})^T$. Thus the adjoint solution $\lambda(\omega)$ directly gives all transfer functions via inner products with the source phasors.

## Source scaling and $mfactor

The transfer function reported for source $\ell$ is the gain from its `mag` parameter to the output, assuming that its `phase` is 0,

For a voltage source `mag` is the imposed terminal voltage, independent of `$mfactor`. For a current source `mag` is the per-instance current; with `$mfactor` $= m$ the $m$ parallel instances together inject $m\,\mathrm{mag}$ amperes, so $H_\ell$ absorbs the `$mfactor` scaling and is thus proportional to `$mfactor`.

The input impedance $Z_{\text{in},\ell}$ and admittance $Y_{\text{in},\ell}$ are evaluated **at the source's terminals**. $Z_{\text{in}}$ and $Y_{\text{in}}$ are intrinsic properties of the linear circuit at the port and are **independent of `$mfactor`**. 

## Why the naive approach is used in practice

The adjoint formulation is optimal when all transfer functions share a **single, fixed output functional** $a$: one adjoint solve $J_{AC}^{\text{T}} \lambda = a$ per frequency yields every $H_\ell$ by an inner product with $U_\ell$.

That assumption is broken when computing input admittance/impedance. Each of those quantities reads the branch response $\tilde X_{\ell,r_1} - \tilde X_{\ell,r_2}$ at the **same source** that supplied the excitation; the output functional therefore changes from source to source ($a^{(\ell)} = e_{r_1^{(\ell)}} - e_{r_2^{(\ell)}}$). Recovering the per-source response unknowns through the adjoint would require one separate adjoint solve per source, so the adjoint cost grows with the source count just as the naive forward cost does - and the forward solve has the advantage of producing $H_\ell$ and the branch response in a single pass through the factored matrix.

For this reason VACASK's implementation uses the **naive forward approach**: one factorization of $J_{AC}(\omega)$ per frequency followed by one right-hand-side solve per source. The adjoint formulation above is retained as a reference because it remains the appropriate strategy whenever only transfer functions to a fixed output are needed (e.g. inside larger sweeps that do not request input impedance/admittance).
