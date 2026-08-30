#ifndef __DEVVISRC_DEFINED
#define __DEVVISRC_DEFINED

#include "value.h"
#include "devbuiltin.h"
#include "common.h"


namespace NAMESPACE {

enum class IndependentSourceType : char { Dc, Sine, Pulse, Sffm, Exp, Pwl, Am, Fm };

struct DevSourceModelParams {
    DevSourceModelParams();
};

struct DevSourceInstanceParams {
    // $mfactor is a real parameter in OSDI, therefore we also make it real
    
    // Common
    Real mfactor; // Number of parallel instances 
    Id type;      // waveform type (dc, pulse, exp, sine, am, fm, pwl)
    Real delay;   // waveform delay in unstretched time for type=sine, am, fm, exp, pulse, pwl
    Real tdphase; // initial waveform phase for type=sine, am, fm, pwl
    Real period;  // waveform period in unstretched time for type=pulse, pwl
    
    // DC
    Real dc;      // dc value for type=dc
    
    // Pulse
    Real val0;    // inital pulse value
    Real val1;    // final pulse value
    // delay      // pulse delay
    Real rise;    // pulse rise time
    Real fall;    // pulse fall time
    Real width;   // pulse width
    // period     // pulse period
    
    // Sine
    // delay      // sine delay
    Real sinedc;  // offset
    Real ampl;    // amplitude
    Real freq;    // frequency
    // tdphase    // initial phase
    Real theta;   // sine damping
    
    // Exp
    // val0, val1, delay .. same meaning as for type=pulse
    Real td2;  // delay from start to beginning of decay
    Real tau1; // rise time constant
    Real tau2; // decay time constant
    
    // Pwl
    RealVector wave; // waveform given as pairs of (x,y) points
    Real offset;  // offset added to waveform
    Real scale;   // vertical scaling factor
    Real stretch; // horizontal stretch factor
    // delay, period, tdphase
    Id breakpt;     // yes, no, auto
                    // aperiodic pwl waveforms in auto mode always generate 
                    // a breakpoint at the first and the last point
    Real slopetol;  // break=auto: absolute slope change tolerance, default=0
    Real sloperel;  // break=auto: relative slope change tolerance, default=0
    Real slopeglob; // break=auto: global relative slope change tolerance, default=0
    
    // AM, FM
    Real modfreq;
    Real modphase;
    Real modindex; 
    // AC, DC incremental
    Real mag;
    Real phase; // degrees (only for AC)
    // (Quasi)periodic small-signal excitation
    ValueVector spur; // spurs where small signal excitations are inserted - list holding 
                       // - reals (frequency), 
                       // - integer vectors (tone weights)
                       // Default is empty list {} - no excitation
    // If the vector is shorter than the number of spurs, zeros are assumed. 
    // If it is longer than the number of spurs, the extra components are ignored. 
    RealVector smag;   // magnitudes corresponding to spurs
                       // Empty vector by default. 
    RealVector sphase; // phases in degrees corresponding to spurs
                       // Empty vector by default. 
    
    DevSourceInstanceParams();
};


struct DevVSourceInstanceData {
    IndependentSourceType typeCode;

    UnknownIndex uP;
    UnknownIndex uN;
    UnknownIndex uFlow;
    double* jacFlowP;
    double* jacFlowN;
    double* jacPFlow;
    double* jacNFlow;

    double flowResidual;
    double eqResidual;

    double v;  // Voltage across instance
    double i;  // Current of one parallel instances

    RealVector slopes; 
    std::vector<bool> breakEnabled;
    std::vector<size_t> nextBreakIndex;
    size_t npts;
    size_t lastPointIndex; // Helps find waveform points faster

    DevVSourceInstanceData();
};

struct DevISourceInstanceData {
    IndependentSourceType typeCode;

    UnknownIndex uP;
    UnknownIndex uN;
    
    double flowResidual;
    
    double i;  // Current of one parallel instance
    double v;  // Voltage across instance

    RealVector slopes;
    std::vector<bool> breakEnabled;
    std::vector<size_t> nextBreakIndex;
    size_t npts;
    size_t lastPointIndex;

