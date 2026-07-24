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
    virtual bool addCoreOutputDescriptors(Status& s);
    virtual bool resolveSave(const PTSave& save, bool verify, Status& s=Status::ignore);
    virtual bool addDefaultOutputDescriptors(Status& s);
    virtual void clearOutputDescriptors();
    virtual bool resolveOutputDescriptors(bool strict, Status& s=Status::ignore);

    virtual std::tuple<bool, bool> preMapping(Status& s=Status::ignore);
    virtual bool populateStructures(Status& s=Status::ignore);
    virtual bool rebuildCores(Status& s=Status::ignore);
    virtual bool initializeOutputs(Status& s=Status::ignore);

    virtual AnalysisCore& analysisCore() { return hbacCore; };
    virtual CoreCoroutine coreCoroutine(bool continuePrevious) {
        return std::move(hbacCore.coroutine(continuePrevious));
    };
    virtual bool formatCoreError(Status& s=Status::ignore) {
        return hbacCore.formatError(s);
    };

    virtual bool finalizeOutputs(Status& s=Status::ignore);
    virtual bool deleteOutputs(Status& s=Status::ignore);

    virtual size_t analysisStateStorageSize() const;
    virtual size_t allocateAnalysisStateStorage(size_t n);
    virtual void deallocateAnalysisStateStorage(size_t n=0);
    virtual bool storeState(size_t ndx, bool storeDetails=true);
    virtual bool restoreState(size_t ndx);
    virtual void makeStateIncoherent(size_t ndx);

    // Add HB operating point output descriptor(s), returns ok, handled
    std::tuple<bool, bool> resolveHbSave(const PTSave& save, bool verify, Status& s=Status::ignore);

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

    HBCore hbCore;
    HBACCore hbacCore;
};

}

#endif
