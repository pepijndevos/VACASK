#!/usr/bin/python3

# Touchstone to VACASK netlist converter
# Use as module, i.e. python3 -m ts2vc ...
# or edit hashbang line and make it executable.
#
# Fits the S-parameters of a touchstone file with scikit-rf's vector
# fitting and dumps the result as a VACASK subcircuit.

import inspect
import re
import sys
from pathlib import Path

import numpy as np
import skrf as rf


def resolved_kwargs(func, overrides):
    """Return func's keyword parameters, defaults merged with overrides."""
    sig = inspect.signature(func)
    resolved = {name: param.default for name, param in sig.parameters.items()
                if name != "self" and param.default is not inspect.Parameter.empty}
    resolved.update(overrides)
    return resolved

# Command line options mapped to skrf.VectorFitting.auto_fit() keyword
# arguments. Options not given on the command line are left at auto_fit's
# own defaults. Each entry is (short, long, dest, converter, target dict).
# parameter_type is intentionally not exposed: the fit is always done on
# S-parameters (skrf converts Y/Z/H/G touchstone data to S on load), since
# write_spice_subcircuit_s() expects an S-domain fit.
autoFitOptions = [
    ("-pr", "--poles-init-real",   "n_poles_init_real",  int,   "autoFit"),
    ("-pc", "--poles-init-cmplx",  "n_poles_init_cmplx", int,   "autoFit"),
    ("-pa", "--poles-add",         "n_poles_add",        int,   "autoFit"),
    ("-mo", "--model-order-max",   "model_order_max",    int,   "autoFit"),
    ("-is", "--iters-start",       "iters_start",         int,   "autoFit"),
    ("-ii", "--iters-inter",       "iters_inter",         int,   "autoFit"),
    ("-if", "--iters-final",       "iters_final",         int,   "autoFit"),
    ("-te", "--target-error",      "target_error",        float, "autoFit"),
    ("-al", "--alpha",             "alpha",                float, "autoFit"),
    ("-ga", "--gamma",             "gamma",                float, "autoFit"),
    ("-nu", "--nu-samples",        "nu_samples",           float, "autoFit"),
]

# Command line options mapped to skrf.VectorFitting.passivity_enforce()
# keyword arguments. Only applied if the fitted model is not passive.
passivityEnforceOptions = [
    ("-ns", "--pe-samples", "n_samples", int,   "passivityEnforce"),
    ("-fm", "--f-max",      "f_max",     float, "passivityEnforce"),
]

# Boolean flag options, consuming no extra argument. Each entry is
# (short, long, dest, value, target dict).
flagOptions = [
    ("-ndc", "--no-preserve-dc", "preserve_dc", False, "passivityEnforce"),
]


def format_command_line(argv):
    """Format argv as a shell-style command line string: arguments containing
    whitespace are wrapped in double quotes, with any double quotes in the
    argument escaped."""
    parts = []
    for arg in argv:
        if re.search(r"\s", arg):
            parts.append('"' + arg.replace('"', '\\"') + '"')
        else:
            parts.append(arg)
    return " ".join(parts)


def fit_quality(vf):
    """Compare the fitted model in vf against its original touchstone network.

    Returns (model_order, rms_error, worst_rms_error, worst_rms_error_freq,
    max_mag_err_db, max_mag_err_freq). rms_error is vf.get_rms_error(); the
    "worst" rms error is the largest per-frequency rms error, combining all
    S-parameters at a single frequency point. max_mag_err_db is the largest
    magnitude error (in dB) seen for any single S-parameter at any frequency.
    Phase error is not reported: it is not meaningful where the magnitude is
    small, since phase is then dominated by noise.
    """

    network = vf.network
    freqs = network.f
    rmsErrSq = np.zeros(len(freqs))
    maxMagErrDb = 0.0
    maxMagErrFreq = freqs[0]
    for i in range(network.nports):
        for j in range(network.nports):
            fit = vf.get_model_response(i, j, freqs)
            orig = network.s[:, i, j]

            rmsErrSq += np.abs(fit - orig)**2

            magErrDb = np.abs(20*np.log10(np.abs(fit)) - 20*np.log10(np.abs(orig)))
            k = np.argmax(magErrDb)
            if magErrDb[k] > maxMagErrDb:
                maxMagErrDb = magErrDb[k]
                maxMagErrFreq = freqs[k]

    rmsErrAtFreq = np.sqrt(rmsErrSq / (network.nports**2))
    k = np.argmax(rmsErrAtFreq)

    return (vf.get_model_order(vf.poles), vf.get_rms_error(), rmsErrAtFreq[k], freqs[k],
            maxMagErrDb, maxMagErrFreq)


