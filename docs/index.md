# VACASK Documentation

VACASK (Verilog-A Circuit Analysis Kernel) is an analog circuit simulator built around the [OSDI](https://openvaf.semimod.de/docs/details/osdi/) device model interface. Device models are written in Verilog-A, compiled to shared libraries by the [OpenVAF-reloaded](https://github.com/arpadbuermen/OpenVAF) compiler, and loaded on demand at runtime. This clean separation between the simulator core and its device library makes it straightforward to use industry-standard compact models or to develop new ones without modifying the simulator itself.

VACASK is not a SPICE clone. Its netlist language has a Spectre-like syntax with a richer expression system, fully parameterized hierarchical circuit descriptions, and conditional netlist blocks. The control block — the part of the netlist that drives simulation — is a small scripting language that sequences analyses, sweeps, circuit modifications, and postprocessing steps in a single run. Almost any circuit or simulator parameter can be swept or modified between analyses without reloading the netlist. Furthermore, the circuit's topology can also be changed without reloading the circuit. 

The simulator supports operating-point, DC small-signal, AC small-signal, noise, transient, and harmonic balance analyses. The nonlinear solver uses residual-based convergence testing and several homotopy strategies for difficult operating-point problems. Numerical linear algebra is handled by the KLU sparse matrix library. Results are written in SPICE raw file format and can be postprocessed by external scripts, with built-in Python integration for launching postprocessors directly from the netlist.

VACASK is developed at the EDA Laboratory, University of Ljubljana, and is released under the GNU Affero General Public License 3.0.

---

# Table of Contents

1. [VACASK Startup and Configuration](startup-overview.md)
   1. [Command Line Options and Startup Sequence](startup-options.md)
   2. [Search Paths and Configuration Files](startup-paths.md)
2. [Input File](input-overview.md)
   1. [Numbers](input-numbers.md)
   2. [Strings](input-strings.md)
   3. [Identifiers](input-identifiers.md)
   4. [Reserved Words](input-reserved.md)
   5. [Including a File](input-include.md)
   6. [Embedded Files](input-embed.md)
3. [Circuit Description and Elaboration](cir-overview.md)
   1. [Loading Devices](cir-loading.md)
   2. [Nodes](cir-nodes.md)
   3. [Masters (Models and Subcircuits)](cir-masters.md)
   4. [Instances](cir-instance.md)
   5. [Subcircuits and Hierarchy](cir-hierarchy.md)
      1. [Defining a Subcircuit](cir-subckt.md)
      2. [Instance Hierarchy](cir-hier.md)
      3. [Nested Subcircuit Definitions](cir-subckt-nested.md)
   6. [Parallel Devices (`$mfactor`)](cir-mfactor.md)
   7. [Conditional Netlist Blocks](cir-conditional.md)
   8. [Automated Binning](cir-binning.md)
   9. [Circuit Elaboration](cir-elaboration.md)
4. [Data Types and Expressions](expr-overview.md)
   1. [Scalar Data Types](expr-scalars.md)
   2. [Vectors and Lists](expr-vectors.md)
   3. [Builtin Constants](expr-constants.md)
   4. [Operators](expr-operators.md)
   5. [Builtin Functions](expr-functions.md)
   6. [Circuit Variables](expr-cirvars.md)
   7. [Special Identifiers](expr-special.md)
5. [The Control Block](cmd-overview.md)
   1. [Analysis Statements](cmd-analysis.md)
   2. [Sweeping](cmd-sweep.md)
   3. [Saving Results](cmd-save.md)
   4. [Simulator Options](cmd-options.md)
      1. [Setting Simulator Options](cmd-options-set.md)
      2. [Options with Special Behavior](cmd-options-special.md)
      3. [Temperature, Scale, and Conductances](cmd-options-temp.md)
      4. [Tolerances](cmd-options-tol.md)
      5. [Relative Tolerance Reference](cmd-options-relref.md)
      6. [Newton-Raphson Solver](cmd-options-nr.md)
      7. [Homotopy Algorithms](cmd-options-homotopy.md)
      8. [Operating Point Options](cmd-options-op.md)
      9. [Small-Signal Analysis Options](cmd-options-smsig.md)
      10. [Transient Analysis Options](cmd-options-tran.md)
      11. [Harmonic Balance Options](cmd-options-hb.md)
      12. [Output and Sweep Options](cmd-options-output.md)
   5. [Modifying Circuit Variables](cmd-var.md)
   6. [Modifying Parameters](cmd-alter.md)
   7. [Circuit Elaboration](cmd-elaboration.md)
   8. Printing
   9. [Postprocessing](cmd-postprocess.md)
   10. [Error Handling](cmd-errorhandling.md)
6. [Circuit Analyses](cmd-analysis-overview.md)
   1. [Operating Point Analysis](cmd-analysis-op.md)
   2. [DC Small-Signal Analysis](cmd-analysis-dcinc.md)
   3. [DC Small-Signal Transfer Function Analysis](cmd-analysis-dcxf.md)
   4. [AC Small-signal Analysis](cmd-analysis-ac.md)
   5. [AC Small-signal Transfer function analysis](cmd-analysis-acxf.md)
   6. [Small-Signal Noise Analysis](cmd-analysis-noise.md)
   7. [Transient Analysis](cmd-analysis-tran.md)
   8. [Harmonic Balance Analysis](cmd-analysis-hb.md)
   9. [Verilog-A Natures and Tolerances](cmd-analysis-natures.md)
7. [Device Model Library](dev-overview.md)
   1. [Builtin Devices](dev-builtin.md)
      1. Independent Sources
      2. Voltage-controlled voltage source
      3. Voltage-controlled current source
      4. Current-controlled voltage source
      5. Current-controlled current source
      6. Inductive Coupling
   2. [VACASK Verilog-A Devices](dev-vacask.md)
   3. [3rd Party Verilog-A Devices](dev-3rdparty.md)
   4. [Converted SPICE Devices](dev-spice.md)
8. Python Helpers
   1. rawfile.py — Reading Raw Files
9. [C++ API](cpp-api.md)
