# Numerical Integration

Working notes on the time-domain integration used by transient analysis and by
the shooting step inside PSS. This file collects the general multistep
formulation; individual methods (BDF / Gear, Adams-Moulton, trapezoidal,
backward Euler) are recovered as special cases.

## DAE setting

VACASK assembles the circuit into a system of differential-algebraic equations

$$f(x(t), t) + \frac{d}{dt} q(x(t)) = 0,$$

with the resistive residual $f$ and the reactive residual $q$. A single
integration step advances the state from a known timepoint $t_k$ (state $x_k$)
to $t_{k+1} = t_k + h_k$, producing $x_{k+1}$. Write $q_j = q(x_j)$ for the
reactive residual at $t_j$ and $\dot q_j$ for its time derivative; let
$\dot q \equiv \frac{d}{dt} q$.

## General multistep ansatz

The implicit linear multistep (LMS) **ansatz** expresses the unknown state
$q_{k+1}$ as a linear combination of past values $q_k, q_{k-1}, \ldots$ and of
both past and future derivatives $\dot q_{k+1}, \dot q_k, \dot q_{k-1}, \ldots$:

$$q_{k+1} = \sum_{i=0}^{m-1} \overline{a}_i\, q_{k-i}
  + h_k \sum_{i=-1}^{m-1} \overline{b}_i\, \dot q_{k-i},$$

with $h_k = t_{k+1} - t_k$ the current step size and $m$ the number of past
samples retained. Indexing convention:

- $i = 0$ refers to the **last known** timepoint $t_k$ (so $q_{k-0} = q_k$,
  $\dot q_{k-0} = \dot q_k$).
- $i = -1$ refers to the **new** timepoint $t_{k+1}$. It appears only in the
  derivative sum, as $\dot q_{k-(-1)} = \dot q_{k+1}$. Its multiplier
  $\overline{b}_{-1}$ is what makes the method implicit; its non-vanishing
  ($\overline{b}_{-1} \ne 0$) is required.
- Positive $i$ ($i = 1, 2, \ldots, m-1$) refers to earlier timepoints
  $t_{k-1}, t_{k-2}, \ldots$

The coefficients $\overline{a}_i$ and $\overline{b}_i$ are **dimensionless
method constants**: they depend on the method, its order, and (for variable
step) the step-size ratios. The explicit $h_k$ in front of the derivative sum
carries the time dimension.

### Special cases

This single ansatz covers the methods VACASK supports:

- **BDF / Gear**: only $\overline{b}_{-1} \neq 0$ among the $\overline{b}$'s.
  The new state is a combination of past $q$'s plus a contribution from
  $\dot q_{k+1}$ alone. E.g. BDF1 ($q_{k+1} = q_k + h_k \dot q_{k+1}$):
  $\overline{a}_0 = 1$, $\overline{b}_{-1} = 1$, all others zero.
- **Adams-Moulton** (including trapezoidal): $\overline{a}_0 = 1$ and higher
  $\overline{a}_i = 0$, with several $\overline{b}_i$ nonzero. E.g. trapezoidal
  ($q_{k+1} = q_k + (h_k/2)(\dot q_{k+1} + \dot q_k)$): $\overline{a}_0 = 1$,
  $\overline{b}_{-1} = \overline{b}_0 = 1/2$.

The rearrangement of this ansatz into the derivative form
$\dot q_{k+1} = a_{-1}\,q_{k+1} + \ldots$ used by the Newton iteration is given
in the PSS notes; see [pss.md](pss.md).

## Computing the coefficients

The method is **of order $n$** if the ansatz holds identically whenever
$q(t)$ is a polynomial of degree at most $n$. By linearity it is enough to
impose exactness on the monomial basis $q(t) = \alpha t^j$, $j = 0, 1, \ldots,
n$ - one scalar equation per $j$.

It is convenient to work in a dimensionless, $t_k$-centered time. Define the
backward offset of each retained timepoint, measured from $t_k$ in units of
$h_k$,

