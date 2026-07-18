#include "netlistrs.h"
#include "netlist_cxx_bridge/lib.h"
#include "simulator.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
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

// Map SPICE source function args to VACASK named vsource/isource params.
// Returns a param string such as: dc=5 type="pulse" val0=0 val1=5 delay=1m rise=1u ...
static std::string spiceSourceParams(const netlist::SpiceSource& src) {
    std::ostringstream os;
    bool first = true;
    auto add = [&](const std::string& kv) {
        if (!kv.empty()) {
            if (!first) os << " ";
            os << kv;
            first = false;
        }
    };

    // DC value
    if (!sv(src.dc).empty())        add("dc=" + sv(src.dc));
    // AC small-signal
    if (!sv(src.ac_mag).empty())    add("mag=" + sv(src.ac_mag));
    if (!sv(src.ac_phase).empty())  add("phase=" + sv(src.ac_phase));

    // Transient function
    std::string tk = sv(src.tran_kind);
    if (!tk.empty()) {
        std::string tklower = tk;
        std::transform(tklower.begin(), tklower.end(), tklower.begin(), ::tolower);
        // SPICE "SIN" → VACASK "sine"
        std::string vaKind = (tklower == "sin") ? "sine" : tklower;
        add("type=\"" + vaKind + "\"");

        const auto& args = src.tran_args;
        if (vaKind == "pulse") {
            // SPICE PULSE(v0 v1 td tr tf pw per) → val0 val1 delay rise fall width period
            static const char* names[] = {
                "val0", "val1", "delay", "rise", "fall", "width", "period"
            };
            for (size_t i = 0; i < args.size() && i < 7; ++i) {
                std::string v = sv(args[i]);
                if (!v.empty()) add(std::string(names[i]) + "=" + v);
            }
        } else if (vaKind == "sine") {
            // SPICE SIN(vo va freq td theta) → sinedc ampl freq delay theta
            static const char* names[] = {
                "sinedc", "ampl", "freq", "delay", "theta"
            };
            for (size_t i = 0; i < args.size() && i < 5; ++i) {
                std::string v = sv(args[i]);
                if (!v.empty()) add(std::string(names[i]) + "=" + v);
            }
        } else if (vaKind == "pwl") {
            // SPICE PWL(t0 v0 t1 v1 ...) → wave=[t0 v0 t1 v1 ...]
            if (!args.empty()) {
                if (!first) os << " ";
                os << "wave=[";
                for (size_t i = 0; i < args.size(); ++i) {
                    if (i > 0) os << " ";
                    os << sv(args[i]);
                }
                os << "]";
                first = false;
            }
        } else if (vaKind == "exp") {
            // SPICE EXP(v1 v2 td1 tau1 td2 tau2) → val0 val1 delay tau1 td2 tau2
            static const char* names[] = {
                "val0", "val1", "delay", "tau1", "td2", "tau2"
            };
            for (size_t i = 0; i < args.size() && i < 6; ++i) {
                std::string v = sv(args[i]);
                if (!v.empty()) add(std::string(names[i]) + "=" + v);
            }
        }
        // Unknown tran types: no positional arg mapping; type= was already emitted.
    }
    return os.str();
}

// Fill a PTSubcircuitDefinition from one SpiceSubckt body (recursive).
static void fillSpiceSubDef(PTSubcircuitDefinition& def, const netlist::SpiceSubckt& s,
                            Parser& p, std::set<std::string>& addedModels);

// Ensure a self-alias model card is emitted once per block.
// These mirror `model resistor resistor` / `model vsource vsource` in .sim files.
static void ensureSpiceModel(PTSubcircuitDefinition& into, const std::string& master,
                             std::set<std::string>& addedModels) {
    if (addedModels.insert(master).second) {
        into.add(PTModel(Id(master.c_str()), Id(master.c_str())));
    }
}

