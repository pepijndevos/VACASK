# Table of Contents

1. [Input File](input-overview.md)
   1. [Numbers](input-numbers.md)
   2. [Strings](input-strings.md)
   3. [Identifiers](input-identifiers.md)
   4. [Reserved Words](input-reserved.md)
   5. [Including a File](input-include.md)
   6. [Embedded Files](input-embed.md)
2. [Circuit Description and Elaboration](cir-overview.md)
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
3. [Data Types and Expressions](expr-overview.md)
   1. [Scalar Data Types](expr-scalars.md)
   2. [Vectors and Lists](expr-vectors.md)
   3. [Builtin Constants](expr-constants.md)
   4. [Operators](expr-operators.md)
   5. [Builtin Functions](expr-functions.md)
   6. [Circuit Variables](expr-cirvars.md)
   7. [Special Identifiers](expr-special.md)
4. [The Control Block](cmd-control.md)
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
   6. Modifying Parameters
   7. [Circuit Elaboration](cmd-elaboration.md)
   8. Printing
   9. [Postprocessing](cmd-postprocess.md)
   10. Error Handling
5. Circuit Analyses
   1. [Operating Point Analysis](cmd-analysis-op.md)
   2. [DC Small-Signal Analysis](cmd-analysis-dcinc.md)
   3. [DC Small-Signal Transfer Function Analysis](cmd-analysis-dcxf.md)
   4. [AC Small-signal Analysis](cmd-analysis-ac.md)
   5. [AC Small-signal Transfer function analysis](cmd-analysis-acxf.md)
   6. [Small-Signal Noise Analysis](cmd-analysis-noise.md)
   7. [Transient Analysis](cmd-analysis-tran.md)
   8. [Harmonic Balance Analysis](cmd-analysis-hb.md)
   9. Verilog-A Natures and Tolerances
6. Device Model Library
   1. Builtin Devices
      1. Independent Sources
      2. Voltage-controlled voltage source
      3. Voltage-controlled current source
      4. Current-controlled voltage source
      5. Current-controlled current source
      6. Inductive Coupling
   2. [VACASK Verilog-A Devices](dev-vacask.md)
   3. [3rd Party Verilog-A Devices](dev-3rdparty.md)
   4. [Converted SPICE Devices](dev-spice.md)
7. VACASK Startup and Configuration
8. Python Helpers
9. [C++ API](cpp-api.md)
