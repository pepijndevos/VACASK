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

// Strip ngspice expression quoting (`{expr}` braces, `'expr'` quotes) and
// collapse SPICE continuation markers (`\n+`) from parameter values.
std::string stripExprQuoting(const std::string& v) {
    std::string out;
    out.reserve(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        char c = v[i];
        if (c == '{' || c == '}' || c == '\'') continue;
        if (c == '\n') {
            size_t j = i + 1;
            while (j < v.size() && (v[j] == ' ' || v[j] == '\t')) ++j;
            if (j < v.size() && v[j] == '+') {
                out += ' ';
                i = j;
                continue;
            }
        }
        out += c;
    }
    return out;
}

// "name=value name2=value2 …" from a list of Param, or "" if none.
std::string paramString(const rust::Vec<netlist::Param>& params) {
    std::ostringstream os;
    bool first = true;
    for (const auto& p : params) {
        if (!first) os << " ";
        os << sv(p.name) << "=" << stripExprQuoting(sv(p.value));
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

// Forward declarations for mutual recursion: fillSubDef ↔ spiceBlockToTables ↔ mergeNetlist.
static bool spiceBlockToTables(const netlist::SpiceBlock& sb, PTSubcircuitDefinition& into,
                               ParserTables& tab, Parser& p,
                               std::set<std::string>& addedModels, Status& s,
                               const std::filesystem::path& baseDir,
                               std::set<std::filesystem::path>& visited);

// Fill a PTSubcircuitDefinition from a netlist::Subckt (has conditionals + ports).
// Returns false (with `st` set) on any error in nested SPICE-block processing.
static bool fillSubDef(PTSubcircuitDefinition& def, const netlist::Subckt& s, Parser& p,
                       ParserTables& tab, std::set<std::string>& addedModels, Status& st,
                       const std::filesystem::path& baseDir,
                       std::set<std::filesystem::path>& visited) {
    auto sp = paramString(s.params);
    if (!sp.empty()) def.add(p.parseParameters(sp));
    for (const auto& m : s.models)       def.add(makeModel(m, p));
    for (const auto& i : s.instances)    def.add(makeInstance(i, p));
    for (const auto& c : s.conditionals) def.add(makeConditional(c, p));
    for (const auto& sub : s.subckts) {
        PTSubcircuitDefinition child(Id(sv(sub.name).c_str()), nodeList(sub.ports));
        if (!fillSubDef(child, sub, p, tab, addedModels, st, baseDir, visited)) return false;
        def.add(std::move(child));
    }
    // Process any SPICE blocks nested inside this Spectre subckt body (Fix 1).
    for (const auto& sb : s.spice_blocks) {
        if (!spiceBlockToTables(sb, def, tab, p, addedModels, st, baseDir, visited)) return false;
    }
    return true;
}

// Build a "name=value …" param string excluding a set of keys (case-insensitive).
// Used to strip dispatch-only keys like "level" before passing params to OSDI.
static std::string paramStringExcluding(const rust::Vec<netlist::Param>& params,
                                        const std::initializer_list<std::string>& exclude) {
    std::ostringstream os;
    bool first = true;
    for (const auto& p : params) {
        std::string key = sv(p.name);
        std::string keylower = key;
        std::transform(keylower.begin(), keylower.end(), keylower.begin(), ::tolower);
        bool skip = false;
        for (const auto& ex : exclude) { if (keylower == ex) { skip = true; break; } }
        if (skip) continue;
        if (!first) os << " ";
        os << key << "=" << stripExprQuoting(sv(p.value));
        first = false;
    }
    return os.str();
}

// True if any parameter name (case-insensitive) matches one of `keys`.
static bool spiceParamsHaveAny(const rust::Vec<netlist::Param>& params,
                               const std::initializer_list<std::string>& keys) {
    for (const auto& p : params) {
        std::string key = sv(p.name);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        for (const auto& k : keys) if (key == k) return true;
    }
    return false;
}

// Map SPICE model_type + level to a VACASK OSDI master name.
// Returns "" if there is no known VACASK master for the given type.
// Emits a warning if the level is unknown for a MOSFET type and falls back
// to the nearest available master.
static std::string spiceModelMaster(const std::string& model_type_raw,
                                    const std::string& level_str,
                                    const std::string& /*version*/) {
    std::string mt = model_type_raw;
    std::transform(mt.begin(), mt.end(), mt.begin(), ::tolower);

    // Diode: d -> diode (top-level diode.osdi, module diode(A,C))
    if (mt == "d") return "diode";

    // MOSFET: nmos/pmos by level
    if (mt == "nmos" || mt == "pmos") {
        int level = 0;
        try { level = std::stoi(level_str); } catch (...) {}

        // Dispatch table: level -> OSDI master name
        // (uses filenames without .osdi suffix; VA module names are the master)
        // bsim3v3.osdi  -> module bsim3   (bsim3v3 3.3, accepts level 49-53)
        // bsim4v8.osdi  -> module bsim4   (bsim4 4.8, level 14 is typical)
        // bsimbulk106.osdi -> module bsimbulk (has thermal port; 5 terminals)
        // psp103v4.osdi -> module psp103va (accepts level 103)
        // Default (levels 1/2/3/49/53 or unknown) -> bsim3
        if (level == 54)               return "bsim4";
        if (level == 70 || level == 72) {
            Simulator::err() << "WARNING: MOSFET level=" << level
                             << " (bsimbulk): has thermal port; connect substrate to 0 or add explicit bulk node\n";
            return "bsimbulk";
        }
        if (level == 103)              return "psp103va";
        // Levels 1,2,3,49,53 -> bsim3 (canonical BSIM3v3 range)
        if (level != 0 && level != 1 && level != 2 && level != 3 &&
            level != 49 && level != 53) {
            Simulator::err() << "WARNING: MOSFET level=" << level
                             << " not in known dispatch table; falling back to bsim3\n";
        }
        return "bsim3";
    }

    // BJT: npn/pnp -> vbic13 (3-terminal, vbic_1p3.osdi, module vbic13(c,b,e))
    // Use 4-terminal variant (vbic13_4t) when the instance has a substrate node.
    if (mt == "npn" || mt == "pnp") return "vbic13";

    // Semiconductor resistor: .model <name> R ... (ngspice). Maps to the
    // ngspice-flavour sp_resistor master (spice/resistor.osdi), which supports
    // tc1/tc2/tnom and instance-level r/w/l — unlike the generic 'resistor'
    // master (r/noisy only). Physical resistor subcircuits (e.g. Sky130
    // sky130_fd_pr__res_*) reference these model cards from R instances.
    if (mt == "r" || mt == "res") return "sp_resistor";

    Simulator::err() << "WARNING: SPICE model_type '" << model_type_raw
                     << "' has no known VACASK OSDI master (model skipped)\n";
    return "";
}

// Project one SPICE `.model` card into a PTModel and add it to `into`.
// Maps model_type+level to an OSDI master (spiceModelMaster), passes model
// params (minus dispatch-only `level`), and injects type polarity where the
// master needs it. Skips (with a warning already emitted) when there is no
// known master. Shared by spiceBlockToTables and fillSpiceSubDef so model
// cards defined inside a .subckt are emitted too, not only top-level ones.
static void addSpiceModelCard(const netlist::SpiceModel& m,
                              PTSubcircuitDefinition& into, Parser& p) {
    std::string mt_raw = sv(m.model_type);
    std::string master = spiceModelMaster(mt_raw, sv(m.level), "");
    if (master.empty()) return; // warning already emitted by spiceModelMaster

    PTModel mod(Id(sv(m.name).c_str()), Id(master.c_str()));

    auto ps = paramStringExcluding(m.params, {"level"});
    if (!ps.empty()) mod.add(p.parseParameters(ps));

    std::string mt = mt_raw;
    std::transform(mt.begin(), mt.end(), mt.begin(), ::tolower);
    if      (mt == "nmos") mod.add(p.parseParameters("type=1"));
    else if (mt == "pmos") mod.add(p.parseParameters("type=-1"));
    else if (mt == "npn")  mod.add(p.parseParameters("type=1"));
    else if (mt == "pnp")  mod.add(p.parseParameters("type=-1"));

    into.add(std::move(mod));
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
static bool fillSpiceSubDef(PTSubcircuitDefinition& def, const netlist::SpiceSubckt& s,
                            Parser& p, std::set<std::string>& addedModels, Status& st);

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
            // Disambiguate the positional token (value-vs-model), mirroring
            // Cadnip's sema.jl: a trailing bare token is a MODEL name (not a
            // resistance) when the instance also carries an explicit r=/l=
            // param — e.g. `rend1 r0 t1 reshead r={rhead}`. Without this the
            // token would be emitted as r=<token> and collide with the
            // explicit r= param ("Parameter 'r' redefinition"). The Rust
            // projection always leaves `model` empty for R (no model slot in
            // the AST), so the decision is made here from value + params.
            std::string master;
            std::string rval;
            if (!mdl.empty()) {
                master = mdl;   // explicit model reference (future-proofing)
                rval = val;
            } else if (!val.empty() && spiceParamsHaveAny(dev.params, {"r", "l"})) {
                master = val;   // value is a model card name
            } else {
                // Plain resistor: value (if any) is the resistance. Default to
                // the ngspice sp_resistor master so instance params like
                // tc1/tc2/w/l are accepted (the generic 'resistor' has r only).
                master = "sp_resistor";
                ensureSpiceModel(into, "sp_resistor", addedModels);
                rval = val;
            }
            PTInstance inst(Id(name.c_str()), Id(master.c_str()), nodeList(dev.nodes));
            if (!rval.empty()) inst.add(p.parseParameters("r=" + rval));
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
        case netlist::SpiceDeviceKind::Diode: {
            // D<name> <pos> <neg> <model> [area=…] [<params>]
            // Instance master = model card name; nodes = [anode, cathode]
            // (diode.osdi module diode(A,C) — positional terminals)
            if (mdl.empty()) {
                Simulator::err() << "WARNING: Diode '" << name
                                 << "' has no model reference (skipped)\n";
                break;
            }
            PTInstance inst(Id(name.c_str()), Id(mdl.c_str()), nodeList(dev.nodes));
            // Note: Rust Diode projects value="" (area not a named OSDI param in diode.va).
            auto ps = paramString(dev.params);
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Mosfet: {
            // M<name> <drain> <gate> <source> <bulk> <model> [W=… L=… …]
            // Instance master = model card name; nodes = [d,g,s,b]
            // (bsim3 module bsim3(d,g,s,b) — positional terminals)
            if (mdl.empty()) {
                Simulator::err() << "WARNING: MOSFET '" << name
                                 << "' has no model reference (skipped)\n";
                break;
            }
            PTInstance inst(Id(name.c_str()), Id(mdl.c_str()), nodeList(dev.nodes));
            auto ps = paramString(dev.params);
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Bjt: {
            // Q<name> <c> <b> <e> [<s>] <model> [params]
            // Instance master = model card name; nodes = [c,b,e,(s)]
            // (vbic13 module vbic13(c,b,e) — 3 terminals; 4-terminal variant
            //  vbic13_4t if a substrate node is present — deferred)
            if (mdl.empty()) {
                Simulator::err() << "WARNING: BJT '" << name
                                 << "' has no model reference (skipped)\n";
                break;
            }
            PTInstance inst(Id(name.c_str()), Id(mdl.c_str()), nodeList(dev.nodes));
            auto ps = paramString(dev.params);
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::SubcktCall: {
            // X<name> <node1> ... <subckt_master> [param=val ...]
            // dev.model holds the subckt master name; dev.nodes are the connections.
            if (mdl.empty()) {
                Simulator::err() << "WARNING: SubcktCall '" << name
                                 << "' has no master subcircuit name (skipped)\n";
                break;
            }
            PTInstance inst(Id(name.c_str()), Id(mdl.c_str()), nodeList(dev.nodes));
            auto ps = paramString(dev.params);
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Vcvs: {
            // E<name> pos neg ctrl_pos ctrl_neg gain
            // VACASK vcvs: terminals [p, n, cp, cn] (4 connection + 1 internal flow),
            // instance param: gain (Real).
            // ctrl_nodes = [cp, cn], ctrl_value = gain (string).
            if (dev.nodes.size() < 2 || dev.ctrl_nodes.size() < 2) {
                Simulator::err() << "WARNING: Vcvs '" << name
                                 << "' has too few nodes/ctrl_nodes (skipped)\n";
                break;
            }
            ensureSpiceModel(into, "vcvs", addedModels);
            PTIdentifierList allNodes;
            for (const auto& n : dev.nodes)       allNodes.push_back(PTParsedIdentifier(sv(n).c_str()));
            for (const auto& cn : dev.ctrl_nodes) allNodes.push_back(PTParsedIdentifier(sv(cn).c_str()));
            PTInstance inst(Id(name.c_str()), Id("vcvs"), std::move(allNodes));
            std::string gainVal = sv(dev.ctrl_value);
            if (!gainVal.empty()) inst.add(p.parseParameters("gain=" + gainVal));
            auto ps = paramString(dev.params);
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Vccs: {
            // G<name> pos neg ctrl_pos ctrl_neg transconductance
            // VACASK vccs: terminals [p, n, cp, cn] (4 connection), param: gain (Real).
            if (dev.nodes.size() < 2 || dev.ctrl_nodes.size() < 2) {
                Simulator::err() << "WARNING: Vccs '" << name
                                 << "' has too few nodes/ctrl_nodes (skipped)\n";
                break;
            }
            ensureSpiceModel(into, "vccs", addedModels);
            PTIdentifierList allNodes;
            for (const auto& n : dev.nodes)       allNodes.push_back(PTParsedIdentifier(sv(n).c_str()));
            for (const auto& cn : dev.ctrl_nodes) allNodes.push_back(PTParsedIdentifier(sv(cn).c_str()));
            PTInstance inst(Id(name.c_str()), Id("vccs"), std::move(allNodes));
            std::string gainVal = sv(dev.ctrl_value);
            if (!gainVal.empty()) inst.add(p.parseParameters("gain=" + gainVal));
            auto ps = paramString(dev.params);
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Cccs: {
            // F<name> pos neg <ctrl_vsource_name> gain
            // VACASK cccs: terminals [p, n] (2 connection), params: gain, ctlinst, ctlnode.
            // ctrl_nodes[0] = controlling vsource instance name; ctrl_value = gain.
            // ctlnode defaults to "flow(br)" in VACASK (matches vsource's internal flow node).
            if (dev.nodes.size() < 2) {
                Simulator::err() << "WARNING: Cccs '" << name
                                 << "' has too few connection nodes (skipped)\n";
                break;
            }
            if (dev.ctrl_nodes.empty()) {
                Simulator::err() << "WARNING: Cccs '" << name
                                 << "' has no controlling source reference (skipped)\n";
                break;
            }
            ensureSpiceModel(into, "cccs", addedModels);
            PTInstance inst(Id(name.c_str()), Id("cccs"), nodeList(dev.nodes));
            std::string ctlsrc  = sv(dev.ctrl_nodes[0]);
            std::string gainVal = sv(dev.ctrl_value);
            // ctlinst is an Id param — must be quoted as a string literal in the expression.
            // ctlnode defaults to "flow(br)" in VACASK; no need to set it explicitly.
            std::string prms = "ctlinst=\"" + ctlsrc + "\"";
            if (!gainVal.empty()) prms += " gain=" + gainVal;
            inst.add(p.parseParameters(prms));
            auto ps = paramString(dev.params);
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Ccvs: {
            // H<name> pos neg <ctrl_vsource_name> transresistance
            // VACASK ccvs: terminals [p, n] (2 connection + 1 internal flow), params: gain, ctlinst, ctlnode.
            // ctrl_nodes[0] = controlling vsource instance name; ctrl_value = transresistance.
            // ctlnode defaults to "flow(br)" in VACASK (matches vsource's internal flow node).
            if (dev.nodes.size() < 2) {
                Simulator::err() << "WARNING: Ccvs '" << name
                                 << "' has too few connection nodes (skipped)\n";
                break;
            }
            if (dev.ctrl_nodes.empty()) {
                Simulator::err() << "WARNING: Ccvs '" << name
                                 << "' has no controlling source reference (skipped)\n";
                break;
            }
            ensureSpiceModel(into, "ccvs", addedModels);
            PTInstance inst(Id(name.c_str()), Id("ccvs"), nodeList(dev.nodes));
            std::string ctlsrc  = sv(dev.ctrl_nodes[0]);
            std::string gainVal = sv(dev.ctrl_value);
            // ctlinst is an Id param — must be quoted as a string literal in the expression.
            std::string prms = "ctlinst=\"" + ctlsrc + "\"";
            if (!gainVal.empty()) prms += " gain=" + gainVal;
            inst.add(p.parseParameters(prms));
            auto ps = paramString(dev.params);
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Jfet:
            Simulator::err() << "WARNING: SPICE device '" << name
                             << "' (Jfet) has no VACASK equivalent; skipped\n";
            break;
        case netlist::SpiceDeviceKind::MutualInductor:
            Simulator::err() << "WARNING: SPICE device '" << name
                             << "' (MutualInductor) has no VACASK equivalent; skipped\n";
            break;
        case netlist::SpiceDeviceKind::Behavioral:
            Simulator::err() << "WARNING: SPICE device '" << name
                             << "' (Behavioral/B-source) has no VACASK equivalent; skipped\n";
            break;
        case netlist::SpiceDeviceKind::Switch:
            Simulator::err() << "WARNING: SPICE device '" << name
                             << "' (Switch) has no VACASK equivalent; skipped\n";
            break;
        case netlist::SpiceDeviceKind::Osdi:
            Simulator::err() << "WARNING: SPICE device '" << name
                             << "' (Osdi) has no SPICE-dialect adapter; skipped\n";
            break;
        default:
            Simulator::err() << "WARNING: SPICE device '" << name
                             << "' has unsupported kind (skipped in this adapter version)\n";
            break;
    }
    return true;
}

static bool fillSpiceSubDef(PTSubcircuitDefinition& def, const netlist::SpiceSubckt& s,
                            Parser& p, std::set<std::string>& addedModels, Status& st) {
    auto sp = paramString(s.params);
    if (!sp.empty()) def.add(p.parseParameters(sp));
    // .model cards inside the .subckt body (e.g. Sky130 res subckts define
    // reshead/resbody locally). Emit BEFORE devices so instances resolve them.
    for (const auto& m : s.models) addSpiceModelCard(m, def, p);
    for (const auto& dev : s.devices) {
        if (!addSpiceDevice(dev, def, p, addedModels, st)) return false;
    }
    for (const auto& sub : s.subckts) {
        PTSubcircuitDefinition child(Id(sv(sub.name).c_str()), nodeList(sub.ports));
        if (!fillSpiceSubDef(child, sub, p, addedModels, st)) return false;
        def.add(std::move(child));
    }
    return true;
}

// Forward declaration of mergeNetlist for spiceBlockToTables to call (Fix 2).
bool mergeNetlist(const netlist::Netlist& nl, PTSubcircuitDefinition& top,
                  ParserTables& tab, Parser& p,
                  const std::filesystem::path& baseDir,
                  std::set<std::filesystem::path>& visited,
                  std::set<std::string>& addedModels,
                  Status& s);

// Map one SpiceBlock into a PTSubcircuitDefinition.  `addedModels` is shared
// across repeated calls so self-alias model cards are emitted only once.
// `baseDir` and `visited` thread through for .include resolution (Fix 2).
static bool spiceBlockToTables(const netlist::SpiceBlock& sb, PTSubcircuitDefinition& into,
                               ParserTables& tab, Parser& p,
                               std::set<std::string>& addedModels, Status& s,
                               const std::filesystem::path& baseDir,
                               std::set<std::filesystem::path>& visited) {
    // Top-level .param declarations from the SPICE block.
    auto sp = paramString(sb.params);
    if (!sp.empty()) into.add(p.parseParameters(sp));

    // .model cards: emit PTModel BEFORE instances (VACASK resolves by name).
    for (const auto& m : sb.models) addSpiceModelCard(m, into, p);

    // Devices (R/C/L/V/I + D/M/Q now handled; others warn+skip).
    for (const auto& dev : sb.devices) {
        if (!addSpiceDevice(dev, into, p, addedModels, s)) return false;
    }

    // TODO: SPICE .tran / .dc / .ac analysis cards inside a SPICE block are not
    // yet projected into PTAnalysis commands here.  The driver (e.g.
    // demo_netlistrs.cpp) configures the analysis directly; consistent with the
    // Spectre path where analyses are already skipped at the top-level merge.

    // Nested .subckt definitions.
    for (const auto& sub : sb.subckts) {
        PTSubcircuitDefinition child(Id(sv(sub.name).c_str()), nodeList(sub.ports));
        if (!fillSpiceSubDef(child, sub, p, addedModels, s)) return false;
        into.add(std::move(child));
    }

    // .include / .lib inside a SPICE block.
    for (const auto& inc : sb.includes) {
        std::filesystem::path incPath = baseDir / sv(inc.path);
        std::filesystem::path absPath;
        try {
            absPath = std::filesystem::canonical(incPath);
        } catch (...) {
            absPath = std::filesystem::absolute(incPath);
        }

        if (visited.count(absPath)) continue;
        visited.insert(absPath);

        std::ifstream ifs(absPath);
        if (!ifs) {
            s.set(Status::NotFound, "cannot open SPICE include: " + absPath.string());
            return false;
        }
        std::stringstream incss; incss << ifs.rdbuf();
        std::string contents = incss.str();

        netlist::Netlist sub;
        if (!inc.section.empty()) {
            sub = netlist::parse_netlist_lib(rust::Str(contents), rust::Str(sv(inc.section)));
        } else {
            std::string iext = absPath.extension().string();
            std::transform(iext.begin(), iext.end(), iext.begin(), ::tolower);
            bool spice = (iext != ".scs");
            sub = netlist::parse_netlist(rust::Str(contents), spice);
        }
        if (!sub.errors.empty()) {
            std::ostringstream os;
            os << "parse error in SPICE include '" << absPath.string() << "': "
               << sub.errors.size() << " error(s)"
               << " (first at bytes [" << sub.errors[0].start << ", "
               << sub.errors[0].end << "))";
            s.set(Status::Syntax, os.str());
            return false;
        }

        if (!mergeNetlist(sub, into, tab, p, absPath.parent_path(), visited, addedModels, s))
            return false;
    }
    return true;
}

// (private) Accumulate one parsed Netlist's toplevel members into `top`,
// its analyses/globals into `tab`, then recurse into its top-level includes
// and SPICE block includes.
// Section-qualified includes (section != "") are skipped (deferred: flat Netlist
// projection does not carry library sections).
// Returns true on success; false with `s` set on any error (open failure, parse
// error, or error in a nested include).
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
        if (!fillSubDef(child, sub, p, tab, addedModels, s, baseDir, visited)) return false;
        top.add(std::move(child));
    }

    // SPICE blocks (R/C/L/V/I adapter + nested .subckt/.model + .include resolution).
    for (const auto& sb : nl.spice_blocks) {
        if (!spiceBlockToTables(sb, top, tab, p, addedModels, s, baseDir, visited)) return false;
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

    // Recurse into top-level includes.
    for (const auto& inc : nl.includes) {
        std::filesystem::path incPath = baseDir / sv(inc.path);
        std::filesystem::path absPath;
        try {
            absPath = std::filesystem::canonical(incPath);
        } catch (...) {
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

        netlist::Netlist sub;
        if (!inc.section.empty()) {
            sub = netlist::parse_netlist_lib(rust::Str(contents), rust::Str(sv(inc.section)));
        } else {
            std::string iext = absPath.extension().string();
            std::transform(iext.begin(), iext.end(), iext.begin(), ::tolower);
            bool spice = (iext != ".scs");
            sub = netlist::parse_netlist(rust::Str(contents), spice);
        }
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