$$\theta_i \;\equiv\; \frac{t_{k-i} - t_k}{h_k}.$$

By construction

- $\theta_{-1} = 1$ (the new point $t_{k+1}$),
- $\theta_0 = 0$ (the last known point $t_k$),
- $\theta_i = -\dfrac{h_{k-1} + h_{k-2} + \cdots + h_{k-i}}{h_k} < 0$
  for $i \geq 1$ (earlier points).

For uniform step size $\theta_i = -i$.

Because the ansatz is invariant under affine reparametrizations of the time
axis, exactness for $q(t) = \alpha t^j$ is equivalent to exactness for the
rescaled monomial $q(t) = \big((t-t_k)/h_k\big)^j$. Substituting that choice,

$$q_{k-i} = \theta_i^{\,j}, \qquad q_{k+1} = 1, \qquad
  h_k\, \dot q_{k-i} = j\, \theta_i^{\,j-1}.$$

Inserting into the ansatz and collecting by $j$ gives the **order conditions**:

$$j = 0: \qquad \sum_{i=0}^{m-1} \overline{a}_i \;=\; 1,$$

$$j = 1, 2, \ldots, n: \qquad
  \sum_{i=0}^{m-1} \overline{a}_i\, \theta_i^{\,j}
  \;+\; j \sum_{i=-1}^{m-1} \overline{b}_i\, \theta_i^{\,j-1}
  \;=\; 1.$$

The $j=0$ equation is stated separately to avoid the formal $0^{-1}$ that would
appear in the $\theta_0^{\,j-1}$ factor; the corresponding sum vanishes anyway
because it is multiplied by $j=0$.

A second boundary case arises at $j=1$, $i=0$, where $\theta_0^{\,j-1} =
0^0$. This term comes from $h_k \dot q_{k-i}|_{i=0,\,j=1}$, i.e. the derivative
of the rescaled monomial $q(t) = (t-t_k)/h_k$ evaluated at the point
$t = t_k$. That derivative equals $1$, so the convention $0^0 = 1$ is the right
one and is used throughout. (Equivalently, $j\,\theta_i^{\,j-1}$ is just the
formal derivative of $\theta_i^{\,j}$ with respect to $\theta_i$, which is a
polynomial in $\theta_i$ and so well-defined at $\theta_i = 0$.) For all
$j \geq 2$ the factor $\theta_0^{\,j-1}$ vanishes outright, so no convention is
needed there.

This is a linear system of $n+1$ equations in the $2m+1$ unknowns

$$\overline{a}_0, \ldots, \overline{a}_{m-1}, \qquad
  \overline{b}_{-1}, \overline{b}_0, \ldots, \overline{b}_{m-1}.$$

Its matrix is of (confluent) Vandermonde type in the distinct nodes
$\{\theta_{-1}, \theta_0, \theta_1, \ldots, \theta_{m-1}\}$, so for any
admissible step-size history the conditions are linearly independent.

The system has more unknowns than equations whenever $2m + 1 > n + 1$, i.e.
$2m > n$. The remaining $2m - n$ degrees of freedom are precisely how
different methods of the same order are distinguished:

- **BDF / Gear** order $n$: set $\overline{b}_i = 0$ for $i \geq 0$ (only
  $\overline{b}_{-1}$ survives), choose $m = n$, and solve the $n+1$ order
  conditions for $\overline{a}_0, \ldots, \overline{a}_{n-1}, \overline{b}_{-1}$
  - that is $n+1$ unknowns for $n+1$ equations.
- **Adams-Moulton** order $n$: set $\overline{a}_0 = 1$ and $\overline{a}_i = 0$
  for $i \geq 1$, choose $m = n - 1$ (with $m=1$ for the trapezoidal/BE pair),
  and solve for $\overline{b}_{-1}, \overline{b}_0, \ldots, \overline{b}_{m-1}$
  - $m+1 = n$ unknowns. The $j=0$ condition is satisfied automatically by the
  $\overline{a}_0 = 1$ choice, leaving $n$ remaining conditions.

