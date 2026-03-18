# VACASK Verilog-A Devices

The following Verilog-A device models are developed and maintained as part of VACASK. They are located in the `devices/` directory. They are precompiled into OSDI files. You can load them without specifying any additional path, just specify the file name with the `.osdi` extension in a `load` netlist directive. 

| Module | File | Device type |
|--------|------|-------------|
| `resistor`  | `resistor.va` | Linear resistor |
| `capacitor` | `capacitor.va` | Linear capacitor |
| `inductor`  | `inductor.va` | Linear inductor |
| `diode`     | `diode.va` | SPICE diode |
| `opamp`     | `opamp.va` | Ideal operational amplifier |

See the Verilog-A sources for the lists of terminals and parameters. 
