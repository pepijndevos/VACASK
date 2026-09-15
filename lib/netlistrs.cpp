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
#include <vector>
#include <cstdlib>
#include <cctype>

namespace sim {

namespace {

std::string toString(const rust::String& s) { return std::string(s); }

// VACASK identifiers are case-sensitive, so canonicalize SPICE-origin names
// and expressions here. Spectre names and filesystem paths remain verbatim.
std::string lowercase(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

Id spiceId(const std::string& s) { return Id(lowercase(s).c_str()); }

// ngspice gives models and subcircuits separate namespaces; VACASK does not.
// Prefix only `.model` names and references to prevent collisions.
std::string spiceModelName(const std::string& s) { return "m_" + lowercase(s); }
Id spiceModelId(const std::string& s) { return Id(spiceModelName(s).c_str()); }

// Preserve double-quoted string parameters, but remove double quotes inside
// expression braces because VACASK would otherwise parse a real as a string.
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

// Rewrite ngspice expression semantics that differ from VACASK. In behavioral
// expressions pwr(x,y) preserves the sign of x; in parameters and models it
// uses abs(x). Probe arguments name nodes or instances and remain verbatim.
std::string spiceExpr(const std::string& in, bool behavioral) {
    auto identChar = [](unsigned char c) {
        return std::isalnum(c) || c == '_' || c == '$';
    };
    std::string out;
    out.reserve(in.size() + 8);
    // One frame per open '(' in the input. A pwr() frame emits none of its own
    // text while its arguments are scanned; the whole call is rewritten in
    // place once the closing ')' arrives and both arguments are known.
    struct Frame {
        bool        access;    // a v(...)/i(...) access, whose argument list names nodes
        bool        pwr;       // a pwr(...) call awaiting rewrite
        std::string name;      // the pwr call's original spelling, to restore on a bad arity
        size_t      argStart;  // index in `out` where this call's argument text begins
        size_t      comma;     // index in `out` of its only top-level ',', npos if none
        int         commas;    // how many top-level ',' have been seen
    };
    std::vector<Frame> frames;
    int accessDepth = 0;             // number of access frames currently open
    size_t i = 0;
    while (i < in.size()) {
        unsigned char c = in[i];
        // Identifier (a leading digit means we are inside a numeric literal;
        // let it fall through to the verbatim copy below).
        if (identChar(c) && !std::isdigit(c)) {
            size_t j = i;
            while (j < in.size() && identChar(static_cast<unsigned char>(in[j]))) ++j;
            std::string ident = in.substr(i, j - i);
            std::string lower = lowercase(ident);
            size_t k = j;
            while (k < in.size() && std::isspace(static_cast<unsigned char>(in[k]))) ++k;
            bool isCall = (k < in.size() && in[k] == '(');
            bool isName = !isCall && accessDepth == 0;   // a variable, not a node/function
            // A pwr() call outside an access argument list; inside one the
            // identifier names a node, so it is copied like any other.
            bool isPwr = isCall && accessDepth == 0 && lower == "pwr";
            if (isName && lower == "temper") {
                out += "$temp";
            } else if (isName && behavioral && lower == "time") {
                out += "$abstime";
            } else if (!isPwr) {
                out += ident;
            }
            if (!isCall) { i = j; continue; }
            bool access = (lower == "v" || lower == "i");
            if (!isPwr) {
                out.append(in, j, k - j);   // whitespace between name and '('
                out += '(';
            }
            frames.push_back({access, isPwr, ident, out.size(), std::string::npos, 0});
            if (access) ++accessDepth;
            i = k + 1;
            continue;
        }
        if (c == '(') {
            frames.push_back({false, false, {}, out.size(), std::string::npos, 0});
            out += '('; ++i; continue;
        }
        if (c == ',' && !frames.empty() && frames.back().pwr) {
            Frame& f = frames.back();
            if (f.commas == 0) f.comma = out.size();
            ++f.commas;
            out += ','; ++i; continue;
        }
        if (c == ')') {
            if (!frames.empty()) {
                Frame f = frames.back();
                frames.pop_back();
                if (f.access) --accessDepth;
                if (f.pwr) {
                    // The arguments are now in out[f.argStart..], already
                    // rewritten themselves (a nested pwr() closed first, and
                    // only ever rewrote text past f.comma).
                    if (f.commas == 1) {
                        std::string x = out.substr(f.argStart, f.comma - f.argStart);
                        std::string y = out.substr(f.comma + 1);
                        out.resize(f.argStart);
                        if (behavioral) {
                            out += "(sgn(" + x + ")*pow(abs(" + x + ")," + y + "))";
                        } else {
                            out += "pow(abs(" + x + ")," + y + ")";
                        }
                    } else {
                        // Not the 2-argument pwr() ngspice defines: put the
                        // call back verbatim and let the parser complain.
                        std::string args = out.substr(f.argStart);
                        out.resize(f.argStart);
                        out += f.name + "(" + args + ")";
                    }
                    ++i; continue;
                }
            }
            out += ')'; ++i; continue;
        }
        if (behavioral && c == '^') { out += "**"; ++i; continue; }
        out += static_cast<char>(c);
        ++i;
    }
    return out;
}

// Behavioral pwr() has different semantics, so callers must select that rewrite
// before the call is consumed.
std::string spiceValue(const rust::String& v, bool behavioral = false) {
    return spiceExpr(stripExprQuoting(toString(v)), behavioral);
}

std::string paramString(const rust::Vec<netlist::Param>& params, bool spiceValues = false) {
    std::ostringstream os;
    bool first = true;
    for (const auto& p : params) {
        if (!first) os << " ";
        os << toString(p.name) << "="
           << (spiceValues ? spiceValue(p.value) : stripExprQuoting(toString(p.value)));
        first = false;
    }
    return os.str();
}

PTIdentifierList nodeList(const rust::Vec<rust::String>& nodes) {
    PTIdentifierList terms;
    for (const auto& n : nodes) terms.push_back(PTParsedIdentifier(toString(n).c_str()));
    return terms;
}

PTIdentifierList spiceNodeList(const rust::Vec<rust::String>& nodes) {
    PTIdentifierList terms;
    for (const auto& n : nodes) terms.push_back(PTParsedIdentifier(lowercase(toString(n)).c_str()));
    return terms;
}

PTInstance makeInstance(const netlist::Instance& i, Parser& p) {
    PTInstance inst(Id(toString(i.name).c_str()), Id(toString(i.master).c_str()), nodeList(i.nodes));
    auto ps = paramString(i.params);
    if (!ps.empty()) inst.add(p.parseParameters(ps));
    return inst;
}

PTModel makeModel(const netlist::Model& m, Parser& p) {
    PTModel mod(Id(toString(m.name).c_str()), Id(toString(m.master).c_str()));
    auto ps = paramString(m.params);
    if (!ps.empty()) mod.add(p.parseParameters(ps));
    return mod;
}

PTBlockSequence makeConditional(const netlist::Conditional& c, Parser& p) {
    PTBlockSequence seq;
    for (const auto& cl : c.clauses) {
        PTBlock block;
        block.add(makeInstance(cl.instance, p));
        // An empty condition is the trailing else clause.
        std::string cond = cl.condition.empty() ? std::string("1") : toString(cl.condition);
        seq.add(p.parseExpression(cond), std::move(block));
    }
    return seq;
}

using IncludeKey = std::pair<std::filesystem::path, std::string>;
using IncludeSet = std::set<IncludeKey>;

struct SpiceBin {
    std::string modelName;
    std::string lmin;
    std::string lmax;
    std::string wmin;
    std::string wmax;
};
using BinnedModels = std::map<std::string, std::vector<SpiceBin>>;
using BehavioralParameterValues = std::map<std::string, std::string>;
using BehavioralResistorModels = std::map<std::string, BehavioralParameterValues>;

static IncludeKey includeKey(const std::filesystem::path& path,
                             const rust::String& section) {
    return {path, lowercase(toString(section))};
}

static bool spiceBlockToTables(const netlist::SpiceBlock& sb, PTSubcircuitDefinition& into,
                               ParserTables& tab, Parser& p,
                               Status& s,
                               const std::filesystem::path& baseDir,
                               IncludeSet& visited,
                               BinnedModels& visibleBins,
                               BehavioralResistorModels& resistorModels,
                               bool projectAnalyses = true,
                               const std::string& language = "");

static bool fillSubDef(PTSubcircuitDefinition& def, const netlist::Subckt& s, Parser& p,
                       ParserTables& tab, Status& st,
                        const std::filesystem::path& baseDir,
                        IncludeSet& visited,
                        const BinnedModels& inheritedBins,
                        const BehavioralResistorModels& inheritedResistorModels) {
    BinnedModels visibleBins = inheritedBins;
    BehavioralResistorModels resistorModels = inheritedResistorModels;
    auto sp = paramString(s.params);
    if (!sp.empty()) def.add(p.parseParameters(sp));
    for (const auto& m : s.models)       def.add(makeModel(m, p));
    for (const auto& i : s.instances)    def.add(makeInstance(i, p));
    for (const auto& c : s.conditionals) def.add(makeConditional(c, p));
    for (const auto& sub : s.subckts) {
        PTSubcircuitDefinition child(Id(toString(sub.name).c_str()), nodeList(sub.ports));
        if (!fillSubDef(child, sub, p, tab, st, baseDir, visited, visibleBins,
                        resistorModels)) return false;
        def.add(std::move(child));
    }
    for (const auto& sb : s.spice_blocks) {
        if (!spiceBlockToTables(sb, def, tab, p, st, baseDir, visited, visibleBins,
                                resistorModels)) return false;
    }
    return true;
}

static std::string paramStringExcluding(const rust::Vec<netlist::Param>& params,
                                        const std::initializer_list<std::string>& exclude) {
    std::ostringstream os;
    bool first = true;
    for (const auto& p : params) {
        std::string key = toString(p.name);
        std::string keylower = key;
        std::transform(keylower.begin(), keylower.end(), keylower.begin(), ::tolower);
        bool skip = false;
        for (const auto& ex : exclude) { if (keylower == ex) { skip = true; break; } }
        if (skip) continue;
        if (!first) os << " ";
        os << key << "=" << spiceValue(p.value);
        first = false;
    }
    return os.str();
}

static bool spiceParamsHaveAny(const rust::Vec<netlist::Param>& params,
                               const std::initializer_list<std::string>& keys) {
    for (const auto& p : params) {
        std::string key = toString(p.name);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        for (const auto& k : keys) if (key == k) return true;
    }
    return false;
}

// `temper` always means simulation temperature in translated expressions, so a
// same-named user parameter cannot affect them.
static void warnIfTemperDeclared(const rust::Vec<netlist::Param>& params,
                                 const std::string& where) {
    if (!spiceParamsHaveAny(params, {"temper"})) return;
    Simulator::err() << "WARNING: " << where << " declares a parameter named 'temper', which"
                     << " is ngspice's simulation temperature; expressions referencing it are"
                     << " translated to $temp, so the declaration has no effect\n";
}

// SPICE subcircuit `m` is forwarded as VACASK's parallel `$mfactor`. Declare it
// unconditionally because includes are resolved before all call sites are known.
static constexpr const char* kMfactorParam = "$mfactor";

static std::string spiceModelMaster(const std::string& model_type_raw,
                                    const std::string& level_str,
                                    const std::string& version) {
    std::string mt = model_type_raw;
    std::transform(mt.begin(), mt.end(), mt.begin(), ::tolower);

    // SPICE diode parameters require sp_diode; `level` remains a model parameter.
    if (mt == "d") return "sp_diode";

    if (mt == "nmos" || mt == "pmos") {
        int level = 0;
        try { level = std::stoi(level_str); } catch (...) {}

        // An explicit BSIM4 version selects the ngspice-compatible master.
        if (level == 54)               return version.empty() ? "bsim4" : "sp_bsim4v8";
        if (level == 70 || level == 72) {
            Simulator::err() << "WARNING: MOSFET level=" << level
                             << " (bsimbulk): has thermal port; connect substrate to 0 or add explicit bulk node\n";
            return "bsimbulk";
        }
        if (level == 103)              return "psp103va";
        if (level != 0 && level != 1 && level != 2 && level != 3 &&
            level != 49 && level != 53) {
            Simulator::err() << "WARNING: MOSFET level=" << level
                             << " not in known dispatch table; falling back to bsim3\n";
        }
        return "bsim3";
    }

    // An absent BJT level means Gummel-Poon; levels 4 and 9 select VBIC.
    if (mt == "npn" || mt == "pnp") {
        int level = 0;
        try { level = std::stoi(level_str); } catch (...) {}
        if (level == 4 || level == 9) return "vbic13";
        if (level == 8) {
            Simulator::err() << "WARNING: BJT level=8 (HICUM2) has no VACASK master; "
                                "falling back to Gummel-Poon sp_bjt\n";
        } else if (level != 0 && level != 1 && level != 2) {
            Simulator::err() << "WARNING: BJT level=" << level
                             << " not in known dispatch table; falling back to sp_bjt\n";
        }
        return "sp_bjt";
    }

    // SPICE semiconductor models need parameters absent from generic masters.
    if (mt == "r" || mt == "res") return "sp_resistor";

    if (mt == "c" || mt == "cap") return "sp_capacitor";

    Simulator::err() << "WARNING: SPICE model_type '" << model_type_raw
                     << "' has no known VACASK OSDI master (model skipped)\n";
    return "";
}

static std::string paramStringExcludingSet(const rust::Vec<netlist::Param>& params,
                                           const std::set<std::string>& exclude) {
    std::ostringstream os;
    bool first = true;
    for (const auto& p : params) {
        std::string key = toString(p.name);
        std::string keylower = key;
        std::transform(keylower.begin(), keylower.end(), keylower.begin(), ::tolower);
        if (exclude.count(keylower)) continue;
        if (!first) os << " ";
        os << key << "=" << spiceValue(p.value);
        first = false;
    }
    return os.str();
}

static std::string spiceParamValue(const rust::Vec<netlist::Param>& params,
                                   const std::string& key, bool behavioral = false) {
    for (const auto& p : params) {
        std::string k = toString(p.name);
        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
        if (k == key) return spiceValue(p.value, behavioral);
    }
    return "";
}

// Quote supported revisions for the string-valued OSDI parameter.
static std::string spiceBsim4Version(const std::string& value) {
    std::string v = value;
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
        v = v.substr(1, v.size() - 2);
    if (v == "4.8" || v == "4.80" || v == "4.8.0" ||
        v == "4.81" || v == "4.8.1" || v == "4.82" ||
        v == "4.8.2" || v == "4.83" || v == "4.8.3")
        return "\"" + v + "\"";
    return "";
}

static bool isSky130Bsim4Version(const std::string& value) {
    std::string v = value;
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
        v = v.substr(1, v.size() - 2);
    return v == "4.5" || v == "4.62";
}

// Combine the enclosing subcircuit multiplier with the device's own `m=`.
static std::string spiceMfactorExpr(const std::string& inherited,
                                    const rust::Vec<netlist::Param>& params) {
    std::string own = lowercase(spiceParamValue(params, "m"));
    if (own.empty())       return inherited;
    if (inherited.empty()) return own;
    return "(" + inherited + ")*(" + own + ")";
}

static std::optional<PTModel> buildSpiceModelCard(const netlist::SpiceModel& m,
                                                  const std::string& nameOverride,
                                                  const std::set<std::string>& extraExclude,
                                                  Parser& p) {
    std::string mt_raw = toString(m.model_type);
    std::string version = spiceParamValue(m.params, "version");
    std::string master = spiceModelMaster(mt_raw, toString(m.level), version);
    if (master.empty()) return std::nullopt;

    std::string modelName = nameOverride.empty() ? toString(m.name) : nameOverride;
    PTModel mod(spiceModelId(modelName), Id(master.c_str()));

    std::set<std::string> excl = extraExclude;
    excl.insert("level");
    // sp_bsim4v8's version parameter is a string although ngspice cards usually
    // leave the selector unquoted. Re-append supported revisions with the right
    // type; unsupported Sky130 revisions retain the existing 4.8.3 default.
    std::string bsim4Version;
    std::string bsim4Rgeomod;
    if (master == "sp_bsim4v8") {
        excl.insert("version");
        bsim4Version = spiceBsim4Version(version);
        // ngspice permits this instance selector as a model-card default. The
        // distilled OSDI module disambiguates it from model parameters.
        bsim4Rgeomod = spiceParamValue(m.params, "rgeomod");
        excl.insert("rgeomod");
        if (bsim4Version.empty() && !isSky130Bsim4Version(version)) {
            Simulator::err() << "WARNING: BSIM4 version '" << version
                             << "' is not supported by sp_bsim4v8; falling back to 4.8.3\n";
        }
    }
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
        std::string lvl = toString(m.level);
        if (!lvl.empty()) ps += (ps.empty() ? "" : " ") + std::string("level=") + lvl;
    }
    if (!bsim4Version.empty())
        ps += (ps.empty() ? "" : " ") + std::string("version=") + bsim4Version;
    if (!bsim4Rgeomod.empty())
        ps += (ps.empty() ? "" : " ") + std::string("instance_rgeomod=") + bsim4Rgeomod;
    if (renameTref && !trefVal.empty())
        ps += (ps.empty() ? "" : " ") + std::string("tnom=") + trefVal;
    if (!ps.empty()) mod.add(p.parseParameters(lowercase(ps)));

    std::string mt = mt_raw;
    std::transform(mt.begin(), mt.end(), mt.begin(), ::tolower);
    if      (mt == "nmos") mod.add(p.parseParameters("type=1"));
    else if (mt == "pmos") mod.add(p.parseParameters("type=-1"));
    else if (mt == "npn")  mod.add(p.parseParameters("type=1"));
    else if (mt == "pnp")  mod.add(p.parseParameters("type=-1"));

    return mod;
}

static void addSpiceModelCard(const netlist::SpiceModel& m,
                              PTSubcircuitDefinition& into, Parser& p) {
    auto mod = buildSpiceModelCard(m, "", {}, p);
    if (mod) into.add(std::move(*mod));
}

static BehavioralParameterValues behavioralResistorModelParams(const netlist::SpiceModel& model);

// ngspice recognizes a bin only when the model name has a numeric suffix.
static std::optional<std::string> binBaseName(const std::string& name) {
    auto pos = name.find_last_of('.');
    if (pos == std::string::npos || pos + 1 >= name.size()) return std::nullopt;
    for (size_t i = pos + 1; i < name.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(name[i]))) return std::nullopt;
    return name.substr(0, pos);
}

// A bin also requires all four geometry bounds.
static bool isBinnedModel(const netlist::SpiceModel& m) {
    return binBaseName(toString(m.name)).has_value()
        && !spiceParamValue(m.params, "lmin").empty()
        && !spiceParamValue(m.params, "lmax").empty()
        && !spiceParamValue(m.params, "wmin").empty()
        && !spiceParamValue(m.params, "wmax").empty();
}

static std::string spiceBinRange(const std::string& value,
                                 const std::string& lower,
                                 const std::string& upper) {
    // Match ngspice's 1e-15 m lower-bound equality tolerance. Geometry
    // expressions such as 0.22*1e-6 can round below the literal 2.2e-7.
    // Keep the upper bound strict so an exact shared edge selects the next bin.
    return "((" + value + " >= (" + lower + ") || abs(" + value + " - (" + lower +
           ")) < 1e-15) && " + value + " < (" + upper + "))";
}

static void emitBinnedModelGroup(std::vector<const netlist::SpiceModel*>& bins,
                                 const std::string& baseName,
                                 PTSubcircuitDefinition& into, Parser& p,
                                 BinnedModels& visibleBins) {
    std::sort(bins.begin(), bins.end(),
              [](const netlist::SpiceModel* a, const netlist::SpiceModel* b) {
        double la = std::atof(spiceParamValue(a->params, "lmin").c_str());
        double lb = std::atof(spiceParamValue(b->params, "lmin").c_str());
        if (la != lb) return la < lb;
        double wa = std::atof(spiceParamValue(a->params, "wmin").c_str());
        double wb = std::atof(spiceParamValue(b->params, "wmin").c_str());
        return wa < wb;
    });

    std::vector<SpiceBin> emitted;
    for (const auto* m : bins) {
        std::string internalName = lowercase(toString(m->name));
        auto mod = buildSpiceModelCard(*m, internalName,
                                       {"lmin", "lmax", "wmin", "wmax"}, p);
        if (!mod) continue; // no master; warning already emitted
        into.add(std::move(*mod));
        emitted.push_back({internalName,
                           spiceParamValue(m->params, "lmin"),
                           spiceParamValue(m->params, "lmax"),
                           spiceParamValue(m->params, "wmin"),
                           spiceParamValue(m->params, "wmax")});
    }
    if (!emitted.empty()) visibleBins[lowercase(baseName)] = std::move(emitted);
}

static void emitSpiceModels(const rust::Vec<netlist::SpiceModel>& models,
                            PTSubcircuitDefinition& into, Parser& p,
                            BinnedModels& visibleBins,
                            BehavioralResistorModels& resistorModels) {
    std::vector<std::string> order;
    std::map<std::string, std::vector<const netlist::SpiceModel*>> groups;
    for (const auto& m : models) {
        std::string modelType = lowercase(toString(m.model_type));
        if (modelType == "r" || modelType == "res") {
            resistorModels[lowercase(toString(m.name))] = behavioralResistorModelParams(m);
        }
        auto base = binBaseName(toString(m.name));
        if (isBinnedModel(m)) {
            std::string key = lowercase(*base);
            if (!groups.count(key)) order.push_back(key);
            groups[key].push_back(&m);
        } else {
            visibleBins.erase(lowercase(toString(m.name)));
            addSpiceModelCard(m, into, p);
        }
    }
    for (const auto& base : order) {
        emitBinnedModelGroup(groups[base], base, into, p, visibleBins);
    }
}

// Builtins and subcircuit masters require no OSDI load entry.
static const std::map<std::string, std::string>& osdiFileForMaster() {
    static const std::map<std::string, std::string> t = {
        {"resistor",   "resistor.osdi"},       {"sp_resistor", "spice/resistor.osdi"},
        {"capacitor",  "capacitor.osdi"},      {"sp_capacitor","spice/capacitor.osdi"},
        {"inductor",    "inductor.osdi"},
        {"diode",      "diode.osdi"},          {"sp_diode",    "spice/diode.osdi"},
        {"sp_bsim4v8", "spice/bsim4v8.osdi"},  {"bsim3",       "bsim3v3.osdi"},
        {"bsim4",      "bsim4v8.osdi"},        {"vbic13",      "vbic_1p3.osdi"},
        {"sp_bjt",     "spice/bjt.osdi"},
        {"bsimbulk",   "bsimbulk106.osdi"},    {"psp103va",    "psp103v4.osdi"},
    };
    return t;
}

static void collectMasters(const PTBlock& b, std::set<std::string>& out) {
    for (const auto& m : b.models())    out.insert(std::string(m.device()));
    for (const auto& i : b.instances()) out.insert(std::string(i.masterName()));
    if (b.hasBlockSequences()) {
        for (const auto& seq : b.blockSequences())
            for (const auto& e : seq.entries())
                collectMasters(std::get<2>(e), out);
    }
}

static void collectMastersDef(const PTSubcircuitDefinition& d, std::set<std::string>& out) {
    collectMasters(d.root(), out);
    for (const auto& sd : d.subDefs()) collectMastersDef(*sd, out);
}

// Load each referenced OSDI master once.
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

