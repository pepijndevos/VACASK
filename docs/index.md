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
   1. [Loading Devices](cir-loading.md)
   2. [Nodes](cir-nodes.md)
   3. [Masters (Models and Subcircuits)](cir-masters.md)
   4. [Instances](cir-instance.md)
   5. Subcircuits and Hierarchy
   6. Parametrization
   7. Parallel Devices (`$mfactor`)
   8. Conditional Netlist Blocks
   9. Automated Binning
   10. Circuit Elaboration
3. Data Types and Expressions
   1. Scalar Data Types
   2. Vectors and Lists
   3. Circuit Variables
   4. [Builtin Constants](expr-constants.md)
   5. [Operators](expr-operators.md)
   6. [Builtin Functions](expr-functions.md)
   7. [Special Identifiers](expr-special.md)
   7. Identifier Value Lookup
4. The Control Block
   1. Analysis Statements
   2. Sweeping
   3. Saving Results
   4. Simulator Options
      1. Setting Simulator Options
      2. Options Causing Circuit Reelaboration
      3. List of Simulator Options
   5. Circuit Variables
   6. Modifying Parameters
   7. Circuit Elaboration
   8. Printing
   9. Postprocessing
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
6. VACASK Startup and Configuration
7. Python Helpers
8. Device Model Library
   1. Builtin Devices
      1. Independent Sources
      2. Linear Controlled Sources
      3. Inductive Coupling
   2. VACASK Verilog-A Devices
   3. 3rd Party Verilog-A Devices
   4. Converted SPICE Devices
9. C++ API
