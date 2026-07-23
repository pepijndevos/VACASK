# Circuit Equation Formulation

Working notes on how VACASK assembles the circuit equations and what the residuals
and Jacobians mean. These notes underpin the formulation of all analyses. 
## Modified Nodal Analysis

VACASK uses Modified Nodal Analysis (MNA) to turn the circuit into a system of
equations. The unknowns are collected in the vector

$$x \in \mathbb{R}^n$$

which contains various kinds of entries:

- **Node potentials** $v_k$ for every non-ground node in the circuit.  Ground
  is the reference and is not an unknown.
- **Branch currents** $i_b$ for every device element that cannot be described
  by an admittance stamp alone.  A voltage source, for example, imposes a
  constraint $v_+ - v_- = V_s$ and introduces its own branch current as an
  extra unknown; an inductor does the same.
- **Non-electrical unknowns** for nodes belonging to non-electrical Verilog-A
  disciplines.  VACASK supports arbitrary disciplines, so a node can carry a
  thermal potential (temperature), a mechanical velocity, an acoustic pressure,
  or any other potential quantity defined by the discipline.  The corresponding
  residual equation enforces flow balance (heat flow, force, volume velocity,
  etc.) at that node, exactly as KCL enforces current balance at an electrical
  node.  Each discipline has its own ground reference; a node tied to that
  ground is not an unknown.

## The DAE

All analyses share the same underlying equation:

$$f(x, t) + \frac{d}{dt}\,q(x, t) = 0.$$

The $k$-th row is the equation for the $k$-th unknown.  For a node unknown $v_k$
it is the KCL equation at that node (sum of currents equals zero).  For a
branch-current unknown $i_b$ it is the KVL constraint that the branch current is
tied to (e.g. $v_+ - v_- - V_s = 0$ for a voltage source, or $v_+ - v_- - L\,\dot i_b = 0$
for an inductor).

$n$ equals the dimension of both $f$ and $q$; the DAE is a square system.

### Resistive residual

$$f : \mathbb{R}^n \times \mathbb{R} \to \mathbb{R}^n$$

$f(x)$ collects all **static** contributions: resistor currents, transistor
drain/gate/source currents, independent voltage and current source values, the
$v_+ - v_- - V_s$ KVL row of a voltage source, and any other algebraic device
equations.  The explicit $t$ dependence enters through time-varying sources.
For DC analysis $f$ is evaluated at a fixed $t$ (or $t=0$); for transient and
PSS analysis it is evaluated at each timepoint.

Device models stamp into $f$.  Each stamp is a signed
current contribution added to the appropriate KCL row (positive out of the positive
terminal, negative into the negative terminal).

### Reactive residual

$$q : \mathbb{R}^n \times \mathbb{R} \to \mathbb{R}^n$$

$q(x)$ collects all **dynamic** contributions: node charges (from capacitors,
gate oxide charges, junction charges, etc.) and branch fluxes (from inductors).
The $k$-th entry is the total charge associated with the $k$-th equation; its
time derivative $\dot q_k$ is the corresponding dynamic current or flux rate. 
Time integrals of non-electrical quantities also appear in the reactive residual. 

Device models stamp into $q$ .  Purely resistive devices
(ideal resistors, independent current sources) contribute nothing to $q$.

## Jacobians

Newton-based solvers need the two partial-derivative matrices evaluated at a
given $x$:

$$J_r(x, t) = \frac{\partial f}{\partial x}(x, t), \qquad
  J_c(x, t) = \frac{\partial q}{\partial x}(x, t).$$

### Resistive Jacobian $J_r$

$J_r \in \mathbb{R}^{n \times n}$ is the resistive Jacobian.  Entry $(J_r)_{ij}$ is the
partial derivative of the $i$-th KCL (or KVL) equation with respect to the $j$-th
unknown.  For a simple conductance $g$ between nodes $p$ and $m$ it contributes

$$(J_r)_{pp} \mathrel{+}= g, \quad (J_r)_{pm} \mathrel{-}= g, \quad (J_r)_{mp} \mathrel{-}= g, \quad (J_r)_{mm} \mathrel{+}= g.$$

Device models stamp $J_r$.  The matrix is sparse and
typically structured as a KLU-factored sparse matrix in VACASK.

### Reactive Jacobian $J_c$

$J_c \in \mathbb{R}^{n \times n}$ is the reactive Jacobian.  Entry $(J_c)_{ij}$ is the
partial derivative of the $i$-th charge/flux entry with respect to the $j$-th
unknown.  For a simple capacitor $C_0$ between nodes $p$ and $m$:

$$(J_c)_{pp} \mathrel{+}= C_0, \quad (J_c)_{pm} \mathrel{-}= C_0, \quad (J_c)_{mp} \mathrel{-}= C_0, \quad (J_c)_{mm} \mathrel{+}= C_0.$$

Device models stamp $J_c$.  Purely resistive devices
contribute nothing to $J_c$.
