#include "netlistrs.h"
#include "parser.h"
#include <iostream>

using namespace sim;

int main() {
    // Covers:
    //  - r1/c1/model (baseline)
    //  - c=2 * c0   SPACED expression in instance param (risky paramString case)
    //  - global      globals loop in buildParserTables
    //  - myTran tran analyses loop in buildParserTables
    const char* src =
        "simulator lang=spectre\n"
        "global 0 vdd\n"
        "parameters c0=1u\n"
        "r1 (1 2) res r=1k\n"
        "c1 (2 0) cap c=2 * c0\n"
        "model res resistor\n"
        "model cap capacitor\n"
        "myTran tran stop=1m\n";
    ParserTables tab("rs smoke");
    Parser p(tab);
    Status s;
    if (!buildParserTables(src, /*startSpice=*/false, tab, p, s)) {
        std::cerr << "adapter failed: " << s.message() << "\n";
        return 1;
    }
    if (!tab.verify(s)) { std::cerr << "verify failed: " << s.message() << "\n"; return 1; }
    tab.dump(0, std::cout);
    std::cout << "PASS: r1/c1/models/global/analysis all loaded and verified\n";
    return 0;
}
