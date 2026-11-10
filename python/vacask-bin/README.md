# vacask-bin

Prebuilt [VACASK](https://github.com/pepijndevos/VACASK) circuit simulator
binaries, packaged as platform-specific Python wheels (`py3-none-<platform>`).
This is separate from the `vacask` package (the pybind11 extension) built from
the repository root.

## Installation

```bash
pip install vacask-bin
```

## Supported platforms

- Linux x86_64 and aarch64 (glibc >= 2.34)
- macOS arm64 (macOS 14+)
- Windows x86_64

## Usage

The `vacask` command is placed on PATH. From Python:

```python
import vacask_bin

vacask_bin.VACASK_CMD   # path to the vacask binary
vacask_bin.OPENVAF_CMD  # path to the openvaf-r binary
vacask_bin.MOD_DIR      # path to the OSDI device models
```

## How the wheels are built

The wheels repackage the release archives produced by `.github/workflows/ci.yml`.
On a `_<version>` tag the `release` job builds the archives, then runs

```bash
python python/vacask-bin/build_wheels.py \
  --release-tag _<version> --cache-dir <dir with archives> --output-dir dist/
```

and attaches the wheels to the GitHub release; the `pypi-vacask-bin` job then uploads them to PyPI via trusted publishing. The wheel version is the tag
normalized for Python (`_0.3.4-dev3` becomes `0.3.4.dev3`). Without
`--cache-dir` contents, the archives are downloaded from the GitHub release.

Run the packaging tests with `python -m pytest python/vacask-bin`.