**Correspondence with `IntegratorCoeffs` (`include/coretrancoef.h`).** The raw
members `a_`, `b_`, `b1_` (exposed via `a()`, `b()`, `b1()`, and populated by
`compute()`) are exactly the overline quantities above: `a_[i]` $=
\overline a_i$, `b_[i]` $= \overline b_i$ for $i\ge0$, and `b1_` $=
\overline b_{-1}$ - VACASK gives the leading derivative coefficient its own
member instead of a `b_[-1]` entry, to avoid a negative array index.

## Sensitivity of the coefficients to $h_k$

For variable-step control and for shooting-method sensitivities it is useful
to know how each $\overline{a}_i, \overline{b}_i$ responds to a change in the
current step size $h_k$, with the past step sizes $h_{k-1}, h_{k-2}, \ldots$
held fixed.

Only the $\theta_i$ with $i \geq 1$ depend on $h_k$ (their numerator is fixed,
only the $1/h_k$ in the denominator varies):

$$\frac{\partial \theta_{-1}}{\partial h_k} = 0, \qquad
  \frac{\partial \theta_0}{\partial h_k} = 0, \qquad
  \frac{\partial \theta_i}{\partial h_k} = -\frac{\theta_i}{h_k} \quad (i \geq 1).$$

Denote derivatives with respect to $h_k$ by a prime. Differentiating the order
conditions of the previous section implicitly (their right-hand sides are
constants, so they contribute nothing) gives a linear system for the
$\overline{a}_i', \overline{b}_i'$.

The $j=0$ condition yields

$$\sum_{i=0}^{m-1} \overline{a}_i' \;=\; 0.$$

For $j = 1, 2, \ldots, n$, applying the product rule and substituting
$\theta_i' = -\theta_i/h_k$ for $i \geq 1$ (the $i \in \{-1, 0\}$ pieces drop
out because their $\theta_i' = 0$):

$$\sum_{i=0}^{m-1} \overline{a}_i'\, \theta_i^{\,j}
  \;+\; j \sum_{i=-1}^{m-1} \overline{b}_i'\, \theta_i^{\,j-1}
  \;=\; \frac{j}{h_k}\left[
  \sum_{i=1}^{m-1} \overline{a}_i\, \theta_i^{\,j}
  \;+\; (j-1) \sum_{i=1}^{m-1} \overline{b}_i\, \theta_i^{\,j-1}
  \right].$$

The left-hand side is **the same matrix** that defines the order conditions
themselves - only the right-hand side changes. The right-hand side is a known
quantity once the coefficients $\overline{a}_i, \overline{b}_i$ have been
solved for at the current $h_k$.

Method-specific constraints (e.g., $\overline{b}_i = 0$ for $i \geq 0$ in BDF,
or $\overline{a}_0 = 1$ and $\overline{a}_i = 0$ for $i \geq 1$ in
Adams-Moulton) are independent of $h_k$. They translate one-to-one to
$\overline{b}_i' = 0$ or $\overline{a}_i' = 0$ on the corresponding rows and
are appended to the system unchanged.

The full system for the primed coefficients therefore reuses the LU
factorization computed for the coefficients themselves, with only the
right-hand side recomputed.

### Backward Euler and trapezoidal

For both methods $m = 1$ (BDF1 has order $n = 1$ with $m = n = 1$; trapezoidal
is Adams-Moulton with order $n = 2$ and $m = n - 1 = 1$). The retained
timepoints are therefore only $t_{k+1}$ and $t_k$, giving $\theta_{-1} = 1$ and
$\theta_0 = 0$. There are no $\theta_i$ with $i \geq 1$, so both sums
$\sum_{i=1}^{m-1}$ on the right-hand side of the sensitivity equation run over
an empty index range and vanish identically:

$$\sum_{i=0}^{m-1} \overline{a}_i'\, \theta_i^{\,j}
  \;+\; j \sum_{i=-1}^{m-1} \overline{b}_i'\, \theta_i^{\,j-1}
  \;=\; 0
  \qquad (j = 1, 2, \ldots, n).$$

