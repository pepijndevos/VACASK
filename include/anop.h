#ifndef __ANOP_DEFINED
#define __ANOP_DEFINED

#include "parameterized.h"
#include "status.h"
#include "circuit.h"
#include "an.h"
#include "klumatrix.h"
#include "output.h"
#include "outrawfile.h"
#include "flags.h"
#include "coreop.h"
#include "common.h"


namespace NAMESPACE {

class OperatingPoint : public Analysis {
public:
    typedef OperatingPointParameters Parameters;
    
    OperatingPoint(Id name, Circuit& circuit, PTAnalysis& ptAnalysis);
    
    OperatingPoint           (const OperatingPoint&)  = delete;
    OperatingPoint           (      OperatingPoint&&) = delete;
    OperatingPoint& operator=(const OperatingPoint&)  = delete;
    OperatingPoint& operator=(      OperatingPoint&&) = delete;

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
    IStruct<OperatingPointParameters> params;

    // Declared before core so the references core binds in its init list
    // (jac, solution, states) refer to fully-constructed members.
    KluRealMatrix jac; // Resistive Jacobian
    VectorRepository<double> solution; // Solution history
    VectorRepository<double> states; // Circuit states

    DelayLines delayLines_;
    DelayMatrixBindings<double*> delayBindings_;

    OperatingPointCore core;
};

}

#endif
