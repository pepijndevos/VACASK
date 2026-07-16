#ifndef __RPNEVAL_DEFINED
#define __RPNEVAL_DEFINED

#include <vector>
#include "value.h"
#include "status.h"
#include "context.h"
#include "rpnexpr.h"
#include "rpnstack.h"
#include "rpnevalctx.h"
#include "parseroutput.h"
#include "common.h"


namespace NAMESPACE {

class RpnEvaluator {
public:
    RpnEvaluator() : mcData_(nullptr) {};

    RpnEvaluator           (const RpnEvaluator&)  = delete;
    RpnEvaluator           (      RpnEvaluator&&) = default;
    RpnEvaluator& operator=(const RpnEvaluator&)  = delete;
    RpnEvaluator& operator=(      RpnEvaluator&&) = default;

    bool isConstant(const Rpn& expr) const;
    bool evaluate(const Rpn& rpn, Value& result, RpnEvaluationNetlistContext& ctx, Status& s=Status::ignore);

    RpnStack& stack() { return stack_; };

    size_t contextMarker() const { return contextStack_.depth(); };
    bool revertContext(size_t marker) { return contextStack_.exit(marker); }; 

    inline ContextStack& contextStack() { return contextStack_; };
    inline const ContextStack& contextStack() const { return contextStack_; };

    void appendLocation(Status& s, const Loc& id);

    inline void setMCData(MCData* data) { mcData_ = data; };
    inline MCData* mcData() { return mcData_; };

protected:
    // OK, condition
    std::tuple<bool, bool> checkCondition(Status& s=Status::ignore) {
        Value* cond = stack_.get(); 
        return cond->checkCondition();
    };

private:
    RpnStack stack_;
    ContextStack contextStack_;
    MCData* mcData_;
};

}

#endif
