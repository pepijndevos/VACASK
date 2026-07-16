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

void MCData::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');
    for(auto& it : generatorIndexMap) {
        auto [ctxType, obj, par, num] = it.first;
        auto ndx = it.second;

        os << pfx;
        switch (ctxType) {
            case CtxType::NoContext:
                os << "No context ";
                break;
            case CtxType::Condition:
                os << "Condition  " << std::string(obj);
                break;
            case CtxType::Instance:
                os << "Instance   " << std::string(obj) <<  " " << std::string(par) << " " << std::to_string(num);
                break;
            case CtxType::Model:
                os << "Model      " << std::string(obj) << " " << std::string(par) << " " << std::to_string(num);
                break;
            case CtxType::Parameters:
                os << "Parameters " << std::string(obj) << " " << std::string(par) << " " << std::to_string(num);
                break;
        }
        os << "\n";
    }
}


}