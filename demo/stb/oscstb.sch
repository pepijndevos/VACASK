v {xschem version=3.4.8RC file_version=1.3}
G {}
K {}
V {}
S {}
F {}
E {}
T {@name} 330 231.25 0 0 0.2 0.2 {name=Vp}
T {@value} 330 276.25 0 0 0.2 0.2 {name=Vp}
T {@spice_get_current} 333.75 240 3 0 0.2 0.2 {layer=17
name=Vp}
N 170 50 170 100 {lab=vcc}
N 400 50 500 50 {lab=vcc}
N 500 50 500 170 {lab=vcc}
N 280 50 280 70 {lab=vcc}
N 400 50 400 70 {lab=vcc}
N 280 150 280 170 {lab=c}
N 280 150 400 150 {lab=c}
N 400 230 400 260 {lab=#net1}
N 170 200 170 250 {lab=b}
N 170 200 240 200 {lab=b}
N 280 340 280 360 {lab=GND}
N 170 360 280 360 {lab=GND}
N 170 310 170 360 {lab=GND}
N 280 360 500 360 {lab=GND}
N 500 230 500 360 {lab=GND}
N 280 360 280 370 {lab=GND}
N 170 50 280 50 {lab=vcc}
N 280 50 400 50 {lab=vcc}
N 280 130 280 150 {lab=c}
N 400 150 400 170 {lab=c}
N 280 260 280 280 {lab=e}
N 170 160 170 200 {lab=b}
N 280 260 310 260 {lab=e}
N 280 230 280 260 {lab=e}
N 370 260 400 260 {lab=#net1}
N 400 130 400 150 {lab=c}
C {npn.sym} 260 200 0 0 {name=Q1
model=q2n2222
device=2n2222
footprint=SOT23
area=1
m=1}
C {res.sym} 170 130 0 0 {name=R1
value=47k
footprint=1206
device=resistor
m=1}
C {res.sym} 280 310 0 0 {name=R2
value=470
footprint=1206
device=resistor
m=1}
C {capa.sym} 280 100 0 0 {name=C1
m=1
value=2n
footprint=1206
device="ceramic capacitor"}
C {capa.sym} 400 200 0 0 {name=C2
m=1
value=5n
footprint=1206
device="ceramic capacitor"}
C {capa.sym} 170 280 0 0 {name=C3
m=1
value=100n
footprint=1206
device="ceramic capacitor"}
C {ind.sym} 400 100 0 0 {name=L1
m=1
value=200u
footprint=1206
device=inductor}
C {vsource.sym} 500 200 0 0 {name=Vcc value="dc=9" savecurrent=false}
C {gnd.sym} 280 370 0 0 {name=l2 lab=GND}
C {lab_wire.sym} 170 200 0 0 {name=p1 sig_type=std_logic lab=b
}
C {lab_wire.sym} 280 150 0 0 {name=p2 sig_type=std_logic lab=c
}
C {lab_wire.sym} 280 260 0 0 {name=p3 sig_type=std_logic lab=e
}
C {lab_wire.sym} 320 50 0 0 {name=p4 sig_type=std_logic lab=vcc
}
C {vsource.sym} 340 260 3 0 {name=Vp value="dc=0" savecurrent=false
hide_texts=true
attach=Vp}
C {command_block.sym} 170 460 0 0 {name=CMD
only_toplevel=false
}
C {acstb.sym} 170 570 0 0 {name=acstb1
only_toplevel=false 
order="1"
sweep=""
probe="\\"Vp\\""
localgnd=""
from=1
to=1M
step=""
mode="\\"dec\\""
points="10000"
values=""
nodeset=""
store=""
write=""
writeop=""
}
C {postproc.sym} 170 780 0 0 {name=postproc1
only_toplevel=false 
order="2"
tool="PYTHON"
file="\\"runme.py\\""
}
C {simulator_commands_shown.sym} 590 60 0 0 {name=COMMANDS1
simulator=vacask
only_toplevel=false 
value="
load \\"spice/bjt.osdi\\"

model q2n2222 sp_bjt ( type=1
  is=15.2f nf=1 bf=105 vaf=98.5 ikf=0.5
  ise=8.2p ne=2 br=4 nr=1 var=20 ikr=0.225
  re=0.373 rb=1.49 rc=0.149 xtb=1.5
  cje=35.5p cjc=12.2p tf=500p tr=85n
  kf=1e-12 af=1
)

embed \\"runme.py\\" <<<FILE
from rawfile import rawread
import numpy as np
import matplotlib.pyplot as plt

fig1 = plt.figure(figsize=(6,6), dpi=100)
fig1.suptitle('LC oscillator, open loop gain')
ax_dict = fig1.subplot_mosaic(\\"\\"\\"
CC
DD
AB
\\"\\"\\",  gridspec_kw=\{
    'height_ratios': [1, 1, 2], 
\})

raw = rawread('acstb1.raw')
plot = raw.get()

ax_dict['C'].set_ylabel('W [dB]')
ax_dict['C'].plot(plot['frequency']/1e3, 20*np.log10(np.abs(plot['w'])))
ax_dict['C'].set_ylim(-30, 40)

ax_dict['D'].set_ylabel('arg(W) [deg]')
ax_dict['D'].plot(plot['frequency']/1e3, np.unwrap(np.angle(plot['w']))/np.pi*180)
ax_dict['D'].set_xlabel('f [kHz]')

ax_dict['A'].set_ylabel('Im')
ax_dict['A'].set_xlabel('Re')
ax_dict['A'].plot(np.real(plot['w']), np.imag(plot['w']))
ax_dict['A'].plot(-1, 0, marker='x', markersize=8) 
ax_dict['A'].set_aspect('equal')

ax_dict['B'].set_ylabel('Im')
ax_dict['B'].set_xlabel('Re')
ax_dict['B'].plot(np.real(plot['w']), np.imag(plot['w']))
ax_dict['B'].plot(-1, 0, marker='x', markersize=8) 
ax_dict['B'].set_xlim(-1.005, -0.995) 
ax_dict['B'].set_ylim(-0.005, 0.005) 
ax_dict['B'].set_aspect('equal')
ax_dict['B'].locator_params(axis='x', nbins=3)
ax_dict['B'].locator_params(axis='y', nbins=3)

fig1.tight_layout()

plt.show()

>>>FILE
"}