Together with $\sum_i \overline{a}_i' = 0$ and the method constraints (zero on
the RHS), the entire system has zero right-hand side. The matrix is
nonsingular, so the unique solution is

$$\overline{a}_i' = 0, \qquad \overline{b}_i' = 0.$$

This matches the underlying fact that the dimensionless coefficients of both
methods are universal numbers:

- **Backward Euler:** $\overline{a}_0 = 1$, $\overline{b}_{-1} = 1$.
- **Trapezoidal:** $\overline{a}_0 = 1$, $\overline{b}_{-1} = \overline{b}_0 = 1/2$.

None depends on $h_k$ or on any past step size. The sensitivity computation can
be **skipped entirely** when the current step uses backward Euler or
trapezoidal; only the derivative-form coefficients ($a_{-1}, a_i, b_i$, defined
in the next section) that carry an explicit $1/h_k$ factor change with $h_k$,
and those are recovered from the closed-form rearrangement of the ansatz
rather than from the linear system above.

## Derivative at the new timepoint

The Newton solve at $t_{k+1}$ needs $\dot q_{k+1}$ as a linear combination of
$q_{k+1}$ and known past values, since the residual $f(x_{k+1}, t_{k+1}) +
\dot q_{k+1}$ must be evaluated and differentiated with respect to $x_{k+1}$.
The ansatz as written has $\dot q_{k+1}$ on its right-hand side (the $i = -1$
term of the derivative sum), so it must be **solved for** $\dot q_{k+1}$.

Split off the $i = -1$ contribution from the derivative sum:

$$q_{k+1} \;=\; \sum_{i=0}^{m-1} \overline{a}_i\, q_{k-i}
            \;+\; h_k \overline{b}_{-1}\, \dot q_{k+1}
            \;+\; h_k \sum_{i=0}^{m-1} \overline{b}_i\, \dot q_{k-i}.$$

With $\overline{b}_{-1} \neq 0$ (the implicit-method requirement), move the
explicit derivative term to the left, divide by $h_k \overline{b}_{-1}$, and
isolate $\dot q_{k+1}$:

$$\dot q_{k+1}
   \;=\; \frac{1}{h_k\,\overline{b}_{-1}}\, q_{k+1}
   \;-\; \sum_{i=0}^{m-1} \frac{\overline{a}_i}{h_k\,\overline{b}_{-1}}\, q_{k-i}
   \;-\; \sum_{i=0}^{m-1} \frac{\overline{b}_i}{\overline{b}_{-1}}\, \dot q_{k-i}.$$

For compactness, collect the rearranged coefficients into

$$a_{-1} \;=\; \frac{1}{h_k\,\overline{b}_{-1}}, \qquad
  a_i    \;=\; -\frac{\overline{a}_i}{h_k\,\overline{b}_{-1}}\ \ (i \geq 0), \qquad
  b_i    \;=\; -\frac{\overline{b}_i}{\overline{b}_{-1}}\ \ (i \geq 0).$$

The **derivative form** of the ansatz then reads

$$\dot q_{k+1} \;=\; a_{-1}\, q_{k+1}
              \;+\; \sum_{i=0}^{m-1} a_i\, q_{k-i}
              \;+\; \sum_{i=0}^{m-1} b_i\, \dot q_{k-i}.$$

Reading off the structure:

- $a_{-1}\, q_{k+1}$ is the **implicit** term; it depends on the unknown
  $x_{k+1}$ through $q_{k+1} = q(x_{k+1})$. Its multiplier $a_{-1}$ is the
  *leading coefficient*, the same quantity used by the Jacobian as
  $J = G + a_{-1} C$.
- $a_i$ and $b_i$ for $i \geq 0$ multiply only known quantities at past
  timepoints; together they form the **history term** the Newton iteration
  treats as constant.
