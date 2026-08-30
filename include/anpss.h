#ifndef __ANPSS_DEFINED
#define __ANPSS_DEFINED

#include "an.h"
#include "corepss.h"
#include "parameterized.h"
#include "common.h"


namespace NAMESPACE {

class Pss : public Analysis {
public:
    typedef PssParameters Parameters;

    Pss(Id name, Circuit& circuit, PTAnalysis& ptAnalysis);

    Pss           (const Pss&)  = delete;
    Pss           (      Pss&&) = delete;
    Pss& operator=(const Pss&)  = delete;
    Pss& operator=(      Pss&&) = delete;

    virtual void dump(std::ostream& os) const;

    virtual Parameterized& parameters() { return params; }
    virtual const Parameterized& parameters() const { return params; }

    // Factory function registered with the analysis dispatcher.
    static Analysis* create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s=Status::ignore);

protected:
    virtual bool addCommonOutputDescriptor(const OutputDescriptor& desc);
    virtual bool addCoreOutputDescriptors(ErrorConsumer& errors);
    virtual bool resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors);
    virtual bool addDefaultOutputDescriptors(ErrorConsumer& errors);
    virtual void clearOutputDescriptors();
    virtual bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors);

    virtual std::tuple<bool, bool> preMapping(ErrorConsumer& errors);
    virtual bool populateStructures(ErrorConsumer& errors);

    virtual bool rebuildCores(ErrorConsumer& errors);
    virtual bool initializeOutputs(ErrorConsumer& errors);
    virtual AnalysisCore& analysisCore() { return pssCore_; }
    virtual CoreCoroutine coreCoroutine(bool continuePrevious, ErrorConsumer& errors) {
        return std::move(pssCore_.coroutine(continuePrevious, errors));
    }

    virtual bool finalizeOutputs(ErrorConsumer& errors);
    virtual bool deleteOutputs(ErrorConsumer& errors);

    virtual size_t analysisStateStorageSize() const;
    virtual size_t allocateAnalysisStateStorage(size_t n);
    virtual void deallocateAnalysisStateStorage(size_t n=0);
    virtual bool storeState(size_t ndx, bool storeDetails=true);
    virtual bool restoreState(size_t ndx);
    virtual void makeStateIncoherent(size_t ndx);

private:
    IStruct<PssParameters> params;

    KluRealMatrix            jac_;
    VectorRepository<double> opSolution_;
    VectorRepository<double> solution_;
    VectorRepository<double> states_;

    // Dummies to make opCore happy
    DelayLines delayLines_; 
    DelayMatrixBindings<double*> delayBindings_;

    OperatingPointCore opCore_;
    TranCore           stabilTran_;
    PssTranCore        pssTran_;
    PssCore            pssCore_;
};

} // namespace NAMESPACE

#endif // __ANPSS_DEFINED
