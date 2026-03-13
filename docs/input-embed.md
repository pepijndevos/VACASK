# Embedded Files

The `embed` directive allows you to include external file contents directly in
the input file. This is useful for embedding postprocessing scripts, data files,
or other auxiliary content without maintaining separate files on disk.

## Syntax

```text
embed "filename" <<<MARKER
file contents here
>>>MARKER
```

The marker is a user‑chosen identifier (alphanumeric and underscores only) that
delineates the start and end of the embedded content. Everything between these 
markers is preserved exactly as written, including newlines and whitespace. 
The content between markers is literal. There is no variable substitution or
macro expansion within embedded text. 

## Common use cases

**Embedded postprocessing scripts** – The most common application. Scripts
(typically Python) perform analysis, visualization, or validation of
simulation results.

```text
embed "postprocess.py" <<<PYEOF
from rawfile import rawread
import matplotlib.pyplot as plt

data = rawread("results.raw").get()
plt.plot(data.v('1'), label='Node 1')
plt.show()
>>>PYEOF
```

**Test data or lookup tables** – Store small datasets directly in the input file.

```text
embed "lookup.csv" <<<DATA
x,y
0,1.0
1,1.5
2,2.0
>>>DATA
```

## File writing

When the input file is parsed, VACASK writes all embedded files to the working
directory before simulation begins. The file is written with the name specified
in the `embed` directive (the first string argument). If a file already exists, 
it is overwritten. 

For each embedded file VACASK creates an origin file named `embedded_file_name.origin`. 
The file contains the path to the input file where the embedded file content is found. 
This origin file and its timestamp are used for determining whether the embedded file 
should be dumped or dumping can be safely skipped because there are no changes to 
the embedded file.

Embedded files can then be referenced by name in `postprocess` commands or
other directives:

```text
control
  analysis op1 op
  postprocess(PYTHON, "postprocess.py")  // Runs the embedded script
endc

embed "postprocess.py" <<<PYEOF
# ... script content ...
>>>PYEOF
```

## Environment variables in postprocessing

VACASK sets the `PYTHON` circuit variable to the full path of the Python 3
interpreter (if available). Use this in the control block to invoke Python
portably:

```text
postprocess(PYTHON, "script.py")
```