    DevISourceInstanceData();
};

template<typename InstanceParams, typename InstanceData> 
std::tuple<double, double> sourceCompute(const InstanceParams& params, InstanceData& data, double time);


using BuiltinVSource = BuiltinDevice<DevSourceModelParams, DevSourceInstanceParams, DevVSourceInstanceData>;
using BuiltinVSourceModel = BuiltinModel<DevSourceModelParams, DevSourceInstanceParams, DevVSourceInstanceData>;
using BuiltinVSourceInstance = BuiltinInstance<DevSourceModelParams, DevSourceInstanceParams, DevVSourceInstanceData>;

using BuiltinISource = BuiltinDevice<DevSourceModelParams, DevSourceInstanceParams, DevISourceInstanceData>;
using BuiltinISourceModel = BuiltinModel<DevSourceModelParams, DevSourceInstanceParams, DevISourceInstanceData>;
using BuiltinISourceInstance = BuiltinInstance<DevSourceModelParams, DevSourceInstanceParams, DevISourceInstanceData>;


// Specializations of methods - need to write these prototypes otherwise 
// gcc will optimize them out in Release build

template<> void BuiltinVSource::defineInternals();
template<> void BuiltinISource::defineInternals();
template<> bool BuiltinVSource::isSource() const;
template<> bool BuiltinISource::isSource() const;
template<> bool BuiltinVSource::isVoltageSource() const;
template<> std::tuple<ParameterIndex, bool> BuiltinVSourceInstance::principalParameterIndex() const;
template<> std::tuple<ParameterIndex, bool> BuiltinISourceInstance::principalParameterIndex() const;
template<> bool BuiltinVSourceInstance::deleteHierarchy(Circuit& circuit, Status& s);
template<> bool BuiltinVSourceInstance::buildHierarchy(Circuit& circuit, RpnEvaluator& evaluator, InstantiationData& idata, Status& s);
template<> bool BuiltinISourceInstance::buildHierarchy(Circuit& circuit, RpnEvaluator& evaluator, InstantiationData& idata, Status& s);
template<> std::tuple<EquationIndex,EquationIndex> BuiltinVSourceInstance::sourceExcitation(Circuit& circuit) const;
template<> std::tuple<UnknownIndex,UnknownIndex> BuiltinVSourceInstance::sourceResponse(Circuit& circuit) const;
template<> double BuiltinVSourceInstance::scaledUnityExcitation() const;
template<> double BuiltinVSourceInstance::responseScalingFactor() const;
template<> std::tuple<const ValueVector&, const RealVector&, const RealVector&> BuiltinVSourceInstance::spur() const;
template<> std::tuple<EquationIndex,EquationIndex> BuiltinISourceInstance::sourceExcitation(Circuit& circuit) const;
template<> std::tuple<UnknownIndex,UnknownIndex> BuiltinISourceInstance::sourceResponse(Circuit& circuit) const;
template<> double BuiltinISourceInstance::scaledUnityExcitation() const;
template<> double BuiltinISourceInstance::responseScalingFactor() const;
template<> std::tuple<const ValueVector&, const RealVector&, const RealVector&> BuiltinISourceInstance::spur() const;
template<> bool BuiltinVSourceInstance::getOutvar(ParameterIndex ndx, Value& v, Status& s) const;
template<> bool BuiltinISourceInstance::getOutvar(ParameterIndex ndx, Value& v, Status& s) const;
template<> std::tuple<bool, OutputSource> BuiltinVSourceInstance::outvarOutputSource(ParameterIndex ndx) const;
template<> std::tuple<bool, OutputSource> BuiltinISourceInstance::outvarOutputSource(ParameterIndex ndx) const;
template<> std::tuple<bool, bool, bool> BuiltinVSourceInstance::setupCore(Circuit& circuit, CommonData& commons, DeviceRequests* devReq, Status& s);
template<> std::tuple<bool, bool, bool> BuiltinISourceInstance::setupCore(Circuit& circuit, CommonData& commons, DeviceRequests* devReq, Status& s);
template<> bool BuiltinVSourceInstance::setStaticTolerancesCore(Circuit& circuit, CommonData& commons, Status& s);
template<> bool BuiltinISourceInstance::setStaticTolerancesCore(Circuit& circuit, CommonData& commons, Status& s);
template<> bool BuiltinVSourceInstance::populateStructuresCore(Circuit& circuit, Status& s);
template<> bool BuiltinISourceInstance::populateStructuresCore(Circuit& circuit, Status& s);
template<> bool BuiltinVSourceInstance::bindCore(
    Circuit& circuit, 
    KluMatrixAccess* matResist, Component compResist, const std::optional<MatrixEntryPosition>& mepResist, 
    KluMatrixAccess* matReact, Component compReact, const std::optional<MatrixEntryPosition>& mepReact, 
    DelayLines* delayLines, 
    ErrorConsumer& ec
);
template<> bool BuiltinISourceInstance::bindCore(
    Circuit& circuit, 
    KluMatrixAccess* matResist, Component compResist, const std::optional<MatrixEntryPosition>& mepResist, 
    KluMatrixAccess* matReact, Component compReact, const std::optional<MatrixEntryPosition>& mepReact, 
    DelayLines* delayLines, 
    ErrorConsumer& ec
);

template<> bool BuiltinVSourceInstance::evalCore(Circuit& circuit, CommonData& commons, EvalSetup& evalSetup, ErrorConsumer& errors);    
template<> bool BuiltinISourceInstance::evalCore(Circuit& circuit, CommonData& commons, EvalSetup& evalSetup, ErrorConsumer& errors);    
template<> bool BuiltinVSourceInstance::loadCore(Circuit& circuit, CommonData& commons, LoadSetup& loadSetup, ErrorConsumer& errors);
template<> bool BuiltinISourceInstance::loadCore(Circuit& circuit, CommonData& commons, LoadSetup& loadSetup, ErrorConsumer& errors);

}

#endif

