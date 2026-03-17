# Table of Contents

1. Input File
   1. [Overview](input-overview.md)
   2. [Numbers](input-numbers.md)
   3. [Strings](input-strings.md)
   4. [Identifiers](input-identifiers.md)
   5. [Reserved Words](input-reserved.md)
   6. [Including a File](input-include.md)
   7. [Embedded Files](input-embed.md)
2. Circuit Description and Elaboration
   1. [Loading Devices](input-cir-loading.md)
   2. [Nodes](input-cir-nodes.md)
   3. [Masters (Models and Subcircuits)](input-cir-masters.md)
   4. [Instances](input-cir-instance.md)
   5. [Subcircuits Revisited](input-cir-subcircuit.md)
   6. [Parametrization](input-cir-param.md)
   7. [Parallel Devices (`$mfactor`)](input-cir-multiplier.md)
   8. [Conditional Netlist Blocks](input-cir-cond.md)
   9. [Automated Binning](input-cir-binning.md)
   10. [Circuit Elaboration](input-cir-elaboration.md)
3. Data Types and Expressions
   1. Scalar Data Types
   2. Vectors and Lists
   3. Circuit Variables
   4. Builtin Constants
   5. Operators
   6. Builtin Functions
   8. Special Identifiers
   7. Identifier Value Lookup
4. The Control Block
   1. [Analysis Statements](cmd-analysis.md)
   2. [Sweeping](cmd-sweep.md)
   3. [Saving Results](cmd-save.md)
   4. [Simulator Options](cmd-options.md)
      1. [Setting Simulator Options](cmd-options-set.md)
      2. [Options Causing Circuit Reelaboration](cmd-options-elaborate.md)
      3. [List of Simulator Options](cmd-options-list.md)
   5. [Circuit Variables](cmd-var.md)
   6. [Modifying Parameters](cmd-alter.md)
   7. [Circuit Elaboration](cmd-elaborate.md)
   8. [Printing](cmd-print.md)
   9. [Postprocessing](cmd-postprocess.md)
   10. [Error Handling](cmd-abort.md)
5. Circuit Analyses
   1. [Operating Point Analysis](cmd-analysis-op.md)
   2. [DC Small-Signal Analysis](cmd-analysis-dcinc.md)
   3. [DC Small-Signal Transfer Function Analysis](cmd-analysis-dcxf.md)
   4. [AC Small-signal Analysis](cmd-analysis-ac.md)
   5. [AC Small-signal Transfer function analysis](cmd-analysis-acxf.md)
   6. [Small-Signal Noise Analysis](cmd-analysis-noise.md)
   7. [Transient Analysis](cmd-analysis-tran.md)
   8. [Harmonic Balance Analysis](cmd-analysis-hb.md)
6. VACASK Startup and Configuration
7. Python Helpers
8. Device Model Library
   1. [Builtin Devices](input-cir-builtin.md)
      1. Independent Sources
      2. Linear Controlled Sources
      3. Inductive Coupling
   2. VACASK Verilog-A Devices
   3. 3rd Party Verilog-A Devices
   4. Converted SPICE Devices
9. C++ API
