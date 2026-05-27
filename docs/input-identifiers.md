# Identifiers

Identifiers name nodes, instances, models, parameters and other named
entities.  The following rules apply:

- **Unquoted identifiers** must start with a letter (`A-Z`, `a-z`), underscore
  (`_`), or dollar sign (`$`). Subsequent characters may be letters, digits,
  underscores or dollar signs.  Examples: `R1`, `node_foo`, `$temp`.
- **Quoted identifiers** (single quotes) may contain any non-whitespace characters.
  A literal single quote is written as `''` (two single quotes). They are
  useful when the name would otherwise be illegal,
  e.g. `'3.3V'` or `'node(1)'`.
- **Mixed identifiers** may combine quoted and unquoted parts concatenated together.
  The first part, if unquoted, must start with a letter, underscore, or `$`. 
  Subsequent parts can be quoted or unquoted.
  Examples: `R'1'`, `node'_'name`, `'3.3'V`, `var'part'2'name'end`.
  Quoted parts handle the `''` escape sequence as above.

Identifiers (like everything else in the input file) are **case-sensitive**; `R1` and `r1` are distinct.

> **Note:** reserved words such as `model`, `control`, `options`, etc., cannot
> be used as unquoted identifiers.  See the [reserved-words](input-reserved.md) 
> section for the complete list.

Identifiers appear in many contexts:

```text
v1 (p n) vsource dc=10         // Instance name v1, node names p and n,
                               // master (model) name vsource,
                               // parameter name dc. 
model resistor resistor        // First resistor is a model identifier,
                               // the second is the device name identifier. 
ground gnd                     // Node identifier gnd.
parameters width = 10u         // Parameter name width. 
```
