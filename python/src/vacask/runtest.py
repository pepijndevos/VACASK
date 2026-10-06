import sys, os
import subprocess
from pathlib import Path
from functools import reduce
import numpy as np
from pprint import pprint

__all__ = [ "absDiff", "relDiff", "isTest", "osdiVersion", "skipUnlessOsdi" ]

# Exit code that ctest treats as "skipped" (SKIP_RETURN_CODE in test/CMakeLists.txt)
SKIP_EXIT_CODE = 77

def osdiVersion(osdiFile):
    """(major, minor) of the OSDI interface a compiled .osdi module exposes."""
    import ctypes
    lib = ctypes.CDLL(os.path.abspath(osdiFile))
    return (
        ctypes.c_uint32.in_dll(lib, "OSDI_VERSION_MAJOR").value,
        ctypes.c_uint32.in_dll(lib, "OSDI_VERSION_MINOR").value,
    )

def skipUnlessOsdi(minor, osdiFile, feature):
    """Skip the test when the compiled module exposes an OSDI interface older than 0.<minor>.

    Some Verilog-A features (absdelay) are only described by OSDI 0.5 modules. An older
    compiler silently drops them, so the test would fail for a reason the simulator
    cannot report. Under ctest the exit code marks the test as skipped, when run by
    hand the message is printed and the script continues.
    """
    major, got = osdiVersion(osdiFile)
    if (major, got) < (0, minor):
        print(f"SKIP: {feature} needs an OSDI 0.{minor} module, the compiler produced OSDI {major}.{got} ({osdiFile}).")
        if isTest():
            sys.exit(SKIP_EXIT_CODE)

def absDiff(a, b):
    return np.abs(a-b)

def relDiff(a, b, abstol):
    aabs = np.abs(a)
    babs = np.abs(b)
    ref = np.where(aabs>=babs, aabs, babs)
    ref = np.where(ref>abstol, ref, abstol)
    return np.abs(a-b)/ref

def isTest():
	return "SIM_TEST" in os.environ and os.environ["SIM_TEST"]=="yes"
