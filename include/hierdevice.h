#ifndef __HIERDEVICE_DEFINED
#define __HIERDEVICE_DEFINED

#include <vector>
#include "identifier.h"
#include "parseroutput.h"
#include "devbase.h"
#include "common.h"


namespace NAMESPACE {

class Circuit;

class HierarchicalModel;

class HierarchicalDevice : public Device {
public:
    friend class HierarchicalModel;

    HierarchicalDevice(Id name, Status& s=Status::ignore);
    
    HierarchicalDevice           (const HierarchicalDevice&)  = delete;
    HierarchicalDevice           (      HierarchicalDevice&&) = default;
    HierarchicalDevice& operator=(const HierarchicalDevice&)  = delete;
    HierarchicalDevice& operator=(      HierarchicalDevice&&) = default;

    bool operator==(const Device& other) const & override;
    bool isHierarchical() const override { return true; }; 
    // Generic model creation is not allowed, constructor must be used
    Model* createModel(Circuit& circuit, Instance* parentInstance, RpnEvaluator& evaluator, const PTModel& parsedSubcircuit, Status& s=Status::ignore) override {
        DBGCHECK(true, "Internal error, requested generic model creation from hierarchical device.");
        return nullptr;
    }
    void dump(int indent, std::ostream& os) const override;
};


class HierarchicalInstance;

class HierarchicalModel : public Model {
public:
    friend class HierarchicalInstance;

    // PTSubcircuitDefinition is derived from PTModel. 
    // Therefore we store a reference to PTModel in Model and static_cast it
    // to PTSubcircuitDefinition, when we need it. 
    // The model is added to the list of models of the given device so that when
    // a device is destroyed all the models in the list are destroyed, too. 
    // HierarchicalModel(HierarchicalDevice* dev, Id name, Status& s=Status::ignore);
    HierarchicalModel(HierarchicalDevice* dev, Id name, Instance* parentInstance, HierarchicalModel* parentModel, const std::string childrenPathPrefix, const PTSubcircuitDefinition& parsedSubcircuit, Status& s=Status::ignore);
    
    HierarchicalModel           (const HierarchicalModel&)  = delete;
    HierarchicalModel           (      HierarchicalModel&&) = default;
    HierarchicalModel& operator=(const HierarchicalModel&)  = delete;
    HierarchicalModel& operator=(      HierarchicalModel&&) = default;

    ParameterIndex parameterCount() const override { 
        return static_cast<const PTSubcircuitDefinition&>(parsedModel_).parameters().valueCount(); 
    };
    std::tuple<ParameterIndex, bool> parameterIndex(Id name) const override;
    Id parameterName(ParameterIndex ndx) const override;
    std::tuple<Value::Type,bool> parameterType(ParameterIndex ndx, Status& s=Status::ignore) const override;
    bool getParameter(ParameterIndex ndx, Value& v, Status& s=Status::ignore) const override;
    std::tuple<bool,bool> setParameter(ParameterIndex ndx, const Value& v, Status& s=Status::ignore) override;
    Instance* createInstance(Circuit& circuit, Instance* parentInstance, RpnEvaluator& evaluator, Context* externalContext, const PTInstance& ptInstance, InstantiationData& idata, Status& s=Status::ignore) override;
    void dump(int indent, std::ostream& os) const override;

    // Device access (as HierarchicalDevice)
    HierarchicalDevice* device() { return static_cast<HierarchicalDevice*>(device_); };
    const HierarchicalDevice* device() const { return static_cast<HierarchicalDevice*>(device_); };
    
    // Get terminal count
    TerminalIndex terminalCount() const { 
        return static_cast<const PTSubcircuitDefinition&>(parsedModel_).terminals().size(); 
    };
    
    // Find terminal by name
    // Return value: terminal index, found
    std::tuple<TerminalIndex, bool> terminalIndex(Id nodeName) const;

    // Terminal name
    Id terminalName(TerminalIndex ndx) const;

    // Expose parser table
    const PTSubcircuitDefinition& definition() const { return static_cast<const PTSubcircuitDefinition&>(parsedModel_); };

    // Definition hierarchy, translate child model
    Id translate(Id childModel) {
        if (childrenPathPrefix_.size()==0) {
            // Toplevel instance has an empty path, does not prepend it to its children
            return childModel;
        } else {
            // Other instances prepend their name to their children
            return childrenPathPrefix_+"::"+std::string(childModel);
        }
    };
    
protected:
    // Build a map from terminal name to terminal index
    bool buildTerminalMap(Status& s=Status::ignore);