// Process a single SpiceDevice into the given subcircuit definition.
static bool addSpiceDevice(const netlist::SpiceDevice& dev, PTSubcircuitDefinition& into,
                           Parser& p, std::set<std::string>& addedModels, Status& s) {
    std::string name = sv(dev.name);
    std::string val  = sv(dev.value);
    std::string mdl  = sv(dev.model);

    switch (dev.kind) {
        case netlist::SpiceDeviceKind::Resistor: {
            std::string master = mdl.empty() ? "resistor" : mdl;
            ensureSpiceModel(into, "resistor", addedModels);
            PTInstance inst(Id(name.c_str()), Id(master.c_str()), nodeList(dev.nodes));
            if (!val.empty()) inst.add(p.parseParameters("r=" + val));
            auto ps = paramString(dev.params);
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Capacitor: {
            std::string master = mdl.empty() ? "capacitor" : mdl;
            ensureSpiceModel(into, "capacitor", addedModels);
            PTInstance inst(Id(name.c_str()), Id(master.c_str()), nodeList(dev.nodes));
            if (!val.empty()) inst.add(p.parseParameters("c=" + val));
            auto ps = paramString(dev.params);
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Inductor: {
            std::string master = mdl.empty() ? "inductor" : mdl;
            ensureSpiceModel(into, "inductor", addedModels);
            PTInstance inst(Id(name.c_str()), Id(master.c_str()), nodeList(dev.nodes));
            if (!val.empty()) inst.add(p.parseParameters("l=" + val));
            auto ps = paramString(dev.params);
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::VSource: {
            ensureSpiceModel(into, "vsource", addedModels);
            PTInstance inst(Id(name.c_str()), Id("vsource"), nodeList(dev.nodes));
            std::string srcParams = spiceSourceParams(dev.source);
            auto ps = paramString(dev.params);
            if (!srcParams.empty() && !ps.empty()) srcParams += " ";
            srcParams += ps;
            if (!srcParams.empty()) inst.add(p.parseParameters(srcParams));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::ISource: {
            ensureSpiceModel(into, "isource", addedModels);
            PTInstance inst(Id(name.c_str()), Id("isource"), nodeList(dev.nodes));
            std::string srcParams = spiceSourceParams(dev.source);
            auto ps = paramString(dev.params);
            if (!srcParams.empty() && !ps.empty()) srcParams += " ";
            srcParams += ps;
            if (!srcParams.empty()) inst.add(p.parseParameters(srcParams));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Diode:
        case netlist::SpiceDeviceKind::Mosfet:
        case netlist::SpiceDeviceKind::Bjt:
        case netlist::SpiceDeviceKind::Jfet:
        case netlist::SpiceDeviceKind::SubcktCall:
        case netlist::SpiceDeviceKind::Vcvs:
        case netlist::SpiceDeviceKind::Vccs:
        case netlist::SpiceDeviceKind::Ccvs:
        case netlist::SpiceDeviceKind::Cccs:
        case netlist::SpiceDeviceKind::MutualInductor:
        case netlist::SpiceDeviceKind::Behavioral:
        case netlist::SpiceDeviceKind::Switch:
        case netlist::SpiceDeviceKind::Osdi:
        default:
            Simulator::err() << "WARNING: SPICE device '" << name
                             << "' has unsupported kind (skipped in this adapter version)\n";
            break;
    }
    return true;
}

static void fillSpiceSubDef(PTSubcircuitDefinition& def, const netlist::SpiceSubckt& s,
                            Parser& p, std::set<std::string>& addedModels) {
    auto sp = paramString(s.params);
    if (!sp.empty()) def.add(p.parseParameters(sp));
    Status dummy;
    for (const auto& dev : s.devices) addSpiceDevice(dev, def, p, addedModels, dummy);
    for (const auto& sub : s.subckts) {
        PTSubcircuitDefinition child(Id(sv(sub.name).c_str()), nodeList(sub.ports));
        fillSpiceSubDef(child, sub, p, addedModels);
        def.add(std::move(child));
    }
}

// Map one SpiceBlock into a PTSubcircuitDefinition.  `addedModels` is shared
// across repeated calls so self-alias model cards are emitted only once.
static bool spiceBlockToTables(const netlist::SpiceBlock& sb, PTSubcircuitDefinition& into,
                               ParserTables& /*tab*/, Parser& p,
                               std::set<std::string>& addedModels, Status& s) {
    // Top-level .param declarations from the SPICE block.
    auto sp = paramString(sb.params);
    if (!sp.empty()) into.add(p.parseParameters(sp));

    // Devices (R/C/L/V/I handled; others warn+skip).
    for (const auto& dev : sb.devices) {
        if (!addSpiceDevice(dev, into, p, addedModels, s)) return false;
    }

    // .model cards (D/M/Q — tasks 6-7; warn and skip for now).
    for (const auto& m : sb.models) {
        Simulator::err() << "WARNING: SPICE .model '" << sv(m.name)
                         << "' type='" << sv(m.model_type)
                         << "' not yet mapped by SPICE adapter (skipped)\n";
    }

    // Nested .subckt definitions.
    for (const auto& sub : sb.subckts) {
        PTSubcircuitDefinition child(Id(sv(sub.name).c_str()), nodeList(sub.ports));
        fillSpiceSubDef(child, sub, p, addedModels);
        into.add(std::move(child));
    }

    // .include / .lib inside a SPICE block — deferred (same as milestone-1 deferrals).
    if (!sb.includes.empty()) {
        Simulator::err() << "WARNING: netlistrs adapter does not yet resolve "
                         << sb.includes.size() << " include(s) inside a SPICE block\n";
    }
    return true;
}

// (private) Accumulate one parsed Netlist's toplevel members into `top`,
// its analyses/globals into `tab`, then recurse into its top-level includes.
// Section-qualified includes (section != "") are skipped (deferred: flat Netlist
// projection does not carry library sections).
// Includes nested inside subckt bodies are also deferred (Subckt drops includes).
// Returns true on success; false with `s` set on any error (open failure, parse
// error, or error in a nested include).
bool mergeNetlist(const netlist::Netlist& nl, PTSubcircuitDefinition& top,
                  ParserTables& tab, Parser& p,
                  const std::filesystem::path& baseDir,
                  std::set<std::filesystem::path>& visited,
                  std::set<std::string>& addedModels,
                  Status& s);

bool mergeNetlist(const netlist::Netlist& nl, PTSubcircuitDefinition& top,
                  ParserTables& tab, Parser& p,
                  const std::filesystem::path& baseDir,
                  std::set<std::filesystem::path>& visited,
                  std::set<std::string>& addedModels,
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

    // SPICE blocks (R/C/L/V/I adapter + nested .subckt/.model).
    for (const auto& sb : nl.spice_blocks) {
        if (!spiceBlockToTables(sb, top, tab, p, addedModels, s)) return false;
    }

    // Globals and analyses into tab.
    for (const auto& g : nl.globals) tab.addGlobal(PTParsedIdentifier(sv(g).c_str()));
    for (const auto& a : nl.analyses) {
        PTAnalysis desc(Id(sv(a.name).c_str()), Id(sv(a.analysis_type).c_str()));
        auto ps = paramString(a.params);
        if (!ps.empty()) desc.add(p.parseParameters(ps));
        tab.addCommand(std::move(desc));
    }

    // Warn about fields that are parsed but not yet transcribed.
    // One-time warnings per category: ics silently change transient results if
    // dropped without notice; saves and ahdl_includes are also deferred.
    if (!nl.saves.empty()) {
        Simulator::err() << "WARNING: netlistrs adapter does not yet transcribe "
                         << nl.saves.size() << " 'save' directive(s); save requests ignored\n";
    }
    if (!nl.ics.empty()) {
        Simulator::err() << "WARNING: netlistrs adapter does not yet transcribe "
                         << nl.ics.size() << " 'ic' directive(s); initial conditions ignored\n";
    }
    if (!nl.ahdl_includes.empty()) {
        Simulator::err() << "WARNING: netlistrs adapter does not yet transcribe "
                         << nl.ahdl_includes.size() << " ahdl_include (VA) directive(s); AHDL includes ignored\n";
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
            return false;
        }
        std::stringstream ss; ss << in.rdbuf();
        std::string contents = ss.str();

        std::string iext = absPath.extension().string();
        std::transform(iext.begin(), iext.end(), iext.begin(), ::tolower);
        bool spice = (iext != ".scs");
        netlist::Netlist sub = netlist::parse_netlist(rust::Str(contents), spice);
        if (!sub.errors.empty()) {
            std::ostringstream os;
            os << "parse error in include '" << absPath.string() << "': "
               << sub.errors.size() << " error(s)"
               << " (first at bytes [" << sub.errors[0].start << ", "
               << sub.errors[0].end << "))";
            s.set(Status::Syntax, os.str());
            return false;
        }

        if (!mergeNetlist(sub, top, tab, p, absPath.parent_path(), visited, addedModels, s))
            return false;
    }
    return true;
}

} // namespace

bool buildParserTables(const std::string& source, bool startSpice,
                       ParserTables& tab, Parser& p, Status& s) {
    // Note: relative includes in `source` are resolved against CWD. Prefer
    // buildParserTablesFromFile for sources that contain include directives.
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
    std::set<std::string> addedModels;
    if (!mergeNetlist(nl, top, tab, p, std::filesystem::current_path(), visited, addedModels, s))
        return false;
    tab.setDefaultSubDef(std::move(top));
    return true;
}

bool buildParserTablesFromFile(const std::string& path,
                               ParserTables& tab, Parser& p, Status& s) {
    namespace fs = std::filesystem;
    fs::path fp(path);
    std::string ext = fp.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

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
    std::set<std::string> addedModels;
    bool spice = (ext != ".scs");
    netlist::Netlist nl = netlist::parse_netlist(rust::Str(source), spice);
    if (!nl.errors.empty()) {
        std::ostringstream os;
        os << "netlist parse error(s) in '" << path << "': " << nl.errors.size()
           << " (first at bytes [" << nl.errors[0].start << ", " << nl.errors[0].end << "))";
        s.set(Status::Syntax, os.str());
        return false;
    }
    if (!mergeNetlist(nl, top, tab, p, absPath.parent_path(), visited, addedModels, s))
        return false;
    tab.setDefaultSubDef(std::move(top));
    return true;
}

}
