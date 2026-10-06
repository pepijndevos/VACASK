#ifndef __OSDIINSTANCE_DEFINED
#define __OSDIINSTANCE_DEFINED

#include <string>
#include "status.h"
#include "osdimodel.h"
#include "devbase.h"
#include "common.h"


namespace NAMESPACE {

SIMPLE_ERRORCLASS(OsdiBindMatrixWrongType, "Matrix is of incorrect type.");

ERRORCLASS(OsdiUnsupportedTranJacOffs)
    Id instance;
    OsdiUnsupportedTranJacOffs(Id instance) : instance(instance) {}
    std::string format() const {
        return "OSDI unsupported transient Jacobian load with offset in instance '" + std::to_string(instance) + "'.";
    }
END_ERRORCLASS(OsdiUnsupportedTranJacOffs);

ERRORCLASS(OsdiEvalFailed)
    Id instance;
    OsdiEvalFailed(Id instance) : instance(instance) {}
    std::string format() const {
        return "OSDI evaluation failed for instance '" + std::to_string(instance) + "'.";
    }
END_ERRORCLASS(OsdiEvalFailed);

ERRORCLASS(OsdiLoadFailed)
    Id instance;
    OsdiLoadFailed(Id instance) : instance(instance) {}
    std::string format() const {
        return "OSDI load failed for instance '" + std::to_string(instance) + "'.";
    }
END_ERRORCLASS(OsdiLoadFailed);

ERRORCLASS(OsdiDelayChangeDetected)
    Id instance;
    OsdiDelayChangeDetected(Id instance) : instance(instance) {}
    std::string format() const {
        return "OSDI delay change detected in instance '" + std::to_string(instance) + "'.";
    }
END_ERRORCLASS(OsdiDelayChangeDetected);

ERRORCLASS(OsdiNoiseExponentChangeDetected)
    Id instance;
    Id srcName;
    OsdiNoiseExponentChangeDetected(Id instance, Id srcName) : instance(instance), srcName(srcName) {}
    std::string format() const {
        return "OSDI noise exponent change detected in instance '" + std::to_string(instance) + "', source '" + std::string(srcName) + "'.";
    }
END_ERRORCLASS(OsdiNoiseExponentChangeDetected);

class Circuit;

// Dangling terminal policy: connect dangling terminals to internal nodes
class OsdiInstance : public Instance {
public:
    friend class OsdiDevice;

    // The instance is added to the list of instances of the given model so that when
    // a model is destroyed all the instances in the list are destroyed, too. 
    OsdiInstance(OsdiModel* mod, Id name, Instance* parentInstance, const PTInstance& parsedInstance, Status& s=Status::ignore);
    ~OsdiInstance();

    OsdiInstance           (const OsdiInstance&)  = delete;
    OsdiInstance           (      OsdiInstance&&) = default;
    OsdiInstance& operator=(const OsdiInstance&)  = delete;
    OsdiInstance& operator=(      OsdiInstance&&) = default;

    ParameterIndex parameterCount() const override;
    std::tuple<ParameterIndex, bool> parameterIndex(Id name) const override;
    Id parameterName(ParameterIndex ndx) const override;
    std::tuple<Value::Type,bool> parameterType(ParameterIndex ndx, Status& s=Status::ignore) const override;
    bool getParameter(ParameterIndex ndx, Value& v, Status& s=Status::ignore) const override;
    std::tuple<bool,bool> setParameter(ParameterIndex ndx, const Value& v, Status& s=Status::ignore) override;
    virtual std::tuple<bool,bool> parameterGiven(ParameterIndex ndx, Status& s=Status::ignore) const;
    
    // First parameter is $mfactor, second is principal parameter (if it exists)
    std::tuple<ParameterIndex, bool> principalParameterIndex() const override { return std::make_tuple(1, parameterCount()>1); };
    TerminalIndex staticNodeCount() const override;
    TerminalIndex terminalCount() const override;
    std::tuple<TerminalIndex, bool> nodeIndex(Id name) const override;
    Id nodeName(TerminalIndex ndx) const override;
    bool bindNode(TerminalIndex n, Node* node, bool dangling=false, Status& s=Status::ignore) override;
    Node* terminal(TerminalIndex n, Status& s=Status::ignore) const override;
    bool unbindTerminals(Circuit& circuit, Status& s=Status::ignore) override;
    bool unbindInternalNodes(Circuit& cir, Status& s=Status::ignore) override;
    bool propagateParameters(Circuit& circuit, RpnEvaluator& evaluator, Status& s=Status::ignore) override;
    bool deleteHierarchy(Circuit& circuit, Status& s=Status::ignore) override;
    virtual bool buildHierarchy(Circuit& circuit, RpnEvaluator& evaluator, Status& s=Status::ignore);
    std::tuple<EquationIndex,EquationIndex> sourceExcitation(Circuit& circuit) const override;
    std::tuple<UnknownIndex,UnknownIndex> sourceResponse(Circuit& circuit) const override;
    ParameterIndex outvarCount() const override;
    std::tuple<ParameterIndex, bool> outvarIndex(Id name) const override;
    Id outvarName(ParameterIndex ndx) const override;
    std::tuple<Value::Type,bool> outvarType(ParameterIndex ndx, Status& s=Status::ignore) const override;
    bool getOutvar(ParameterIndex ndx, Value& v, Status& s=Status::ignore) const override; 
    std::tuple<bool, OutputSource> outvarOutputSource(ParameterIndex ndx) const override;
    virtual std::tuple<bool, bool, bool> setup(Circuit& circuit, CommonData& commons, bool force, DeviceRequests* devReq, Status& s=Status::ignore);
    virtual bool setStaticTolerances(Circuit& circuit, CommonData& commons, Status& s=Status::ignore);
    void dump(int indent, const Circuit& circuit, std::ostream& os) const override;