def write_vacask_subcircuit_s(vf, qualif, f, fitted_model_name="s_equivalent", create_reference_pins=False,
                               auto_fit_options=None, passivity_enforce_options=None, command_line=None):
    """Write a VACASK subcircuit equivalent to the vector fitted S-matrix in vf to
    the open file f. This follows the same state-space circuit synthesis as
    skrf.VectorFitting.write_spice_subcircuit_s(), translated to VACASK's netlist
    syntax (subckt/ends, instances as "name (nodes) master params", builtin vccs/
    cccs/vsource devices, and OSDI resistor/capacitor/inductor devices).

    auto_fit_options and passivity_enforce_options, if given, are dicts of the
    resolved keyword arguments used for vf.auto_fit() and vf.passivity_enforce()
    (see resolved_kwargs()); they are recorded in the file header as comments for
    reproducibility. passivity_enforce_options should be left None if the fit was
    already passive and passivity_enforce() was not called.

    command_line, if given, is a preformatted command line string (see
    format_command_line()) recorded in the file header as a comment.

    The enclosing circuit must load the resistor, capacitor (and, if the fit has
    proportional terms, inductor) OSDI modules before this subcircuit is used,
    since a "load" directive cannot appear inside a subckt definition.
    """

    build_e = bool(np.any(vf.proportional_coeff))

    modelOrder, _, _, _, maxMagErrDb, maxMagErrFreq = qualif

    f.write("// Equivalent circuit for vector fitted s-matrix\n")
    f.write(f"// Created using scikit-rf {rf.__version__} vectorFitting.py\n")
    if command_line is not None:
        f.write(f"// Command line: {command_line}\n")
    f.write(f"//\n")
    f.write("// auto_fit options:\n")
    for optName, optValue in (auto_fit_options or {}).items():
        f.write(f"//   {optName}={optValue}\n")
    if passivity_enforce_options is not None:
        f.write("// passivity_enforce options:\n")
        for optName, optValue in passivity_enforce_options.items():
            f.write(f"//   {optName}={optValue}\n")
    else:
        f.write("// passivity_enforce: not called, fit was already passive\n")
    f.write("// write_vacask_subcircuit_s options:\n")
    f.write(f"//   fitted_model_name={fitted_model_name!r}\n")
    f.write(f"//   create_reference_pins={create_reference_pins}\n")
    f.write(f"//\n")
    f.write(f"// Model order: {modelOrder}\n")
    f.write(f"// Maximal magnitude error: {maxMagErrDb:.6g} dB at {maxMagErrFreq:.6g} Hz\n")
    f.write("//\n")
    f.write("// The enclosing circuit must load these modules beforehand:\n")
    f.write('//   load "spice/resistor.osdi"\n')
    f.write('//   load "spice/capacitor.osdi"\n')
    if build_e:
        f.write('//   load "spice/inductor.osdi"\n')
    f.write("//\n")

    nports = vf.network.nports

    if create_reference_pins:
        pins = " ".join(f"p{x + 1} p{x + 1}_ref" for x in range(nports))
    else:
        pins = " ".join(f"p{x + 1}" for x in range(nports))

    f.write(f"subckt {fitted_model_name} ({pins})\n")

    f.write("  model resistor resistor noisy=0\n")
    f.write("  model capacitor capacitor\n")
    if build_e:
        f.write("  model inductor inductor\n")
    f.write("  model vsource vsource\n")
    f.write("  model vccs vccs\n")
    f.write("  model cccs cccs\n")

    for i in range(nports):
        f.write("  //\n")
        f.write(f"  // Port network for port {i + 1}\n")

        node_ref_i = f"p{i + 1}_ref" if create_reference_pins else "0"

        # reference impedance (real, i.e. resistance) of port i
        z0_i = np.real(vf.network.z0[0, i])

        # transfer gain of the controlled current sources representing the incident power wave a_i at port i
        #
        # the gain values result from the definition of the incident power wave:
        # a_i = 1 / 2 / sqrt(Z0_i) * (V_i + Z0_i * I_i) = 1 / 2 / sqrt(Z0_i) * V_i + sqrt(Z0_i) / 2 * I_i
        gain_vccs_a_i = 1 / 2 / np.sqrt(z0_i)
        gain_cccs_a_i = np.sqrt(z0_i) / 2

        # transfer gain of the controlled current source representing the reflected power wave b_i at port i
        #
        # the gain values result from the definition of the reflected power wave:
        # b_i = 1 / 2 / sqrt(Z0_i) * (V_i - Z0_i * I_i)
        #
        # depending on the circuit topology used for the equivalent port network, this can be implemented
        # with either controlled current and/or controlled voltage sources. in case of the Norton current
        # source used in this implementation, the reflected power wave relates to the source current as:
        # b_i = sqrt(Z0_i) / 2 * I_b_i <==> I_b_i = 2 / sqrt(Z0_i) * b_i
        gain_b_i = 2 / np.sqrt(z0_i)

        # dummy voltage source (v = 0) for port current sensing (I_i)
        f.write(f"  v{i + 1} (p{i + 1} s{i + 1}) vsource dc=0\n")

        # adding port reference resistor Ri = Z0_i
        f.write(f"  r{i + 1} (s{i + 1} {node_ref_i}) resistor r={z0_i}\n")

        # transfer of states and inputs from port j to input/output network of port i
        for j in range(nports):
            node_ref_j = f"p{j + 1}_ref" if create_reference_pins else "0"

            # reference impedance (real, i.e. resistance) of port j
            z0_j = np.real(vf.network.z0[0, j])

            # Stacking order in VectorFitting class variables:
            # s11, s12, s13, ..., s21, s22, s23, ...
            idx_S_i_j = i * nports + j

            # VCCS and CCCS adding their currents to represent the incident wave a_j
            gain_vccs_a_j = 1 / 2 / np.sqrt(z0_j)
            gain_cccs_a_j = np.sqrt(z0_j) / 2

            d = vf.constant_coeff[idx_S_i_j]
            e = vf.proportional_coeff[idx_S_i_j]

            if d != 0.0:
                # avoid zero-valued coefficients (in case of fit_constant=False)

                # input a_j is scaled by constant term d_i_j and by current gain for b_i
                g_ij = gain_b_i * d * gain_vccs_a_j
                f_ij = gain_b_i * d * gain_cccs_a_j
                f.write(f"  gd{i + 1}_{j + 1} ({node_ref_i} s{i + 1} p{j + 1} {node_ref_j}) vccs gain={g_ij}\n")
                f.write(f'  fd{i + 1}_{j + 1} ({node_ref_i} s{i + 1}) cccs ctlinst="v{j + 1}" gain={f_ij}\n')

            if build_e and e != 0.0:
                # avoid zero-valued coefficients (in case of fit_proportional=False)
                # proportional coefficients require an extra node for the differentiation using an inductor
                # [Y(s) ~ s * E * U(s)]

                # differentiated input a_j is scaled by proportional term e_i_j and by current gain for b_i
                g_ij = gain_b_i * e
                f.write(f"  ge{i + 1}_{j + 1} ({node_ref_i} s{i + 1} e{j + 1} 0) vccs gain={g_ij}\n")

            # each residue rk_i_j at port i is multiplied by its respective state signal xk_j
            for k in range(len(vf.poles)):
                pole = vf.poles[k]
                residue = vf.residues[idx_S_i_j, k]
                g_re = gain_b_i * np.real(residue)
                g_im = gain_b_i * np.imag(residue)

                if np.imag(pole) == 0.0:
                    # Real pole/residue pair; represented by one state
                    xkj = f"x{k + 1}_a{j + 1}"
                    f.write(f"  gr{k + 1}_{i + 1}_{j + 1} ({node_ref_i} s{i + 1} {xkj} 0) vccs gain={g_re}\n")
                else:
                    # Complex-conjugate pole/residue pair; represented by two states
                    # real part at x_{k + 1}_re_{j + 1}
                    # imaginary part at x_{k + 1}_im_{j + 1}
                    xk_re_j = f"x{k + 1}_re_a{j + 1}"
                    xk_im_j = f"x{k + 1}_im_a{j + 1}"
                    f.write(f"  gr{k + 1}_re_{i + 1}_{j + 1} ({node_ref_i} s{i + 1} {xk_re_j} 0) vccs gain={g_re}\n")
                    f.write(f"  gr{k + 1}_im_{i + 1}_{j + 1} ({node_ref_i} s{i + 1} {xk_im_j} 0) vccs gain={g_im}\n")

        # create state networks driven by this port i (input variable u = a_i)
        f.write("  //\n")
        f.write(f"  // State networks driven by port {i + 1}\n")
        for k in range(len(vf.poles)):
            pole = vf.poles[k]
            pole_re = np.real(pole)
            pole_im = np.imag(pole)

            # Transfer of input (a_i) to state networks (node xk_i) using VCCS and CCCS
            if pole_im == 0.0:
                # Real pole; represented by one state, input a_i is scaled by b = 1
                xki = f"x{k + 1}_a{i + 1}"
                f.write(f"  cx{k + 1}_a{i + 1} ({xki} 0) capacitor c=1.0\n")  # 1F capacitor makes math easy
                f.write(f"  gx{k + 1}_a{i + 1} (0 {xki} p{i + 1} {node_ref_i}) vccs gain={1 * gain_vccs_a_i}\n")
                f.write(f'  fx{k + 1}_a{i + 1} (0 {xki}) cccs ctlinst="v{i + 1}" gain={1 * gain_cccs_a_i}\n')
                f.write(f"  rp{k + 1}_a{i + 1} (0 {xki}) resistor r={-1 / pole_re}\n")
            else:
                # Complex pole of a conjugate pair; represented by two states
                # real part at x_{k + 1}_re_{i + 1}, input a_i is scaled by b = 2
                xk_re_i = f"x{k + 1}_re_a{i + 1}"
                xk_im_i = f"x{k + 1}_im_a{i + 1}"
                f.write(f"  cx{k + 1}_re_a{i + 1} ({xk_re_i} 0) capacitor c=1.0\n")  # 1F capacitor makes math easy
                f.write(
                    f"  gx{k + 1}_re_a{i + 1} (0 {xk_re_i} p{i + 1} {node_ref_i}) vccs gain={2 * gain_vccs_a_i}\n")
                f.write(f'  fx{k + 1}_re_a{i + 1} (0 {xk_re_i}) cccs ctlinst="v{i + 1}" gain={2 * gain_cccs_a_i}\n')
                f.write(f"  rp{k + 1}_re_re_a{i + 1} (0 {xk_re_i}) resistor r={-1 / pole_re}\n")
                f.write(f"  gp{k + 1}_re_im_a{i + 1} (0 {xk_re_i} {xk_im_i} 0) vccs gain={pole_im}\n")

                # imaginary part at x_{k + 1}_im_{i + 1}, input a_i is inactive (b = 0)
                f.write(f"  cx{k + 1}_im_a{i + 1} ({xk_im_i} 0) capacitor c=1.0\n")  # 1F capacitor makes math easy
                f.write(f"  gp{k + 1}_im_re_a{i + 1} (0 {xk_im_i} {xk_re_i} 0) vccs gain={-1 * pole_im}\n")
                f.write(f"  rp{k + 1}_im_im_a{i + 1} (0 {xk_im_i}) resistor r={-1 / pole_re}\n")

        # create differentiation network for this port i (input variable u = a_i)
        if build_e:
            f.write("  //\n")
            f.write(f"  // Network with derivative of input a_{i + 1} for proportional term\n")
            # voltage on node 'e{i + 1}' to gnd (0) represents time-derivative of input a_i for terms e_j_i
            f.write(f"  le{i + 1} (e{i + 1} 0) inductor l=1.0\n")  # 1H inductor makes math easy
            f.write(f"  ge{i + 1} (0 e{i + 1} p{i + 1} {node_ref_i}) vccs gain={gain_vccs_a_i}\n")
            f.write(f'  fe{i + 1} (0 e{i + 1}) cccs ctlinst="v{i + 1}" gain={gain_cccs_a_i}\n')

    f.write(f"ends {fitted_model_name}\n")


