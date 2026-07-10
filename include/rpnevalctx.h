#ifndef __RPNEVALCTX_DEFINED
#define __RPNEVALCTX_DEFINED

#include "identifier.h"
#include "hash.h"
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include "common.h"


namespace NAMESPACE {

// Evaluation context defining where this function is found
// via instance/model Id, parameter Id, RPN command index
// Passed at RpnBuiltinFunc call
//
// A random generator is recognized by
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
// Context stores a map generatorIdentifier -> normalized uniform random value (0,1)
// If map does not have an entry, a new entry is added and a random number generated.
// For an existing entry the random number is taken from the map.
// On MC advance a new set of values is generated for the whole map using a single
// random number generator.
struct RpnEvaluationContext {
public:
    enum class CxType {
        Instance,  // obj, param, cmdIndex
        Model,     // obj, param, cmdIndex
    };
    CxType cxType;
    Id obj;
    Id param;
    RpnArity callIndex;

    // Generator identifier: (cxType, obj, param, callIndex)
    typedef std::tuple<CxType, Id, Id, RpnArity> GeneratorId;

    // Hash functor for GeneratorId, passed explicitly to generatorIndex below
    // instead of specializing std::hash<GeneratorId>. Equality uses the
    // compiler-generated std::tuple::operator==, which already works
    // element-wise since CxType, Id, and RpnArity all support ==.
    struct GeneratorIdHash {
        std::size_t operator()(const GeneratorId& k) const {
            auto& [cxType, obj, param, callIndex] = k;
            return hash_val(static_cast<std::underlying_type_t<CxType>>(cxType), obj, param, callIndex);
        }
    };

    // Return the GeneratorId for current context
    GeneratorId generatorId() const {
        return GeneratorId(cxType, obj, param, callIndex);
    };

    // Clear map and values
    void clear() {
        generatorIndexMap.clear();
        generatorValue.clear();
    }

    // For generator id, add a new zero entry at end of vector, add a corresponding entry to map
    // If the generator id already has an entry, return its existing index instead
    size_t generatorIndex(const GeneratorId& id) {
        auto it = generatorIndexMap.find(id);
        if (it!=generatorIndexMap.end()) {
            return it->second;
        }
        size_t ndx = generatorValue.size();
        // TODO: generate a random value here
        generatorValue.push_back(0.0);
        generatorIndexMap.emplace(id, ndx);
        return ndx;
    }

    // Maps a generator identifier to its index in the set of MC generators seen so far
    std::unordered_map<GeneratorId, size_t, GeneratorIdHash> generatorIndexMap;

    // Actual generator values
    std::vector<double> generatorValue;
};

}

#endif
