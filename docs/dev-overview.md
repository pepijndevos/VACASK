# Device Model Library

VACASK separates the simulator core from its device library. Most device models are compiled Verilog-A shared libraries; the simulator loads them at runtime and calls them through a fixed C interface. This means any model that the [OpenVAF-reloaded](https://github.com/arpadbuermen/OpenVAF) compiler can produce is directly usable without modifying the simulator.

The library is organized into four groups.

**Builtin devices** are compiled into the simulator and require no `load` directive. These devices use the C++ API of the simulator directly. They cover independent voltage and current sources, the four linear controlled source types (VCVS, VCCS, CCVS, CCCS), and inductive coupling. These are the primitives used to express stimulus, feedback, and magnetic coupling in a netlist.

**VACASK Verilog-A devices** are models developed and maintained as part of VACASK. They provide a small set of general-purpose passive components — resistor, capacitor, inductor, SPICE diode, and ideal op-amp. These models can be loaded by filename alone, without specifying a path.

**3rd party Verilog-A devices** are industry-standard compact models distributed with VACASK. The collection includes VBIC 1.3, BSIM3v3, BSIM4v8, BSIMBULK, and PSP103. Like the VACASK models, they can be loaded by filename alone.

**Converted SPICE devices** were translated from SPICE3 C source to Verilog-A using the [Verilog-A Distiller](https://codeberg.org/arpadbuermen/VADistiller). They cover the full classical SPICE3 device set — resistor, capacitor, inductor, diode, Gummel-Poon BJT, JFET 1-2, MESFET, MOSFET levels 1–3/6/9, VDMOS, BSIM3, and BSIM4. Load them by prefixing the filename with `spice/`.

Any Verilog-A model outside these groups — from a PDK, a foundry, or a research group — can be loaded directly as a `.va` source file. VACASK compiles it on the fly using OpenVAF-reloaded and caches the resulting OSDI file in the working directory. See [Loading Devices](cir-loading.md) for the full `load` directive syntax, search path rules, and module renaming. Note that it makes sense to precompile Verilog-A models and put the compiled versions in the module search path to avoid the compilation overhead for each working directory. 

All these models are distributed in source form. In VACASK installation these models are precompiled for fast loading. 