    auto expr = [](const rust::String& value) {
        return spiceValue(value);
    };

    if (!toString(src.dc).empty())        add("dc=" + expr(src.dc));
    if (!toString(src.ac_mag).empty())    add("mag=" + expr(src.ac_mag));
    if (!toString(src.ac_phase).empty())  add("phase=" + expr(src.ac_phase));

    std::string tk = toString(src.tran_kind);
    if (!tk.empty()) {
        std::string tklower = tk;
        std::transform(tklower.begin(), tklower.end(), tklower.begin(), ::tolower);
        std::string vaKind = (tklower == "sin") ? "sine" : tklower;
        add("type=\"" + vaKind + "\"");

        const auto& args = src.tran_args;
        if (vaKind == "pulse") {
            static const char* names[] = {
                "val0", "val1", "delay", "rise", "fall", "width", "period"
            };
            for (size_t i = 0; i < args.size() && i < 7; ++i) {
                std::string v = expr(args[i]);
                if (!v.empty()) add(std::string(names[i]) + "=" + v);
            }
        } else if (vaKind == "sine") {
            static const char* names[] = {
                "sinedc", "ampl", "freq", "delay", "theta"
            };
            for (size_t i = 0; i < args.size() && i < 5; ++i) {
                std::string v = expr(args[i]);
                if (!v.empty()) add(std::string(names[i]) + "=" + v);
            }
        } else if (vaKind == "pwl") {
            if (!args.empty()) {
                if (!first) os << " ";
                os << "wave=[";
                for (size_t i = 0; i < args.size(); ++i) {
                    if (i > 0) os << " ";
                    os << expr(args[i]);
                }
                os << "]";
                first = false;
            }
        } else if (vaKind == "exp") {
            static const char* names[] = {
                "val0", "val1", "delay", "tau1", "td2", "tau2"
            };
            for (size_t i = 0; i < args.size() && i < 6; ++i) {
                std::string v = expr(args[i]);
                if (!v.empty()) add(std::string(names[i]) + "=" + v);
            }
        }
    }
    return os.str();
}

