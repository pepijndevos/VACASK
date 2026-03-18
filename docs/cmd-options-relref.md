# Relative Tolerance Reference

The relative tolerance check compares a change (solution delta, residual, or LTE estimate) to a reference value. These options control how that reference is computed.

## Master option

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `relref` | string | `alllocal` | `alllocal`, `sigglobal`, `allglobal` | Default reference policy used when `relrefsol`, `relrefres`, or `relreflte` are set to `relref`. |

## Per-check overrides

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `relrefsol` | string | `relref` | `pointlocal`, `local`, `pointglobal`, `global`, `relref` | Reference for solution-change relative tolerance. |
| `relrefres` | string | `relref` | same as above | Reference for residual relative tolerance. |
| `relreflte` | string | `relref` | same as above | Reference for local truncation error relative tolerance. |

- `pointlocal` = per-unknown, current point, 
- `local` = per-unknown, maximum over past points, 
- `pointglobal` = maximum over all unknowns of same type, current point, 
- `global` = maximum over all unknowns of same type, maximum over past points, 
- `relref` = delegate to `relref` option,

The `relref` option sets the following relative reference strategies for Newton-Raphson solver solution change (`relrefsol`), residual change (`relrefres`), and local truncation error (`relreflte`) when the choice is delegated to `relref`. 

| `relref` | `relrefsol` | `relrefres` | `relreflte` |
|----------|-------------|-------------|-------------|
| `alllocal`  | `local`  | `local`  | `local`  |
| `sigglobal` | `global` | `local`  | `global` |
| `allglobal` | `global` | `global` | `global` |

