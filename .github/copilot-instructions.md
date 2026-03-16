# Project overview

## Repository layout
- `include/`: simulator library include files
- `lib/`: source code of the simulator library
- `simulator/`: source code of the entry point, control block interpreter
- `config/`: sample simulator TOML config file
- `test/`: test netlists with embedded Python postprecessing scripts that check the results
- `benchmark/`: benchmarking system and netlists
- `demo/`: usage examples
- `devices/`: source code of devices (Verilog-A)
- `python/`: Python helper scripts, Ngspice, Xschem, and IHP PDK converters
- `inc/`: supplied netlist include files
- `docs/`: documetation

## Documentation rules
- Entry file is index.md
- Each subject in its own file. 
- Look in index.md where you will find the outline of the docs. 
- You are allowed to add links to index.md, but keep the structure. 
- Prefer concise technical writing.
- Start each document with a short overview.
- Use one H1 only.
- Use relative links within the repository. Link only to other docs. 
- Keep examples minimal and runnable.
- Input file syntax is gven by the flex and bison files in the library. 
- What the developer redacts in a file must be kept. He knows better. 

## Notes on documenting anaylses
- Analyses are described in files named an<suffix>.h. 
- An analysis uses one or more cores from files named core<suffix>.h. 
- Analysis parameters are in a data structure defined in the core file. 
- Document parameters in a table. 
- An analysis can expose some of the parameters of a core that is a dependency, 
  e.g. ac analysis uses an operating point core and exposes some of its parameters. 
- The save directives available for an analysis are found in an<suffix>.h where output 
  descriptors are created. 
- Output descriptors are resolved in core<suffix>.h. 
- Document recognized save directives in a table. 
- Use comments in an<suffix>.cpp and core<suffix>.cpp. 

## Style
- Use American English.
- Prefer active voice.
- Explain why, not just what.

## When generating docs
- Do not invent modules or commands.
- Follow the existing docs structure in `docs/`.
- In examples do not declare node `0` as ground (it is by default the ground node anyway)
- In long examples the first line should be the title of a circuit. 
- Look at docs of similar subjects and follow their style and outline. 