# Operating Point Analysis

Working notes on the operating point (DC) calculation and the Newton-Raphson
solution method. See equations.md for the DAE formulation.

## System simplification for DC

Operating point analysis finds a steady-state solution where all time derivatives
are zero. Starting from the DAE

$$f(x, t) + \frac{d}{dt}\,q(x, t) = 0,$$

setting $\frac{d}{dt}\,q(x, t) = 0$ gives the **resistive equation**

$$f(x, t_0) = 0,$$

where $t_0$ is a fixed time (typically $t_0=0$).
All reactive elements (capacitors, inductors) are ignored. Time-varying sources
are evaluated at the fixed time $t_0$ and treated as constants. Only the static
device equations and the instantaneous source values at $t_0$ matter. 

The unknown vector $x$ still contains node potentials and branch currents from
voltage sources, inductors, etc., so $x \in \mathbb{R}^n$ with the same dimension
as the transient DAE. The resulting system of algebraic equations has $n$ equations 
and $n$ unknowns.

## Newton-Raphson iteration

The nonlinear system $f(x, t_0) = 0$ is solved iteratively. At each iteration $l$,
linearize $f$ about the current guess $x^{(l)}$:

$$f(x^{(l)}, t_0) + J_r(x^{(l)}, t_0)\,\Delta x = 0,$$

where $\Delta x = x - x^{(l)}$ is the correction and

$$J_r(x, t_0) = \frac{\partial f}{\partial x}(x, t_0)$$

is the resistive Jacobian (see equations.md). Solving the linear system

$$J_r(x^{(l)}, t_0)\,\Delta x^{(l)} = -f(x^{(l)}, t_0)$$

gives the Newton step, and the new iterate is

$$x^{(l+1)} = x^{(l)} + \Delta x^{(l)}.$$

Repeat until convergence: typically when both the residual $f$ and the step
$\Delta x$ are small relative to the tolerances.

### Convergence

For a well-conditioned problem near the solution, Newton-Raphson converges
quadratically: the error roughly squares at each iteration. The Jacobian must
be nonsingular at the solution.

## Initial guess and nodesets

The Newton-Raphson method needs a starting guess $x^{(0)}$. A poor guess can
cause divergence or convergence to an undesired solution (e.g. wrong operating
point of a bistable circuit). Two strategies:

### Default initial guess

If no nodesets are provided, $x^{(0)} = 0$ (all node potentials at ground, all
branch currents zero).

### Nodesets

A **nodeset** is a set of guessed values for a subset of the unknowns. The user
specifies them as a list of `(unknown, value)` pairs.

**Forcing** is the mechanism VACASK uses to apply nodesets. For each forced
unknown $x_i$, the equation corresponding to that unknown is augmented with a
large penalty term:

$$f_i(x, t_0) + A(x_i - \text{nodeset}_i) = 0,$$

where $A$ is a large penalty coefficient. This strongly drives $x_i$ toward its
nodeset value while allowing the Newton solver to find consistent values for all
unknowns.

By default, forcing is kept active for one Newton-Raphson iteration. After that,
the penalty terms are removed and all unknowns are free to move according to the
residual equations alone. Nodesets are temporary initialization aids; they guide
the solver to a promising region of the solution space but are not enforced
constraints during the bulk of the Newton solve.

## Damped Newton-Raphson

The standard Newton-Raphson method solves the nonlinear system by updating the solution as:

$$x_{k+1} = x_k - J(x_k)^{-1} f(x_k)$$

where $J$ is the Jacobian matrix and $f$ is the residual vector. This step is guaranteed to converge only when sufficiently close to a solution. For circuits with strong nonlinearities (diodes, transistors far from equilibrium), a full Newton step can overshoot and diverge.

The damped Newton method restricts the step size by introducing a damping factor $\lambda \in (0, 1]$:

$$x_{k+1} = x_k - \lambda J(x_k)^{-1} f(x_k)$$

With $\lambda < 1$, the update is fractional, allowing the iterative process to approach the solution more conservatively. 

Damping is particularly effective when:
- Initial guesses are far from the solution
- Device models have steep characteristics (diodes in reverse bias, poorly biased transistors)
- The circuit exhibits multiple solutions or instability regions

## Limiting (Step Limiting)

When Newton steps would push solution variables into physically unrealistic regions, limiting prevents them from exceeding a maximum change. Instead of taking the full Newton step, limiting restricts the update while reusing the Jacobian from the previous iteration, effectively linearizing the problem locally.

For a variable $x_i$ with computed Newton step $\Delta x_i$, limiting enforces:

$$|x_i^{(k+1)} - x_i^{(k)}| \leq \max_{\text{step},i}$$

where $\max_{\text{step},i}$ is a problem-dependent bound. Common examples include:

### Device-level limiting

Limiting takes place at the device evaluation level: if a device input (such as gate voltage for a transistor) would change by more than a specified threshold, the element's Jacobian contribution is evaluated at the input value threshold rather than at the full Newton step. Beyond the threshold the element's Jacobian contribution is constant. The residual contribution beyond the limiting point is then treated as linear with respect to the limited device input. This approach prevents overshooting to unphysical regions and helps the solver converge smoothly to the correct steady-state operating point.

The Newton step is not modified. The Newton-Raphson algorithm computes corrections $\Delta x$ in the standard way: $\Delta x^{(k)} = -J^{-1}(x^{(k)}) f(x^{(k)})$. Limiting does not apply a damping factor or clip the solution vector before applying the step. Instead, the step is applied normally, but the *evaluation* of the residual and Jacobian for each device is controlled by the device model itself.

NR is not allowed to converge while at least one element applies limiting. Convergence of Newton-Raphson requires that the residual norm and step norm both fall below their respective tolerances. However, if any device is applying limiting in the current iteration, the solution is provisional: the device's true nonlinear behavior has not been fully explored yet. Convergence is deferred until a complete Newton iteration occurs where no device needs limiting. At that point, the full nonlinear model is active, the Jacobian accurately represents the circuit behavior, and Newton convergence criteria can be reliably assessed.

Verilog-A device models implement limiting using the built-in `$limit` function. 
