#ifndef __NETLISTRS_DEFINED
#define __NETLISTRS_DEFINED

#include <string>
#include "parser.h"
#include "parseroutput.h"
#include "status.h"
#include "common.h"

namespace sim {

// Parse `source` with the Rust netlist-cxx parser and transcribe the result
// into `tab`'s default toplevel subcircuit definition, nested subckt defs, and
// analyses. Parameters/expressions are re-parsed with `p` (parseParameters), so
// VACASK owns value-vs-expression classification. `startSpice` selects the
// starting dialect (true = SPICE, false = Spectre). OSDI PTLoads for the masters
// referenced by the parsed devices/models are auto-emitted into `tab`.
// Relative includes in `source` are resolved against CWD; prefer
// buildParserTablesFromFile for sources containing include directives.
// Returns false and populates `s` if the parser reported error nodes.
bool buildParserTables(const std::string& source, bool startSpice,
                       ParserTables& tab, Parser& p, Status& s);

// Parse a netlist FILE, resolving top-level includes relative to its directory.
//  .sim  -> VACASK's own native parser (Parser::parseNetlistFile); Rust path skipped.
//  .scs  -> Rust parser starting in Spectre.
//  else  -> Rust parser starting in SPICE.
// Known limitations (deferred — emit warnings, not errors):
//  - Section-qualified includes (.lib "file" section): not projected into the flat Netlist.
//  - Includes nested inside subckt bodies: Subckt does not carry includes.
//  - saves: not yet transcribed (save requests are dropped with a warning).
//  - ics: not yet transcribed (initial conditions are dropped with a warning).
//  - ahdl_includes (VA): not yet transcribed (dropped with a warning).
// OSDI PTLoads for the masters referenced by the parsed devices/models are
// auto-emitted into `tab`.
// Returns false and populates `s` on error.
bool buildParserTablesFromFile(const std::string& path,
                               ParserTables& tab, Parser& p, Status& s);

// Parse a foreign-format netlist FILE (SPICE by extension; Spectre for
// .scs/.spectre) and merge its models/subckts/devices into the caller-provided
// `top` subcircuit definition — used by the native parser's `include` handler to
// dispatch a foreign-format include into the in-progress toplevel definition.
// Analysis/command directives in the file are ignored (with a warning); OSDI
// loads for referenced masters are auto-emitted into `tab` (de-duplicated).
// `section` (if non-empty) selects a `.lib`-style section. Unlike the functions
// above it does NOT call defaultGround()/setDefaultSubDef().
// Returns false and populates `s` on error.
bool mergeForeignFile(const std::string& path, const std::string& section,
                      const std::string& language,
                      PTSubcircuitDefinition& top, ParserTables& tab,
                      Parser& p, Status& s);

}

#endif
