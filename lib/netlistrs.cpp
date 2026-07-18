#include "netlistrs.h"
#include "netlist_cxx_bridge/lib.h"
#include <filesystem>
#include <fstream>
#include <set>
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

// (private) Accumulate one parsed Netlist's toplevel members into `top`,
// its analyses/globals into `tab`, then recurse into its top-level includes.
// Section-qualified includes (section != "") are skipped (deferred: flat Netlist
// projection does not carry library sections).
// Includes nested inside subckt bodies are also deferred (Subckt drops includes).
void mergeNetlist(const netlist::Netlist& nl, PTSubcircuitDefinition& top,
                  ParserTables& tab, Parser& p,
                  const std::filesystem::path& baseDir,
                  std::set<std::filesystem::path>& visited,
                  Status& s);

void mergeNetlist(const netlist::Netlist& nl, PTSubcircuitDefinition& top,
                  ParserTables& tab, Parser& p,
                  const std::filesystem::path& baseDir,
                  std::set<std::filesystem::path>& visited,
                  Status& s) {
    // Accumulate toplevel params/models/instances/subckts.
    auto sp = paramString(nl.params);
    if (!sp.empty()) top.add(p.parseParameters(sp));
    for (const auto& m : nl.models)    top.add(makeModel(m, p));
    for (const auto& i : nl.instances) top.add(makeInstance(i, p));
    for (const auto& sub : nl.subckts) {
        PTSubcircuitDefinition child(Id(sv(sub.name).c_str()), nodeList(sub.ports));
        fillSubDef(child, sub, p);
        top.add(std::move(child));
    }

    // Globals and analyses into tab.
    for (const auto& g : nl.globals) tab.addGlobal(PTParsedIdentifier(sv(g).c_str()));
    for (const auto& a : nl.analyses) {
        PTAnalysis desc(Id(sv(a.name).c_str()), Id(sv(a.analysis_type).c_str()));
        auto ps = paramString(a.params);
        if (!ps.empty()) desc.add(p.parseParameters(ps));
        tab.addCommand(std::move(desc));
    }

    // Recurse into top-level includes (section-qualified deferred).
    for (const auto& inc : nl.includes) {
        // Skip section-qualified includes (library sections not yet projected).
        if (!inc.section.empty()) continue;

        std::filesystem::path incPath = baseDir / sv(inc.path);
        std::filesystem::path absPath;
        try {
            absPath = std::filesystem::canonical(incPath);
        } catch (...) {
            // File doesn't exist — fall through, ifstream will fail below.
            absPath = std::filesystem::absolute(incPath);
        }

        if (visited.count(absPath)) continue;
        visited.insert(absPath);

        std::ifstream in(absPath);
        if (!in) {
            s.set(Status::NotFound, "cannot open include: " + absPath.string());
            return;
        }
        std::stringstream ss; ss << in.rdbuf();
        std::string contents = ss.str();

        std::string ext = absPath.extension().string();
        bool spice = (ext != ".scs");
        netlist::Netlist sub = netlist::parse_netlist(rust::Str(contents), spice);
        if (!sub.errors.empty()) {
            std::ostringstream os;
            os << "parse error in include '" << absPath.string() << "': "
               << sub.errors.size() << " error(s)"
               << " (first at bytes [" << sub.errors[0].start << ", "
               << sub.errors[0].end << "))";
            s.set(Status::Syntax, os.str());
            return;
        }

        mergeNetlist(sub, top, tab, p, absPath.parent_path(), visited, s);
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
    PTSubcircuitDefinition top;
    std::set<std::filesystem::path> visited;
    mergeNetlist(nl, top, tab, p, std::filesystem::current_path(), visited, s);
    tab.setDefaultSubDef(std::move(top));
    return true;
}

bool buildParserTablesFromFile(const std::string& path,
                               ParserTables& tab, Parser& p, Status& s) {
    namespace fs = std::filesystem;
    fs::path fp(path);
    std::string ext = fp.extension().string();

    if (ext == ".sim") {
        // VACASK's own native parser fills tab directly.
        // FileStack::addFile() registers the file and returns its index.
        FileStackFileIndex idx = tab.fileStack().addFile(path);
        return p.parseNetlistFile(idx, s);
    }

    std::ifstream in(path);
    if (!in) {
        s.set(Status::NotFound, "cannot open: " + path);
        return false;
    }
    std::stringstream ss; ss << in.rdbuf();
    std::string source = ss.str();

    tab.defaultGround();
    PTSubcircuitDefinition top;
    fs::path absPath;
    try {
        absPath = fs::canonical(fp);
    } catch (...) {
        absPath = fs::absolute(fp);
    }
    std::set<fs::path> visited{ absPath };
    bool spice = (ext != ".scs");
    netlist::Netlist nl = netlist::parse_netlist(rust::Str(source), spice);
    if (!nl.errors.empty()) {
        std::ostringstream os;
        os << "netlist parse error(s) in '" << path << "': " << nl.errors.size()
           << " (first at bytes [" << nl.errors[0].start << ", " << nl.errors[0].end << "))";
        s.set(Status::Syntax, os.str());
        return false;
    }
    mergeNetlist(nl, top, tab, p, absPath.parent_path(), visited, s);
    tab.setDefaultSubDef(std::move(top));
    return true;
}

}