// Probes require per-iteration evaluation rather than elaboration-time parsing.
static bool spiceExprHasProbe(const std::string& expr) {
    auto identChar = [](unsigned char c) {
        return std::isalnum(c) || c == '_' || c == '$';
    };
    size_t i = 0;
    while (i < expr.size()) {
        unsigned char c = expr[i];
        if (!identChar(c)) { ++i; continue; }
        size_t j = i;
        while (j < expr.size() && identChar(static_cast<unsigned char>(expr[j]))) ++j;
        if (!std::isdigit(c)) {
            std::string ident = expr.substr(i, j - i);
            size_t k = j;
            while (k < expr.size() && std::isspace(static_cast<unsigned char>(expr[k]))) ++k;
            if (k < expr.size() && expr[k] == '(' && (ident == "v" || ident == "i")) return true;
        }
        i = j;
    }
    return false;
}

static bool addSpiceBehavioral(const std::string& name, PTIdentifierList&& terms,
                               const std::string& ngspiceExpr, bool currentSource,
                               const char* what, PTSubcircuitDefinition& into,
                               Parser& p, Status& s) {
    Rpn expr;
    try {
        expr = p.parseExpression(spiceExpr(ngspiceExpr, /*behavioral=*/true));
    } catch (const std::exception&) {
        // parseExpression() already set `s` before throwing.
        s.extend(std::string("  in ") + what + " '" + name + "'.");
        return false;
    }
    PTBehavioral behavioral(Id(name.c_str()), std::move(terms));
    if (currentSource) behavioral.setCurrent(std::move(expr));
    else               behavioral.setVoltage(std::move(expr));
    into.add(std::move(behavioral));
    return true;
}

