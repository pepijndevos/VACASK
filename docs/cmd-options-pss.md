# Periodic Steady-State Options

These options control the outer Newton (shooting) loop of the [periodic steady-state analysis](cmd-analysis-pss.md). The transients run inside the loop are governed by the [transient analysis options](cmd-options-tran.md) and the [Newton-Raphson solver options](cmd-options-nr.md).

## Iteration limit

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `pss_itl` | int | 50 | >0 | Maximum number of shooting (outer Newton) iterations. |

## Convergence

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `pss_tolscale` | real | 1.0 | >0 | Scale factor for the shooting residual tolerance. The per-unknown tolerance is `pss_tolscale * max(reltol*|x|, abstol)`. Values below 1 tighten the periodicity requirement, values above 1 relax it. |

## Debugging

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `pss_debug` | int | 0 | ≥0 | Debug verbosity. 0 disables debug messages. ≥1 prints the per-iteration shooting state (initial point, endpoint, period), the worst residual ratio, and the monodromy matrices. |
