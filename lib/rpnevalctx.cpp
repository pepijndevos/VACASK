#include "rpnevalctx.h"
#include "common.h"
#include <algorithm>
#include <numeric>

namespace NAMESPACE {

std::ostream& operator<<(std::ostream& os, const MCData::GeneratorId& genId) {
    auto& [ctxType, obj, par, num] = genId;
    switch (ctxType) {
        case MCData::CtxType::NoContext:
            os << "No context ";
            break;
        case MCData::CtxType::Condition:
            os << "Condition  " << std::string(obj);
            break;
        case MCData::CtxType::Instance:
            os << "Instance   " << std::string(obj) << " par=" << std::string(par) << " #" << num;
            break;
        case MCData::CtxType::Model:
            os << "Model      " << std::string(obj) << " par=" << std::string(par) << " #" << num;
            break;
        case MCData::CtxType::Parameters:
            os << "Parameters " << std::string(obj) << " par=" << std::string(par) << " #" << num;
            break;
    }
    return os;
}

void MCData::setSeed(Int seed) {
    randomGenerator.seed(seed);
}

void MCData::clear() {
    generatorIndexMap.clear();
    value.clear();
    lhPerm.clear();
    lhSamples = 0;
    sample_ = 0;
}

void MCData::advance() {
    size_t cnt = 0;
    for(auto& v : value) {
        v = gen(cnt);
        cnt++;
    }
    sample_++;
}

void MCData::setLHSamples(size_t nsam) { 
    lhSamples = nsam;
    if (!lhSamples) {
        lhPerm.clear();
    }
}

double MCData::retrieveOrAdd(const MCGeneratorId& genId) {
    auto it = generatorIndexMap.find(genId);
    size_t ndx;
    if (it!=generatorIndexMap.end()) {
        ndx = it->second;
    } else {
        ndx = value.size();
        generatorIndexMap.emplace(genId, ndx);
        if (lhSamples>0) {
            std::vector<size_t> perm(lhSamples);
            std::iota(perm.begin(), perm.end(), 0);
            std::shuffle(perm.begin(), perm.end(), randomGenerator);
            lhPerm.push_back(std::move(perm));
        }
        value.push_back(gen(ndx));
    }
    return value[ndx];
}

size_t MCData::count() const { 
    return value.size(); 
}

size_t MCData::sample() const { 
    return sample_; 
}

void MCData::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');
    size_t cnt = 0;
    for(auto& it : generatorIndexMap) {
        auto ndx = it.second;

        os << pfx << it.first;
        if (lhSamples>0) {
            os << " : bin=" << lhPerm[cnt][sample_];
        }
        os << " : unif=" << value[cnt];
        os << "\n";
        cnt++;
    }
}


}