static constexpr const char* kBehavioralResistorDeclarations = R"(
parameter real w=10e-6;
parameter real l=10e-6;
parameter real temp=27;
parameter real dtemp=0;
parameter real kf=0;
parameter real af=1;
parameter real ef=1;
parameter real short=0;
parameter real narrow=0;
parameter real lf=1;
parameter real wf=1;
parameter integer noise=1;
parameter real mfactor=1;
real devTemp;
real noiseArea;
real cpscale=$simparam("scale", 1);
real r;
real i;
)";

static constexpr const char* kBehavioralResistorEvaluation = R"(
if ($param_given(temp)) devTemp=temp+`P_CELSIUS0;
else devTemp=$temperature+dtemp;
if ($param_given(w) || $param_given(l))
  noiseArea=pow(l*cpscale-2*short, lf)*pow(w*cpscale-2*narrow, wf);
else
  noiseArea=1;
r=#expr#;
if (r>=0 && r<1e-12) r=1e-12;
else if (r<0 && r>-1e-12) r=-1e-12;
i=V(br)/r;
I(br)<+mfactor*i;
if (noise) begin
  I(br)<+white_noise(mfactor*4*`P_K*devTemp/r, "thermal");
  I(br)<+flicker_noise(mfactor*((i>=0) ? 1 : -1)*kf*pow(abs(i), af)/noiseArea, ef, "flicker");
