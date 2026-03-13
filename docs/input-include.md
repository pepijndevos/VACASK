# Including a File

The `include` directive allows you to split large input files into smaller,
manageable pieces. It inserts the contents of another file into the current
input file at the point where the directive appears.

## Basic include

The simplest form includes a file by name:

```text
include "common.inc"
```

The filename must be enclosed in double quotes. Path resolution follows this
order:

1. **Directory of the including file** – If the current input file is in
   `/home/user/circuits/main.sim` and it includes `"models.inc"`, VACASK looks
   for `/home/user/circuits/models.inc`.

2. **Current working directory** – If not found in the including file's
   directory, VACASK checks the directory where VACASK was started.

3. **Include path** – Finally, VACASK searches the include path. This defaults
   to `<VACASK_LIB>/inc` but can be overridden with the `SIM_INCLUDE_PATH`
   environment variable. Use colons to separate multiple directories (semicolons
   on Windows).

Absolute paths are supported and bypass the search order:

```text
include "/usr/local/share/vacask/models.inc"
```

If the file is not found VACASK reports an error and stops parsing. 

## Library includes with sections

For larger projects, you can organize files into libraries with named sections.
This allows selective inclusion of specific parts of a file:

```text
include "library.inc" section=common
```

The file `library.inc` might contain multiple sections:

```text
section common
// Common definitions
model resistor resistor
model capacitor capacitor
endsection

section analog
// Analog-specific models
model opamp opamp
endsection

section digital
// Digital-specific models
model inverter inverter
endsection
```

When you include with `section=common`, only the content between `section common` 
and the next `endsection` directive is included. If the section is not found VACASK
reports an error and stops parsing. 

### Section syntax

Sections begin with:

```text
section section_name
```

The section name follows identifier rules: letters, digits, underscores, and
dollar signs, starting with a letter or underscore. Sections end at the next
`endsection` directive. 

### Library file search

When including with a section, VACASK searches for the file in the same
locations as basic includes, but treats it as a library file. Library files
can contain multiple sections for different purposes.

## Nesting includes

Includes can be nested arbitrarily deep. A file included with sections can
itself include other files. VACASK maintains a stack of open files and
prevents infinite recursion by tracking the file stack.

## Use cases

**Modular circuit design** – Split large circuits into functional blocks:

```text
// main.sim
include "power.inc"
include "analog.inc"
include "digital.inc"

// Circuit definition here
```

**Reusable model libraries** – Create libraries of device models:

```text
// models.lib
section basic
model resistor resistor
model capacitor capacitor
endsection

section advanced
model bsim4 bsim4
endsection
```

Then include selectively:

```text
include "models.lib" section=basic
```

## Debugging includes

Use the `-df` command-line option to see which files VACASK loads and their
paths:

```bash
vacask -df circuit.sim
```
