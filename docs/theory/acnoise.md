# AC Noise Analysis

AC noise analysis computes the small-signal noise power spectral density (PSD) at a chosen output, decomposed into per-instance and per-source contributions, together with the power gain from a designated input source. The analysis sweeps frequency across a user-specified range.

## Overview

About a DC operating point $x^*$ the circuit is linearized as in [AC analysis](ac.md):

$$[J_r(x^*) + j\omega\, J_c(x^*)]\,\tilde X = \Delta U.$$

The output is a single node or a pair of nodes; the output variable is the corresponding (single-ended or differential) phasor

$$V_{\text{out}}(\omega) \;=\; \tilde X_{u^+}(\omega) - \tilde X_{u^-}(\omega)$$

with $u^- = $ ground if a single node was given. Noise contributions are computed independently for every noise source in every device instance, summed coherently in power (uncorrelated sources), and aggregated into per-instance totals and an overall output noise PSD.

The operating point and the frozen Jacobians at $x^*$ are computed once. Per frequency the cost is one reactive load, one factorization of $J_r + j\omega J_c$, and one right-hand-side solve per excitation: one for the input source plus one for every noise source in the circuit.

## Power gain

The input source designated by the `in` parameter is excited with its **scaled unit excitation** $u_0$ (handled by the source instance, accounting for `$mfactor`), all other sources held at zero. Solving the AC system gives a phasor solution $\tilde X^{(\text{in})}$, and the transfer function from input to output is

$$H_{\text{in}}(\omega) \;=\; \tilde X^{(\text{in})}_{u^+} - \tilde X^{(\text{in})}_{u^-}.$$

The reported **power gain** is

$$G(\omega) \;=\; |H_{\text{in}}(\omega)|^2.$$

For a voltage source `$mfactor` has no effect on $H_{\text{in}}$. For a current source `$mfactor` scales the injected current, so $H_{\text{in}}$ is the gain from the source's `mag` (the magnitude written in the netlist). Input-referred noise uses the source's `mag` as the input signal. 

## Per-source noise contribution

Each device instance reports a list of noise sources to the simulator. For each source $k$ in instance $\ell$:

1. The source's PSD $S_{\ell,k}(\omega)$ is evaluated by the device model.
2. A unit excitation is injected at the source's excitation node pair $(e_1, e_2)$; the AC system is solved with the existing factorization, yielding a phasor solution $\tilde X^{(\ell,k)}$.
3. The noise-source-to-output transfer function and its power gain are

   $$H_{\ell,k}(\omega) \;=\; \tilde X^{(\ell,k)}_{u^+} - \tilde X^{(\ell,k)}_{u^-},
     \qquad G_{\ell,k}(\omega) \;=\; |H_{\ell,k}(\omega)|^2.$$

4. The source's contribution to the output noise PSD is

   $$N_{\ell,k}(\omega) \;=\; G_{\ell,k}(\omega) \cdot |S_{\ell,k}(\omega)|.$$

The absolute value on $S_{\ell,k}$ is intentional: some compact-model flicker-noise formulations can yield numerically negative PSD values at certain bias points (see Coram et al., *Flicker Noise Formulations in Compact Models*, IEEE TCAD vol. 39, 2020). These negative values are needed for correct flicker noise simulation in transient noise analysis. Treating the absolute value as the contribution preserves the physical meaning. 

## Adjoint formulation

The direct approach above solves one $n \times n$ complex linear system per noise source plus one for the input - a total of $N_{\text{src}} + 1$ solves per frequency, where $N_{\text{src}}$ is the number of noise sources in the circuit. For circuits with many noisy devices this dominates the runtime, but every solve uses the **same** matrix $J_{AC}(\omega) = J_r + j\omega J_c$ and differs only in the right-hand side. The observed output is common to all these solves. The adjoint reformulation collapses them into a single one per frequency.

The output is a fixed linear functional of the phasor solution:

$$V_{\text{out}}(\omega) \;=\; a^{\text{T}}\,\tilde X(\omega), \qquad a \;=\; e_{u^+} - e_{u^-},$$

where $e_i$ is the standard basis vector with a $1$ in position $i$. The vector $a$ is real, sparse (one or two nonzero entries), and frequency-independent.

For any small-signal excitation $U$ (an input source or a noise source), $\tilde X = J_{AC}^{-1} U$, so

