#ifndef __ANHBAC_DEFINED
#define __ANHBAC_DEFINED

#include "an.h"
#include "corehb.h"
#include "corehbac.h"
#include "parameterized.h"
#include "klubsmatrix.h"
#include "common.h"


namespace NAMESPACE {

// HBAC analysis
class HBAC : public Analysis {
public:
    typedef HBACParameters Parameters;

    HBAC(Id name, Circuit& circuit, PTAnalysis& ptAnalysis);

    HBAC           (const HBAC&)  = delete;
    HBAC           (      HBAC&&) = delete;
    HBAC& operator=(const HBAC&)  = delete;
    HBAC& operator=(      HBAC&&) = delete;

    virtual Parameterized& parameters() { return params; };
    virtual const Parameterized& parameters() const { return params; };

    static Analysis* create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s=Status::ignore);

    virtual void dump(std::ostream& os) const;

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

    virtual AnalysisCore& analysisCore() { return hbacCore; };
    virtual CoreCoroutine coreCoroutine(bool continuePrevious, ErrorConsumer& errors) {
        return std::move(hbacCore.coroutine(continuePrevious, errors));
    };

    virtual bool finalizeOutputs(ErrorConsumer& errors);
    virtual bool deleteOutputs(ErrorConsumer& errors);

    virtual size_t analysisStateStorageSize() const;
    virtual size_t allocateAnalysisStateStorage(size_t n);
    virtual void deallocateAnalysisStateStorage(size_t n=0);
    virtual bool storeState(size_t ndx, bool storeDetails=true);
    virtual bool restoreState(size_t ndx);
    virtual void makeStateIncoherent(size_t ndx);

    // Add HB operating point output descriptor(s), returns ok, handled
    std::tuple<bool, bool> resolveHbSave(const PTSave& save, bool verify, ErrorConsumer& errors);

    IStruct<HBACParameters> params;

    // Declared before the cores so the references they bind in their init lists
    // (hbCore: jacColoc/jac/solution; hbacCore: jacSpec/hbSolution/acMatrix/acSolution)
    // refer to fully-constructed members. hbCore precedes hbacCore because hbacCore
    // binds a reference to hbCore.
    KluBlockSparseRealMatrix jacColoc;
    KluBlockSparseRealMatrix jac;
    VectorRepository<double> solution;
    KluBlockSparseComplexMatrix jacSpec;
    VectorRepository<Complex> hbSolution;
    KluBlockSparseComplexMatrix acMatrix;
    Vector<Complex> acSolution;

    DelayLines delayLines_;
    DelayMatrixBindings<DenseMatrixView<double>> hbDelayBindings_;
    DelayMatrixBindings<DenseMatrixView<Complex>> hbacDelayBindings_;

    HBCore hbCore;
    HBACCore hbacCore;
};

}

#endif
