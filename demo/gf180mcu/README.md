# Converting Global Foundries Open PDK (gf180mcu) for use with VACASK

First, download the PDK using [ciel](https://github.com/fossi-foundation/ciel). You can install ciel and set a directory to store all of your PDKs in with 
```
python3 -m pip install --user --upgrade --no-cache-dir ciel
export PDK_ROOT=<common_pdk_path>
```
Print a list of the available PDK versions.
```
ciel ls-remote --pdk-family gf180mcu
```
Then copy the hash of the latest version and download it with:
```
ciel enable --pdk-family gf180mcu <version>
```
Set the current PDK with
```
export PDK=gf180mcuD
```


To run the converter, you will need the path to VACASK's Python scripts. If you don't know where these scripts are, type
```
vacask -dp
```
and look for "Python path addition". Suppose the python path addition is `/usr/local/lib/vacask/python`. Type
```
PYTHONPATH=/usr/local/lib/vacask/python python3 -m gf180tovc
```


The converter will process the Ngspice models and 
* create directory `gf180mcuD/libs.tech/vacask/models` with the converted models (a common `design.lib` and one `.lib` file for each corner)
* create a VACASK config file `gf180mcuD/libs.tech/vacask/.vacaskrc.toml` (copy this file to the directory where your toplevel netlist is located)
* create an include file `ihp-sg13g2/libs.tech/vacask/models/gf180_vacask_common.lib`. Always include this file along with all other PDK files

An example and a [.vacaskrc.toml](.vacaskrc.toml) configuration file are available in this directory. Just set the `PDK_ROOT` and the `PDK` environmental variables and run the example with VACASK. The [.vacaskrc.toml](.vacaskrc.toml) file must be copied either to the user's home directory or to the directory where VACASK is started. 
