#include "netlistrs.h"
#include "netlist_cxx_bridge/lib.h"
#include <sstream>

namespace sim {

namespace {

// Convert rust::String to std::string.
std::string sv(const rust::String& s) { return std::string(s); }

// "name=value name2=value2 …" from a list of Param, or "" if none.
std::string paramString(const rust::Vec<netlist::Param>& params) {
    std::ostringstream os;
    bool first = true;
    for (const auto& p : params) {
        if (!first) os << " ";
        os << sv(p.name) << "=" << sv(p.value);
        first = false;
    }
    return os.str();
}

PTIdentifierList nodeList(const rust::Vec<rust::String>& nodes) {
    PTIdentifierList terms;
    for (const auto& n : nodes) terms.push_back(PTParsedIdentifier(sv(n).c_str()));
    return terms;
}

PTInstance makeInstance(const netlist::Instance& i, Parser& p) {
    PTInstance inst(Id(sv(i.name).c_str()), Id(sv(i.master).c_str()), nodeList(i.nodes));
    auto ps = paramString(i.params);
    if (!ps.empty()) inst.add(p.parseParameters(ps));
    return inst;
}

PTModel makeModel(const netlist::Model& m, Parser& p) {
    PTModel mod(Id(sv(m.name).c_str()), Id(sv(m.master).c_str()));
    auto ps = paramString(m.params);
    if (!ps.empty()) mod.add(p.parseParameters(ps));
    return mod;
}

// if/else-if/else over instances → one PTBlockSequence.
PTBlockSequence makeConditional(const netlist::Conditional& c, Parser& p) {
    PTBlockSequence seq;
    for (const auto& cl : c.clauses) {
        PTBlock block;
        block.add(makeInstance(cl.instance, p));
        // Trailing else has empty condition → use trivially-true expression (1).
        std::string cond = cl.condition.empty() ? std::string("1") : sv(cl.condition);
        seq.add(p.parseExpression(cond), std::move(block));
    }
    return seq;
}

// Fill a PTSubcircuitDefinition from a netlist::Subckt (has conditionals + ports).
void fillSubDef(PTSubcircuitDefinition& def, const netlist::Subckt& s, Parser& p) {
    auto sp = paramString(s.params);
    if (!sp.empty()) def.add(p.parseParameters(sp));
    for (const auto& m : s.models)       def.add(makeModel(m, p));
    for (const auto& i : s.instances)    def.add(makeInstance(i, p));
    for (const auto& c : s.conditionals) def.add(makeConditional(c, p));
    for (const auto& sub : s.subckts) {
        PTSubcircuitDefinition child(Id(sv(sub.name).c_str()), nodeList(sub.ports));
        fillSubDef(child, sub, p);
        def.add(std::move(child));
    }
}

// Fill a PTSubcircuitDefinition from the top-level netlist::Netlist
// (no conditionals or ports at the top level).
void fillTopLevel(PTSubcircuitDefinition& def, const netlist::Netlist& nl, Parser& p) {
    auto sp = paramString(nl.params);
    if (!sp.empty()) def.add(p.parseParameters(sp));
    for (const auto& m : nl.models)    def.add(makeModel(m, p));
    for (const auto& i : nl.instances) def.add(makeInstance(i, p));
    for (const auto& sub : nl.subckts) {
        PTSubcircuitDefinition child(Id(sv(sub.name).c_str()), nodeList(sub.ports));
        fillSubDef(child, sub, p);
        def.add(std::move(child));
    }
}

} // namespace

bool buildParserTables(const std::string& source, bool startSpice,
                       ParserTables& tab, Parser& p, Status& s) {
    netlist::Netlist nl = netlist::parse_netlist(rust::Str(source), startSpice);
    if (!nl.errors.empty()) {
        std::ostringstream os;
        os << "netlist parse error(s): " << nl.errors.size()
           << " (first at bytes [" << nl.errors[0].start << ", " << nl.errors[0].end << "))";
        s.set(Status::Syntax, os.str());
        return false;
    }

    tab.defaultGround();

    // Toplevel: global params + top-level models/instances + nested subckts.
    PTSubcircuitDefinition top;
    fillTopLevel(top, nl, p);
    tab.setDefaultSubDef(std::move(top));

    for (const auto& g : nl.globals) tab.addGlobal(PTParsedIdentifier(sv(g).c_str()));

    // Analyses → control block.
    for (const auto& a : nl.analyses) {
        PTAnalysis desc(Id(sv(a.name).c_str()), Id(sv(a.analysis_type).c_str()));
        auto ps = paramString(a.params);
        if (!ps.empty()) desc.add(p.parseParameters(ps));
        tab.addCommand(std::move(desc));
    }
    return true;
}

}
