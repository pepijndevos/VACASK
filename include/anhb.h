#ifndef __ANHB_DEFINED
#define __ANHB_DEFINED

#include <memory>
#include "parameterized.h"
#include "status.h"
#include "circuit.h"
#include "an.h"
#include "cscmatrix.h"
#include "output.h"
#include "outrawfile.h"
#include "flags.h"
#include "corehb.h"
#include "common.h"


namespace NAMESPACE {

class HB : public Analysis {
public:
    typedef HBParameters Parameters;

    static inline const Id analysisId = Id::createStatic("hb");

    HB(Id name, Circuit& circuit, PTAnalysis& ptAnalysis);
    
    HB           (const HB&)  = delete;
    HB           (      HB&&) = delete;
    HB& operator=(const HB&)  = delete;
    HB& operator=(      HB&&) = delete;

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
    IStruct<HBParameters> params;

    // Declared before core so the references core binds in its init list
    // (jacColoc, jac, solution) refer to fully-constructed members.
    CSCBlockSparseRealMatrix jacColoc; // Jacobian entries at colocation points
    CSCBlockSparseRealMatrix jac; // HB Jacobian
    VectorRepository<double> solution; // Solution history

    DelayLines delayLines_;
    DelayMatrixBindings<DenseMatrixView<double>> delayBindings_;

    std::unique_ptr<RealSparseSolver> linearSolver_;

    HBCore core;
};

}

#endif
