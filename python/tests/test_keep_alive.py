"""Lifetimes of objects the binding keeps references to.

A Circuit refers to its ParserTables and SourceCompiler, an Analysis to its
PTAnalysis description and its Circuit. The Python objects that hold these
references must keep the referenced objects alive, or the simulator uses freed
memory once Python drops them. Builds everything from temporaries, collects
garbage (and churns the heap so that freed memory gets reused) and checks the
simulation result. Uses only builtin devices. Runs as a script or under pytest.
"""
import gc
import os
import tempfile

NETLIST = """Divider from builtin devices only
ground 0
model vsrc vsource
model g vccs
v1 (in 0) vsrc dc=2
g1 (in mid in mid) g gain=1m
g2 (mid 0 mid 0) g gain=0.333333333333333m
"""
# g1 = 1k and g2 = 3k in series: mid = 3/4 in.


def churn():
    """Allocates and frees objects like the ones dropped, to reuse their memory."""
    from vacask import Parser, ParserTables, PTAnalysis, PV
    for i in range(50):
        tab = ParserTables("churn %d" % i)
        Parser(tab).parseNetlistString(NETLIST.replace("in", "x%d" % i))
        desc = PTAnalysis("x%d" % i, "tran")
        desc.add(PV("step", 1e-3))
        del tab, desc
    gc.collect()


def make_circuit():
    from vacask import Circuit, OpenvafCompiler, Parser, ParserTables, Status

    s = Status()
    tab = ParserTables("keep alive")
    assert Parser(tab).parseNetlistString(NETLIST), "netlist does not parse"
    assert tab.verify(s), s.message()
    # The tables and the compiler (a temporary) are dropped on return.
    return Circuit(tab, OpenvafCompiler(), s)


def make_analysis():
    from vacask import Analysis, PTAnalysis, Status

    cir = make_circuit()
    gc.collect()
    churn()
    s = Status()
    assert cir.isValid(), s.message()
    assert cir.elaborate([], "__topdef__", "__topinst__", status=s), s.message()
    # The description (a temporary) and the circuit are dropped on return.
    return Analysis.create(PTAnalysis("op1", "op"), cir, s)


def check():
    from vacask import PTSave, Status
    from vacask import simulator as sim
    from vacask.rawfile import rawread

    sim.setup()
    analysis = make_analysis()
    assert analysis
    gc.collect()
    churn()
    s = Status()
    analysis.add(PTSave("default"))
    ok, _ = analysis.run(s)
    assert ok, s.message()
    del analysis
    gc.collect()
    op = rawread("op1.raw").get()
    vin, vmid = (float(op[n][0]) for n in ("in", "mid"))
    assert vin == 2.0, vin
    assert abs(vmid - 1.5) < 1e-9, vmid


def test_keep_alive(tmp_path=None, monkeypatch=None):
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
    print("keep-alive checks passed")