end
)";

static BehavioralParameterValues behavioralResistorModelParams(const netlist::SpiceModel& model) {
    static const std::map<std::string, std::string> supported = {
        {"af", "af"}, {"kf", "kf"}, {"ef", "ef"},
        {"lf", "lf"}, {"wf", "wf"}, {"short", "short"},
        {"dlr", "short"}, {"narrow", "narrow"}, {"dw", "narrow"},
        {"noise", "noise"}, {"noisy", "noise"},
    };
    BehavioralParameterValues values;
    for (const auto& param : model.params) {
        auto it = supported.find(lowercase(toString(param.name)));
        if (it == supported.end()) continue;
        values[it->second] = lowercase(spiceValue(param.value));
    }
    return values;
}

static BehavioralParameterValues behavioralResistorInstanceParams(const netlist::SpiceDevice& dev) {
    static const std::map<std::string, std::string> supported = {
        {"w", "w"}, {"l", "l"}, {"temp", "temp"}, {"dtemp", "dtemp"},
        {"noise", "noise"}, {"noisy", "noise"},
    };
    BehavioralParameterValues values;
    for (const auto& param : dev.params) {
        auto it = supported.find(lowercase(toString(param.name)));
        if (it == supported.end()) continue;
        values[it->second] = lowercase(spiceValue(param.value));
    }
    return values;
}

static bool addSpiceBehavioralResistor(const std::string& name, PTIdentifierList&& terms,
                                       const std::string& resistance,
                                       const BehavioralParameterValues& modelParams,
                                       const BehavioralParameterValues& instanceParams,
                                       const std::string& mfactor,
                                       PTSubcircuitDefinition& into,
                                       Parser& p, Status& s) {
    Rpn expr;
    try {
        expr = p.parseExpression(spiceExpr(resistance, /*behavioral=*/true));
    } catch (const std::exception&) {
        s.extend("  in behavioral resistor '" + name + "'.");
        return false;
    }

    PTBehavioral behavioral(Id(name.c_str()), std::move(terms));
    behavioral.setExpression(std::move(expr));
    behavioral.setUserDeclarations(std::string(kBehavioralResistorDeclarations));
    behavioral.setUserEvaluation(std::string(kBehavioralResistorEvaluation));
    BehavioralParameterValues values = modelParams;
    for (const auto& value : instanceParams) values[value.first] = value.second;
    if (!mfactor.empty()) values["mfactor"] = mfactor;
    std::ostringstream params;
    for (const auto& value : values) params << value.first << "=" << value.second << " ";
    if (!values.empty()) behavioral.add(p.parseParameters(params.str()));
    into.add(std::move(behavioral));
    return true;
}

static bool fillSpiceSubDef(PTSubcircuitDefinition& def, const netlist::SpiceSubckt& s,
                            Parser& p, Status& st, const BinnedModels& inheritedBins,
                            const BehavioralResistorModels& inheritedResistorModels);

static void ensureSpiceModel(PTSubcircuitDefinition& into, const std::string& master) {
    for (const auto& model : into.root().models()) {
        if (std::string(model.name()) == master) return;
    }
    into.add(PTModel(Id(master.c_str()), Id(master.c_str())));
}

static bool hasSpiceModel(const PTSubcircuitDefinition& into, const std::string& name) {
    for (const auto& model : into.root().models()) {
        if (std::string(model.name()) == name) return true;
    }
    return false;
}

