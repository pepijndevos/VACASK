#ifndef __ANTRAN_DEFINED
#define __ANTRAN_DEFINED

#include <memory>
#include "an.h"
#include "coreop.h"
#include "coretran.h"
#include "parameterized.h"
#include "common.h"


namespace NAMESPACE {

class Tran : public Analysis {
public:
    typedef TranParameters Parameters;

    static inline const Id analysisId = Id::createStatic("tran");

    Tran(Id name, Circuit& circuit, PTAnalysis& ptAnalysis);
    
    Tran           (const Tran&)  = delete;
    Tran           (      Tran&&) = delete;
    Tran& operator=(const Tran&)  = delete;
    Tran& operator=(      Tran&&) = delete;

    void dump(std::ostream& os) const override;

    Parameterized& parameters() override { return params; };
    const Parameterized& parameters() const override { return params; };

    // Factory function for operating point analysis
    static Analysis* create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s=Status::ignore);

protected:
    bool addCommonOutputDescriptor(const OutputDescriptor& desc) override;
    bool addCoreOutputDescriptors(ErrorConsumer& errors) override;
    bool resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) override;
    bool addDefaultOutputDescriptors(ErrorConsumer& errors) override;
    void clearOutputDescriptors() override;
    bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors) override;

    std::tuple<bool, bool> preMapping(ErrorConsumer& errors) override;
    bool populateStructures(ErrorConsumer& errors) override;

    bool rebuildCores(ErrorConsumer& errors) override;
    bool initializeOutputs(ErrorConsumer& errors) override;
    AnalysisCore& analysisCore() override { return tranCore; };
    CoreCoroutine coreCoroutine(bool continuePrevious, ErrorConsumer& errors) override {
        return std::move(tranCore.coroutine(continuePrevious, errors));
    };
    bool finalizeOutputs(ErrorConsumer& errors) override;
    bool deleteOutputs(ErrorConsumer& errors) override;

    size_t analysisStateStorageSize() const override;
    size_t allocateAnalysisStateStorage(size_t n) override;
    virtual void deallocateAnalysisStateStorage(size_t n=0);
    bool storeState(size_t ndx, bool storeDetails=true) override;
    bool restoreState(size_t ndx) override;
    void makeStateIncoherent(size_t ndx) override;
    
private:
    IStruct<TranParameters> params;

    // Declared before the cores so the references they bind in their init lists
    // (jac, solution, states) refer to fully-constructed members. opCore precedes
    // tranCore because tranCore binds a reference to opCore.
    CSCRealMatrix jac; // Jacobian
    VectorRepository<double> opSolution; // OP solution history
    VectorRepository<double> solution; // Solution history
    VectorRepository<double> states; // Circuit states

    DelayLines delayLines_;
    DelayMatrixBindings<double*> delayBindings_;

    std::unique_ptr<RealSparseSolver> linearSolver_;

    OperatingPointCore opCore;
    TranCore tranCore;
};

}

#endif