    // Model access (as OsdiModel)
    OsdiModel* model() { return static_cast<OsdiModel*>(model_); };
    const OsdiModel* model() const { return static_cast<const OsdiModel*>(model_); };

    // Model core access
    void* core() { return core_; };
    const void* core() const { return core_; };

    // Noise API
    ParameterIndex noiseSourceCount() const override { return model()->device()->noiseSourceCount(); };
    ParameterIndex uniqueNoiseSourceCount() const override { return model()->device()->uniqueNoiseSourceCount(); };
    ParameterIndex modulatedNoiseSourceCount() const override { return model()->device()->modulatedNoiseSourceCount(); };
    Id noiseSourceName(ParameterIndex ndx) const override { return model()->device()->noiseSourceName(ndx); };
    std::tuple<ParameterIndex, bool> uniqueNoiseSourceIndex(Id name) const override { return model()->device()->uniqueNoiseSourceIndex(name); }
    std::tuple<EquationIndex, EquationIndex> noiseExcitation(Circuit& cir, ParameterIndex ndx) const override;
    bool loadNoise(Circuit& circuit, double freq, double* noiseDensity) override;
    NoiseType noiseSourceType(ParameterIndex ndx) const override { return model()->device()->noiseSourceType(ndx); };
    GlobalStorageIndex noiseModulationBase() const override { return offsNoiseModulation; };
    bool loadNoiseParameters(Circuit& circuit, double* power, double* exponent) override { 
        model()->device()->descriptor()->load_noise_params(core(), model()->core(), power, exponent);
        return true;
    }; 

    

    // Helpers for inlining in device, model, and instance virtual functions
    std::tuple<bool, bool, bool> setupWrapper(Circuit& circuit, OsdiSimParas& sp, double temp, bool force, DeviceRequests* devReq, Status& s=Status::ignore);
    bool setStaticTolerancesCore(Circuit& circuit, CommonData& commons, Status& s=Status::ignore);
    bool collapseNodesCore(Circuit& circuit, Status& s);
    bool populateStructuresCore(Circuit& circuit, Status& s=Status::ignore);
    bool bindCore(
        Circuit& circuit, 
        CSCMatrixAccess* matResist, Component compResist, const std::optional<MatrixEntryPosition>& mepResist, 
        CSCMatrixAccess* matReact, Component compReact, const std::optional<MatrixEntryPosition>& mepReact, 
        DelayLines* delayLines, 
        ErrorConsumer& ec
    );
    bool inputBypassCheckCore(Circuit& circuit, CommonData& commons, EvalSetup& evalSetup);
    bool outputBypassCheckCore(Circuit& circuit, CommonData& commons, EvalSetup& evalSetup);
    bool evalCore(Circuit& circuit, CommonData& commons, OsdiSimInfo& simInfo, EvalSetup& evalSetup, ErrorConsumer& errors);
    bool loadCore(Circuit& circuit, CommonData& commons, LoadSetup& loadSetup, ErrorConsumer& errors);
    
protected:
    OsdiFile::OsdiCollapsedNodesIndex collapsedNodesPatternSize() const { return model()->device()->collapsedNodesPatternSize(); };
    bool* collapsedNodesPattern() const { return getDataPtr<bool*>(core_, model()->device()->descriptor()->collapsed_offset); };
    OsdiFile::OsdiNodeIndex* nodeMappingArray() { 
        return getDataPtr<OsdiFile::OsdiNodeIndex*>(core_, model()->device()->descriptor()->node_mapping_offset);
    };
    double** resistiveJacobianPointers() { return getDataPtr<double**>(core_, model()->device()->descriptor()->jacobian_ptr_resist_offset); }; 
    double** reactiveJacobianPointer(OsdiFile::JacobianEntryIndex ndx) { 
        auto& entry = model()->device()->descriptor()->jacobian_entries[ndx];
        if (entry.react_ptr_off==OsdiFile::noJacobianEntry) {
            return nullptr;
        } else {
            return getDataPtr<double**>(core_, entry.react_ptr_off);
        }
    };
    // State index table of instance
    // Contains uint32_t offsets based on a pointer passed to eval()
    // in simInfo.prev_state and simInfo.next_state arrays of doubles
    OsdiFile::OsdiStateIndex* stateIndexTable() {
        return getDataPtr<OsdiFile::OsdiStateIndex*>(core_, model()->device()->descriptor()->state_idx_off);
    };
    
private:
    void* core_;
    std::vector<Node*> nodes_;
    TerminalIndex connectedTerminalCount;
    GlobalStorageIndex offsStates;
    GlobalStorageIndex offsDeviceStates;
    GlobalStorageIndex offsDelayHistory;
    GlobalStorageIndex offsNoiseModulation;
};

}

#endif
