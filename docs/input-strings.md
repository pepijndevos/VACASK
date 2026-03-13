# Strings

Strings represent text values in the input file. They are used for filenames,
script content, and other text data. VACASK supports two string forms: short
strings with escape sequences and long (heredoc-style) strings for
multi-line content.

## Short strings

Short strings are enclosed in **double quotes** (`"`). They are typically used
for filenames and single-line text:

```text
load "resistor.osdi"
parameters title = "My Circuit"
postprocess(PYTHON, "analysis.py")
```

### Escape sequences

Within double-quoted strings, the following escape sequences are recognized:

| Sequence | Result              | Unicode |
|----------|---------------------|---------|
| `\n`     | Newline             | U+000A  |
| `\t`     | Tab                 | U+0009  |
| `\r`     | Carriage return     | U+000D  |
| `\b`     | Backspace           | U+0008  |
| `\f`     | Form feed           | U+000C  |
| `\"`     | Double quote        | U+0022  |
| `\\`     | Backslash           | U+005C  |
| `\`*ooo* | Octal escape (1–3 digits) | *varies* |
| `\`*x*   | Any other character *x*   | *x*      |

The last two rows merit explanation:

**Octal escapes** – `\123` represents the character with octal value 123
(decimal 83, the letter `S`). Sequences may be 1, 2, or 3 octal digits. For
example, `\7` is bell (U+0007), `\101` is `A` (U+0041).

**Arbitrary escapes** – `\x` for any character not listed above yields that
character. For example, `\$` yields `$`, and `\#` yields `#`. This is useful
when the character has special meaning in other contexts.

### Newlines in strings

Literal newlines are **not allowed** in double-quoted strings. To include a
newline, use the `\n` escape sequence.

## Long strings

Long (heredoc-style) strings begin with `<<<MARKER` on a line by itself and
extend to a line containing only `>>>MARKER`. They are useful for embedding
multi-line content such as scripts or data:

```text
embed "script.py" <<<PYEOF
import numpy as np
print("Hello")
>>>PYEOF
```

The marker (here `PYEOF`) is user-chosen and must consist of alphanumeric
characters and underscores only. It is case-sensitive and appears twice: once
to begin the string and once to end it. VACASK trims trailing whitespace on 
the marker line before matching.

Everything between the markers is preserved exactly as written:

- **Newlines** are literal (not escaped).
- **Whitespace** (spaces, tabs) is preserved.
- **Escape sequences** are *not* processed; `\n` in the content remains `\n`.

```text
embed "data.txt" <<<DATA
x,y
1,\tspecial
>>>DATA
```

In this example, the second line contains a backslash followed by `t` (not a
tab), because escape processing does not occur in long strings.

## Context and usage

Strings appear in different contexts:

- **Filenames**: `load "model.osdi"`, `include "circuit.inc"`, `embed "test.py"`
- **Parameter values**: `parameters name = "value"`
- **Command arguments**: `postprocess(PYTHON, "script.py")`
- **Embedded content**: Any text between `<<<` and `>>>` markers

## Examples

**Short string with escape sequences:**

```text
var msg = "Line 1\nLine 2\tTabbed"
```

Result: `Line 1` (newline) `Line 2` (tab) `Tabbed`

**Long string with literal backslash:**

```text
embed "regex.txt" <<<PATTERNS
\d+ matches digits
\w+ matches word chars
>>>PATTERNS
```

Result: literal backslashes (not escape sequences).

**Octal escape:**

```text
var bell = "\7"  // ASCII bell character
```
