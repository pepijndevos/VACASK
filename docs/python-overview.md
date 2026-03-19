# Python Helpers

VACASK ships a small collection of Python modules in the `python/` directory of the installation. When a `postprocess` command runs, this directory is automatically prepended to `PYTHONPATH`, so all modules can be imported directly without any path setup.

`rawfile` is the primary helper for postprocessing scripts. It reads SPICE binary raw files written by VACASK and returns the data as NumPy arrays, making it straightforward to plot or numerically verify results.

`runtest` provides simple helpers used by the bundled test suite. It is available to user scripts as well.
