#ifndef __ANHB_DEFINED
#define __ANHB_DEFINED

#include "parameterized.h"
#include "status.h"
#include "circuit.h"
#include "an.h"
#include "klumatrix.h"
#include "output.h"
#include "outrawfile.h"
#include "flags.h"
#include "corehb.h"
#include "common.h"


namespace NAMESPACE {

class HB : public Analysis {
public:
    typedef HBParameters Parameters;
    
    HB(Id name, Circuit& circuit, PTAnalysis& ptAnalysis);
    
    HB           (const HB&)  = delete;
    HB           (      HB&&) = delete;
    HB& operator=(const HB&)  = delete;
    HB& operator=(      HB&&) = delete;

    virtual void dump(std::ostream& os) const;

    virtual Parameterized& parameters() { return params; }; 
    virtual const Parameterized& parameters() const { return params; }; 

    // Factory function for operating point analysis
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
    virtual AnalysisCore& analysisCore() { return core; };
    virtual CoreCoroutine coreCoroutine(bool continuePrevious, ErrorConsumer& errors) {
        return std::move(core.coroutine(continuePrevious, errors));
    };
    virtual bool finalizeOutputs(ErrorConsumer& errors);
    virtual bool deleteOutputs(ErrorConsumer& errors);
    
    virtual size_t analysisStateStorageSize() const;
    virtual size_t allocateAnalysisStateStorage(size_t n);
    virtual void deallocateAnalysisStateStorage(size_t n=0);
    virtual bool storeState(size_t ndx, bool storeDetails=true);
    virtual bool restoreState(size_t ndx);
    virtual void makeStateIncoherent(size_t ndx);

    
private:
    IStruct<HBParameters> params;

    // Declared before core so the references core binds in its init list
    // (jacColoc, jac, solution) refer to fully-constructed members.
    KluBlockSparseRealMatrix jacColoc; // Jacobian entries at colocation points
    KluBlockSparseRealMatrix jac; // HB Jacobian
    VectorRepository<double> solution; // Solution history

    DelayLines delayLines_;
    DelayMatrixBindings<DenseMatrixView<double>> delayBindings_;

    HBCore core;
};

}

#endif