static bool addSpiceDevice(const netlist::SpiceDevice& dev, PTSubcircuitDefinition& into,
                           Parser& p, Status& s, const std::string& mfactorIn,
                           const BinnedModels& visibleBins,
                           const BehavioralResistorModels& resistorModels) {
    std::string name = lowercase(toString(dev.name));
    std::string val  = lowercase(spiceValue(dev.value));
    std::string mdl  = lowercase(toString(dev.model));

    std::string mfac = spiceMfactorExpr(mfactorIn, dev.params);

    // Remove ngspice's `m`, which is not a VACASK master parameter, and append
    // $mfactor in its place.
    auto paramsWithMfactor = [&]() {
        std::string ps = lowercase(paramStringExcluding(dev.params, {"m"}));
        if (!mfac.empty()) {
            if (!ps.empty()) ps += " ";
            ps += std::string(kMfactorParam) + "=" + mfac;
        }
        return ps;
    };

    // Potential-imposing devices do not scale when replicated in parallel.
    auto paramsWithoutMfactor = [&](const char* what) {
        if (!spiceParamValue(dev.params, "m").empty()) {
            Simulator::err() << "WARNING: " << what << " '" << name
                             << "' does not take a multiplier; m= ignored\n";
        }
        return lowercase(paramStringExcluding(dev.params, {"m"}));
    };

    switch (dev.kind) {
        case netlist::SpiceDeviceKind::Resistor: {
            // A positional token alongside r=/l= names a model, not resistance.
            bool valIsModel = mdl.empty() && !val.empty() &&
                              spiceParamsHaveAny(dev.params, {"r", "l"});
            std::string rval = valIsModel ? "" : val;

            // A resistance containing a probe must be evaluated per iteration.
            // Use a behavioral conductance with sp_resistor's 1e-12 floor.
            bool rFromParam = rval.empty();
            std::string rexpr = rFromParam ? lowercase(spiceParamValue(dev.params, "r")) : rval;
            if (spiceExprHasProbe(rexpr)) {
                // Re-read because behavioral pwr() has different semantics.
                rexpr = rFromParam ? lowercase(spiceParamValue(dev.params, "r", /*behavioral=*/true))
                                   : lowercase(spiceValue(dev.value, /*behavioral=*/true));
                if (dev.nodes.size() != 2) {
                    Simulator::err() << "WARNING: behavioral resistor '" << name
                                     << "' needs exactly 2 nodes (skipped)\n";
                    break;
                }
                std::string modelName = mdl.empty() ? (valIsModel ? val : "") : mdl;
                BehavioralParameterValues modelParams;
                if (!modelName.empty()) {
                    auto model = resistorModels.find(modelName);
                    if (model != resistorModels.end()) modelParams = model->second;
                    else Simulator::err() << "WARNING: behavioral resistor '" << name
                                          << "' references unknown model '" << modelName << "'\n";
                }
                std::string dropped;
                for (const auto& prm : dev.params) {
                    std::string key = lowercase(toString(prm.name));
                    if (key == "r" || key == "m" || key == "w" || key == "l" ||
                        key == "temp" || key == "dtemp" || key == "noise" || key == "noisy")
                        continue;
                    if (!dropped.empty()) dropped += ", ";
                    dropped += key;
                }
                if (!dropped.empty()) {
                    Simulator::err() << "WARNING: behavioral resistor '" << name
                                     << "' has no place for " << dropped << "; ignored\n";
                }
                if (!addSpiceBehavioralResistor(
                        name, spiceNodeList(dev.nodes), rexpr, modelParams,
                        behavioralResistorInstanceParams(dev), mfac, into, p, s)) {
                    return false;
                }
                break;
            }

            std::string master;
            if (!mdl.empty()) {
                master = spiceModelName(mdl);
            } else if (valIsModel) {
                master = spiceModelName(val);
            } else {
                // The SPICE master accepts instance parameters absent from `resistor`.
                master = "sp_resistor";
                ensureSpiceModel(into, "sp_resistor");
            }
            PTInstance inst(Id(name.c_str()), Id(master.c_str()), spiceNodeList(dev.nodes));
            if (!rval.empty()) inst.add(p.parseParameters("r=" + rval));
            auto ps = paramsWithMfactor();
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Capacitor: {
            std::string master;
            std::string cval;
            if (!mdl.empty()) {
                master = spiceModelName(mdl);
                cval = val;
            } else if (!val.empty() &&
                       (hasSpiceModel(into, spiceModelName(val)) ||
                        spiceParamsHaveAny(dev.params, {"c", "w", "l"}))) {
                master = spiceModelName(val);
            } else {
                master = "sp_capacitor";
                ensureSpiceModel(into, "sp_capacitor");
                cval = val;
            }
            PTInstance inst(Id(name.c_str()), Id(master.c_str()), spiceNodeList(dev.nodes));
            if (!cval.empty()) inst.add(p.parseParameters("c=" + cval));
            auto ps = paramsWithMfactor();
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Inductor: {
            std::string master = mdl.empty() ? "inductor" : spiceModelName(mdl);
            ensureSpiceModel(into, "inductor");
            PTInstance inst(Id(name.c_str()), Id(master.c_str()), spiceNodeList(dev.nodes));
            if (!val.empty()) inst.add(p.parseParameters("l=" + val));
            auto ps = paramsWithMfactor();
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::VSource: {
            ensureSpiceModel(into, "vsource");
            PTInstance inst(Id(name.c_str()), Id("vsource"), spiceNodeList(dev.nodes));
            std::string srcParams = spiceSourceParams(dev.source);
            auto ps = paramsWithoutMfactor("voltage source");
            if (!srcParams.empty() && !ps.empty()) srcParams += " ";
            srcParams += ps;
            if (!srcParams.empty()) inst.add(p.parseParameters(lowercase(srcParams)));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::ISource: {
            ensureSpiceModel(into, "isource");
            PTInstance inst(Id(name.c_str()), Id("isource"), spiceNodeList(dev.nodes));
            std::string srcParams = spiceSourceParams(dev.source);
            auto ps = paramsWithMfactor();
            if (!srcParams.empty() && !ps.empty()) srcParams += " ";
            srcParams += ps;
            if (!srcParams.empty()) inst.add(p.parseParameters(lowercase(srcParams)));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Diode: {
            if (mdl.empty()) {
                Simulator::err() << "WARNING: Diode '" << name
                                 << "' has no model reference (skipped)\n";
                break;
            }
            PTInstance inst(Id(name.c_str()), spiceModelId(mdl), spiceNodeList(dev.nodes));
            auto ps = paramsWithMfactor();
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Mosfet: {
            if (mdl.empty()) {
                Simulator::err() << "WARNING: MOSFET '" << name
                                 << "' has no model reference (skipped)\n";
                break;
            }
            auto ps = paramsWithMfactor();
            auto bins = visibleBins.find(mdl);
            if (bins == visibleBins.end()) {
                PTInstance inst(Id(name.c_str()), spiceModelId(mdl), spiceNodeList(dev.nodes));
                if (!ps.empty()) inst.add(p.parseParameters(ps));
                into.add(std::move(inst));
                break;
            }

            std::string l = lowercase(spiceParamValue(dev.params, "l"));
            std::string w = lowercase(spiceParamValue(dev.params, "w"));
            if (l.empty() || w.empty()) {
                PTInstance inst(Id(name.c_str()), spiceModelId(mdl), spiceNodeList(dev.nodes));
                if (!ps.empty()) inst.add(p.parseParameters(ps));
                into.add(std::move(inst));
                break;
            }
            std::string nf = lowercase(spiceParamValue(dev.params, "nf"));
            l = "(" + l + ")";
            w = "(" + w + ")";
            nf = nf.empty() ? "1" : "(" + nf + ")";

            PTBlockSequence seq;
            for (const auto& bin : bins->second) {
                std::string guard =
                    spiceBinRange(l + "*$scale", bin.lmin, bin.lmax) + " && " +
                    spiceBinRange(w + "*$scale/" + nf, bin.wmin, bin.wmax);
                PTInstance inst(Id(name.c_str()), spiceModelId(bin.modelName),
                                spiceNodeList(dev.nodes));
                if (!ps.empty()) inst.add(p.parseParameters(ps));
                PTBlock block;
                block.add(std::move(inst));
                seq.add(p.parseExpression(guard), std::move(block));
            }
            // Preserve the ordinary missing-model error when no bin contains
            // the instance geometry instead of silently dropping the MOSFET.
            PTInstance fallback(Id(name.c_str()), spiceModelId(mdl), spiceNodeList(dev.nodes));
            if (!ps.empty()) fallback.add(p.parseParameters(ps));
            PTBlock fallbackBlock;
            fallbackBlock.add(std::move(fallback));
            seq.add(p.parseExpression("1"), std::move(fallbackBlock));
            into.add(std::move(seq));
            break;
        }
        case netlist::SpiceDeviceKind::Bjt: {
            // An omitted sp_bjt substrate resolves to ground, matching ngspice.
            // VBIC accepts only three terminals in the available master.
            if (mdl.empty()) {
                Simulator::err() << "WARNING: BJT '" << name
                                 << "' has no model reference (skipped)\n";
                break;
            }
            PTInstance inst(Id(name.c_str()), spiceModelId(mdl), spiceNodeList(dev.nodes));
            auto ps = paramsWithMfactor();
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::SubcktCall: {
            if (mdl.empty()) {
                Simulator::err() << "WARNING: SubcktCall '" << name
                                 << "' has no master subcircuit name (skipped)\n";
                break;
            }
            PTInstance inst(Id(name.c_str()), Id(mdl.c_str()), spiceNodeList(dev.nodes));
            auto ps = paramsWithMfactor();
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Vcvs: {
            if (dev.nodes.size() < 2 || dev.ctrl_nodes.size() < 2) {
                Simulator::err() << "WARNING: Vcvs '" << name
                                 << "' has too few nodes/ctrl_nodes (skipped)\n";
                break;
            }
            ensureSpiceModel(into, "vcvs");
            PTIdentifierList allNodes;
            for (const auto& n : dev.nodes)       allNodes.push_back(PTParsedIdentifier(lowercase(toString(n)).c_str()));
            for (const auto& cn : dev.ctrl_nodes) allNodes.push_back(PTParsedIdentifier(lowercase(toString(cn)).c_str()));
            PTInstance inst(Id(name.c_str()), Id("vcvs"), std::move(allNodes));
            std::string gainVal = lowercase(toString(dev.ctrl_value));
            if (!gainVal.empty()) inst.add(p.parseParameters("gain=" + gainVal));
            auto ps = paramsWithoutMfactor("VCVS");
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Vccs: {
            if (dev.nodes.size() < 2 || dev.ctrl_nodes.size() < 2) {
                Simulator::err() << "WARNING: Vccs '" << name
                                 << "' has too few nodes/ctrl_nodes (skipped)\n";
                break;
            }
            ensureSpiceModel(into, "vccs");
            PTIdentifierList allNodes;
            for (const auto& n : dev.nodes)       allNodes.push_back(PTParsedIdentifier(lowercase(toString(n)).c_str()));
            for (const auto& cn : dev.ctrl_nodes) allNodes.push_back(PTParsedIdentifier(lowercase(toString(cn)).c_str()));
            PTInstance inst(Id(name.c_str()), Id("vccs"), std::move(allNodes));
            std::string gainVal = lowercase(toString(dev.ctrl_value));
            if (!gainVal.empty()) inst.add(p.parseParameters("gain=" + gainVal));
            auto ps = paramsWithMfactor();
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Cccs: {
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
            ensureSpiceModel(into, "cccs");
            PTInstance inst(Id(name.c_str()), Id("cccs"), spiceNodeList(dev.nodes));
            std::string ctlsrc  = lowercase(toString(dev.ctrl_nodes[0]));
            std::string gainVal = lowercase(toString(dev.ctrl_value));
            // Id parameters require string literals; ctlnode defaults to flow(br).
            std::string prms = "ctlinst=\"" + ctlsrc + "\"";
            if (!gainVal.empty()) prms += " gain=" + gainVal;
            inst.add(p.parseParameters(prms));
            auto ps = paramsWithMfactor();
            if (!ps.empty()) inst.add(p.parseParameters(ps));
            into.add(std::move(inst));
            break;
        }
        case netlist::SpiceDeviceKind::Ccvs: {
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
            ensureSpiceModel(into, "ccvs");
            PTInstance inst(Id(name.c_str()), Id("ccvs"), spiceNodeList(dev.nodes));
            std::string ctlsrc  = lowercase(toString(dev.ctrl_nodes[0]));
            std::string gainVal = lowercase(toString(dev.ctrl_value));
            std::string prms = "ctlinst=\"" + ctlsrc + "\"";
            if (!gainVal.empty()) prms += " gain=" + gainVal;
            inst.add(p.parseParameters(prms));
            auto ps = paramsWithoutMfactor("CCVS");
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
        case netlist::SpiceDeviceKind::Behavioral: {
            if (dev.nodes.size() != 2) {
                Simulator::err() << "WARNING: B-source '" << name
                                 << "' needs exactly 2 nodes (skipped)\n";
                break;
            }
            std::string vexpr, iexpr, dropped;
            bool haveV = false, haveI = false;
            for (const auto& prm : dev.params) {
                std::string key = lowercase(toString(prm.name));
                bool isExpr = (key == "v" || key == "i");
                std::string val = lowercase(spiceValue(prm.value, /*behavioral=*/isExpr));
                if (key == "v")      { vexpr = val; haveV = true; }
                else if (key == "i") { iexpr = val; haveI = true; }
                else if (key == "m") { /* decided below, once v=/i= is known */ }
                else {
                    if (!dropped.empty()) dropped += ", ";
                    dropped += key;
                }
            }
            if (haveV == haveI) {
                Simulator::err() << "WARNING: B-source '" << name
                                 << "' needs exactly one of v= or i= (skipped)\n";
                break;
            }
            // Parallel current sources scale; parallel voltage sources do not.
            if (haveI) {
                if (!mfac.empty()) iexpr = "(" + iexpr + ")*(" + mfac + ")";
            } else if (!spiceParamValue(dev.params, "m").empty()) {
                Simulator::err() << "WARNING: voltage-defining B-source '" << name
                                 << "' does not take a multiplier; m= ignored\n";
            }
            if (!dropped.empty()) {
                Simulator::err() << "WARNING: B-source '" << name
                                 << "' parameter(s) " << dropped << " ignored\n";
            }
            if (!addSpiceBehavioral(name, spiceNodeList(dev.nodes), haveI ? iexpr : vexpr,
                                    haveI, "B-source", into, p, s)) {
                return false;
            }
            break;
        }
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
                            Parser& p, Status& st, const BinnedModels& inheritedBins,
                            const BehavioralResistorModels& inheritedResistorModels) {
    BinnedModels visibleBins = inheritedBins;
    BehavioralResistorModels resistorModels = inheritedResistorModels;
    // Every subcircuit accepts a multiplier so X-line m= can cross file boundaries.
    warnIfTemperDeclared(s.params, "SPICE .subckt '" + lowercase(toString(s.name)) + "'");
    auto sp = lowercase(paramString(s.params, /*spiceValues=*/true));
    if (!sp.empty()) sp += " ";
    sp += std::string(kMfactorParam) + "=1";
    def.add(p.parseParameters(sp));
    // Models must precede the devices that reference them.
    emitSpiceModels(s.models, def, p, visibleBins, resistorModels);
    for (const auto& dev : s.devices) {
        if (!addSpiceDevice(dev, def, p, st, kMfactorParam, visibleBins,
                            resistorModels)) return false;
    }
    for (const auto& sub : s.subckts) {
        PTSubcircuitDefinition child(spiceId(toString(sub.name)), spiceNodeList(sub.ports));
        if (!fillSpiceSubDef(child, sub, p, st, visibleBins, resistorModels)) return false;
        def.add(std::move(child));
    }
    return true;
}

// Foreign-format includes suppress their analysis and command projection.
bool mergeNetlist(const netlist::Netlist& nl, PTSubcircuitDefinition& top,
                  ParserTables& tab, Parser& p,
                  const std::filesystem::path& baseDir,
                   IncludeSet& visited,
                   BinnedModels& visibleBins,
                   BehavioralResistorModels& resistorModels,
                   Status& s, bool projectAnalyses = true,
                  const std::string& language = "");

static bool spiceBlockToTables(const netlist::SpiceBlock& sb, PTSubcircuitDefinition& into,
                               ParserTables& tab, Parser& p,
                               Status& s,
                               const std::filesystem::path& baseDir,
                               IncludeSet& visited,
                               BinnedModels& visibleBins,
                               BehavioralResistorModels& resistorModels,
                               bool projectAnalyses,
                               const std::string& language) {
    // Includes define models and parameters visible to this block. The bridge
    // groups AST nodes by kind, so resolve includes before projecting devices.
    for (const auto& inc : sb.includes) {
        std::filesystem::path incPath = baseDir / toString(inc.path);
        std::filesystem::path absPath;
        try {
            absPath = std::filesystem::canonical(incPath);
        } catch (...) {
            absPath = std::filesystem::absolute(incPath);
        }

        IncludeKey key = includeKey(absPath, inc.section);
        if (!visited.insert(key).second) continue;

        std::ifstream ifs(absPath);
        if (!ifs) {
            s.set(Status::NotFound, "cannot open SPICE include: " + absPath.string());
            return false;
        }
        std::stringstream incss; incss << ifs.rdbuf();
        std::string contents = incss.str();

        static constexpr const char* kSpiceBlockDialect = "ngspice";
        netlist::Netlist sub;
        if (!inc.section.empty()) {
            sub = netlist::parse_netlist_lib(rust::Str(contents), rust::Str(toString(inc.section)),
                                             rust::Str(kSpiceBlockDialect));
        } else {
            sub = netlist::parse_netlist(rust::Str(contents),
                                         rust::Str(kSpiceBlockDialect));
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

        if (!mergeNetlist(sub, into, tab, p, absPath.parent_path(), visited, visibleBins,
                          resistorModels, s,
                          projectAnalyses, kSpiceBlockDialect))
            return false;
    }

    warnIfTemperDeclared(sb.params, "SPICE block");
    auto sp = paramString(sb.params, /*spiceValues=*/true);
    if (!sp.empty()) into.add(p.parseParameters(lowercase(sp)));

    emitSpiceModels(sb.models, into, p, visibleBins, resistorModels);

    // The top level has no multiplier to inherit.
    for (const auto& dev : sb.devices) {
        if (!addSpiceDevice(dev, into, p, s, "", visibleBins, resistorModels)) return false;
    }

    for (const auto& sub : sb.subckts) {
        PTSubcircuitDefinition child(spiceId(toString(sub.name)), spiceNodeList(sub.ports));
        if (!fillSpiceSubDef(child, sub, p, s, visibleBins, resistorModels)) return false;
        into.add(std::move(child));
    }

    return true;
}

bool mergeNetlist(const netlist::Netlist& nl, PTSubcircuitDefinition& top,
                  ParserTables& tab, Parser& p,
                  const std::filesystem::path& baseDir,
                   IncludeSet& visited,
                   BinnedModels& visibleBins,
                   BehavioralResistorModels& resistorModels,
                   Status& s, bool projectAnalyses,
                  const std::string& language) {
    auto sp = paramString(nl.params);
    if (!sp.empty()) top.add(p.parseParameters(sp));
    for (const auto& m : nl.models)    top.add(makeModel(m, p));
    for (const auto& i : nl.instances) top.add(makeInstance(i, p));
    for (const auto& sub : nl.subckts) {
        PTSubcircuitDefinition child(Id(toString(sub.name).c_str()), nodeList(sub.ports));
        if (!fillSubDef(child, sub, p, tab, s, baseDir, visited, visibleBins,
                        resistorModels)) return false;
        top.add(std::move(child));
    }

    for (const auto& sb : nl.spice_blocks) {
        if (!spiceBlockToTables(sb, top, tab, p, s, baseDir, visited, visibleBins,
                                resistorModels,
                                projectAnalyses, language))
            return false;
    }

    for (const auto& g : nl.globals) tab.addGlobal(PTParsedIdentifier(toString(g).c_str()));
    if (projectAnalyses) {
        for (const auto& a : nl.analyses) {
            PTAnalysis desc(Id(toString(a.name).c_str()), Id(toString(a.analysis_type).c_str()));
            auto ps = paramString(a.params);
            if (!ps.empty()) desc.add(p.parseParameters(ps));
            tab.addCommand(std::move(desc));
        }
    } else if (!nl.analyses.empty()) {
        Simulator::err() << "WARNING: netlistrs adapter ignoring " << nl.analyses.size()
                         << " analysis command(s) from an included foreign-format file"
                         << " (write analyses in the native VACASK deck)\n";
    }

    // Warn because silently dropping initial conditions changes simulation results.
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

    for (const auto& inc : nl.includes) {
        std::filesystem::path incPath = baseDir / toString(inc.path);
        std::filesystem::path absPath;
        try {
            absPath = std::filesystem::canonical(incPath);
        } catch (...) {
            absPath = std::filesystem::absolute(incPath);
        }

        IncludeKey key = includeKey(absPath, inc.section);
        if (!visited.insert(key).second) continue;

        std::ifstream in(absPath);
        if (!in) {
            s.set(Status::NotFound, "cannot open include: " + absPath.string());
            return false;
        }
        std::stringstream ss; ss << in.rdbuf();
        std::string contents = ss.str();

        netlist::Netlist sub;
        if (!inc.section.empty()) {
            sub = netlist::parse_netlist_lib(rust::Str(contents), rust::Str(toString(inc.section)),
                                             rust::Str(language));
        } else {
            sub = netlist::parse_netlist(rust::Str(contents), rust::Str(language));
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

        if (!mergeNetlist(sub, top, tab, p, absPath.parent_path(), visited, visibleBins,
                          resistorModels, s,
                          projectAnalyses, language))
            return false;
    }
    return true;
}

} // namespace

bool mergeForeignFile(const std::string& path, const std::string& section,
                      const std::string& language,
                      PTSubcircuitDefinition& top, ParserTables& tab,
                      Parser& p, Status& s) {
    namespace fs = std::filesystem;

    static const std::set<std::string> kDialects =
        {"ngspice", "hspice", "pspice", "xyce", "spectre"};
    if (language.empty() || !kDialects.count(language)) {
        s.set(Status::Syntax, "include of '" + path +
              "': missing or unknown lang= (expected ngspice|hspice|pspice|xyce|spectre)");
        return false;
    }
    if (!section.empty() && language == "spectre") {
        s.set(Status::Syntax, "include of '" + path +
              "': section= is not supported with lang=spectre");
        return false;
    }

    std::ifstream in(path);
    if (!in) { s.set(Status::NotFound, "cannot open foreign include: " + path); return false; }
    std::stringstream ss; ss << in.rdbuf();
    std::string source = ss.str();

    try {
        netlist::Netlist nl = section.empty()
            ? netlist::parse_netlist(rust::Str(source), rust::Str(language))
            : netlist::parse_netlist_lib(rust::Str(source), rust::Str(section), rust::Str(language));
        if (!nl.errors.empty()) {
            std::ostringstream os;
            os << "netlist parse error(s) in '" << path << "': " << nl.errors.size()
               << " (first at bytes [" << nl.errors[0].start << ", " << nl.errors[0].end << "))";
            s.set(Status::Syntax, os.str());
            return false;
        }

        fs::path fp(path);
        fs::path absPath;
        try { absPath = fs::canonical(fp); } catch (...) { absPath = fs::absolute(fp); }
        IncludeSet visited{{absPath, lowercase(section)}};
        BinnedModels visibleBins;
        BehavioralResistorModels resistorModels;
        if (!mergeNetlist(nl, top, tab, p, absPath.parent_path(), visited, visibleBins,
                          resistorModels, s,
                          /*projectAnalyses=*/false, language))
            return false;
        emitOsdiLoads(tab, top);
        return true;
    } catch (const std::exception& e) {
        s.set(Status::Syntax, "failed to translate foreign include '" + path + "': " + e.what());
        return false;
    }
}

}
