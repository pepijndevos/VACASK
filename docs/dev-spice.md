# Converted SPICE Devices

The following device models were converted from the SPICE3 C model format to Verilog-A using the [Verilog-A Distiller](https://codeberg.org/arpadbuermen/VADistiller). They are located in the `spice/` directory and are precompiled. Load them by prefixing the osdi file name with `spice/`. 

|Verilog-A device (SPICE)             |OSDI file      |Module       |
|-------------------------------------|---------------|-------------|
|Linear resistor                      |resistor.osdi  |sp_resistor  |
|Linear capacitor                     |capacitor.osdi |sp_capacitor |
|Linear inductor                      |inductor.osdi  |sp_inductor  |
|Diode (levels 1 and 3)               |diode.osdi     |sp_diode     |
|Gummel-Poon BJT                      |bjt.osdi       |sp_bjt       |
|JFET level 1 (Schichman-Hodges)      |jfet1.osdi     |sp_jfet1     |
|JFET level 2 (Parker-Skellern) *     |jfet2.osdi     |sp_jfet2     |
|MESFET level 1 (Statz et. al.) *     |mes1.osdi      |sp_mes1      |
|MOSFET level 1 (Schichman-Hodges) *  |mos1.osdi      |sp_mos1      |
|MOSFET level 2 (Grove-Frohman) *     |mos2.osdi      |sp_mos2      |
|MOSFET level 3 (empirical) *         |mos3.osdi      |sp_mos3      |
|MOSFET level 6 (Sakurai-Newton) *    |mos6.osdi      |sp_mos6      |
|MOSFET level 9 (modified level 3) *  |mos9.osdi      |sp_mos9      |
|VDMOS *                              |vdmos.osdi     |sp_vdmos     |
|BSIM3 3.3.0                          |bsim3v3.osdi   |sp_bsim3v3   |
|BSIM4 4.8.0, 4.8.1, 4.8.2, 4.8.3     |bsim4v8.osdi   |sp_bsim4v8   |

(*) These devices do not conserve charge due to the modeling approach used by their original authors.

## Model variants

| Variant | Directory | Output variables | Noise model |
|---------|-----------|-----------------|-------------|
| Default | `spice/`      | No variables that introduce internal nodes | All noise analyses |
| `sn`    | `spice/sn/`   | No variables that introduce internal nodes | Ordinary small-signal noise only |
| `full`  | `spice/full/` | All output variables | All noise analyses |

Devices in `spice/` are the default model versions. They do not expose output variables that would introduce additional internal nodes. The noise model of these devices is appropriate for all types on noise analysis. 

Use the `sn` variant for maximum simulation speed when only ordinary small-signal noise analysis is needed. Use the `full` variant when collecting device output variables not available in the default verion. If a device does not have a `sn` or `full` variant use the default variant since it does not differ from the respective `sn` or `full` variant. 
