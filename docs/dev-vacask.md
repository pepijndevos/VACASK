# VACASK Verilog-A Devices

The following Verilog-A device models are developed and maintained as part of VACASK. They are located in the `devices/` directory. They are precompiled into OSDI files. You can load them without specifying any additional path, just specify the file name with the `.osdi` extension in a `load` netlist directive. 

|Verilog-A device             |OSDI file       |Module      |
|-----------------------------|----------------|------------|
|Linear resistor              |resistor.osdi   |`resistor`  |
|Linear capacitor             |capacitor.osdi  |`capacitor` |
|Linear inductor              |inductor.osdi   |`inductor`  |
|SPICE diode                  |diode.osdi      |`diode`     |
|Ideal operational amplifier  |opamp.osdi      |`opamp`     |

See the Verilog-A sources for the lists of terminals and parameters. 
