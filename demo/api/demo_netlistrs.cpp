#include "netlist_cxx_bridge/lib.h"
#include <iostream>

int main() {
    netlist::Netlist nl =
        netlist::parse_netlist(rust::Str("simulator lang=spectre\nr1 (a b) resistor r=1k\n"), false);
    std::cout << "instances: " << nl.instances.size() << "\n";
    return (nl.instances.size() == 1 && nl.errors.empty()) ? 0 : 1;
}
