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
    virtual bool addCoreOutputDescriptors(Status& s=Status::ignore);
    virtual bool resolveSave(const PTSave& save, bool verify, Status& s=Status::ignore);
    virtual bool addDefaultOutputDescriptors(Status& s=Status::ignore);
    virtual void clearOutputDescriptors();
    virtual bool resolveOutputDescriptors(bool strict, Status& s=Status::ignore);

    virtual std::tuple<bool, bool> preMapping(Status& s=Status::ignore);
    virtual bool populateStructures(Status& s=Status::ignore);
    
    virtual bool rebuildCores(Status& s=Status::ignore);
    virtual bool initializeOutputs(Status& s=Status::ignore);
    virtual AnalysisCore& analysisCore() { return pssCore_; }
    virtual CoreCoroutine coreCoroutine(bool continuePrevious) {
        return std::move(pssCore_.coroutine(continuePrevious));
    }
    virtual bool formatCoreError(Status& s=Status::ignore) {
        return pssCore_.formatError(s);
    }

    virtual bool finalizeOutputs(Status& s=Status::ignore);
    virtual bool deleteOutputs(Status& s=Status::ignore);

    virtual size_t analysisStateStorageSize() const;
    virtual size_t allocateAnalysisStateStorage(size_t n);
    virtual void deallocateAnalysisStateStorage(size_t n=0);
    virtual bool storeState(size_t ndx);
    virtual bool restoreState(size_t ndx);
    virtual void makeStateIncoherent(size_t ndx);

private:
    IStruct<PssParameters> params;

    KluRealMatrix            jac_;
    VectorRepository<double> solution_;
    VectorRepository<double> states_;

    OperatingPointCore opCore_;
    TranCore           stabilTran_;
    PssTranCore        pssTran_;
    PssCore            pssCore_;
};

} // namespace NAMESPACE

#endif // __ANPSS_DEFINED
