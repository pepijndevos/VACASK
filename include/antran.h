#ifndef __ANTRAN_DEFINED
#define __ANTRAN_DEFINED

#include "an.h"
#include "coreop.h"
#include "coretran.h"
#include "parameterized.h"
#include "common.h"


namespace NAMESPACE {

class Tran : public Analysis {
public:
    typedef TranParameters Parameters;

    Tran(Id name, Circuit& circuit, PTAnalysis& ptAnalysis);
    
    Tran           (const Tran&)  = delete;
    Tran           (      Tran&&) = delete;
    Tran& operator=(const Tran&)  = delete;
    Tran& operator=(      Tran&&) = delete;

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
    virtual AnalysisCore& analysisCore() { return tranCore; };
    virtual CoreCoroutine coreCoroutine(bool continuePrevious, ErrorConsumer& errors) {
        return std::move(tranCore.coroutine(continuePrevious, errors));
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
    IStruct<TranParameters> params;

    // Declared before the cores so the references they bind in their init lists
    // (jac, solution, states) refer to fully-constructed members. opCore precedes
    // tranCore because tranCore binds a reference to opCore.
    KluRealMatrix jac; // Jacobian
    VectorRepository<double> opSolution; // OP solution history
    VectorRepository<double> solution; // Solution history
    VectorRepository<double> states; // Circuit states

    DelayLines delayLines_;
    DelayMatrixBindings<double*> delayBindings_;

    OperatingPointCore opCore;
    TranCore tranCore;
};

}

#endif
