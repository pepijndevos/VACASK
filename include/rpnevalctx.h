#ifndef __RPNEVALCTX_DEFINED
#define __RPNEVALCTX_DEFINED

#include "identifier.h"
#include "value.h"
#include "hash.h"
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <random>
#include "common.h"


namespace NAMESPACE {

// Monte Carlo data structure
class MCData {
public:
    // Context of a Monte Carlo random generator
    enum class CtxType {
        NoContext,  // No special context
        Condition,  // Conditional netlist block condition expression
        Instance,   // Instance creation
        Model,      // Model creation
        Parameters, // Parameterized expresions evaluated inside a hierarchical instance
    };

    // Generator identifier: (cxType, obj, param, callIndex)
    typedef std::tuple<CtxType, Id, Id, RpnArity> GeneratorId;

    // Hash functor for GeneratorId, passed explicitly to generatorIndex below
    // instead of specializing std::hash<GeneratorId>. Equality uses the
    // compiler-generated std::tuple::operator==, which already works
    // element-wise since CxType, Id, and RpnArity all support ==.
    struct GeneratorIdHash {
        std::size_t operator()(const GeneratorId& k) const {
            auto& [cxType, obj, param, callIndex] = k;
            return hash_val(static_cast<std::underlying_type_t<CtxType>>(cxType), obj, param, callIndex);
        }
    };

    MCData() : sample_(0), lhSamples(0), dbgStream(nullptr) { setSeed(0); };
    
    // Monte Carlo generator ID
    // Context type, object ID, parameter ID, consecutive number of MC function in expression
    typedef std::tuple<CtxType, Id, Id, RpnArity> MCGeneratorId;

    // Set generator seed
    void setSeed(Int seed);

    // Enable/disable Latin hypercube sampling
    void setLHSamples(size_t nsam);

    // Clear generator map
    void clear();

    // Compute new sample
    void advance();

    // Retrieve generator value, create and initalize a new one if not found
    double retrieveOrAdd(const MCGeneratorId& genId);

    // Return the number of generators
    size_t count() const; 

    // Return the consecutive number of the sample
    size_t sample() const;

    void setDebugStream(std::ostream* debug) { dbgStream = debug; };
    std::ostream* debugStream() const { return dbgStream; };
    
    void dump(int indent, std::ostream& os) const;

private:
    // Make generator return numbers from (0,1) so we avoid +-inf in Gaussian distribution
    double gen(size_t sourceNdx) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double v;
        do {
            if (lhSamples==0) {
                // Ordinary sampling
                v = dist(randomGenerator);
            } else {
                // Latin hypercube
                auto bin = lhPerm[sourceNdx][sample_];
                v = (bin + dist(randomGenerator)) / lhSamples;
            }
        } while (v==0 || v==1);
        return v;
    };

    // Map from MCGeneratorId to generator index
    // Maps a generator identifier to its index in the set of MC generators seen so far
    std::unordered_map<GeneratorId, size_t, GeneratorIdHash> generatorIndexMap;

    // Generator values (all generators are uniform random (0,1))
    std::vector<double> value;

    // Latin hypercube permutations
    std::vector<std::vector<size_t>> lhPerm;

    // Sample counter
    size_t sample_;

    // Latin hypercube sample count, 0 if disabled
    size_t lhSamples;

    // Random generator
    std::mt19937_64 randomGenerator;

    std::ostream* dbgStream;
};

// Stream a GeneratorId
std::ostream& operator<<(std::ostream& os, const MCData::GeneratorId& genId);

// Netlist context of a RPN evaluation
// A random generator is recognized by
// - context type (instance/model creation, subcircuit parameters evaluation)
// - instance/model qualified name
// - parameter name
// - consecutive number of MC generator function in expression
//
// If MC generator appears in one conditional branch, it must appear in others too
// under same instance/model qualified name, parameter name, and consecutive
// generator number.
// This guarantees that the set of random numbers is the same regardless of which
// conditional netlist branch is active.
//
// Evaluator adds apointer to Momnte Carlo Data before evaluation.
// This structure is passed to builtin functions so that MC  
// generators can produce a random value consistently until 
// the generator is advanced by one sample. 
struct RpnEvaluationNetlistContext {
public:
    RpnEvaluationNetlistContext(MCData::CtxType cxType=MCData::CtxType::NoContext, Id obj=Id())
        : cxType_(cxType), obj(obj), param(Id()), callIndex(0), mcData_(nullptr) {};

    void setParameterId(Id id) { param = id; };
    void setCallIndex(RpnArity ndx) { callIndex = ndx; };
    void nextCallIndex() { callIndex++; };
    void setMCData(MCData* data) { mcData_ = data; };
    MCData* mcData() { return mcData_; };
    MCData::CtxType ctxType() const { return cxType_; };

    // Return the GeneratorId for current context
    MCData::GeneratorId generatorId() const {
        return MCData::GeneratorId(cxType_, obj, param, callIndex);
    };

private:
    MCData::CtxType cxType_;
    Id obj;
    Id param;
    RpnArity callIndex;
    
    MCData* mcData_;
};

}

#endif
