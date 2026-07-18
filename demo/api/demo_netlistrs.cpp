#include "netlistrs.h"
#include "parser.h"
#include <iostream>

using namespace sim;

int main() {
    const char* src =
        "simulator lang=spectre\n"
        "parameters c0=1u\n"
        "r1 (1 2) res r=1k\n"
        "c1 (2 0) cap c=2*c0\n"
        "model res resistor\n"
        "model cap capacitor\n";
    ParserTables tab("rs smoke");
    Parser p(tab);
    Status s;
    if (!buildParserTables(src, /*startSpice=*/false, tab, p, s)) {
        std::cerr << "adapter failed: " << s.message() << "\n";
        return 1;
    }
    if (!tab.verify(s)) { std::cerr << "verify failed: " << s.message() << "\n"; return 1; }
    tab.dump(0, std::cout);
    return 0;
}
