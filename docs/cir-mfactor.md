# Parallel Devices (`$mfactor`)

`$mfactor` is a special instance parameter supported by all devices. It represents the number of identical devices connected in parallel. Setting `$mfactor=N` is equivalent to placing N copies of the same device between the same nodes, without the cost of explicitly instantiating or, later, simulating each one.

## Usage

`$mfactor` is set on an instance like any other parameter:

```text
m1 (drain gate source bulk) nmos w=1u l=180n $mfactor=4
```

The default value is `1`. Fractional values are allowed and may be used for scaling.

## Effect on device equations

For current-driven branches, the respective currents and their Jacobian contributions are multiplied by `$mfactor`. The device sees the same voltages as a single device but contributes `$mfactor` times the current to the KCL equations.

For voltage-driven branches, the contributed voltage value is unaffected, but the current contribution is multiplied by `$mfactor`. The branch current reported by such a device represents the current drawn by a single parallel instance. 

A noise current source contributes a `$mfactor` times greater power spectral density. A noise voltage source contributes a `$mfactor` times smaller power spectral density. 

## Passing `$mfactor` through subcircuits

Because `$mfactor` starts with `$` it is a valid parameter name and can appear in `parameters` declarations. This lets a wrapper subcircuit accept and forward a multiplicity factor:

```text
subckt nmos_w (d g s b)
parameters w=1u l=180n $mfactor=1
  m0 (d g s b) nmos w=w l=l $mfactor=$mfactor
ends

x1 (d g s b) nmos_w w=2u $mfactor=8
```

You must manualy add `$mfactor` to the parameters list of a subcircuit definition and establish the necessary forwarding to enclosed instances. 

## Verilog-A devices

For OSDI (Verilog-A) devices the device model is responsible for applying the multiplicity factor internally according to the Verilog-A specification. 
