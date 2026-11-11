"""Simulations with the device models (OSDI) bundled in the wheel.

Runs with the native wheels and with the WebAssembly (Pyodide) wheel, whose
models are WebAssembly side modules compiled by OpenVAF-reloaded
(--target wasm32-unknown-emscripten). Checks that every bundled model loads,
compares simple circuits with analytic results and checks the big compact
models (BSIM4, PSP103) against results of the native build. Runs as a script
or under pytest.
"""
import math
import os
import tempfile

import numpy as np

K_B = 1.380649e-23
Q_E = 1.602176634e-19
VT = K_B * 300.15 / Q_E  # thermal voltage at the default temperature (27C)


def mod_dir():
    import vacask
    return os.path.join(os.path.dirname(vacask.__file__), "mod")


def bundled_models():
    """Paths of all bundled .osdi files, relative to the module directory."""
    d = mod_dir()
    return sorted(
        os.path.relpath(os.path.join(root, f), d).replace(os.sep, "/")
        for root, _, files in os.walk(d) for f in files if f.endswith(".osdi")
    )


def elaborate(netlist):
    from vacask import Circuit, OpenvafCompiler, Parser, ParserTables, Status
    from vacask import simulator as sim

    sim.setup()
    s = Status()
    tab = ParserTables("models")
    assert Parser(tab).parseNetlistString(netlist), "netlist does not parse"
    assert tab.verify(s), s.message()
    compiler = OpenvafCompiler()
    cir = Circuit(tab, compiler, s)
    assert cir.isValid(), s.message()
    assert cir.elaborate([], "__topdef__", "__topinst__", status=s), s.message()
    # The circuit refers to the parser tables and the compiler, but does not
    # keep them alive: return them with it.
    return cir, (tab, compiler, s)


def simulate(netlist, name, kind, **params):
    """Elaborates the netlist, runs one analysis, returns the rawfile data."""
    from vacask import Analysis, PTAnalysis, PTSave, PV, Status
    from vacask.rawfile import rawread

    s = Status()
    cir, keep = elaborate(netlist)
    desc = PTAnalysis(name, kind)
    for k, v in params.items():
        desc.add(PV(k, v))
    analysis = Analysis.create(desc, cir, s)
    assert analysis, s.message()
    analysis.add(PTSave("default"))
    ok, _ = analysis.run(s)
    assert ok, s.message()
    return rawread(name + ".raw").get()


def op(netlist):
    res = simulate(netlist, "op1", "op")
    return lambda node: float(res[node][0])


def check_all_models_load():
    models = bundled_models()
    # At least the models listed in devices/CMakeLists.txt (53 at the time of writing)
    assert len(models) >= 50, models
    # One circuit per model: the variants (spice/, spice/sn/, spice/full/)
    # define modules of the same name.
    for m in models:
        elaborate(f"""Load {m}
ground 0
load "{m}"
model g vccs
model v vsource
v1 (1 0) v dc=1
g1 (1 0 1 0) g gain=1m
""")
    return len(models)


def check_rc_transient(measured):
    # Step response of an RC circuit (tau = 1ms), as test/test_tran.sim
    r = simulate("""RC
ground 0
load "resistor.osdi"
load "capacitor.osdi"
model resistor resistor
model capacitor capacitor
model vsource vsource
v1 (1 0) vsource type="pulse" val0=1 val1=2 rise=1u delay=1m
r1 (1 2) resistor r=1k
c1 (2 0) capacitor c=1u
""", "tran1", "tran", stop=10e-3, step=1e-6, icmode="op")
    t = r["time"]
    exact = 1 + (1 - np.exp(-(t - 1e-3) / 1e-3)) * (t > 1e-3)
    err = np.max(np.abs(r["2"] - exact) / np.maximum(np.abs(exact), 1e-6))
    assert err < 0.0035, err
    measured["tran: rc v(2) at 2ms"] = float(np.interp(2e-3, t, r["2"]))


