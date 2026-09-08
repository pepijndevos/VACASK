#ifndef __NETLISTRS_DEFINED
#define __NETLISTRS_DEFINED

#include <string>
#include "parser.h"
#include "parseroutput.h"
#include "status.h"
#include "common.h"

namespace sim {

// Parse a foreign-format netlist FILE and merge its models/subckts/devices into
// the caller-provided `top` subcircuit definition, used by the native parser's
// `include` handler to dispatch a foreign-format include into the in-progress
// toplevel definition. The dialect is determined solely by the explicit
// `language` argument (one of ngspice|hspice|pspice|xyce|spectre); an
// empty or unrecognised value is rejected with an error. `section=` is not
// supported with lang=spectre. Analysis/command directives in the file are
// ignored (with a warning); OSDI loads for referenced masters are auto-emitted
// into `tab` (de-duplicated). `section` (if non-empty) selects a `.lib`-style
// section. It does NOT call defaultGround()/setDefaultSubDef().
// Returns false and populates `s` on error.
bool mergeForeignFile(const std::string& path, const std::string& section,
                      const std::string& language,
                      PTSubcircuitDefinition& top, ParserTables& tab,
                      Parser& p, Status& s);

}

#endif
