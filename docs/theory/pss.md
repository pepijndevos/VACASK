  # PSS Analysis - Implementation Notes

Working notes for implementing periodic steady-state (PSS) analysis in VACASK.
Not user documentation. This file collects the formulation, algorithm choices,
and integration points as the design is worked out.

## Circuit equations

VACASK assembles the circuit into a system of differential-algebraic equations (DAE)

$$f(x(t)) + \frac{d}{dt} q(x(t)) = 0$$

where

- $x(t) \in \mathbb{R}^n$ is the vector of circuit unknowns (MNA node potentials and
  the extra branch currents introduced by voltage sources, inductors, etc.),
- $f : \mathbb{R}^n \to \mathbb{R}^n$ is the resistive (static) residual,
- $q : \mathbb{R}^n \to \mathbb{R}^n$ is the reactive residual (node charges and
  branch fluxes), whose time derivative produces the dynamic currents,
- $n$ is the size of the MNA system.

This is the same formulation used by transient and harmonic balance analysis.

### Explicit time dependence (driven circuits)

The form above is autonomous in notation only. Independent sources make the
residuals depend explicitly on time, so in general

$$f(x(t), t) + \frac{d}{dt} q(x(t), t) = 0.$$

For PSS the excitation is periodic with a known period $T$, i.e.
$f(\cdot, t+T) = f(\cdot, t)$ and $q(\cdot, t+T) = q(\cdot, t)$. The reactive
residual carries no explicit time dependence in practice ($q = q(x)$); the
explicit $t$ enters $f$ through source values.

### Jacobians

Newton-based methods need the two circuit Jacobians evaluated at a given $x$:

$$G(x) = \frac{\partial f}{\partial x}, \qquad C(x) = \frac{\partial q}{\partial x},$$

the conductance matrix and the capacitance/reactive matrix. Both are already
assembled by the existing device evaluation path (used by transient and HB);
PSS reuses them.

## Periodicity condition

A periodic steady state is a solution that repeats with the period $T$ of the
excitation (or, for an autonomous circuit, with an unknown period $T$ to be
determined as part of the solve):

$$x(t + T) = x(t) \quad \text{for all } t,$$

equivalently the two-point boundary condition

$$x(T) = x(0).$$

PSS solves for the initial state $x(0)$ (and, in the autonomous case, $T$) that
satisfies this condition, instead of integrating from a given initial condition
until the transient dies out.

## Numerical integration of one step

Shooting-based PSS advances the state over one period by the same time-domain
integration used in transient analysis. The basic operation is a single step
from a known timepoint $t_k$ (state $x_k$) to the next timepoint
$t_{k+1} = t_k + h_k$, producing the new state $x_{k+1}$.

Write $q_j = q(x_j)$ for the reactive residual and $\dot q_j$ for its time
derivative at timepoint $t_j$. Let $\dot q \equiv \frac{d}{dt} q$.

### General integration formula

Start from the general implicit linear multistep (LMS) **ansatz**, expressing the
unknown state $q_{k+1}$ as a linear combination of past values $q_k, q_{k-1},
\ldots$ and both past and future derivatives $\dot q_{k+1}, \dot q_k, \dot q_{k-1},
\ldots$:

$$q_{k+1} = \sum_{i=0}^{m-1} \overline{a}_i\, q_{k-i}
  + h_k \sum_{i=-1}^{m-1} \overline{b}_i\, \dot q_{k-i},$$

with $h_k = t_{k+1} - t_k$ the current step size. The coefficients
$\overline{a}_i, \overline{b}_i$ are **dimensionless method constants**: they
depend on the method, its order, and (for variable step) the step-size
ratios. The leading future-derivative coefficient is
$\overline{b}_{-1}$ (the multiplier of $\dot q_{k+1}$); its non-vanishing is what
makes the method implicit.

This single ansatz covers the methods VACASK supports:

- **BDF / Gear**: only $\overline{b}_{-1} \neq 0$ among the $\overline{b}$'s — the
  new state is a combination of past $q$'s plus a contribution from $\dot q_{k+1}$
  alone. E.g. BDF1 ($q_{k+1} = q_k + h_k \dot q_{k+1}$): $\overline{a}_0 = 1$,
  $\overline{b}_{-1} = 1$, all others zero.
- **Adams-Moulton** (including trapezoidal): $\overline{a}_0 = 1$ and higher
  $\overline{a}_i = 0$, with several $\overline{b}_i$ nonzero. E.g. trapezoidal
  ($q_{k+1} = q_k + (h_k/2)(\dot q_{k+1} + \dot q_k)$): $\overline{a}_0 = 1$,
  $\overline{b}_{-1} = \overline{b}_0 = 1/2$.

Solve the ansatz for $\dot q_{k+1}$ by moving the $i=-1$ term of the $\dot q$ sum
to the LHS and dividing by $h_k \overline{b}_{-1}$:

$$\dot q_{k+1}
  = \frac{1}{h_k\overline{b}_{-1}}\,q_{k+1}
  - \sum_{i=0}^{m-1} \frac{\overline{a}_i}{h_k\overline{b}_{-1}}\,q_{k-i}
  - \sum_{i=0}^{m-1} \frac{\overline{b}_i}{\overline{b}_{-1}}\,\dot q_{k-i}.$$

For compactness, collect the rearranged coefficients into

$$a_{-1} = \frac{1}{h_k\overline{b}_{-1}},
  \qquad a_i = - \frac{\overline{a}_i}{h_k\overline{b}_{-1}}\ (i\ge 0),
  \qquad b_i = - \frac{\overline{b}_i}{\overline{b}_{-1}}\ (i\ge 0),$$

so the LMS reconstruction takes the compact form

$$\dot q_{k+1} =  a_{-1}\,q_{k+1} + \sum_{i=-1}^{m-1} a_i\, q_{k-i} + \sum_{i=0}^{m-1} b_i\, \dot q_{k-i}.$$