def check_diodes(measured):
    # 1mA forced through a diode: V = n Vt ln(I/Is + 1)
    for path, module in [("diode.osdi", "diode"), ("spice/diode.osdi", "sp_diode"),
                         ("spice/sn/diode.osdi", "sp_diode"),
                         ("spice/full/diode.osdi", "sp_diode")]:
        v = op(f"""Diode
ground 0
load "{path}"
model i isource
model d {module} is=1e-14 n=1.5
i1 (0 a) i dc=1m
d1 (a 0) d
""")
        expected = 1.5 * VT * math.log(1e-3 / 1e-14 + 1)
        # The models' physical constants differ from CODATA 2018 by some ppm
        assert abs(v("a") / expected - 1) < 1e-5, (path, v("a"), expected)
        measured[f"{path} v(a)"] = v("a")


def check_rectifier(measured):
    # Half-wave rectifier, the diode's junction capacitance and series
    # resistance exercise the reactive Jacobian entries.
    r = simulate("""Rectifier
ground 0
load "spice/diode.osdi"
load "resistor.osdi"
load "capacitor.osdi"
model v vsource
model r resistor
model c capacitor
model d sp_diode is=1e-14 rs=10 cjo=10p tt=1n
v1 (in 0) v type="sine" sinedc=0 ampl=5 freq=1k
d1 (in out) d
cl (out 0) c c=1u
rl (out 0) r r=10k
""", "tran1", "tran", stop=3e-3, step=1e-6)
    out = r["out"]
    # Peak detector: charges to about 5 V minus a diode drop, droops by
    # T/(R C) = 10% between peaks
    assert 3.8 < out.max() < 4.6 and out[-1] > 0.85 * out.max(), (out.max(), out[-1])
    measured["tran: rectifier v(out) at 3ms"] = float(out[-1])
    measured["tran: rectifier max v(out)"] = float(out.max())


def check_mos1(measured):
    # Level 1 NMOS in saturation: Id = kp/2 W/L (Vgs-Vto)^2 = 150uA through 1k
    for path in ["spice/mos1.osdi", "spice/sn/mos1.osdi", "spice/full/mos1.osdi"]:
        v = op(f"""MOS1
ground 0
load "{path}"
load "resistor.osdi"
model r resistor
model v vsource
model nfet sp_mos1 type=1 vto=0.5 kp=120e-6
vdd (vdd 0) v dc=2.5
vg (g 0) v dc=1
rd (vdd d) r r=1k
m1 (d g 0 0) nfet w=10u l=1u
""")
        assert abs(v("d") - 2.35) < 1e-5, (path, v("d"))
        measured[f"{path} v(d)"] = v("d")


def check_bjt(measured):
    # Gummel-Poon NPN with default parameters (only Is, Bf): Ic = Is exp(Vbe/Vt)
    v = op("""BJT
ground 0
load "spice/bjt.osdi"
load "resistor.osdi"
model r resistor
model v vsource
model npn sp_bjt type=1 is=1e-16 bf=100
vcc (vcc 0) v dc=5
vb (b 0) v dc=0.7
rc (vcc c) r r=10k
q1 (c b 0 0) npn
""")
    ic = 1e-16 * math.exp(0.7 / VT)
    assert abs(v("c") / (5 - 10e3 * ic) - 1) < 1e-5, (v("c"), 5 - 10e3 * ic)
    measured["spice/bjt.osdi v(c)"] = v("c")


# CMOS inverters with (mostly) the models' default parameters (Vdd = 1.2 V):
# file, module, instance parameters, NMOS and PMOS model parameters
INVERTERS = {
    "bsim3v3": ("bsim3v3.osdi", "bsim3", "w=1u l=0.18u", "vth0=0.4", "vth0=-0.4"),
    "bsim4": ("bsim4v8.osdi", "bsim4", "w=1u l=0.1u", "", ""),
    "bsimbulk": ("bsimbulk106.osdi", "bsimbulk", "w=1u l=0.1u", "", ""),
    "psp103": ("psp103v4.osdi", "psp103va", "w=1u l=0.1u", "", ""),
}
VDD = 1.2


def inverter_netlist(path, module, size, npar, ppar, vin, load=""):
    return f"""Inverter
ground 0
load "{path}"
load "capacitor.osdi"
model v vsource
model c capacitor
model n {module} type=1 {npar}
model p {module} type=-1 {ppar}
vdd (vdd 0) v dc={VDD}
vin (in 0) v {vin}
m1 (out in 0 0) n {size}
m2 (out in vdd vdd) p {size}
{load}
"""