$$H(\omega) \;=\; a^{\text{T}}\,J_{AC}^{-1}\,U \;=\; \big(J_{AC}^{-\text{T}}\,a\big)^{\text{T}}\,U \;=\; \lambda(\omega)^{\text{T}}\,U,$$

where $\lambda(\omega)$ is the **adjoint phasor** defined by

$$J_{AC}(\omega)^{\text{T}}\,\lambda(\omega) \;=\; a.$$

Note the plain (non-conjugate) transpose: the duality used here is the bilinear $a^{\text{T}} \tilde X$, not the Hermitian inner product. The same KLU factorization of $J_{AC}$ used by the direct approach also supports a transpose solve, so no extra factorization is needed - one back-substitution pair per frequency suffices to obtain $\lambda$.

### Extracting individual transfer functions

Every source excitation in this analysis has the same sparse structure: a unit phasor contribution at one node pair $(e_1, e_2)$, i.e. $U = e_{e_1} - e_{e_2}$. The inner product collapses to a difference of two entries of $\lambda$:

$$H(\omega) \;=\; \lambda_{e_1}(\omega) - \lambda_{e_2}(\omega).$$

Hence, from one adjoint solve per frequency:

- **Input transfer function** (and power gain):

  $$H_{\text{in}}(\omega) \;=\; u_0 \big(\lambda_{e_1^{\text{in}}} - \lambda_{e_2^{\text{in}}}\big),
    \qquad G(\omega) \;=\; |H_{\text{in}}(\omega)|^2,$$

  where $u_0$ is the input source's scaled unit excitation (the same factor the direct approach injects into the RHS). It equals 1 for a voltage source and $m$ for a current source. 

- **Per-source noise transfer functions:**

  $$H_{\ell,k}(\omega) \;=\; \lambda_{e_1^{(\ell,k)}}(\omega) - \lambda_{e_2^{(\ell,k)}}(\omega),
    \qquad G_{\ell,k}(\omega) \;=\; |H_{\ell,k}(\omega)|^2.$$

  The per-source PSD contributions $N_{\ell,k} = G_{\ell,k}\,|S_{\ell,k}|$ and the aggregation below are unchanged.

### Cost

The adjoint reformulation replaces $N_{\text{src}} + 1$ back-substitutions per frequency with one back-substitution plus $N_{\text{src}} + 1$ two-entry lookups in $\lambda$. 

## Aggregation

Per-instance noise:

$$N_\ell(\omega) \;=\; \sum_{k} N_{\ell,k}(\omega).$$

Total output noise PSD (summing all instances that own at least one noise source):

$$S_{\text{out}}(\omega) \;=\; \sum_\ell N_\ell(\omega).$$

All cross-source terms vanish under the standard assumption that distinct noise sources are mutually uncorrelated, which is why contributions add in power rather than amplitude.

## Input-referred noise

The output noise PSD $S_{\text{out}}(\omega)$ describes the noise as measured at the output. To compare designs whose gains differ, or to compare against a specification given at the input port, the noise is **referred back to the input** by dividing out the input-to-output power gain:

$$S_{\text{in}}(\omega) \;=\; \frac{S_{\text{out}}(\omega)}{G(\omega)} \;=\; \frac{S_{\text{out}}(\omega)}{|H_{\text{in}}(\omega)|^2}.$$

This is the PSD of an equivalent fictitious noise source placed at the input that, in a noiseless copy of the circuit, would produce the same output noise.

**Units.** Because $G$ is the gain from the input source's `mag` to the output voltage, $S_{\text{in}}$ inherits the unit of `mag` squared per hertz:

- Voltage-source input: `mag` is in volts, so $S_{\text{in}}$ has units of $\mathrm{V}^2/\mathrm{Hz}$.
- Current-source input: `mag` is in amperes, so $S_{\text{in}}$ has units of $\mathrm{A}^2/\mathrm{Hz}$.

In either case the result is the conventional "input-referred noise spectral density" with the units appropriate to the input source type.

**Reporting.** $S_{\text{in}}$ is not stored as a separate column in the rawfile - the user computes it in post-processing as `onoise / gain`. The same formula yields a per-source or per-instance input-referred noise by replacing $S_{\text{out}}$ with $N_{\ell,k}$ or $N_\ell$. Take care near frequencies where $G \to 0$ (output is decoupled from the input): $S_{\text{in}}$ diverges there even though the circuit is well behaved, and the ratio should be interpreted only over the band where the gain is meaningfully nonzero.
