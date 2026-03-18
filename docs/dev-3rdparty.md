# 3rd Party Verilog-A Devices

The following 3rd party Verilog-A device models are distributed with VACASK. They are precompiled into OSDI files. You can load them without specifying any additional path, just specify the file name with the `.osdi` extension in a `load` netlist directive. 

|Verilog-A device          |File               |OSDI file       |Module   |
|--------------------------|-------------------|----------------|---------|
|VBIC 1.3, 3 terminals     |vbic/vbic_1p3.va   |vbic1p3.osdi    |vbic13   |
|VBIC 1.3, 4 terminals     |vbic/vbic_1p3.va   |vbic1p3_4t.osdi |vbic13_4t|
|VBIC 1.3, 5 terminals     |vbic/vbic_1p3.va   |vbic1p3_5t.osdi |vbic13_5t|
|BSIM3v3 MOSFET (Cogenda)  |bsim3v3.va         |bsim3v3.osdi    |bsim3    |
|BSIM4v8 MOSFET (Cogenda)  |bsim4v8.va         |bsim4v8.osdi    |bsim4    |
|PSP103.4 MOSFET           |psp103v4/psp103.va |psp103.osdi     |psp103va |
|BSIMBULK MOSFET 106.2.0   |bsimbulk.va        |bsimbulk.osdi   |bsimbulk |

See the Verilog-A sources for the lists of terminals and parameters. 