The first sum starts at $i=-1$: it includes the present, still-unknown value
$q_{k+1}$ (at $i=-1$, $q_{k-(-1)} = q_{k+1}$), which is what makes the method
implicit. The second sum starts at $i=0$, because the present derivative
$\dot q_{k+1}$ is the quantity being defined and only past derivatives appear on
the right. Here

- $a_{-1} = 1/(h_k\overline{b}_{-1})$ is the leading coefficient.
- $a_i$ ($i \geq 0$), $b_i$ ($i \geq 0$) are the remaining derivative-form
  coefficients.
- All $a_i$ carry an explicit $1/h_k$ from the rearrangement; 
  $b_i = -\overline{b}_i/\overline{b}_{-1}$ is dimensionless.

Rearranging the derivative-form gives

$$\dot q_{k+1} = a_{-1}\, q_{k+1} + a_0\, q_k + h_k\, b_0\, \dot q_k + s_k, \qquad
  s_k = \sum_{i=1}^{m-1} a_i\, q_{k-i} + h_k \sum_{i=1}^{m-1} b_i\, \dot q_{k-i}.$$

Three groups appear, distinguished by what they depend on:

- $a_{-1}\, q_{k+1}$ - the implicit term; depends on the unknown $x_{k+1}$.
- $a_0\, q_k + h_k\, b_0\, \dot q_k$ - depends on the previous state $x_k$ through
  $q_k = q(x_k)$ and the stored derivative $\dot q_k$. This is constant during the
  Newton solve at $t_{k+1}$, but **not** constant when differentiating with
  respect to $x_k$. It must be kept explicit for the one-step sensitivity
  $dx_{k+1}/dx_k$ used by the shooting method.
- $s_k$ - the **history term**, built only from timepoints $t_{k-1}$ and earlier.
  Constant both during the $t_{k+1}$ solve and with respect to $x_k$.

This single form covers the methods VACASK already supports:

- **BDF / Gear** set $b_i = 0$ - the derivative is built only from $q$ samples
  $q_{k+1}, q_k, q_{k-1}, \ldots$
- **Adams-Moulton** (including trapezoidal and backward Euler) also
  use stored past derivatives $\dot q_{k-i}$ if order is greater than 1.

### The stored derivative

The current step reuses $\dot q_k$, the derivative stored at $t_k$. That value was
produced by the **previous step** (from $t_{k-1}$ to $t_k$). Adaptive order/step
control means the previous step may use a different method, order, and step size,
hence different coefficients - mark them with a prime. Applying the same general
formula one step earlier,

$$\dot q_k = a_{-1}'\, q_k + a_0'\, q_{k-1} + h_{k-1}\, b_0'\, \dot q_{k-1} + s_{k-1}',
  \qquad s_{k-1}' = \sum_{i=1}^{m'-1} a_i'\, q_{k-1-i} + h_{k-1} \sum_{i=1}^{m'-1} b_i'\, \dot q_{k-1-i},$$

where $a_{-1}'$ is the previous step's leading coefficient,
$h_{k-1} = t_k - t_{k-1}$ is the previous step size, and $m'$ is its number of
steps. The explicit $h_{k-1}$ on the $\dot q$ sums uses the **previous** step
size, consistent with the convention applied one step earlier.

Of these terms only $a_{-1}'\, q_k$ depends on $x_k$; everything else comes from
$t_{k-1}$ and earlier. So the dependence of the stored derivative on $x_k$ is

$$\frac{\partial \dot q_k}{\partial x_k} = a_{-1}'\, C(x_k),$$

which is exactly what the one-step sensitivity $dx_{k+1}/dx_k$ needs. The
implementation must therefore keep, alongside $\dot q_k$, the previous step's
leading coefficient $a_{-1}'$.

### Per-timepoint nonlinear system

Substituting the derivative approximation into the DAE
$f(x_{k+1}, t_{k+1}) + \dot q_{k+1} = 0$ gives a nonlinear algebraic system in
the single unknown $x_{k+1}$:

$$R(x_{k+1}) \equiv f(x_{k+1}, t_{k+1}) + a_{-1}\, q(x_{k+1}) + a_0\, q_k + h_k\, b_0\, \dot q_k + s_k = 0.$$

During the Newton solve at $t_{k+1}$ the last three terms come from earlier
timepoints and are constant; only $f$ and $a_{-1}\, q$ vary with $x_{k+1}$.

Newton-Raphson solves it. Each iteration solves the linear system

$$J(x_{k+1})\, \Delta x = -R(x_{k+1}), \qquad x_{k+1} \leftarrow x_{k+1} + \Delta x,$$

with the iteration matrix

$$J(x_{k+1}) = G(x_{k+1}) + a_{-1}\, C(x_{k+1}),$$

where $G = \partial f/\partial x$ and $C = \partial q/\partial x$. This is exactly
the matrix VACASK already assembles for a transient timepoint, with $a_{-1}$ the
integration coefficient supplied by the chosen method.

After convergence, $\dot q_{k+1} = a_{-1} q_{k+1} + a_0 q_k + h_k\, b_0 \dot q_k + s_k$ is
stored so it can serve as a past derivative $\dot q$ in the history term of
subsequent steps (needed by Adams-type methods).

The converged step defines the one-step state map $x_k \mapsto x_{k+1}$; chaining
these maps across one period is the integration that the shooting method drives
toward the periodicity condition $x(T) = x(0)$.

### One-step sensitivity from the Newton solve

The shooting method needs the sensitivity $dx_{k+1}/dx_k$ of the converged step.
At convergence the residual vanishes identically in the previous state,
$R(x_{k+1}, x_k) = 0$, so differentiating with respect to $x_k$ gives

$$\frac{\partial R}{\partial x_{k+1}}\, \frac{dx_{k+1}}{dx_k}
  + \frac{\partial R}{\partial x_k} = 0.$$

