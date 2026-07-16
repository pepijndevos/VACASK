#include "rpnevalctx.h"
#include "common.h"

namespace NAMESPACE {

void MCData::setSeed(Int seed) {
    randomGenerator.seed(seed);
}

void MCData::clear() {
    generatorIndexMap.clear();
    value.clear();
}

void MCData::advance() {
    for(auto& v : value) {
        v = gen();
    }
}

double MCData::retrieveOrAdd(const MCGeneratorId& genId) {
    auto it = generatorIndexMap.find(genId);
    size_t ndx;
    if (it!=generatorIndexMap.end()) {
        ndx = it->second;
    } else {
        ndx = value.size();
        value.push_back(gen());
        generatorIndexMap.emplace(genId, ndx);
    }
    return value[ndx];
}

size_t MCData::count() const { 
    return value.size(); 
}

}