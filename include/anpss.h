#ifndef __ANPSS_DEFINED
#define __ANPSS_DEFINED

#include <memory>
#include "an.h"
#include "corepss.h"
#include "parameterized.h"
#include "common.h"


namespace NAMESPACE {

class PSS : public Analysis {
public:
    typedef PssParameters Parameters;

    static inline const Id analysisId = Id::createStatic("pss");

    PSS(Id name, Circuit& circuit, PTAnalysis& ptAnalysis);

    PSS           (const PSS&)  = delete;
    PSS           (      PSS&&) = delete;
    PSS& operator=(const PSS&)  = delete;
    PSS& operator=(      PSS&&) = delete;

    void dump(std::ostream& os) const override;

    Parameterized& parameters() override { return params; }
    const Parameterized& parameters() const override { return params; }

    // Factory function registered with the analysis dispatcher.
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
    AnalysisCore& analysisCore() override { return pssCore_; }
    CoreCoroutine coreCoroutine(bool continuePrevious, ErrorConsumer& errors) override {
        return std::move(pssCore_.coroutine(continuePrevious, errors));
    }

    bool finalizeOutputs(ErrorConsumer& errors) override;
    bool deleteOutputs(ErrorConsumer& errors) override;

    size_t analysisStateStorageSize() const override;
    size_t allocateAnalysisStateStorage(size_t n) override;
    virtual void deallocateAnalysisStateStorage(size_t n=0);
    bool storeState(size_t ndx, bool storeDetails=true) override;
    bool restoreState(size_t ndx) override;
    void makeStateIncoherent(size_t ndx) override;

private:
    IStruct<PssParameters> params;

    CSCRealMatrix            jac_;
    VectorRepository<double> opSolution_;
    VectorRepository<double> solution_;
    VectorRepository<double> states_;

    // Dummies to make opCore happy
    DelayLines delayLines_;
    DelayMatrixBindings<double*> delayBindings_;

    std::unique_ptr<RealSparseSolver> linearSolver_;

    OperatingPointCore opCore_;
    TranCore           stabilTran_;
    PssTranCore        pssTran_;
    PssCore            pssCore_;
};

} // namespace NAMESPACE

#endif // __ANPSS_DEFINED