if __name__ == "__main__":
    help="""Touchstone to VACASK netlist converter.
Usage: python3 -m ts2vc [<args>] <input file> [<output file>]

Reads a Touchstone (.sNp) file holding S, Y, Z, H, or G parameters,
converts it to S-parameters, fits them with scikit-rf's vector fitting,
and writes an equivalent VACASK subcircuit.

If no output file is provided, the output file name is derived from
the input file name by replacing its extension with .inc.

Arguments:
  -h  --help              print help
  -n  --name              subcircuit name (default: derived from
                           input file name)
  -p  --plot              plot the original and fitted S-parameters
                           (magnitude in dB and phase in degrees)
  -rp --reference-pins    give the subcircuit a separate reference pin
                           per port (p1 p1_ref p2 p2_ref ...) instead of
                           tying all port references to node 0
  -sp --spice             write a generic SPICE subcircuit with scikit-rf's
                           own write_spice_subcircuit_s() instead of a
                           VACASK-syntax subcircuit
  -no --no-output         skip writing the output file; only fit and print
                           the quality report (and plot, if -p is given)

Options passed to scikit-rf's VectorFitting.auto_fit() (see its
documentation for details, defaults are auto_fit's own):
  -pr --poles-init-real   number of initial real poles
  -pc --poles-init-cmplx  number of initial complex conjugate poles
  -pa --poles-add         number of poles added per refinement iteration
  -mo --model-order-max   maximal model order
  -is --iters-start       number of initial pole relocation iterations
  -ii --iters-inter       number of intermediate pole relocation iterations
  -if --iters-final       number of final pole relocation iterations
  -te --target-error      target model error
  -al --alpha             error decay stopping threshold
  -ga --gamma             spurious pole detection threshold
  -nu --nu-samples        required pole spacing in frequency samples

Options passed to scikit-rf's VectorFitting.passivity_enforce(), used
only if the fitted model is not already passive (see its documentation
for details, defaults are passivity_enforce's own):
  -ns  --pe-samples        number of frequency samples for passivity
                            evaluation and enforcement
  -fm  --f-max              highest frequency of interest (Hz)
  -ndc --no-preserve-dc     do not preserve the dc point during
                             passivity enforcement
"""
    ndx = 1
    fromFile = None
    toFile = None
    name = None
    plot = False
    referencePins = False
    spice = False
    noOutput = False
    targets = {"autoFit": {}, "passivityEnforce": {}}
    while ndx<len(sys.argv):
        arg = sys.argv[ndx]
        if arg[0]=="-":
            if arg == "--help" or arg == "-h":
                # Print help and exit
                print(help)
                sys.exit(0)
            elif arg == "-n" or arg == "--name":
                if ndx+1>=len(sys.argv):
                    print("Too few arguments.")
                    sys.exit(1)
                ndx += 1
                name = sys.argv[ndx]
            elif arg == "-p" or arg == "--plot":
                plot = True
            elif arg == "-rp" or arg == "--reference-pins":
                referencePins = True
            elif arg == "-sp" or arg == "--spice":
                spice = True
            elif arg == "-no" or arg == "--no-output":
                noOutput = True
            else:
                for short, long, dest, value, target in flagOptions:
                    if arg == short or arg == long:
                        targets[target][dest] = value
                        break
                else:
                    for short, long, dest, conv, target in autoFitOptions + passivityEnforceOptions:
                        if arg == short or arg == long:
                            if ndx+1>=len(sys.argv):
                                print("Too few arguments.")
                                sys.exit(1)
                            ndx += 1
                            targets[target][dest] = conv(sys.argv[ndx])
                            break
                    else:
                        print("Unknown argument:", arg)
                        print(help)
                        sys.exit(1)
        else:
            # Need 1 or 2 more args
            fromFile = arg

            if ndx+2<len(sys.argv):
                print("Too many arguments.")
                print(help)
                sys.exit(1)

            if ndx+2==len(sys.argv):
                toFile = sys.argv[ndx+1]
            else:
                toFile = None
            break

        ndx += 1

    if fromFile is None:
        print("Need input file.")
        print(help)
        sys.exit(1)

    fromPath = Path(fromFile)

    if toFile is None:
        toFile = str(fromPath.with_suffix(".inc"))

    if name is None:
        name = re.sub(r'\W|^(?=\d)', '_', fromPath.stem)

    network = rf.Network(fromFile)

    vf = rf.VectorFitting(network)
    vf.auto_fit(**targets["autoFit"])
    autoFitResolved = resolved_kwargs(rf.VectorFitting.auto_fit, targets["autoFit"])

    passivityEnforceResolved = None
    if not vf.is_passive():
        vf.passivity_enforce(**targets["passivityEnforce"])
        passivityEnforceResolved = resolved_kwargs(rf.VectorFitting.passivity_enforce, targets["passivityEnforce"])

    # Check fit quality against the original touchstone data.
    qualif = fit_quality(vf)
    modelOrder, rmsErr, maxRmsErr, maxRmsErrFreq, maxMagErrDb, maxMagErrFreq = qualif

    print(f"Model order: {modelOrder}")
    print(f"RMS error: {rmsErr:.6g}, "
          f"worst at {maxRmsErr:.6g} at {maxRmsErrFreq:.6g} Hz")
    print(f"Maximal magnitude error: {maxMagErrDb:.6g} dB "
          f"at {maxMagErrFreq:.6g} Hz")

    if maxMagErrDb > 1.0:
        print("""
Fit quality looks poor. Raising -mo/--model-order-max alone often has no
effect: refinement can stop early because of pole stagnation or because
the spurious-pole skimming threshold discards poles as fast as they are
added. Things worth trying instead, one at a time:
  -ga --gamma             lower it (e.g. 0.01, 0.003, ...) to skim fewer
                          poles as spurious
  -pr --poles-init-real   raise the initial pole counts
  -pc --poles-init-cmplx
  -pa --poles-add         raise the number of poles added per iteration
  -te --target-error      lower it to demand a tighter fit
No single combination works for every network, and pushing these too far
can make the fit non-passive, which -ndc/passivity_enforce may then fail
to fix cleanly (watch for warnings above). Re-fitting with fewer poles is
often more effective than pushing the order higher. Use -p/--plot to see
where the fit deviates.""")

    if plot:
        import matplotlib.pyplot as plt
        from matplotlib.markers import MarkerStyle

        fig, (axMag, axPhase) = plt.subplots(2, 1, sharex=True)
        vf.plot_s_db(ax=axMag)
        vf.plot_s_deg(ax=axPhase)
        # scikit-rf draws the touchstone sample points with matplotlib's
        # default scatter marker; switch it to '.' so the fitted curve
        # stays visible underneath.
        dotPath = MarkerStyle('.').get_path().transformed(MarkerStyle('.').get_transform())
        for ax in (axMag, axPhase):
            for collection in ax.collections:
                collection.set_paths([dotPath])
        fig.tight_layout()
        plt.show()

    if not noOutput:
        if spice:
            vf.write_spice_subcircuit_s(toFile, fitted_model_name=name, create_reference_pins=referencePins)
        else:
            with open(toFile, "w") as toFileObj:
                write_vacask_subcircuit_s(vf, qualif, toFileObj, fitted_model_name=name,
                                           create_reference_pins=referencePins,
                                           auto_fit_options=autoFitResolved,
                                           passivity_enforce_options=passivityEnforceResolved,
                                           command_line=format_command_line(sys.argv))