The two partials reuse quantities the per-timepoint Newton already produced:

- $\dfrac{\partial R}{\partial x_{k+1}} = J = G(x_{k+1}) + a_{-1}\, C(x_{k+1})$ - the
  converged Newton iteration matrix, already assembled and factored.
- $\dfrac{\partial R}{\partial x_k}$ - only the $i=0$ terms $a_0\, q_k + h_k\, b_0\, \dot q_k$
  in $R$ depend on $x_k$. With $\partial q_k/\partial x_k = C(x_k)$ and
  $\partial \dot q_k/\partial x_k = a_{-1}'\, C(x_k)$ from the previous step,

$$\frac{\partial R}{\partial x_k} = a_0\, C(x_k) + h_k\, b_0\, a_{-1}'\, C(x_k)
  = (a_0 + h_k\, b_0\, a_{-1}')\, C(x_k).$$

Hence the one-step sensitivity is

$$\frac{dx_{k+1}}{dx_k} = -J^{-1} (a_0 + h_k\, b_0\, a_{-1}')\, C(x_k)
  = -\big(G(x_{k+1}) + a_{-1}\, C(x_{k+1})\big)^{-1} (a_0 + h_k\, b_0\, a_{-1}')\, C(x_k).$$

The same factored $J$ from the last Newton iteration is reused, so no extra
factorization is needed - only solves against the right-hand side
$(a_0 + h_k\, b_0\, a_{-1}')\, C(x_k)$. This is the per-step factor of the monodromy matrix
that the shooting method accumulates over one period.

For a multistep method of order > 2, $R$ also depends on $x_{k-1}, x_{k-2}, \ldots$
through $s_k$; the expression above is the partial $\partial x_{k+1}/\partial x_k$
that holds those older states fixed (valid when the shooting sensitivity is built
on a one-step-equivalent or augmented state).

## Comparison with Djurhuus and Krozer

Reference: Djurhuus and Krozer, arXiv:2512.10373v1
(https://arxiv.org/html/2512.10373v1). A time-domain (shooting) PSS module added to
the open-source qucsator transient engine, for both driven and autonomous circuits.

### Notation mapping

| Their notation | Our notation | Meaning |
|---|---|---|
| $\dot q(x) + i(x) + s(t) = 0$ | $f(x,t) + \frac{d}{dt}q(x) = 0$ | DAE; their $i(x)+s(t)$ is our $f(x,t)$ |
| $A(t) = a_0\,C(t) + G(t)$ | $J = G + a_{-1}\,C$ | timepoint Jacobian; their $a_0$ = our $a_{-1}$, the LMS (linear multistep) leading coefficient |
| $F_p(x_0) = x_0 - x_T(x_0) = 0$ | $x(T) = x(0)$ | single-shooting periodicity condition |
| $\Phi_T = \partial x_T/\partial x_0$ | $\prod_k dx_{k+1}/dx_k$ | monodromy / state-transition matrix |
| $\dot q + i + s = 0$ linearized: $\frac{d}{dt}(C\,\delta x) + G\,\delta x = 0$ | $dx_{k+1}/dx_k$ recursion | linear-response (variational) equation |

### Agreement

- Same DAE form and same timepoint Jacobian structure (their $a_0\,C + G$, our
  $a_{-1}\,C + G$), with the LMS-method-and-order-dependent leading coefficient
  identical to our $a_{-1}$ (modulo the index shift). They even
  recover $C(t)$ by subtracting $G$ from $A$ and dividing by their $a_0$ (our
  $a_{-1}$), the same decomposition we rely on.
- Same single-shooting periodicity statement and the same monodromy-based picture
  of the sensitivity.

### Where our notes go further

- **Explicit discrete per-step sensitivity.** They build $\Phi_T$ by integrating
  the continuous linear-response equation $\frac{d}{dt}(C\,\delta x) + G\,\delta x = 0$
  "in parallel with the full solution," and require it to follow "the exact same
  LMS method & order, time-step mesh, and histories" as the transient solver - but
  they give no explicit per-step update. Our
  $dx_{k+1}/dx_k = -(G + a_{-1} C)^{-1}(a_0 + h_k\,b_0\,a_{-1}')\,C(x_k)$ is exactly that
  update: differentiating the converged discrete step (internal differentiation)
  is the discretization of their LR equation under the same LMS scheme, so the
  LMS-matching they demand holds by construction rather than by careful bookkeeping.
- **Factorization reuse.** We reuse the already-factored timepoint $J$ for the
  sensitivity back-solves; the paper does not discuss factorization caching.
- **Variable order/step + stored derivative.** We carry the previous step's leading
  coefficient $a_{-1}'$ (and the $a_0, b_0$ split) so the sensitivity stays correct
  across order/step changes. The paper only states the meshes must match.

### Krozer's LR integration, discretized

The paper integrates the continuous linear-response (variational) equation but
gives no per-step formula. Discretizing it with the nominal LMS scheme recovers
our recursion, which is the explicit form of what they do "in parallel with the
full solution."

Linearizing the DAE about the nominal trajectory gives the LR equation

$$\frac{d}{dt}\big(C(t)\,\delta x(t)\big) + G(t)\,\delta x(t) = 0.$$

Introduce the **charge variation** $\delta q(t) = C(t)\,\delta x(t)$ (because
$\delta q = \tfrac{\partial q}{\partial x}\,\delta x = C\,\delta x$). The LR
equation becomes

$$G(t)\,\delta x + \dot{\delta q} = 0, \qquad \delta q = C(t)\,\delta x,$$

which has the **same DAE structure** as the nominal system: resistive term
$G\,\delta x$ (now linear in $\delta x$) plus reactive term $\delta q$. Krozer
requires the LR integration to reuse the nominal LMS method, order, mesh, and
histories - so apply the same integration formula to $\delta q$ at $t_{k+1}$:

$$\dot{\delta q}_{k+1} = a_{-1}\,\delta q_{k+1} + a_0\,\delta q_k + h_k\,b_0\,\dot{\delta q}_k + s_k^{\delta},
  \qquad s_k^{\delta} = \sum_{i=1}^{m-1} a_i\,\delta q_{k-i} + h_k \sum_{i=1}^{m-1} b_i\,\dot{\delta q}_{k-i},$$

with the **same** coefficients $a_i, b_i$ (and the same explicit $h_k$ scaling on
the $\dot{\delta q}$ sums) as the nominal step. Substituting into the discretized
LR equation $G_{k+1}\,\delta x_{k+1} + \dot{\delta q}_{k+1} = 0$ and using
$\delta q_j = C_j\,\delta x_j$,

$$\underbrace{(G_{k+1} + a_{-1} C_{k+1})}_{J_{k+1}}\,\delta x_{k+1}
  + a_0 C_k\,\delta x_k + h_k\,b_0\,\dot{\delta q}_k + s_k^{\delta} = 0.$$

Substitute $\dot{\delta q}_k$ from the previous step's integration formula (primed
coefficients because the previous step may use a different method and order,
mirroring the nominal $\dot q_k$ expansion, with the explicit factor $h_{k-1}$ on
the $\dot{\delta q}$ sums):

$$\dot{\delta q}_k = a_{-1}'\,\delta q_k + a_0'\,\delta q_{k-1}
  + h_{k-1}\,b_0'\,\dot{\delta q}_{k-1} + s_{k-1}^{\delta'},
  \qquad s_{k-1}^{\delta'} = \sum_{i=1}^{m'-1} a_i'\,\delta q_{k-1-i}
  + h_{k-1} \sum_{i=1}^{m'-1} b_i'\,\dot{\delta q}_{k-1-i}.$$

Using $\delta q_j = C_j\,\delta x_j$ and inserting into the equation above:

$$J_{k+1}\,\delta x_{k+1}
  + \underbrace{(a_0 + h_k\,b_0 a_{-1}')\,C_k\,\delta x_k}_{\text{depends on }t_k}
  + \underbrace{h_k\,b_0\,a_0'\,C_{k-1}\,\delta x_{k-1}
    + h_k\,h_{k-1}\,b_0 b_0'\,\dot{\delta q}_{k-1}
    + h_k\,b_0\,s_{k-1}^{\delta'} + s_k^{\delta}}_{\text{depends on }t_{k-1}\text{ and earlier}}
  = 0.$$

Every term after $(a_0 + h_k\,b_0 a_{-1}')\,C_k\,\delta x_k$ belongs to timepoints
$t_{k-1}$ and earlier. Differentiating with respect to $\delta x_k$ those terms
have zero partial, so

$$J_{k+1}\,\frac{\partial\,\delta x_{k+1}}{\partial\,\delta x_k}
  + (a_0 + h_k\,b_0 a_{-1}')\,C_k = 0.$$

The per-step sensitivity is therefore

$$\boxed{\frac{\partial\,\delta x_{k+1}}{\partial\,\delta x_k}
  = M_k = -J_{k+1}^{-1}\,(a_0 + h_k\,b_0\,a_{-1}')\,C_k.}$$

After the $\dot{\delta q}_k$ substitution the $\delta x_k$ dependence is fully
isolated in the single term $(a_0+h_k\,b_0 a_{-1}')\,C_k\,\delta x_k$, so the partial
is exact and complete. The result is identical to the one-step sensitivity derived
from the nonlinear residual in the section above, confirming that differentiating
the converged discrete step and discretizing the continuous LR equation with the
same LMS scheme give the same per-step propagator.

## Monodromy accumulation

The **monodromy matrix** $\Phi_T = \partial x_T/\partial x_0$ is the sensitivity of
the state after one period to the initial state — equivalently, the product of the
per-step propagators along the discrete trajectory:

$$\Phi_T = \frac{\partial x_T}{\partial x_0}
  = \frac{\partial x_N}{\partial x_{N-1}}\,
    \frac{\partial x_{N-1}}{\partial x_{N-2}}\,\cdots\,
    \frac{\partial x_1}{\partial x_0}
  = \prod_{k=N-1}^{0} M_k.$$

Its eigenvalues are the Floquet multipliers; one of them is exactly $1$ in the
autonomous case, all lie strictly inside the unit circle for a stable driven limit
cycle. PSS Newton needs $\Phi_T$ to assemble the Jacobian of the periodicity
condition.

Computationally $\Phi_T$ is built by applying the per-step propagator $M_k$ to a
running matrix, initialized to the identity, in the same forward sweep as the
nominal transient integration:

$$\Phi_0 = I, \qquad \Phi_{k+1} = M_k\,\Phi_k = -J_{k+1}^{-1}(a_0 + h_k\,b_0\,a_{-1}')\,C_k\,\Phi_k.$$

After $N$ steps $\Phi_T = \Phi_N$ is the monodromy. The four operations per step are:

1. **Scale and multiply:** $W = (a_0 + h_k\,b_0\,a_{-1}')\,C_k\,\Phi_k$.
   $C_k$ is sparse and already evaluated at the converged $x_k$; $(a_0+h_k\,b_0 a_{-1}')$
   is a scalar. This is a sparse $\times$ dense product, $O(nnz(C_k)\cdot n)$.
2. **Negate:** $W \leftarrow -W$.
3. **Solve:** $J_{k+1}\,\Phi_{k+1} = W$, i.e. $n$ back-solves against the
   LU factorization of $J_{k+1} = G(x_{k+1}) + a_{-1}\,C(x_{k+1})$ that the transient
   Newton already produced at $t_{k+1}$. No re-factorization needed.
4. **Shift history:** store $\Phi_{k+1}$ for the next step.

### First step ($k=0$)

The first step of the period integration always uses the **order-1 method** (backward
Euler / BDF1), because there is no LMS history yet. This means $b_0 = 0$ and $s_0 = 0$,
so the formula reduces to

$$M_0 = -J_1^{-1}\,a_0\,C_0, \qquad a_0 = -\frac{1}{h_0},$$

and $a_{-1}'$ does not appear. From step $k=1$ onward the integrator has a genuine
predecessor step and $a_{-1}'$ is the leading coefficient of that predecessor.

The factorization of $J_{k+1}$ is reused from the last Newton iteration of the
nominal transient step - so the sensitivity propagation adds only back-solves,
not factorizations. Both must happen in the same forward sweep: the LU of $J_{k+1}$
is needed immediately after step $k+1$ converges, before it is overwritten by the
next step.

## PSS Newton-Raphson: driven circuit

For a driven circuit the excitation period $T$ is known. The problem is to find
$x_0 \in \mathbb{R}^n$ such that the state after one period equals the initial state:

$$F_p(x_0) = x_0 - x_T(x_0) = 0.$$

$x_T(x_0)$ denotes the state at $t = T$ obtained by integrating the DAE from $x_0$
over one period. The Jacobian of $F_p$ is

$$J_p = \frac{\partial F_p}{\partial x_0} = I - \Phi_T,$$

where $\Phi_T$ is the monodromy matrix. The Newton-Raphson iteration is

$$\boxed{(I - \Phi_T^{(l)})\,\Delta x_0^{(l)} = x_T^{(l)} - x_0^{(l)}, \qquad
  x_0^{(l+1)} = x_0^{(l)} + \Delta x_0^{(l)}.}$$

### Per-iteration work

Each iteration requires one forward integration of the DAE over one period:

- **Residual** $x_T^{(l)}$: the final state of the period integration started from
  $x_0^{(l)}$.
- **Jacobian** $\Phi_T^{(l)}$: the monodromy accumulated in the same forward sweep
  (no extra integration needed; see Monodromy accumulation above).
- **Newton solve**: factor the dense $n\times n$ matrix $I - \Phi_T^{(l)}$ and
  solve for $\Delta x_0^{(l)}$. Cost $O(n^3)$ once per shooting iteration.

### Nonsingularity

For a stable driven limit cycle all Floquet multipliers (eigenvalues of $\Phi_T$)
lie strictly inside the unit circle, so none equals 1 and $I - \Phi_T$ is
nonsingular. Convergence of Newton is quadratic.

### Initialization

A reasonable starting point $x_0^{(0)}$ is the state at $t = T$ of a long transient
run that has nearly settled to steady state, or the DC operating point.

## PSS Newton-Raphson: autonomous circuit

For an autonomous circuit the period $T$ is unknown. The augmented unknown is

$$\tilde x_0 = \begin{bmatrix} x_0 \\ T \end{bmatrix} \in \mathbb{R}^{n+1}.$$

The periodicity condition alone ($n$ equations for $n+1$ unknowns) is underdetermined
because the oscillator has a continuous family of solutions shifted in phase. A
**phase condition** pins one degree of freedom and closes the system. The
standard choice constrains the Newton update $\Delta x_0 = x_0^{(l+1)} - x_0^{(l)}$
itself, not $x_0$ against the origin:

$$c(\Delta x_0) = \alpha^T \Delta x_0 = 0,$$

where $\alpha = \dot x_s(0)$ is the velocity of the limit cycle at the reference
point $t = 0$. This enforces that the update $\Delta x_0$ is orthogonal to the
current trajectory direction, i.e. it has no phase-shift component - for any
$x_0^{(l)}$, since it constrains $\Delta x_0$ directly rather than pinning
$x_0$ itself to a fixed hyperplane through the origin. (Writing the condition
instead as $\alpha^Tx_0=0$ is not equivalent: unless $x_0^{(l)}$ already lies
on that hyperplane, its Newton linearization forces $\Delta x_0$ to pick up a
component along $\alpha$ to cancel the residual $\alpha^Tx_0^{(l)}$,
contradicting the orthogonality this condition is meant to enforce.)

The augmented residual is

$$\tilde F(\tilde x_0) = \begin{bmatrix} x_0 - x_T(x_0, T) \\ \alpha^T \Delta x_0 \end{bmatrix} = 0.$$

Its Jacobian with respect to $\tilde x_0 = [x_0,\ T]^T$ is

$$\tilde J_p = \begin{bmatrix} I - \Phi_T & -\Psi_T \\ \alpha^T & 0 \end{bmatrix} \in \mathbb{R}^{(n+1)\times(n+1)},$$

where $\Psi_T = \partial x_T/\partial T\!\restriction_{x_0}$ encodes how the final
state moves when $T$ is varied at fixed $x_0$. In the continuous theory this is
simply the velocity of the periodic solution, $\dot x_s(T) = \dot x_s(0)$, but in
the discrete shooting implementation $x_T$ is the output of the last LMS step and
$\Psi_T$ is not given by that velocity - it picks up an $O(h)$ correction from the
$T$-dependence of the truncated last step. The precise expression is derived in
[Computing $\Psi_T$](#computing-psi_t) below. The Newton update solves

$$\boxed{\tilde J_p^{(l)} \begin{bmatrix} \Delta x_0 \\ \Delta T \end{bmatrix}
  = \begin{bmatrix} x_T^{(l)} - x_0^{(l)} \\ 0 \end{bmatrix},
  \qquad \tilde x_0^{(l+1)} = \tilde x_0^{(l)} + \begin{bmatrix}\Delta x_0\\\Delta T\end{bmatrix}.}$$

The phase-condition row's RHS is exactly $0$, not $-\alpha^Tx_0^{(l)}$: since
the condition constrains $\Delta x_0$ directly, it contributes only through
the $\alpha^T$ row of $\tilde J_p$, never through a nonzero forcing term.

### Computing $\Psi_T$

$x_T = x_N$ is the output of the **last LMS step**, not the value of an analytic
trajectory at time $T$. The shooting integrator lays down a mesh
$0 = t_0 < t_1 < \cdots < t_N$ adaptively as it sweeps the period. The natural
adaptive step from $t_{N-1}$ would generally overshoot $T$, so it is truncated to
land exactly on $T$:

$$h_{N-1} = T - t_{N-1}.$$

When $T$ varies infinitesimally, the earlier mesh points $t_0, \ldots, t_{N-1}$ and
states $x_0, \ldots, x_{N-1}$ stay fixed — they were chosen on the way out, before
$T$ was reached — and only $h_{N-1}$ moves, with $dh_{N-1}/dT = 1$. So $\Psi_T$ is
the sensitivity of one discrete step to its own step size, not the continuous
velocity $\dot x_s(T)$.

**How $x_N$ is produced.** The last step enforces the DAE at $t_N$,

$$f(x_N) + \dot q_N = 0,$$

with $\dot q_N$ supplied by the LMS reconstruction (with the explicit $h_{N-1}$
factor on the $\dot q$ contribution as introduced earlier):

$$\dot q_N = a_{-1}(h_{N-1})\,q(x_N) + \sum_{i\ge0} a_i(h_{N-1})\,q_{N-1-i}
  + h_{N-1}\sum_{i\ge0} b_i(h_{N-1})\,\dot q_{N-1-i}.$$

The coefficients $a_i, b_i$ are fixed by the LMS method, its order, and the past
step ratios; their (and the explicit $h_{N-1}$ factor's) dependence on $h_{N-1}$ is
all we will need. Every retained $a_i, b_i$ depends on $h_{N-1}$ this way - not
just $a_0, b_0$, but also $a_1, a_2, \ldots$ and $b_1, b_2, \ldots$ whenever the
method retains more than one past point (BDF order $\ge2$, Adams-Moulton order
$\ge3$). Only the data $q_{N-1-i}, \dot q_{N-1-i}$ these coefficients multiply is
frozen, drawn from earlier, already-accepted timepoints. Newton-Raphson solves
this implicit equation for $x_N$, with iteration matrix
$J_N = G(x_N) + a_{-1}\,C(x_N)$ — already factored at convergence.

**Derivative w.r.t. $T$.** Treat $x_N$ as the implicit function $x_N(T)$ defined by
the converged last step, and apply $d/dT$ to both sides of $f(x_N) + \dot q_N = 0$.

*What depends on $T$ and what does not.* The last step's truncated size
$h_{N-1} = T - t_{N-1}$ is the only quantity that responds to $T$: $t_{N-1}$ is
frozen (it was set by the adaptive controller on the way out, before the truncation
to $T$ happened), and so are $x_{N-1}$ and all the earlier-state data
$q_{N-1-i}, \dot q_{N-1-i}$ ($i\ge0$). But through $h_{N-1}$, every retained LMS
coefficient $a_i(h_{N-1})$, $b_i(h_{N-1})$ depends on $T$ too - not just $a_{-1},
a_0, b_0$. And $x_N$ itself depends on $T$, because the step equation it satisfies
depends on $T$. So when we differentiate, the live pieces are $x_N(T)$ and all the
coefficients, with $dh_{N-1}/dT = 1$.

*Differentiating the DAE residual.* Apply chain rule to $f(x_N)$:

$$\frac{d}{dT}\,f(x_N) = G(x_N)\,\frac{dx_N}{dT}.$$

*Differentiating the LMS reconstruction.* $\dot q_N$ depends on $T$ both directly
(through the coefficients $a_i, b_i$, with $x_N$ held fixed) and indirectly (through
$x_N$, which enters $\dot q_N$ via the $i = -1$ term $a_{-1}\,q(x_N)$). Split it
accordingly:

$$\frac{d \dot q_N}{dT}
  = \underbrace{a_{-1}\,C(x_N)\,\frac{dx_N}{dT}}_{\text{via $x_N$ in }a_{-1}\,q(x_N)}
   + \underbrace{\frac{\partial \dot q_N}{\partial h_{N-1}}\bigg|_{x_N}}_{\text{via coefficients, $x_N$ frozen}},$$

where the coefficient-driven part is (applying $\partial/\partial h_{N-1}$ to the
explicit $h_{N-1}$ factor in front of the $\dot q$ sum as well). From here on,
within this "Computing $\Psi_T$" section only, a prime denotes
$\partial/\partial h_{N-1}$ - matching [numint.md](numint.md)'s
$a_{-1}', a_i', b_i'$ convention - **not** the "previous step's coefficient"
prime used in [Monodromy accumulation](#monodromy-accumulation) above:

$$\frac{\partial \dot q_N}{\partial h_{N-1}}\bigg|_{x_N}
  = a_{-1}'\,q(x_N)
   + \sum_{i\ge 0} a_i'\,q_{N-1-i}
   + \sum_{i\ge 0} b_i\,\dot q_{N-1-i}
   + h_{N-1}\sum_{i\ge 0} b_i'\,\dot q_{N-1-i}.$$

Note that $q_{N-1}, \dot q_{N-1}$ etc. carry no $T$-dependence (they sit at frozen
earlier states), so the only thing the coefficients multiply that could move with
$T$ is the $i=-1$ piece $a_{-1}\,q(x_N)$, which is already pulled out into the
$x_N$-channel.

*Assembly.* Adding the two contributions and using $J_N = G(x_N) + a_{-1}\,C(x_N)$:

$$G\,\frac{dx_N}{dT} + a_{-1} C\,\frac{dx_N}{dT}
  + \frac{\partial \dot q_N}{\partial h_{N-1}}\bigg|_{x_N} = 0,$$

i.e.

$$J_N\,\Psi_T + \frac{\partial \dot q_N}{\partial h_{N-1}}\bigg|_{x_N} = 0
  \;\Rightarrow\;
  \boxed{\Psi_T = -J_N^{-1}\,\frac{\partial \dot q_N}{\partial h_{N-1}}\bigg|_{x_N}.}$$

The $J_N$ factor is exactly the iteration matrix the Newton-Raphson at $t_N$ ended
with - already LU-factored, so $\Psi_T$ costs only a back-solve against the
coefficient-derivative right-hand side.

**BDF1 (backward Euler).** $a_{-1}=1/h_{N-1}$ and the only other coefficient,
$a_0=-1/h_{N-1}$, is fixed by $\bar a_0=1$ alone - a single past point, so no
step *ratio* is involved and $\gamma_0=a_0h_{N-1}=-1$ is trivially
independent of $h_{N-1}$. Differentiating both,

$$a_{-1}' = -\frac{1}{h_{N-1}^2} = -\frac{a_{-1}}{h_{N-1}}, \qquad
  a_0'    = \frac{1}{h_{N-1}^2} = -\frac{a_0}{h_{N-1}}.$$

All $b_i=0$ (no $\dot q$ sum). The reconstruction
$\dot q_N = a_{-1}\,q_N + a_0\,q_{N-1}$ then gives

$$\frac{\partial \dot q_N}{\partial h_{N-1}}\bigg|_{x_N}
  = a_{-1}'\,q_N + a_0'\,q_{N-1}
  = -\frac{1}{h_{N-1}}\big(a_{-1}\,q_N + a_0\,q_{N-1}\big)
  = -\frac{\dot q_N}{h_{N-1}},
  \qquad
  \Psi_T^{\text{BDF1}} = \frac{1}{h_{N-1}}\,J_N^{-1}\,\dot q_N.$$

**BDF order $\ge 2$ (Gear) - the above does not carry over.** For $p\ge 2$
past $q$-values, $a_0,\ldots,a_{p-1}$ are the solution of the linear system in
[General integration formula](#general-integration-formula), whose entries are
built from the *normalized* past time points $t_{N-1-i}/h_{N-1}$ - i.e. from
the ratios of the past step sizes $h_{N-2}, h_{N-3},\ldots$ to $h_{N-1}$.
Computing $\Psi_T$ holds $t_{N-2}, t_{N-3},\ldots$ (hence $h_{N-2},
h_{N-3},\ldots$) fixed while varying $h_{N-1}$ - exactly the setup used here,
since only the truncated last step responds to $T$ - so those ratios change
with $h_{N-1}$ too, and $\gamma_i = a_i h_{N-1}$ is *not* independent of
$h_{N-1}$ as it is for BDF1's single past point. The
$a_i' = -a_i/h_{N-1}$ shortcut above, and the
$\Psi_T^{\text{BDF1}}$ formula built on it, is therefore only rigorously
justified for BDF1. For order $\ge 2$, $a_i'$ needs
implicit differentiation of the full coefficient system (through the
$t_{N-1-i}/h_{N-1}$ ratios), not the bare $1/h_{N-1}$ rescaling used above -
not yet worked out here.

**Adams-Moulton.** The $a$-channel simplifies at *any* order, unlike BDF:
$\overline a_0=1$ and $\overline a_i=0$ ($i\ge1$) are method constraints, fixed
independent of $h_{N-1}$ (see [General integration formula](#general-integration-formula)),
so $\overline a_0'=\overline a_i'=0$ identically. Consequently
$a_0=-\overline a_0/(h_{N-1}\overline b_{-1})=-1/(h_{N-1}\overline b_{-1})=-a_{-1}$
is an *exact identity* at every order - not a coincidence special to backward
Euler or trapezoidal - so differentiating both sides gives $a_0'=-a_{-1}'$
always, with (the general closed form from
[numint.md](numint.md#sensitivity-of-a_-1-a_i-b_i-to-h_k))

$$a_{-1}' = -\frac{1}{h_{N-1}\overline b_{-1}}
  \left(\frac{1}{h_{N-1}} + \frac{\overline b_{-1}'}{\overline b_{-1}}\right).$$

The $b$-channel does **not** simplify the same way: $\overline b_{-1}, \overline
b_0, \ldots, \overline b_{m-1}$ are jointly solved from the order-condition
system whenever the method retains more than one $b$-coefficient ($m\ge2$, i.e.
order $\ge3$) - the same step-ratio dependence that breaks the
$\mathrm{BDF}\ge2$ shortcut above - so $\overline b_{-1}', \overline b_i'$ are
generally nonzero there, and $b_i'$ must come from the general sensitivity
system in [numint.md](numint.md), not a closed form.

**Backward Euler and trapezoidal** (Adams-Moulton order 1 and 2) are the only
cases where the $b$-channel collapses too: $\overline b_{-1}$ (and
$\overline b_0$ for trapezoidal) are hardcoded constants - fixed by the method
definition or the $x_\mu$ parameter, never solved from an $h_{N-1}$-dependent
system - so $\overline b_{-1}'=\overline b_i'=0$ identically, and with it
$b_i'=0$ ([numint.md](numint.md) verifies this in closed form). The
$h_{N-1}\sum_{i\ge0} b_i'\,\dot q_{N-1-i}$ term therefore drops out entirely
(it does not "cancel" against anything), leaving

$$\frac{\partial \dot q_N}{\partial h_{N-1}}\bigg|_{x_N}
  = a_{-1}'\,q_N + a_0'\,q_{N-1} + \sum_{i\ge 0} b_i\,\dot q_{N-1-i}
  = a_{-1}'\,(q_N - q_{N-1}) + \sum_{i\ge 0} b_i\,\dot q_{N-1-i},$$

using $a_0'=-a_{-1}'$. The two methods differ only in how many terms the
$\sum_{i\ge0} b_i\,\dot q_{N-1-i}$ sum has left to contribute.

*Backward Euler* has $b_i\equiv0$ (no $\dot q$ sum at all - same method as
BDF1), so the sum drops out and this is exactly the BDF1 result already
derived:

$$\frac{\partial \dot q_N}{\partial h_{N-1}}\bigg|_{x_N}
  = a_{-1}'\,(q_N-q_{N-1}) = -\frac{\dot q_N}{h_{N-1}},
  \qquad
  \Psi_T^{\text{BE}} = \frac{1}{h_{N-1}}\,J_N^{-1}\,\dot q_N.$$

*Trapezoidal* has exactly one nonzero $b_i$ ($b_0$, $m=1$), so the sum keeps
its single $i=0$ term:

$$\frac{\partial \dot q_N}{\partial h_{N-1}}\bigg|_{x_N}
  = a_{-1}'\,(q_N-q_{N-1}) + b_0\,\dot q_{N-1},
  \qquad
  \Psi_T^{\text{Trap}} = -J_N^{-1}\Big(a_{-1}'\,(q_N-q_{N-1}) + b_0\,\dot q_{N-1}\Big),$$

with $a_{-1}' = -2/h_{N-1}^2$ and $b_0 = -\overline b_0/\overline b_{-1}$
(e.g. $b_0=-1$ for pure trapezoidal, $x_\mu=1/2$). Unlike backward Euler, this
does **not** collapse into a bare multiple of $\dot q_N$ - the surviving
$b_0\,\dot q_{N-1}$ term is a genuine, separate contribution, not something a
naive "$b_i'=-b_i/h_{N-1}$" shortcut would have produced.

**Continuous limit.** As $h_{N-1} \to 0$,
$J_N \to a_{-1}\,C(x_N) = C(x_N)/(h_{N-1}\overline b_{-1})$
(with $\overline b_{-1}=1$ for BDF1), so
$J_N^{-1} \to h_{N-1}\,\overline b_{-1}\,C(x_N)^{-1}$ and

$$\Psi_T \to C(x_N)^{-1}\,\dot q_N = \dot x_s(T),$$

recovering the continuous-trajectory velocity. For finite $h_{N-1}$ the discrete
$\Psi_T$ differs from $\dot x_s(T)$ by $O(h_{N-1})$, and using the continuous form
costs the augmented Newton its quadratic convergence to the discrete fixed point.

**Rank deficiency.** $C(x_N)$ is rank-deficient on algebraic components, but $J_N$
itself is nonsingular (it is the iteration matrix used to solve the last step), so
the back-solve for $\Psi_T$ is well posed without any special handling of the
algebraic block.

### Choosing $\alpha$

$\alpha$ is updated each NR iteration to the current velocity $\dot x_s^{(l)}(0)$.
This "updating phase condition" ensures the hyperplane rotates with the solution and
gives quadratic convergence of the augmented Newton. A fixed $\alpha$ (set once from
the initial guess) is simpler but may slow convergence far from the solution.

**Computing $\alpha$.** Since $\alpha=\dot x_s(0)$ is the velocity at the
*start* of the shooting interval, the cheapest and most direct estimate is a
finite difference between the first two points of the shoot itself:

$$\alpha \approx \frac{x_1 - x_0}{h_0},$$

where $x_1$ is the state at the first accepted timepoint and $h_0$ is the
first step size. This needs no extra evaluation (both points are already
produced by the shooting transient) and no $C^{-1}$ or $J^{-1}$
approximation - unlike reusing $\Psi_T$ (a $t=T$ quantity, only equal to
$\dot x_s(0)$ at convergence, by periodicity, and even then only up to the
$O(h_{N-1})$ discretization error discussed above) or approximating
$\dot x_s(0) = -C(x_0)^{-1}f(x_0)$ via a back-solve. The finite-difference
estimate is anchored directly at $x_0$, so it carries no periodicity
assumption at all - only the usual $O(h_0)$ discretization error of a
first-order difference.

### Relation between $\alpha$ and $\Psi_T$

In the continuous theory, with the standard choice $\alpha = \dot x_s(0)$, the
analytic time-shift argument gives $\alpha = \Psi_T^{\text{cont}} = \dot x_s(0)$
exactly, making $\Psi_T^{\text{cont}}$ the right null vector of $I - \Phi_T^{\text{cont}}$
(the Floquet eigenvector for the unit multiplier).

The discrete picture is the same to leading order. The discrete $\Psi_T$ derived
above equals $\dot x_s(T) + O(h_{N-1})$, and $\dot x_s(T) = \dot x_s(0)$ by
periodicity, so

$$(I - \Phi_T)\,\Psi_T = O(h_{N-1}),$$

i.e. $I - \Phi_T$ is **numerically** singular at the discrete fixed point but not
exactly so. The augmented matrix is conditioned by the $\alpha$ row/column rather
than by exact rank-deficiency of $I - \Phi_T$; the driven-circuit LU of $I - \Phi_T$
alone is still unsafe to reuse.

### Efficient solve

Because $I - \Phi_T$ is singular, the $(n+1)\times(n+1)$ augmented system must be
solved as a whole. Form $\tilde J_p$ explicitly and factor it with standard dense LU:

$$\tilde J_p = \begin{bmatrix} I - \Phi_T & -\alpha \\ \alpha^T & 0 \end{bmatrix}.$$

The extra row and column add negligible cost to the $O(n^3)$ factorization since
$\Phi_T$ is already dense. Solve gives $[\Delta x_0,\ \Delta T]^T$ directly.

### Nonsingularity

For an autonomous oscillator exactly one Floquet multiplier equals 1 (the phase
mode); the remaining $n-1$ lie inside the unit circle. Consequently $I - \Phi_T$ is
singular, but the augmented matrix $\tilde J_p$ is nonsingular when $\alpha$ is not
orthogonal to $\Psi_T$, which holds for the standard choice $\alpha = \dot x_s(0)$.
