# Vectors and Lists

VACASK expressions support two compound types: vectors and lists.

## Vectors

A vector is an ordered sequence of elements all sharing the same scalar type (`integer`, `real`, or `string`). Vectors are written with comma-separated elements in square brackets:

```text
[1, 2, 3]          // integer vector
[1.0, 2.5, 3.14]   // real vector
["a", "b", "c"]    // string vector
[1k, 50k]          // real vector [1000.0, 50000.0]
```

An empty vector is written `[]` or `[,]` and has type `integer`.

### Type promotion in vectors

When a vector contains mixed `integer` and `real` elements the entire vector becomes `real`:

```text
[1, 2.0, 3]   // real vector [1.0, 2.0, 3.0]
```

Mixing numeric and string elements is an error.

### Flattening

If an element of a vector literal is itself a vector, it is recursively flattened into the result:

```text
parameters v = [2, 3]
[1, v, 4]   // [1, 2, 3, 4]
```

### Element access

Elements are accessed with zero-based indexing using `[]`:

```text
parameters v = [10, 20, 30]
parameters x = v[0]   // 10
parameters y = v[2]   // 30
```

## Lists

A list is an ordered sequence of elements that may have different types. Elements are wrapped in curly braces and separated by commas:

```text
{"node1", 1.0, "node2", 2.0}   // list: string, real, string, real
{1, "yes", [2, 3]}             // list: integer, string, integer vector
```

An empty list is written `{}` or `{,}`.

Lists are used where parameters accept heterogeneous data, such as initial conditions and nodesets:

```text
analysis tran1 tran ic={"vout", 3.3, "vgate", 0.8}
```

Unlike vectors, list elements are stored intact - nested lists remain as sub-lists.

## Merging lists

Lists can be merged by first building a list of lists and then calling `flatten`. Non-list elements are kept in their places by `flatten`. This makes it easy to combine partial lists:

```text
parameters ic_stage1 = {"vout1", 1.0}
parameters ic_stage2 = {"vout2", 2.0}
// Combine into one flat list:
//   {"vout1", 1.0, "vout2", 2.0, "vout3", 3.0}
parameters ic = flatten({ic_stage1, ic_stage2, "vout3", 3.0})
```

Flattening affects only the first nested level of lists. 

## Summary

| Syntax | Separator | Result type | Element handling |
|--------|-----------|-------------|-----------------|
| `[a, b, c]` | `,` | Vector (homogeneous) | Nested vectors/lists flattened |
| `{a, b, c}` | `;` | List (heterogeneous) | Elements stored intact |
