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
// starting dialect (true = SPICE, false = Spectre). PTLoads are NOT added here.
// Relative includes in `source` are resolved against CWD; prefer
// buildParserTablesFromFile for sources containing include directives.
// Returns false and populates `s` if the parser reported error nodes.
bool buildParserTables(const std::string& source, bool startSpice,
                       ParserTables& tab, Parser& p, Status& s);

// Parse a netlist FILE, resolving top-level includes relative to its directory.
//  .sim  -> VACASK's own native parser (Parser::parseNetlistFile); Rust path skipped.
//  .scs  -> Rust parser starting in Spectre.
//  else  -> Rust parser starting in SPICE.
// Section-qualified includes and includes inside subckt bodies are deferred.
// PTLoads are NOT added here.
// Returns false and populates `s` on error.
bool buildParserTablesFromFile(const std::string& path,
                               ParserTables& tab, Parser& p, Status& s);

}

#endif
