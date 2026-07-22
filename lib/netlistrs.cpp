#include "netlistrs.h"
#include "netlist_cxx_bridge/lib.h"
#include "simulator.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <optional>
#include <cstdlib>
#include <cctype>

namespace sim {

namespace {

// Convert rust::String to std::string.
std::string sv(const rust::String& s) { return std::string(s); }

// SPICE is case-insensitive, but VACASK interns every identifier into a
// case-sensitive Id (exact strcmp) keyed in unordered_map<Id,…>. To make
// SPICE-origin names bind, canonicalize them to lowercase here — the netlistrs
// adapter is the one place that knows the content came from a SPICE block.
// This matches VACASK's own OSDI loader (which lowercases device/parameter/
// terminal storage keys) and its lowercase builtin functions (agauss, gauss,
// sin, …). Spectre-origin names (makeInstance/makeModel/fillSubDef) are left
// verbatim: Spectre is case-sensitive by contract.
//
// Whole-string lowercasing of a param/expression string is safe because SPICE
// is case-insensitive throughout — no identifier loses meaning, and PDK model/
// param expressions carry no case-significant string literals. Filesystem
// include paths are NOT run through this (they stay case-sensitive).
//
// Caveat: builtin *constants* (M_PI, P_Q, …) are registered uppercase, so a
// SPICE expression referencing one by name would not resolve after lowercasing.
// Not observed in PDK expressions; add lowercase aliases in context.cpp if it
// ever occurs.
std::string lc(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Lowercased Id / node list for SPICE-origin identifiers.
Id spiceId(const std::string& s) { return Id(lc(s).c_str()); }

// Strip ngspice expression quoting (`{expr}` braces, `'expr'` quotes) and
// collapse SPICE continuation markers (`\n+`) from parameter values.
//
// Double quotes are stripped only *inside* `{ }` (expression context): some
// ngspice model cards wrap an expression in both, e.g. Sky130's
// `dw = {"-sw_activecd-nfom_dw/2"}`, and the inner quotes would otherwise reach
// VACASK as a string literal for a real-valued OSDI parameter ("cannot convert
// string into real"). Quotes *outside* braces are kept: they mark genuine
// string parameters such as `type="pulse"` or a BSIM `version="4.8.3"`.
std::string stripExprQuoting(const std::string& v) {
    std::string out;
    out.reserve(v.size());
    int braceDepth = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        char c = v[i];
        if (c == '{') { ++braceDepth; continue; }
        if (c == '}') { if (braceDepth > 0) --braceDepth; continue; }
        if (c == '\'') continue;
        if (c == '"' && braceDepth > 0) continue;
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

// SPICE-origin node list: lowercased (see lc above).
PTIdentifierList spiceNodeList(const rust::Vec<rust::String>& nodes) {
    PTIdentifierList terms;
    for (const auto& n : nodes) terms.push_back(PTParsedIdentifier(lc(sv(n)).c_str()));
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
                               std::set<std::filesystem::path>& visited,
                               bool projectAnalyses = true);

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

    // Diode: d -> generic diode (diode.osdi, module diode(A,C)) when no level
    // is given. ngspice diodes with an explicit level (1/3) — e.g. Sky130
    // sky130_fd_pr__diode_* models — need the fuller ngspice sp_diode master
    // (spice/diode.osdi), which has js/jsw/cj/cjsw/tlevc/gap/… params the
    // generic diode lacks.
    if (mt == "d") {
        int dlevel = 0;
        try { dlevel = std::stoi(level_str); } catch (...) {}
        return (dlevel > 0) ? "sp_diode" : "diode";
    }

    // MOSFET: nmos/pmos by level
    if (mt == "nmos" || mt == "pmos") {
        int level = 0;
        try { level = std::stoi(level_str); } catch (...) {}

        // Dispatch table: level -> OSDI master name
        // (uses filenames without .osdi suffix; VA module names are the master)
        // bsim3v3.osdi        -> module bsim3      (bsim3v3 3.3, accepts level 49-53)
        // spice/bsim4v8.osdi  -> module sp_bsim4v8 (arpad ngspice-flavored BSIM4 4.8,
        //                        level 54 — same spice/ family as sp_resistor/sp_diode;
        //                        this is the variant the Sky130 VACASK port uses)
        // bsimbulk106.osdi    -> module bsimbulk   (has thermal port; 5 terminals)
        // psp103v4.osdi       -> module psp103va   (accepts level 103)
        // Default (levels 1/2/3/49/53 or unknown) -> bsim3
        if (level == 54)               return "sp_bsim4v8";
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

    // Semiconductor capacitor: .model <name> C ... (ngspice). Maps to the
    // ngspice-flavour sp_capacitor master (spice/capacitor.osdi), which supports
    // cox/capsw (aliases of cj/cjsw), w/l, tc1/tc2, tnom — unlike the generic
    // 'capacitor' master (c only). Sky130 MiM/junction cap subcircuits reference
    // these model cards from C instances.
    if (mt == "c" || mt == "cap") return "sp_capacitor";

    Simulator::err() << "WARNING: SPICE model_type '" << model_type_raw
                     << "' has no known VACASK OSDI master (model skipped)\n";
    return "";
}

// Build a "name=value …" param string excluding a set of keys (case-insensitive).
static std::string paramStringExcludingSet(const rust::Vec<netlist::Param>& params,
                                           const std::set<std::string>& exclude) {
    std::ostringstream os;
    bool first = true;
    for (const auto& p : params) {
        std::string key = sv(p.name);
        std::string keylower = key;
        std::transform(keylower.begin(), keylower.end(), keylower.begin(), ::tolower);
        if (exclude.count(keylower)) continue;
        if (!first) os << " ";
        os << key << "=" << stripExprQuoting(sv(p.value));
        first = false;
    }
    return os.str();
}

// Value of the first parameter whose name matches `key` (case-insensitive),
// brace/quote-stripped; "" if absent.
static std::string spiceParamValue(const rust::Vec<netlist::Param>& params,
                                   const std::string& key) {
    for (const auto& p : params) {
        std::string k = sv(p.name);
        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
        if (k == key) return stripExprQuoting(sv(p.value));
    }
    return "";
}

// Build a PTModel from a SPICE `.model` card. Returns nullopt if there is no
// known OSDI master (warning already emitted). `nameOverride` (if non-empty)
// replaces the card name — used to collapse binned cards to their base name.
// `extraExclude` names are dropped from the emitted params in addition to the
// dispatch-only `level` — used to strip binning bounds lmin/lmax/wmin/wmax.
static std::optional<PTModel> buildSpiceModelCard(const netlist::SpiceModel& m,
                                                  const std::string& nameOverride,
                                                  const std::set<std::string>& extraExclude,
                                                  Parser& p) {
    std::string mt_raw = sv(m.model_type);
    std::string master = spiceModelMaster(mt_raw, sv(m.level), "");
    if (master.empty()) return std::nullopt;

    std::string modelName = nameOverride.empty() ? sv(m.name) : nameOverride;
    PTModel mod(spiceId(modelName), Id(master.c_str()));

    std::set<std::string> excl = extraExclude;
    excl.insert("level");
    // sp_bsim4v8 declares `version` as a string parameter (e.g. "4.8.3").
    // Sky130 model cards write `version=4.5` (unquoted real), which would cause
    // a type-mismatch error at elaboration.  Strip it so the OSDI default ("4.8.3")
    // is used; the version selector affects only minor equation variants.
    if (master == "sp_bsim4v8") excl.insert("version");
    // ngspice R/C model cards name the parameter-measurement temperature `tref`;
    // the distilled sp_resistor/sp_capacitor masters expose it as `tnom`. Drop
    // `tref` from the verbatim params and re-append it under the master's name.
    bool renameTref = (master == "sp_resistor" || master == "sp_capacitor");
    std::string trefVal;
    if (renameTref) {
        trefVal = spiceParamValue(m.params, "tref");
        excl.insert("tref");
    }
    auto ps = paramStringExcludingSet(m.params, excl);
    // sp_diode has a real `level` model param (junction-cap selector); re-append.
    if (master == "sp_diode") {
        std::string lvl = sv(m.level);
        if (!lvl.empty()) ps += (ps.empty() ? "" : " ") + std::string("level=") + lvl;
    }
    if (renameTref && !trefVal.empty())
        ps += (ps.empty() ? "" : " ") + std::string("tnom=") + trefVal;
    if (!ps.empty()) mod.add(p.parseParameters(lc(ps)));

    std::string mt = mt_raw;
    std::transform(mt.begin(), mt.end(), mt.begin(), ::tolower);
    if      (mt == "nmos") mod.add(p.parseParameters("type=1"));
    else if (mt == "pmos") mod.add(p.parseParameters("type=-1"));
    else if (mt == "npn")  mod.add(p.parseParameters("type=1"));
    else if (mt == "pnp")  mod.add(p.parseParameters("type=-1"));

    return mod;
}

// Project one SPICE `.model` card into a PTModel and add it to `into`.
static void addSpiceModelCard(const netlist::SpiceModel& m,
                              PTSubcircuitDefinition& into, Parser& p) {
    auto mod = buildSpiceModelCard(m, "", {}, p);
    if (mod) into.add(std::move(*mod));
}

// True if the card carries all four numeric binning bounds.
static bool isBinnedModel(const netlist::SpiceModel& m) {
    return !spiceParamValue(m.params, "lmin").empty()
        && !spiceParamValue(m.params, "lmax").empty()
        && !spiceParamValue(m.params, "wmin").empty()
        && !spiceParamValue(m.params, "wmax").empty();
}

// Strip a trailing ".N" or "_N" bin suffix. "nshort_model.7" -> "nshort_model".
static std::string binBaseName(const std::string& name) {
    auto pos = name.find_last_of("._");
    if (pos != std::string::npos && pos + 1 < name.size()) {
        bool digits = true;
        for (size_t i = pos + 1; i < name.size(); ++i)
            if (!std::isdigit(static_cast<unsigned char>(name[i]))) { digits = false; break; }
        if (digits) return name.substr(0, pos);
    }
    return name;
}

// Emit one bin group as an @if/@elseif PTBlockSequence. Each branch defines a
// model under `baseName`, guarded by that bin's scaled L/W range. Bins are
// sorted by (lmin, wmin); first matching branch wins. No @else fallback.
static void emitBinnedModelGroup(std::vector<const netlist::SpiceModel*>& bins,
                                 const std::string& baseName,
                                 PTSubcircuitDefinition& into, Parser& p) {
    std::sort(bins.begin(), bins.end(),
              [](const netlist::SpiceModel* a, const netlist::SpiceModel* b) {
        double la = std::atof(spiceParamValue(a->params, "lmin").c_str());
        double lb = std::atof(spiceParamValue(b->params, "lmin").c_str());
        if (la != lb) return la < lb;
        double wa = std::atof(spiceParamValue(a->params, "wmin").c_str());
        double wb = std::atof(spiceParamValue(b->params, "wmin").c_str());
        return wa < wb;
    });

    PTBlockSequence seq;
    bool any = false;
    for (const auto* m : bins) {
        auto mod = buildSpiceModelCard(*m, baseName, {"lmin", "lmax", "wmin", "wmax"}, p);
        if (!mod) continue; // no master; warning already emitted
        std::string guard =
            "l*$scale >= "  + spiceParamValue(m->params, "lmin") +
            " && l*$scale < " + spiceParamValue(m->params, "lmax") +
            " && w*$scale >= " + spiceParamValue(m->params, "wmin") +
            " && w*$scale < " + spiceParamValue(m->params, "wmax");
        PTBlock block;
        block.add(std::move(*mod));
        seq.add(p.parseExpression(guard), std::move(block));
        any = true;
    }
    if (any) into.add(std::move(seq));
}

// Emit all `.model` cards for a block. Non-binned cards emit unchanged; binned
// cards are grouped by base name (first-seen order) into @if chains, emitted
// after the non-binned cards but before any instances (caller ordering).
static void emitSpiceModels(const rust::Vec<netlist::SpiceModel>& models,
                            PTSubcircuitDefinition& into, Parser& p) {
    std::vector<std::string> order;
    std::map<std::string, std::vector<const netlist::SpiceModel*>> groups;
    for (const auto& m : models) {
        if (isBinnedModel(m)) {
            std::string base = binBaseName(sv(m.name));
            if (!groups.count(base)) order.push_back(base);
            groups[base].push_back(&m);
        } else {
            addSpiceModelCard(m, into, p);
        }
    }
    for (const auto& base : order) emitBinnedModelGroup(groups[base], base, into, p);
}

// --- OSDI auto-load ---------------------------------------------------------
// SPICE-flavored master name -> OSDI module file. Builtins (vsource/isource and
// the controlled-source masters vcvs/vccs/cccs/ccvs) are intentionally absent:
// they are built into VACASK and need no `load`. Subcircuit-call masters are
// also absent (they resolve to subckt definitions, not OSDI modules).
static const std::map<std::string, std::string>& osdiFileForMaster() {
    static const std::map<std::string, std::string> t = {
        {"resistor",   "resistor.osdi"},       {"sp_resistor", "spice/resistor.osdi"},
        {"capacitor",  "capacitor.osdi"},      {"sp_capacitor","spice/capacitor.osdi"},
        {"inductor",    "inductor.osdi"},
        {"diode",      "diode.osdi"},          {"sp_diode",    "spice/diode.osdi"},
        {"sp_bsim4v8", "spice/bsim4v8.osdi"},  {"bsim3",       "bsim3v3.osdi"},
        {"bsim4",      "bsim4v8.osdi"},        {"vbic13",      "vbic_1p3.osdi"},
        {"bsimbulk",   "bsimbulk106.osdi"},    {"psp103va",    "psp103v4.osdi"},
    };
    return t;
}

// Collect every model/instance master referenced in a block, recursing into
// @if/@elseif block sequences (where binned models live).
static void collectMasters(const PTBlock& b, std::set<std::string>& out) {
    for (const auto& m : b.models())    out.insert(std::string(m.device()));
    for (const auto& i : b.instances()) out.insert(std::string(i.masterName()));
    if (b.hasBlockSequences()) {
        for (const auto& seq : b.blockSequences())
            for (const auto& e : seq.entries())
                collectMasters(std::get<2>(e), out);
    }
}

// Collect masters across a subcircuit definition and all nested definitions.
static void collectMastersDef(const PTSubcircuitDefinition& d, std::set<std::string>& out) {
    collectMasters(d.root(), out);
    for (const auto& sd : d.subDefs()) collectMastersDef(*sd, out);
}

// Emit a toplevel `load "<file>.osdi"` for each OSDI master referenced by `def`,
// de-duplicated against loads already present in `tab`. Replaces callers'
// previously hardcoded PTLoad lists: including a PDK auto-pulls exactly the OSDI
// modules its devices need.
static void emitOsdiLoads(ParserTables& tab, const PTSubcircuitDefinition& def) {
    std::set<std::string> masters;
    collectMastersDef(def, masters);
    std::set<std::string> have;
    for (const auto& ld : tab.loads()) have.insert(ld.file());
    for (const auto& m : masters) {
        auto it = osdiFileForMaster().find(m);
        if (it == osdiFileForMaster().end()) continue;
        if (have.insert(it->second).second) tab.add(PTLoad(it->second));
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
    // SPICE-origin identifiers/expressions → lowercase (see lc/spiceId above).
    std::string name = lc(sv(dev.name));
    std::string val  = lc(sv(dev.value));
    std::string mdl  = lc(sv(dev.model));

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
            PTInstance inst(Id(name.c_str()), Id(master.c_str()), spiceNodeList(dev.nodes));
            if (!rval.empty()) inst.add(p.parseParameters("r=" + rval));
            auto ps = lc(paramString(dev.params));
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Capacitor: {
            std::string master = mdl.empty() ? "capacitor" : mdl;
            ensureSpiceModel(into, "capacitor", addedModels);
            PTInstance inst(Id(name.c_str()), Id(master.c_str()), spiceNodeList(dev.nodes));
            if (!val.empty()) inst.add(p.parseParameters("c=" + val));
            auto ps = lc(paramString(dev.params));
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Inductor: {
            std::string master = mdl.empty() ? "inductor" : mdl;
            ensureSpiceModel(into, "inductor", addedModels);
            PTInstance inst(Id(name.c_str()), Id(master.c_str()), spiceNodeList(dev.nodes));
            if (!val.empty()) inst.add(p.parseParameters("l=" + val));
            auto ps = lc(paramString(dev.params));
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::VSource: {
            ensureSpiceModel(into, "vsource", addedModels);
            PTInstance inst(Id(name.c_str()), Id("vsource"), spiceNodeList(dev.nodes));
            std::string srcParams = spiceSourceParams(dev.source);
            auto ps = lc(paramString(dev.params));
            if (!srcParams.empty() && !ps.empty()) srcParams += " ";
            srcParams += ps;
            if (!srcParams.empty()) inst.add(p.parseParameters(lc(srcParams)));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::ISource: {
            ensureSpiceModel(into, "isource", addedModels);
            PTInstance inst(Id(name.c_str()), Id("isource"), spiceNodeList(dev.nodes));
            std::string srcParams = spiceSourceParams(dev.source);
            auto ps = lc(paramString(dev.params));
            if (!srcParams.empty() && !ps.empty()) srcParams += " ";
            srcParams += ps;
            if (!srcParams.empty()) inst.add(p.parseParameters(lc(srcParams)));
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
            PTInstance inst(Id(name.c_str()), Id(mdl.c_str()), spiceNodeList(dev.nodes));
            // Note: Rust Diode projects value="" (area not a named OSDI param in diode.va).
            auto ps = lc(paramString(dev.params));
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
            PTInstance inst(Id(name.c_str()), Id(mdl.c_str()), spiceNodeList(dev.nodes));
            auto ps = lc(paramString(dev.params));
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
            PTInstance inst(Id(name.c_str()), Id(mdl.c_str()), spiceNodeList(dev.nodes));
            auto ps = lc(paramString(dev.params));
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
            PTInstance inst(Id(name.c_str()), Id(mdl.c_str()), spiceNodeList(dev.nodes));
            auto ps = lc(paramString(dev.params));
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
            for (const auto& n : dev.nodes)       allNodes.push_back(PTParsedIdentifier(lc(sv(n)).c_str()));
            for (const auto& cn : dev.ctrl_nodes) allNodes.push_back(PTParsedIdentifier(lc(sv(cn)).c_str()));
            PTInstance inst(Id(name.c_str()), Id("vcvs"), std::move(allNodes));
            std::string gainVal = lc(sv(dev.ctrl_value));
            if (!gainVal.empty()) inst.add(p.parseParameters("gain=" + gainVal));
            auto ps = lc(paramString(dev.params));
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
            for (const auto& n : dev.nodes)       allNodes.push_back(PTParsedIdentifier(lc(sv(n)).c_str()));
            for (const auto& cn : dev.ctrl_nodes) allNodes.push_back(PTParsedIdentifier(lc(sv(cn)).c_str()));
            PTInstance inst(Id(name.c_str()), Id("vccs"), std::move(allNodes));
            std::string gainVal = lc(sv(dev.ctrl_value));
            if (!gainVal.empty()) inst.add(p.parseParameters("gain=" + gainVal));
            auto ps = lc(paramString(dev.params));
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
            PTInstance inst(Id(name.c_str()), Id("cccs"), spiceNodeList(dev.nodes));
            std::string ctlsrc  = lc(sv(dev.ctrl_nodes[0]));
            std::string gainVal = lc(sv(dev.ctrl_value));
            // ctlinst is an Id param — must be quoted as a string literal in the expression.
            // ctlnode defaults to "flow(br)" in VACASK; no need to set it explicitly.
            std::string prms = "ctlinst=\"" + ctlsrc + "\"";
            if (!gainVal.empty()) prms += " gain=" + gainVal;
            inst.add(p.parseParameters(prms));
            auto ps = lc(paramString(dev.params));
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
            PTInstance inst(Id(name.c_str()), Id("ccvs"), spiceNodeList(dev.nodes));
            std::string ctlsrc  = lc(sv(dev.ctrl_nodes[0]));
            std::string gainVal = lc(sv(dev.ctrl_value));
            // ctlinst is an Id param — must be quoted as a string literal in the expression.
            std::string prms = "ctlinst=\"" + ctlsrc + "\"";
            if (!gainVal.empty()) prms += " gain=" + gainVal;
            inst.add(p.parseParameters(prms));
            auto ps = lc(paramString(dev.params));
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
    if (!sp.empty()) def.add(p.parseParameters(lc(sp)));
    // .model cards inside the .subckt body (e.g. Sky130 res subckts define
    // reshead/resbody locally). Emit BEFORE devices so instances resolve them.
    emitSpiceModels(s.models, def, p);
    for (const auto& dev : s.devices) {
        if (!addSpiceDevice(dev, def, p, addedModels, st)) return false;
    }
    for (const auto& sub : s.subckts) {
        PTSubcircuitDefinition child(spiceId(sv(sub.name)), spiceNodeList(sub.ports));
        if (!fillSpiceSubDef(child, sub, p, addedModels, st)) return false;
        def.add(std::move(child));
    }
    return true;
}

// Forward declaration of mergeNetlist for spiceBlockToTables to call (Fix 2).
// `projectAnalyses` = false suppresses (and warns about) analysis/command
// projection — used for foreign-format includes, whose commands are ignored.
bool mergeNetlist(const netlist::Netlist& nl, PTSubcircuitDefinition& top,
                  ParserTables& tab, Parser& p,
                  const std::filesystem::path& baseDir,
                  std::set<std::filesystem::path>& visited,
                  std::set<std::string>& addedModels,
                  Status& s, bool projectAnalyses = true);

// Map one SpiceBlock into a PTSubcircuitDefinition.  `addedModels` is shared
// across repeated calls so self-alias model cards are emitted only once.
// `baseDir` and `visited` thread through for .include resolution (Fix 2).
static bool spiceBlockToTables(const netlist::SpiceBlock& sb, PTSubcircuitDefinition& into,
                               ParserTables& tab, Parser& p,
                               std::set<std::string>& addedModels, Status& s,
                               const std::filesystem::path& baseDir,
                               std::set<std::filesystem::path>& visited,
                               bool projectAnalyses) {
    // Top-level .param declarations from the SPICE block.
    auto sp = paramString(sb.params);
    if (!sp.empty()) into.add(p.parseParameters(lc(sp)));

    // .model cards: emit PTModel BEFORE instances (VACASK resolves by name).
    emitSpiceModels(sb.models, into, p);

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
        PTSubcircuitDefinition child(spiceId(sv(sub.name)), spiceNodeList(sub.ports));
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

        if (!mergeNetlist(sub, into, tab, p, absPath.parent_path(), visited, addedModels, s,
                          projectAnalyses))
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
                  Status& s, bool projectAnalyses) {
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
        if (!spiceBlockToTables(sb, top, tab, p, addedModels, s, baseDir, visited, projectAnalyses))
            return false;
    }

    // Globals and analyses into tab.
    for (const auto& g : nl.globals) tab.addGlobal(PTParsedIdentifier(sv(g).c_str()));
    if (projectAnalyses) {
        for (const auto& a : nl.analyses) {
            PTAnalysis desc(Id(sv(a.name).c_str()), Id(sv(a.analysis_type).c_str()));
            auto ps = paramString(a.params);
            if (!ps.empty()) desc.add(p.parseParameters(ps));
            tab.addCommand(std::move(desc));
        }
    } else if (!nl.analyses.empty()) {
        Simulator::err() << "WARNING: netlistrs adapter ignoring " << nl.analyses.size()
                         << " analysis command(s) from an included foreign-format file"
                         << " (write analyses in the native VACASK deck)\n";
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

        if (!mergeNetlist(sub, top, tab, p, absPath.parent_path(), visited, addedModels, s,
                          projectAnalyses))
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
    emitOsdiLoads(tab, top);
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
    emitOsdiLoads(tab, top);
    tab.setDefaultSubDef(std::move(top));
    return true;
}

// Parse a foreign-format netlist FILE and merge its models/subckts/devices into
// the caller-provided `top` subcircuit definition (the native parser's in-progress
// toplevel def). Analysis/command directives are ignored (with a warning); OSDI
// loads for referenced masters are auto-emitted into `tab`. `section` (non-empty)
// selects a `.lib` section. Extension picks the dialect (.scs/.spectre = Spectre,
// else SPICE). Does NOT touch defaultGround()/setDefaultSubDef() — that stays the
// grammar's responsibility.
bool mergeForeignFile(const std::string& path, const std::string& section,
                      PTSubcircuitDefinition& top, ParserTables& tab,
                      Parser& p, Status& s) {
    namespace fs = std::filesystem;
    fs::path fp(path);
    std::string ext = fp.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    bool spice = (ext != ".scs" && ext != ".spectre");

    std::ifstream in(path);
    if (!in) { s.set(Status::NotFound, "cannot open foreign include: " + path); return false; }
    std::stringstream ss; ss << in.rdbuf();
    std::string source = ss.str();

    netlist::Netlist nl = section.empty()
        ? netlist::parse_netlist(rust::Str(source), spice)
        : netlist::parse_netlist_lib(rust::Str(source), rust::Str(section));
    if (!nl.errors.empty()) {
        std::ostringstream os;
        os << "netlist parse error(s) in '" << path << "': " << nl.errors.size()
           << " (first at bytes [" << nl.errors[0].start << ", " << nl.errors[0].end << "))";
        s.set(Status::Syntax, os.str());
        return false;
    }

    fs::path absPath;
    try { absPath = fs::canonical(fp); } catch (...) { absPath = fs::absolute(fp); }
    std::set<fs::path> visited{ absPath };
    std::set<std::string> addedModels;
    if (!mergeNetlist(nl, top, tab, p, absPath.parent_path(), visited, addedModels, s,
                      /*projectAnalyses=*/false))
        return false;
    emitOsdiLoads(tab, top);
    return true;
}

}
