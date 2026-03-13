# Reserved Words

Certain words have special meaning in the netlist language and cannot be used
as unquoted identifiers. They are recognised only when they appear at the
beginning of a line and are case‑sensitive.

| Word         | Description |
|--------------|-------------|
| `include`    | Include (a part) of another file into the netlist. |
| `section`    | Start of a file section. |
| `endsection` | End of a file section. |
| `load`       | Load a device model (Verilog‑A source). |
| `model`      | Declare a model. |
| `global`     | Declare one or more global nodes. |
| `ground`     | Declare one or more ground/reference nodes. |
| `subckt`     | Start a subcircuit definition. |
| `ends`       | End a subcircuit definition. |
| `parameters` | Begin a block of parameter declarations. |
| `control`    | Begin the control block for simulation setup. |
| `endc`       | End the control block. |
| `analysis`   | Define an analysis (op, dc, ac, tran, hb, etc.). |
| `sweep`      | Specify a parameter sweep. |
| `embed`      | Embed an external file's contents directly. |
| `save`       | Request that certain values be saved. |
| `@if`        | Conditional netlist block start. |
| `@elseif`    | Conditional netlist block alternative. |
| `@else`      | Conditional netlist block fallback. |
| `@end`       | End of conditional netlist block. |

> **Note:** the `save`, `sweep`, and `analysis` keywords are only treated specially 
> inside the control block; outside of `control` they are parsed as a regular identifiers.

Quoted identifiers (single quotes) may still use these words as names.
