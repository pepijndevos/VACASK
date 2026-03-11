import sys

class VacaskSupport:
    def preamble(self):
        return """
subckt not ( in out vdd vss )
  mp (out in vdd vdd) pmos w=1u l=0.2u
  mn (out in vss vss) nmos w=0.5u l=0.2u
ends

subckt nor2 ( in1 in2 out vdd vss )
  mp2 (int in2 vdd vdd) pmos w=1u l=0.2u
  mp1 (out in1 int vdd) pmos w=1u l=0.2u
  mn1 (out in1 vss vss) nmos w=0.5u l=0.2u
  mn2 (out in2 vss vss) nmos w=0.5u l=0.2u
ends

subckt and2 ( in1 in2 out vdd vss )
  mp2 (outx in2 vdd vdd) pmos w=1u l=0.2u
  mp1 (outx in1 vdd vdd) pmos w=1u l=0.2u
  mn1 (outx in1 int vss) nmos w=0.5u l=0.2u
  mn2 (int  in2 vss vss) nmos w=0.5u l=0.2u
  mp3 (out outx vdd vdd) pmos w=1u l=0.2u
  mn3 (out outx vss vss) nmos w=0.5u l=0.2u
ends

subckt drv(out)
  parameters v0=0 v1=1
  parameters rs=1 delay=0.1n rise=0.1n
  vdrv (int 0) v type="pulse" val0=v0*vdd val1=v1*vdd delay=delay rise=rise
  rdrv (int out) r r=rs
ends
"""

    def inv(self, name, in1, out, vdd, vss):
        return (name+" ( "+in1+" "+out+" "+vdd+" "+vss+" ) not")

    def and2(self, name, in1, in2, out, vdd, vss):
        return (name+" ( "+in1+" "+in2+" "+out+" "+vdd+" "+vss+" ) and2")

    def nor2(self, name, in1, in2, out, vdd, vss):
        return (name+" ( "+in1+" "+in2+" "+out+" "+vdd+" "+vss+" ) nor2")

    def ha(self, name, in1, in2, s, c, vdd, vss):
        return (name+" ( "+in1+" "+in2+" "+s+" "+c+" "+vdd+" "+vss+" ) ha")

    def fa(self, name, in1, in2, cin, s, c, vdd, vss):
        return (name+" ( "+in1+" "+in2+" "+cin+" "+s+" "+c+" "+vdd+" "+vss+" ) fa")

    def comment(self, txt):
        return "// "+txt
    
    def subinst(self, name, definition, pins, params=[]):
        return (name+" ( "+(" ".join(pins))+" ) "+definition+" "+(" ".join(params)))
    
    def subwrap(self, name, core, pins):
        out = [ "subckt "+name+" ( "+(" ".join(pins))+" )" ]
        out.extend(["  "+row for row in core])
        out.append("ends")
        return out

def fasub(sim, name, vdd, vss):
    core = []
    core.append(sim.nor2("x1", "in1", "cin", "o1", vdd, vss))
    core.append(sim.nor2("x2", "in1", "o1", "o2", vdd, vss))
    core.append(sim.nor2("x3", "o1", "cin", "o3", vdd, vss))
    core.append(sim.nor2("x4", "o2", "o3", "o4", vdd, vss))

    core.append(sim.nor2("x5", "in2", "o4", "o5", vdd, vss))
    core.append(sim.nor2("x6", "in2", "o5", "o6", vdd, vss))
    core.append(sim.nor2("x7", "o5", "o4", "o7", vdd, vss))
    core.append(sim.nor2("x8", "o6", "o7", "sout", vdd, vss))

    core.append(sim.nor2("x9", "o5", "o1", "cout", vdd, vss))

    pins = [ "in1", "in2", "cin", "sout", "cout", vdd, vss]

    return sim.subwrap(name, core, pins)

def hasub(sim, name, vdd, vss):
    core=[]
    core.append(sim.inv("x1", "in1", "in1n", vdd, vss))
    
    core.append(sim.nor2("x5", "in2", "in1n", "o5", vdd, vss))
    core.append(sim.nor2("x6", "in2", "o5", "o6", vdd, vss))
    core.append(sim.nor2("x7", "o5", "in1n", "cout", vdd, vss))
    core.append(sim.nor2("x8", "o6", "cout", "sout", vdd, vss))

    pins = [ "in1", "in2", "sout", "cout", vdd, vss]

    return sim.subwrap(name, core, pins)

