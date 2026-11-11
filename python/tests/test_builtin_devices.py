"""Simulation test that needs no OSDI models (and so no OpenVAF).

Uses only the builtin devices (vsource, vccs, vcvs): a resistor divider made of
transconductances and an amplifier, run through the operating point, AC and
transient analyses and compared with the analytic result. It exercises the
parser, circuit elaboration, matrix assembly, the sparse solver and the
analyses, which makes it usable where no compiled device models exist, i.e.
the WebAssembly (Pyodide) wheel. Runs as a script or under pytest.
"""
import os
import tempfile

import numpy as np

NETLIST = """Divider and amplifier from builtin devices only
ground 0
model vsrc vsource
model g vccs
model e vcvs
v1 (in 0) vsrc type="sine" sinedc=1 ampl=2 freq=1k mag=1
g1 (in mid in mid) g gain=1m
g2 (mid 0 mid 0) g gain=0.333333333333333m
e1 (out 0 mid 0) e gain=4
g3 (out 0 out 0) g gain=1m
"""
# g1 = 1k and g2 = 3k in series: mid = 3/4 in; e1 amplifies by 4: out = 3 in.


def check():
    from vacask import (Analysis, Circuit, OpenvafCompiler, Parser, ParserTables,
                        PTAnalysis, PTSave, PV, Status)
    from vacask import simulator as sim
    from vacask.rawfile import rawread

    sim.setup()
    s = Status()
    tab = ParserTables("builtin")
    assert Parser(tab).parseNetlistString(NETLIST), "netlist does not parse"
    assert tab.verify(s), s.message()
    cir = Circuit(tab, OpenvafCompiler(), s)
    assert cir.isValid(), s.message()
    assert cir.elaborate([], "__topdef__", "__topinst__", status=s), s.message()
    assert cir.instanceCount() == 6

    def run(desc):
        analysis = Analysis.create(desc, cir, s)
        assert analysis, s.message()
        analysis.add(PTSave("default"))
        ok, _ = analysis.run(s)
        assert ok, s.message()

    # Operating point (the sine source holds sinedc=1 at t=0)
    run(PTAnalysis("op1", "op"))
    op = rawread("op1.raw").get()
    vin, vmid, vout = (float(op[n][0]) for n in ("in", "mid", "out"))
    assert vin == 1.0
    assert abs(vmid - 0.75 * vin) < 1e-9 and abs(vout - 3 * vin) < 1e-9

    # AC: the gains are flat
    ac = PTAnalysis("ac1", "ac")
    for p in (PV("from", 1.0), PV("to", 1e6), PV("mode", "dec"), PV("points", 5)):
        ac.add(p)
    run(ac)
    r = rawread("ac1.raw").get()
    assert len(r["frequency"]) == 31
    assert np.allclose(np.abs(r["mid"]), 0.75) and np.allclose(np.abs(r["out"]), 3.0)

    # Transient: out(t) = 3 (1 + 2 sin(2 pi f t))
    tr = PTAnalysis("tran1", "tran")
    tr.add(PV("step", 1e-6))
    tr.add(PV("stop", 3e-3))
    run(tr)
    r = rawread("tran1.raw").get()
    t = r["time"]
    assert len(t) > 10 and abs(t[-1] - 3e-3) < 1e-12
    expected = 3 * (1 + 2 * np.sin(2 * np.pi * 1e3 * t))
    assert np.max(np.abs(r["out"] - expected)) < 1e-6


def test_builtin_devices(tmp_path=None, monkeypatch=None):
    # Rawfiles are written to the working directory.
    if monkeypatch is not None and tmp_path is not None:
        monkeypatch.chdir(tmp_path)
    check()


if __name__ == "__main__":
    start = os.getcwd()
    with tempfile.TemporaryDirectory() as d:
        os.chdir(d)
        try:
            check()
        finally:
            # Leave the directory before it is removed (not possible otherwise
            # on every platform, e.g. Pyodide's filesystem).
            os.chdir(start)
    print("builtin device simulation checks passed")
