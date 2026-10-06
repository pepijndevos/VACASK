#ifndef __ANOP_DEFINED
#define __ANOP_DEFINED

#include <memory>
#include "parameterized.h"
#include "status.h"
#include "circuit.h"
#include "an.h"
#include "cscmatrix.h"
#include "solver.h"
#include "output.h"
#include "outrawfile.h"
#include "flags.h"
#include "coreop.h"
#include "common.h"


namespace NAMESPACE {

class OperatingPoint : public Analysis {
public:
    typedef OperatingPointParameters Parameters;

    static inline const Id analysisId = Id::createStatic("op");

    OperatingPoint(Id name, Circuit& circuit, PTAnalysis& ptAnalysis);
    
    OperatingPoint           (const OperatingPoint&)  = delete;
    OperatingPoint           (      OperatingPoint&&) = delete;
    OperatingPoint& operator=(const OperatingPoint&)  = delete;
    OperatingPoint& operator=(      OperatingPoint&&) = delete;

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
    AnalysisCore& analysisCore() override { return core; };
    CoreCoroutine coreCoroutine(bool continuePrevious, ErrorConsumer& errors) override {
        return std::move(core.coroutine(continuePrevious, errors));
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
    IStruct<OperatingPointParameters> params;

    // Declared before core so the references core binds in its init list
    // (jac, solution, states) refer to fully-constructed members.
    CSCRealMatrix jac; // Resistive Jacobian
    VectorRepository<double> solution; // Solution history
    VectorRepository<double> states; // Circuit states

    DelayLines delayLines_;
    DelayMatrixBindings<double*> delayBindings_;

    std::unique_ptr<RealSparseSolver> linearSolver_;

    OperatingPointCore core;
};

}

#endif
