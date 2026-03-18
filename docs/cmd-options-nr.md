# Newton-Raphson Solver

## Convergence

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `nr_damping` | real | 1.0 | 0 < x ≤ 1 | Newton-Raphson step damping factor. Values less than 1 limit the step size. |
| `nr_conviter` | int | 1 | >0 | Number of consecutive convergent iterations required to confirm convergence. |
| `nr_residualcheck` | boolean | 1 | 0, 1 | Also check residual (not only solution change) for convergence. |
| `nr_force` | real | 1e5 | >0 | Forcing factor for nodeset and initial condition constraints. |
| `strictforce` | int | 1 | 0, 1 | How to handle conflicting nodeset/ic constraints. 0 = warn and continue, 1 = abort. |

## Instance bypass

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `nr_bypass` | boolean | 0 | 0, 1 | Enable instance evaluation bypass. A converged instance whose inputs change by less than `nr_bypasstol` times the tolerances is skipped. |
| `nr_convtol` | real | 0.01 | < 1 | Tolerance factor for the instance convergence check. Smaller values make the check stricter by allowing bypass when a smaller tolerance is met. |
| `nr_bypasstol` | real | 0.01 | < 1 | Tolerance factor for the bypass input-change check. Smaller values make bypass less aggressive by forcing reevaluation when a larger input change is observed. |
| `nr_contbypass` | boolean | 1 | 0, 1 | Allow forced instance bypass in the first NR iteration when continuation mode is active. |

## Numerical checks

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `matrixcheck` | boolean | 0 | 0, 1 | Check matrix entries for inf/nan before each linear solve. |
| `rhscheck` | boolean | 1 | 0, 1 | Check RHS vector entries for inf/nan. |
| `solutioncheck` | boolean | 1 | 0, 1 | Check solution vector entries for inf/nan. |
| `rcondcheck` | real | 0 | ≥0 | If >0, check the matrix reciprocal condition number in small-signal analyses and fail if it is below this value. 0 disables. |

## Debugging

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `nr_debug` | int | 0 | ≥0 | Debug verbosity. 0 disables debug messages. |

- ≥1 = messages, 
- ≥2 = new solution vector, 
- ≥3 = old solution vector, 
- ≥4 = linear system