- All $a_i$ carry an explicit $1/h_k$ factor inherited from the rearrangement;
  the $b_i$ are dimensionless because the $h_k$ in the original ansatz
  cancels against the $h_k\,\overline{b}_{-1}$ on the LHS.

### Special cases

- **Backward Euler** ($\overline{a}_0 = 1$, $\overline{b}_{-1} = 1$, all
  others zero):

  $$a_{-1} = \frac{1}{h_k}, \qquad a_0 = -\frac{1}{h_k}, \qquad b_i = 0,$$

  giving the familiar
  $\dot q_{k+1} = (q_{k+1} - q_k)/h_k.$

- **Trapezoidal** ($\overline{a}_0 = 1$, $\overline{b}_{-1} = \overline{b}_0 =
  1/2$, all others zero):

  $$a_{-1} = \frac{2}{h_k}, \qquad a_0 = -\frac{2}{h_k}, \qquad b_0 = -1,$$

  giving
  $\dot q_{k+1} = \frac{2}{h_k}(q_{k+1} - q_k) - \dot q_k,$
  the standard trapezoidal rearrangement.

For methods of order $> 2$ the history term also retains contributions from
$q_{k-1}, q_{k-2}, \ldots$ and from $\dot q_{k-1}, \dot q_{k-2}, \ldots$ The
PSS notes ([pss.md](pss.md)) group these into a single symbol $s_k$ and use
the resulting compact form
$\dot q_{k+1} = a_{-1} q_{k+1} + a_0 q_k + h_k b_0 \dot q_k + s_k$
for the shooting Newton iteration.

**Correspondence with `IntegratorCoeffs` (`include/coretrancoef.h`).**
`aScaled_`, `bScaled_`, `leading_` (from `scaleDifferentiator(hk)`) are
exactly the derivative-form coefficients above: 
`aScaled_[i]` $= a_i = -\overline a_i/(h_k\overline b_{-1})$, 
`bScaled_[i]` $= b_i = -\overline b_i/\overline b_{-1}$ for
$i\ge0$, and `leading_` $= a_{-1} = 1/(h_k\overline b_{-1})$. `differentiate()`
reads directly off the boxed derivative form,
`deriv = leading_*future + sum aScaled_[i]*hist[i] + sum bScaled_[i]*hist[state+1]`
(all `+=`), since $a_i, b_i$ already carry their own sign.

## Sensitivity of $a_{-1}$, $a_i$, $b_i$ to $h_k$

The Jacobian $J = G + a_{-1}\,C$ and the history term depend on $h_k$ both
explicitly through the $1/h_k$ prefactor in the rearrangement and implicitly
through the dimensionless $\overline{a}_i(h_k), \overline{b}_i(h_k)$. To
support variable-step control and step-size sensitivity in the shooting
Jacobian, the derivatives $\partial a_{-1}/\partial h_k$, $\partial
a_i/\partial h_k$, $\partial b_i/\partial h_k$ are needed.

Denote $\partial/\partial h_k$ by a prime, and differentiate the rearrangement
formulas directly - keeping everything in terms of the overline quantities
and $h_k$, without reintroducing $a_{-1}, a_i, b_i$ on the right-hand side.
From $a_{-1} = 1/(h_k\overline b_{-1})$, the quotient rule gives