def mulsub(sim, n=2):
    core = []
    core.append(sim.comment("Partial products"))
    for row in range(n):
        # row is B
        for col in range(n):
            # col is A
            name = f"pp_{row}_{col}"
            out = "p0" if row==0 and col==0 else name
            core.append(sim.and2(f"x{name}", f"b{row}", f"a{col}", out, "vdd", "vss"))

    core.append("")
    core.append(sim.comment("Row 0 (half adders)"))
    for col in range(1, n):
        out = "p1" if col==1 else f"s_0_{col}"
        core.append(sim.ha(f"xha_0_{col}", f"pp_0_{col}", f"pp_1_{col-1}", out, f"c_0_{col}", "vdd", "vss"))

    for row in range(1, n-1):
        core.append("")
        core.append(sim.comment(f"Row {row}"))
        for col in range(1, n):
            in2 = f"s_{row-1}_{col+1}" if col<n-1 else f"pp_{row}_{col}"
            out = f"p{row+1}" if col==1 else f"s_{row}_{col}"
            core.append(sim.fa(f"xfa_{row}_{col}", f"c_{row-1}_{col}", in2, f"pp_{row+1}_{col}", out, f"c_{row}_{col}", "vdd", "vss"))
    
    core.append("")
    core.append(sim.comment(f"Row {n-1} (last row)"))
    for col in range(1, n):
        in1 = f"c_{n-2}_{col}"
        in2 = f"s_{n-2}_{col+1}" if col<n-1 else f"pp_{n-1}_{n-1}"
        cin = f"c_{n-1}_{col-1}" if col>1 else None
        out = f"p{n+col-1}"
        cout = f"c_{n-1}_{col}" if col<n-1 else f"p{2*n-1}"
        if cin is None:
            core.append(sim.ha(f"xha_{row}_{col}", in1, in2, out, cout, "vdd", "vss"))
        else:
            core.append(sim.fa(f"xha_{row}_{col}", in1, in2, cin, out, cout, "vdd", "vss"))
    
    pins = (
        [f"a{ii}" for ii in range(n)] + 
        [f"b{ii}" for ii in range(n)] + 
        [f"p{ii}" for ii in range(2*n)] + 
        [ "vdd", "vss" ] 
    )
    
    return sim.subwrap(f"mul{n}", core, pins)

def tb(sim, a, b, n=2):
    core = []
    core.append(sim.subinst(
        "xmul", f"mul{n}", 
        [f"a{ii}" for ii in range(n)]+[f"b{ii}" for ii in range(n)]+[f"p{ii}" for ii in range(2*n)]+["vdd", "vss"]
    ))
    for ii in range(n):
        bit = a % 2
        a = a // 2
        core.append(sim.subinst(f"da{ii}", "drv", [f"a{ii}"], ["v0=0", f"v1={bit}"]))
    for ii in range(n):
        bit = b % 2
        b = b // 2
        core.append(sim.subinst(f"db{ii}", "drv", [f"b{ii}"], ["v0=0", f"v1={bit}"]))

    return core

if __name__=="__main__":
    if len(sys.argv)<3 or len(sys.argv)>4:
        print("Args: <simulator> <n> [dut|tb|full]")
        sys.exit(1)

    simulator = sys.argv[1]
    if simulator == "vacask":
        sim = VacaskSupport()
    else:
        print("Unsupported simulator.")
        sys.exit(1)

    n = int(sys.argv[2])
    if n<2:
        print("size must be at least 2.")
        sys.exit(1)

    if len(sys.argv)==4:
        what = sys.argv[3]
    else:
        what = "full"

    printed = False
    if what in ["dut", "full"]:
        print(sim.preamble())
        print("\n".join(fasub(sim, "fa", "vdd", "vss")))
        print()
        print("\n".join(hasub(sim, "ha", "vdd", "vss")))
        print()
        print("\n".join(mulsub(sim, n)))
        printed = True
    
    if what in ["tb", "full"]:
        core = tb(sim, 2**n-1, 2**n-1, n)
        if simulator == "vacask":
            pins = (
                [f"a{ii}" for ii in range(n)] + 
                [f"b{ii}" for ii in range(n)] + 
                [f"p{ii}" for ii in range(2*n)]
            )
            core = sim.subwrap(f"mul_test", core, [])
        print()
        print("\n".join(core))
        printed = True
    
    if not printed:
        print(f"Don't know how to build '{sys.argv[3]}'.")
        sys.exit(1)
    