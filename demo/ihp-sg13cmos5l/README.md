# Converting IHP Open PDK (sg13cmos5l) for use with VACASK

First, obtain and convert the ihp-sg13g2 PDK. See [../ihp-sg13g2/README.md](../ihp-sg13g2/README.md) for details. 

Enter the directory where the subdirectory with ihp-sg13g2 is found. Then clone the IHP SG13CMOS5L PDK
```
cd IHP-Open-PDK
git clone https://github.com/IHP-GmbH/ihp-sg13cmos5l.git
```

Set environmental variables
* `PDK_ROOT` to the directory where you downloaded the PDK (e.g. `/home/myname/IHP-Open-PDK`) and
* `PDK` to `ihp-sg13cmos5l`. 

You will need the path to VACASK's Python scripts. If you don't know where these scripts are, type
```
vacask -dp
```
and look for "Python path addition". Suppose the python path addition is `/usr/local/lib/vacask/python`. Type
```
PYTHONPATH=/usr/local/lib/vacask/python python3 -m sg13cmos5ltovc
```
Additional options can be passed to the OpenVAF Verilog-A compiler with `--openvaf-options`. All arguments that follow this flag are forwarded to OpenVAF. For example, to build generic models that run on any x86-64 CPU
```
PYTHONPATH=/usr/local/lib/vacask/python python3 -m sg13g2tovc --openvaf-options --target_cpu generic
```