def check_inverters(measured):
    for name, inv in INVERTERS.items():
        lo, hi, mid = (op(inverter_netlist(*inv, f"dc={vin}"))("out")
                       for vin in (0, VDD, VDD / 2))
        assert lo > 0.99 * VDD and hi < 0.01 * VDD, (name, lo, hi)
        assert 0 < mid < VDD, (name, mid)
        measured[f"{name} inverter v(out) at Vin=Vdd/2"] = mid

    # Transient of the BSIM4 inverter driving a capacitor
    r = simulate(inverter_netlist(
        *INVERTERS["bsim4"],
        f'type="pulse" val0=0 val1={VDD} delay=1n rise=100p fall=100p width=5n period=10n',
        "cl (out 0) c c=10f"), "tran1", "tran", stop=12e-9, step=10e-12)
    t, out = r["time"], r["out"]
    # Input high from 1.1ns to 6.1ns (and again from 11.1ns)
    low, high = np.interp(3e-9, t, out), np.interp(8e-9, t, out)
    assert out[0] > 0.99 * VDD and low < 0.01 * VDD and high > 0.99 * VDD, (out[0], low, high)
    measured["tran: bsim4 inverter v(out) at 8ns"] = float(high)


# Results of the native build (x86_64 Linux). The WebAssembly models must give
# the same results up to rounding: OpenVAF lets LLVM reassociate floating point
# operations, which can round differently on different targets.
NATIVE = {
    'tran: rc v(2) at 2ms': 1.6314023752055626,
    'diode.osdi v(a)': 0.9826804677176605,
    'spice/diode.osdi v(a)': 0.9826803508030543,
    'spice/sn/diode.osdi v(a)': 0.9826803508030543,
    'spice/full/diode.osdi v(a)': 0.9826803508030543,
    'tran: rectifier v(out) at 3ms': 4.019091632311872,
    'tran: rectifier max v(out)': 4.356735241452538,
    'spice/mos1.osdi v(d)': 2.34999999764,
    'spice/sn/mos1.osdi v(d)': 2.34999999764,
    'spice/full/mos1.osdi v(d)': 2.34999999764,
    'spice/bjt.osdi v(c)': 4.432965241196468,
    'bsim3v3 inverter v(out) at Vin=Vdd/2': 0.317169096557318,
    'bsim4 inverter v(out) at Vin=Vdd/2': 0.21084826325442887,
    'bsimbulk inverter v(out) at Vin=Vdd/2': 0.6000001277366686,
    'psp103 inverter v(out) at Vin=Vdd/2': 0.1267681032755292,
    'tran: bsim4 inverter v(out) at 8ns': 1.1999998561856025,
}
NATIVE_RTOL = 1e-9
# Transient results: the time steps may differ a little
NATIVE_RTOL_TRAN = 1e-6


def check():
    measured = {}
    n = check_all_models_load()
    check_rc_transient(measured)
    check_diodes(measured)
    check_rectifier(measured)
    check_mos1(measured)
    check_bjt(measured)
    check_inverters(measured)
    for key, ref in NATIVE.items():
        rtol = NATIVE_RTOL_TRAN if key.startswith("tran:") else NATIVE_RTOL
        assert abs(measured[key] - ref) <= rtol * max(abs(ref), 1e-3), \
            (key, measured[key], ref)
    return n, measured


def test_models(tmp_path, monkeypatch):
    # Rawfiles are written to the working directory.
    monkeypatch.chdir(tmp_path)
    check()


if __name__ == "__main__":
    start = os.getcwd()
    with tempfile.TemporaryDirectory() as d:
        os.chdir(d)
        try:
            n, measured = check()
        finally:
            # Leave the directory before it is removed (not possible otherwise
            # on every platform, e.g. Pyodide's filesystem).
            os.chdir(start)
    print(f"device model checks passed ({n} models load, "
          f"{len(NATIVE)} results compared with the native build)")
    for key, value in measured.items():
        print(f"    {key!r}: {value!r},")
