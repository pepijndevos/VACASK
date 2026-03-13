# Identifiers

Identifiers name nodes, instances, models, parameters and other named
entities.  The following rules apply:

- Unquoted identifiers must start with a letter (`A–Z`, `a–z`), underscore
  (`_`), or dollar sign (`$`). Subsequent characters may be letters, digits,
  underscores or dollar signs.  Examples: `R1`, `node_foo`, `$temp`.
- Quoted identifiers (single quotes, `''`) may also contain other printable
  characters except the quote itself (currently `(`, `)`, `.`, `:`, and `!`
  are allowed). They are useful when the name would otherwise be illegal, 
  e.g. `'3.3V'` or `'node(1)'`.

Identifiers (like everything else in the input file) are **case‑sensitive**; `R1` and `r1` are distinct.

> **Note:** reserved words such as `model`, `control`, `options`, etc., cannot
> be used as unquoted identifiers.  See the [reserved‑words](input-reserved.md) 
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
