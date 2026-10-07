# Python Helpers

VACASK ships a Python package named `vacask` in the `python/` subdirectory of the VACASK library directory (`<vacask library directory>/python/vacask`). When a `postprocess` command runs, `<vacask library directory>/python` is automatically prepended to `PYTHONPATH`, so the package's modules can be imported as `vacask.<module>` without any path setup.

`vacask.rawfile` is the primary helper for postprocessing scripts. It reads SPICE binary raw files written by VACASK and returns the data as NumPy arrays, making it straightforward to plot or numerically verify results.

`vacask.runtest` provides simple helpers used by the bundled test suite. It is available to user scripts as well.

The package also contains netlist and PDK converters (e.g. `ng2vc`, `ts2vc`, `sg13g2tovc`). Run them as modules, i.e. `python3 -m vacask.<converter>`, with `<vacask library directory>/python` in `PYTHONPATH`. Type `vacask -dp` and look for "Python path addition" to find this directory.
