#include "rpnstack.h"
#include "common.h"

namespace NAMESPACE {

void RpnStack::dump(std::ostream& os) {
    int i = 0;
    for(auto& v : stack) {
        os << (stack.size()-i-1) << " : " << v << "\n";
        i++;
    }
}
    
}
