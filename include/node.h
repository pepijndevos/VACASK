#ifndef __NODE_DEFINED
#define __NODE_DEFINED

#include <type_traits>
#include "flags.h"
#include "identifier.h"
#include "common.h"


namespace NAMESPACE {

enum class NodeFlags : uint8_t {
    // Legacy (SPICE) tolerance management
    // Node type mask (for bits that define node type)
    NodeTypeMask = 1,
    // In SPICE tolerance mode use
    //   vntol, fluxtol for this node's unknown
    //   abstol, chgtol for this node's equation
    PotentialNode = 0,
    // In SPICE tolerance mode use
    //   abstol, chgtol for this node's unknown
    //   vntol, fluxtol for this node's equation
    FlowNode = 1,

    // This node can be shunted
    Shuntable = 2, 
    // Skip residual convergence check for corresponding equation
    ResidualCheck = 4, 
    // Internal device node, avoid them when choosing representative nodes for unknowns
    InternalDeviceNode = 8,
    // Ground node, in node ordering ground nodes come before all others
    Ground = 16,
};
DEFINE_FLAG_OPERATORS(NodeFlags);

class Node : public FlagBase<NodeFlags> {
public:
    Node(Id name, Flags f);
    ~Node();

    Node           (const Node&)  = delete;
    Node           (      Node&&) = delete;
    Node& operator=(const Node&)  = delete;
    Node& operator=(      Node&&) = delete;

    Id name() { return name_; };
    
    void setUnknownIndex(UnknownIndex u) { unknown_=u; };
    UnknownIndex unknownIndex() { return unknown_; };

    RefCountIndex incRef();
    RefCountIndex decRef();
    
private:
    Id name_;
    Flags flags_;
    UnknownIndex unknown_;
    RefCountIndex refCnt_;
};

}

#endif
