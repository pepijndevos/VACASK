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
// Returns false and populates `s` if the parser reported error nodes.
bool buildParserTables(const std::string& source, bool startSpice,
                       ParserTables& tab, Parser& p, Status& s);

}

#endif
