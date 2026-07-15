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
    std::uniform_real_distribution<double> dist(0.0, 1.0);;
    for(auto& v : value) {
        v = dist(randomGenerator);
    }
}

double MCData::retrieveOrAdd(const MCGeneratorId& genId) {
    auto it = generatorIndexMap.find(genId);
    size_t ndx;
    if (it!=generatorIndexMap.end()) {
        ndx = it->second;
    } else {
        ndx = value.size();
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        value.push_back(dist(randomGenerator));
        generatorIndexMap.emplace(genId, ndx);
    }
    return value[ndx];
}

size_t MCData::count() const { 
    return value.size(); 
}

}