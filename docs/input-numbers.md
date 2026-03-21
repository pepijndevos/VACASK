# Numbers

The netlist parser recognises numeric literals and identifiers according to the
rules below. Understanding them is crucial for writing correct parameterised
circuits and expressions.

## Numeric literals

VACASK accepts the following forms of numbers in circuit statements and
expressions:

1. **Integer** - base-10 digits, e.g. `0`, `42`, `12345`.
2. **Hexadecimal integer** - prefix `0x` or `0X`, e.g. `0xFF`, `0x1a3`.
3. **Floating point**
   - with a decimal point: `3.14`, `2.`, `.5`
   - with optional exponent: `6.022e23`, `1.0E-3`
4. **SI-prefix notation** - integer or floating object with a decimal point
   followed immediately by an SI prefix and optional unit string
   (letters and `_` are allowed after the prefix).
   Examples: `10k`, `1.5meg`, `100nF`, `3.3u`, `2mil`.
5. **Special values** - case-insensitive `inf` and `nan` are recognised as
   floating point values.

## SI prefixes

The lexer implements the prefixes shown in the table below. When a prefix is
encountered the numeric value is multiplied by the corresponding factor.

| Prefix       | Name     | Multiplier              | Notes                               |
|--------------|----------|-------------------------|-------------------------------------|
| a            | atto     | $1\times10^{-18}$       |                                     |
| f            | femto    | $1\times10^{-15}$       |                                     |
| p            | pico     | $1\times10^{-12}$       |                                     |
| n            | nano     | $1\times10^{-9}$        |                                     |
| u            | micro    | $1\times10^{-6}$        | the letter `$\mu$` is not allowed   |
| m            | milli    | $1\times10^{-3}$        |                                     |
| k, K         | kilo     | $1\times10^{3}$         | case-insensitive                    |
| M, x, X, meg | mega     | $1\times10^{6}$         |                                     |
| G            | giga     | $1\times10^{9}$         |                                     |
| T            | tera     | $1\times10^{12}$        |                                     |
| mil          | mil      | $25.4\times10^{-6}$     | 0.001 inch; a legacy unit           |

`meg`, `mil` are three-letter prefixes; other prefixes are single characters.
Any trailing characters after the prefix (for example the unit symbol `F` or
`Hz`) are ignored by the numeric parser.