    // Build a map from parameter name to parameter index
    bool buildParameterMap(Status& s=Status::ignore);

private:
    std::unordered_map<Id,TerminalIndex> terminalMap;
    std::unordered_map<Id,ParameterIndex> parameterMap;
    // Parent subcircuit definition - set at elaboration 
    // This is the hierarchical definition parent. 
    HierarchicalModel* parentDefinition_;
    std::string childrenPathPrefix_;
};


// Dangling terminal policy: connect dangling terminals to internal nodes
class HierarchicalInstance : public Instance {
public:
    friend class HierarchicalModel;

    enum class Build { Models, Instances };

    // The instance is added to the list of instances of the given model so that when
    // a model is destroyed all the instances in the list are destroyed, too. 
    HierarchicalInstance(HierarchicalModel* mod, Id name, Instance* parentInstance, const PTInstance& parsedInstance, Status& s=Status::ignore);
    ~HierarchicalInstance() override;

    HierarchicalInstance           (const HierarchicalInstance&)  = delete;
    HierarchicalInstance           (      HierarchicalInstance&&) = default;
    HierarchicalInstance& operator=(const HierarchicalInstance&)  = delete;
    HierarchicalInstance& operator=(      HierarchicalInstance&&) = default;

    ParameterIndex parameterCount() const override { return model()->parameterCount(); };
    std::tuple<ParameterIndex, bool> parameterIndex(Id name) const override;
    Id parameterName(ParameterIndex ndx) const override;
    std::tuple<Value::Type,bool> parameterType(ParameterIndex ndx, Status& s=Status::ignore) const override;
    bool getParameter(ParameterIndex ndx, Value& v, Status& s=Status::ignore) const override;
    std::tuple<bool,bool> setParameter(ParameterIndex ndx, const Value& v, Status& s=Status::ignore) override;
    // Principal parameter is the first parameter if there are any parameters
    std::tuple<ParameterIndex, bool> principalParameterIndex() const override { return std::make_tuple(0, parameterCount()>0);};
    TerminalIndex staticNodeCount() const override { return model()->terminalCount(); };
    TerminalIndex terminalCount() const override { return model()->terminalCount(); };
    std::tuple<TerminalIndex, bool> nodeIndex(Id name) const override { return model()->terminalIndex(name); };
    Id nodeName(TerminalIndex ndx) const override { return model()->terminalName(ndx); };
    bool bindNode(TerminalIndex n, Node* node, bool dangling=false, Status& s=Status::ignore) override;
    Node* terminal(TerminalIndex n, Status& s=Status::ignore) const override;
    bool unbindTerminals(Circuit& circuit, Status& s=Status::ignore) override;
    bool unbindInternalNodes(Circuit& cir, Status& s=Status::ignore) override;
    std::vector<Instance*>* childInstances() override { return &childInstances_; };
    std::vector<Model*>* childModels() override { return &childModels_; };
    Id translateNode(Circuit& circuit, Id node) override;
    void dump(int indent, const Circuit& circuit, std::ostream& os) const override;

    // Model access (as HierarchicalModel)
    HierarchicalModel* model() { return static_cast<HierarchicalModel*>(model_); };
    const HierarchicalModel* model() const { return static_cast<HierarchicalModel*>(model_); };
    
    // Add child instances and models
    void addChild(Instance* child) override;
    void addChild(Model* child) override;
    
    // Context handling, parameter propagation, hierarchy building
    std::tuple<bool, size_t> enterContext(Circuit& circuit, Context* externalContext, bool addToPath, bool rebuild, Status& s=Status::ignore) override;
    bool propagateParameters(Circuit& circuit, RpnEvaluator& evaluator, Status& s=Status::ignore) override;
    bool deleteHierarchy(Circuit& circuit, Status& s=Status::ignore) override;
    bool buildHierarchy(Circuit& circuit, RpnEvaluator& evaluator, InstantiationData& idata, Status& s=Status::ignore) override;
    std::tuple<bool, bool> subhierarchyChanged(Circuit& circuit, RpnEvaluator& evaluator, Status& s=Status::ignore) override; 

protected:
    bool buildBlock(Circuit& circuit, RpnEvaluator& evaluator, InstantiationData& idata, const PTBlock& block, Build what=Build::Instances, Status& s=Status::ignore);
    bool recomputeBlockConditions(Circuit& circuit, RpnEvaluator& evaluator, std::vector<const PTBlock*>& newBlocks, Status& s=Status::ignore); 
    
private:
    // OK, cond value
    std::tuple<bool, bool> recomputeBlockConditionsWorker(Circuit& circuit, const PTBlockSequenceEntry& seqEntry, RpnEvaluator& evaluator, std::vector<const PTBlock*>& newBlocks, Status& s=Status::ignore); 

    std::vector<Node*> connections;
    TerminalIndex connectedTerminalCount;
    Value* parameters;
    std::vector<Instance*> childInstances_;
    std::vector<Model*> childModels_;
    // Active conditional blocks
    std::vector<const PTBlock*> activeBlocks;
};

}

#endif
