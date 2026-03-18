# Startup and Configuration

VACASK is invoked from the command line with an optional set of flags followed by an input file:

```text
vacask [options] [<filename>]
```

Before the input file is parsed, VACASK sets up its runtime environment: it reads [TOML configuration files](startup-toml.md) to determine where device models and include files live, which OpenVAF compiler binary to use, and how to locate Python. Search paths can also be overridden with environment variables. See [Search Paths and Configuration Files](startup-paths.md) for details.

Command line flags control diagnostic output and let you suppress embedded file extraction, postprocessing, or result file writing. A full reference along with the startup sequence is in [Command Line Options and Startup Sequence](startup-options.md).

Run `vacask -dp` whenever a file cannot be found — it prints the path of every file the simulator is trying to read.