$$a_{-1}'
   \;=\; -\frac{1}{h_k\,\overline{b}_{-1}}
         \left(\frac{1}{h_k} + \frac{\overline{b}_{-1}'}{\overline{b}_{-1}}\right).$$

From $a_i = -\overline{a}_i/(h_k\overline{b}_{-1})$ ($i\ge0$), the same
quotient rule (product rule on the numerator, since both $\overline a_i$ and
$h_k\overline b_{-1}$ depend on $h_k$) gives

$$a_i'
   \;=\; \frac{1}{h_k\,\overline{b}_{-1}}
         \left[\overline{a}_i\left(\frac{1}{h_k}
         + \frac{\overline{b}_{-1}'}{\overline{b}_{-1}}\right)
         \;-\; \overline{a}_i'\right]
   \quad (i \geq 0).$$

From $b_i = -\overline{b}_i/\overline{b}_{-1}$ ($i\ge0$),

$$b_i'
   \;=\; \frac{1}{\overline{b}_{-1}}
         \left(\overline{b}_i\,\frac{\overline{b}_{-1}'}{\overline{b}_{-1}}
         \;-\; \overline{b}_i'\right)
   \quad (i \geq 0).$$

The dimensionless sensitivities $\overline{a}_i', \overline{b}_i'$ (including
$\overline{b}_{-1}'$) come from solving the linear system in the
[earlier sensitivity section](#sensitivity-of-the-coefficients-to-h_k); once
those are known, $a_{-1}', a_i', b_i'$ above are closed-form evaluations, no
further solve needed. In particular the leading coefficient's sensitivity,
$a_{-1}'$, always carries a $-1/(h_k^2\overline b_{-1})$ piece from the
explicit $1/h_k$ in the rearrangement, plus a
$-\overline b_{-1}'/(h_k\overline b_{-1}^2)$ piece present only when
$\overline b_{-1}$ itself responds to $h_k$ (i.e. for genuine multistep
methods - see the BDF order $\ge2$ discussion in [pss.md](pss.md)).

### Backward Euler and trapezoidal

For both methods the dimensionless coefficients are universal constants, so
$\overline{a}_i' = 0$ and $\overline{b}_i' = 0$ (in particular
$\overline{b}_{-1}' = 0$), and every term above that carries a prime drops
out:

- **Backward Euler** ($\overline{a}_0 = 1$, $\overline{b}_{-1} = 1$, all
  other $\overline{a}_i, \overline{b}_i = 0$):

  $$a_{-1}' = -\frac{1}{h_k}\cdot\frac{1}{h_k}
    = -\frac{1}{h_k^{\,2}}, \qquad
    a_0'    = \frac{1}{h_k}\cdot\frac{1}{h_k}
    = +\frac{1}{h_k^{\,2}}, \qquad
    b_i'    = 0.$$

- **Trapezoidal** ($\overline{a}_0 = 1$, $\overline{b}_{-1} =
  \overline{b}_0 = 1/2$, all other $\overline{a}_i, \overline{b}_i = 0$):

  $$a_{-1}' = -\frac{1}{h_k\cdot\frac12}\cdot\frac{1}{h_k}
    = -\frac{2}{h_k^{\,2}}, \qquad
    a_0'    = \frac{1}{h_k\cdot\frac12}\cdot\frac{1}{h_k}
    = +\frac{2}{h_k^{\,2}}, \qquad
    b_0'    = 0.$$

No linear solve is required for either method; the derivative-form
sensitivities are direct closed-form expressions in $h_k$ and the (constant)
overline coefficients.

## Polynomial predictor

Before the implicit corrector solves for $x_{k+1}$, Newton needs a starting
guess. VACASK gets one by **extrapolating** the past solution history forward
to $t_{k+1}$ - a separate, purely explicit computation from the corrector
above, sharing only the general ansatz machinery.

### Ansatz

Set *every* $\overline b_i$ in the general ansatz to zero, including
$\overline b_{-1}$ (unlike the corrector, which requires $\overline b_{-1}
\neq 0$ precisely to be implicit). The derivative sum drops out entirely,
leaving a pure value-extrapolation formula:

$$\hat q_{k+1} \;=\; \sum_{i=0}^{m-1} \overline{a}_i\, q_{k-i}.$$

With no implicit term, $\hat q_{k+1}$ (or, in practice, $\hat x_{k+1}$ - the
predictor operates on the state vector directly, not on $q$) is available
immediately from history, with no NR solve.

### Order conditions

The general order conditions specialize the same way: drop every $\overline
b_i$ term (the entire second sum in both the $j=0$ and $j\ge1$ conditions
vanishes), leaving

$$\sum_{i=0}^{m-1} \overline{a}_i\, \theta_i^{\,j} \;=\; 1
  \qquad (j = 0, 1, \ldots, n).$$

An order-$n$ predictor needs $n+1$ unknowns $\overline a_0, \ldots,
\overline a_n$ for these $n+1$ equations, so $m = n+1$ - one more retained
point than the order, since (unlike the corrector) there is no
$\overline b_{-1}$ degree of freedom to absorb one of the conditions. The
system is Vandermonde in the nodes $\theta_0, \ldots, \theta_n$ only:
$\theta_{-1}$ never appears as a matrix entry (there is no $\overline
b_{-1}\theta_{-1}^{j-1}$ term to contribute it), it only ever appears through
$q_{k+1} = \theta_{-1}^{\,j} = 1$ on the right-hand side, since $\theta_{-1}=1$
identically.

This is exactly polynomial extrapolation stated as a linear system: fit the
degree-$n$ polynomial through the $n+1$ past samples at $\theta_0, \ldots,
\theta_n$, then evaluate it at $\theta_{-1} = 1$. The closed form is the
Lagrange basis polynomial evaluated at the new point:

$$\overline{a}_i \;=\; \prod_{\substack{l=0 \\ l \neq i}}^{n}
  \frac{1 - \theta_l}{\theta_i - \theta_l}.$$

`IntegratorCoeffs::compute()`'s `PolynomialExtrapolation` branch solves the
same Vandermonde system numerically (row $0$: $\sum_i\overline a_i=1$; rows
$j=1,\ldots,n$: $\sum_i\overline a_i\theta_i^{\,j}=1$) rather than evaluating
the closed form directly - the two agree by uniqueness of the Vandermonde
solution.

### Special cases

- Order $0$: $m=1$, $\overline a_0=1$ - zero-order hold, $\hat x_{k+1}=x_k$.
- Order $1$: $m=2$, linear extrapolation through $x_k$ and $x_{k-1}$.

### Correspondence with `IntegratorCoeffs`

`Method::PolynomialExtrapolation`'s `size()` sets `numX_=order_+1`,
`numXdot_=0` - $n+1$ retained values, no derivative history at all, matching
$\overline b_i=0$ universally above.

`scalePredictor(hk)` refuses implicit methods (`implicit_` must be false) and
sets `aScaled_ = a_` **unscaled** - unlike the corrector's `aScaled_`, no
$1/(h_k\overline b_{-1})$ rescaling applies here, since the pure
value-extrapolation formula carries no explicit $h_k$ at all. It also sets
`bScaled_[i] = b_[i]*hk`, the scaling an Adams-Bashforth-style predictor
($q_{k+1}=\overline a_0 q_k + h_k\sum_i\overline b_i\dot q_{k-i}$, still
explicit since $\overline b_{-1}=0$ there too) would need - but `predict()`
only ever reads `aScaled_`, never `bScaled_`, so this computation is
currently inert. Consistent with this, `preparePredictorHistory()` `DBGCHECK`s
`numXdot_==0`, i.e. it refuses to prepare a predictor that needs derivative
history at all. Between the two, `Method::AdamsBashforth`'s coefficients can
be computed (`compute()` supports it), but its predictor cannot actually be
evaluated through the current `predict()`/`preparePredictorHistory()` path.
In practice `coretran.cpp` only ever constructs `predictorCoeffs` with
`Method::PolynomialExtrapolation` - `AdamsBashforth` is unused as a predictor.

`preparePredictorHistory(repo, historyOffset=1)` grabs `repo.data(1),
..., repo.data(numX_)`, i.e. history slots $1,\ldots,n+1$ - matching
$\theta_i$'s indexing ($i=0\ldots n \to$ slot $i+1 \to t_k, t_{k-1},
\ldots$).

The predictor's own `errorCoeff()`, together with the corrector's, feeds the
local error estimate used for step-size control (`coretran.cpp`: `factor =
corrector.errorCoeff()/(corrector.errorCoeff()-predictor.errorCoeff())`) -
not derived here.

