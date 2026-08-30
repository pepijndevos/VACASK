#ifndef __ANSWEEP_DEFINED
#define __ANSWEEP_DEFINED

#include "value.h"
#include "flags.h"
#include "identifier.h"
#include "options.h"
#include "parseroutput.h"
#include "errorstack.h"
#include "progress.h"
#include "common.h"
#include <memory>


namespace NAMESPACE {

//
// Sweep errors
//
// ScalarSweep pushes the leaf cause; ParameterSweeper wraps it with the
// offending sweep name and follows up with a SweepLocation entry.
//

// Location of the offending sweep directive, pushed after a ParameterSweeper error.
ERRORCLASS(SweepLocation)
    Loc location;
    SweepLocation(Loc location) : location(location) {}
    std::string format() const { return location.toString(); }
END_ERRORCLASS(SweepLocation);

// ScalarSweep setup - leaf causes
SIMPLE_ERRORCLASS(SweepMultipleSpecs, "Sweep needs to specify only one of the following: values, mode, step.");

SIMPLE_ERRORCLASS(SweepUnknownMode, "Unknown sweep mode.");

SIMPLE_ERRORCLASS(SweepNoSpec, "Sweep needs to specify values, mode, or step.");

SIMPLE_ERRORCLASS(SweepStepTooSmall, "Sweep step too small.");

SIMPLE_ERRORCLASS(SweepBadSteppedRange, "Bad stepped sweep range. Check from, to, and step.");

SIMPLE_ERRORCLASS(SweepValuesNotVector, "Sweep values must be a vector.");

SIMPLE_ERRORCLASS(SweepValuesEmpty, "Values vector must have at least one component.");

SIMPLE_ERRORCLASS(SweepTooManyValues, "Too many sweep values given.");

SIMPLE_ERRORCLASS(SweepNegativeIntervals, "Number of intervals (specified by points parameter) must be nonnegative.");

SIMPLE_ERRORCLASS(SweepNonPositivePoints, "Number of sweep points must be greater than zero.");

SIMPLE_ERRORCLASS(SweepNonPositiveFactor, "Factor must be greater than zero.");

SIMPLE_ERRORCLASS(SweepNonPositiveLogEndpoints, "Starting point and end point of a logarithmic sweep must be greater than zero.");

SIMPLE_ERRORCLASS(SweepTooManyPoints, "Too many points in sweep.");

ERRORCLASS(SweepValueNotReadable)
    long index;
    SweepValueNotReadable(long index) : index(index) {}
    std::string format() const { return "Failed to read sweep value at index " + std::to_string(index) + "."; }
END_ERRORCLASS(SweepValueNotReadable);

// ParameterSweeper - wrappers carrying the offending sweep name
ERRORCLASS(SweepSettingsEvalError)
    Id sweep;
    SweepSettingsEvalError(Id sweep) : sweep(sweep) {}
    std::string format() const { return "Error in settings evaluation for sweep '" + std::string(sweep) + "'."; }
END_ERRORCLASS(SweepSettingsEvalError);

ERRORCLASS(SweepVectorComponentUnsupported)
    Id sweep;
    SweepVectorComponentUnsupported(Id sweep) : sweep(sweep) {}
    std::string format() const { return "Sweep '" + std::string(sweep) + "': vector component sweeps are not supported yet."; }
END_ERRORCLASS(SweepVectorComponentUnsupported);

ERRORCLASS(SweepMultipleFamilies)
    Id sweep;
    SweepMultipleFamilies(Id sweep) : sweep(sweep) {}
    std::string format() const { return "Sweep '" + std::string(sweep) + "': specify only one of the following: global, option, model, instance."; }
END_ERRORCLASS(SweepMultipleFamilies);

ERRORCLASS(SweepSetupFailed)
    Id sweep;
    SweepSetupFailed(Id sweep) : sweep(sweep) {}
    std::string format() const { return "Failed to set up sweep '" + std::string(sweep) + "'."; }
END_ERRORCLASS(SweepSetupFailed);

ERRORCLASS(SweepUpdateFailed)
    Id sweep;
    SweepUpdateFailed(Id sweep) : sweep(sweep) {}
    std::string format() const { return "Failed to update sweep '" + std::string(sweep) + "'."; }
END_ERRORCLASS(SweepUpdateFailed);

ERRORCLASS(SweepVariableNotFound)
    Id sweep;
    Id variable;
    SweepVariableNotFound(Id sweep, Id variable) : sweep(sweep), variable(variable) {}
    std::string format() const { return "Sweep '" + std::string(sweep) + "': variable '" + std::string(variable) + "' not found."; }
END_ERRORCLASS(SweepVariableNotFound);

ERRORCLASS(SweepOptionNotFound)
    Id sweep;
    Id option;
    SweepOptionNotFound(Id sweep, Id option) : sweep(sweep), option(option) {}
    std::string format() const { return "Sweep '" + std::string(sweep) + "': simulator option '" + std::string(option) + "' not found."; }
END_ERRORCLASS(SweepOptionNotFound);

ERRORCLASS(SweepInstanceNameMissing)
    Id sweep;
    SweepInstanceNameMissing(Id sweep) : sweep(sweep) {}
    std::string format() const { return "Sweep '" + std::string(sweep) + "': instance name not given."; }
END_ERRORCLASS(SweepInstanceNameMissing);

ERRORCLASS(SweepModelNotFound)
    Id sweep;
    Id model;
    SweepModelNotFound(Id sweep, Id model) : sweep(sweep), model(model) {}
    std::string format() const { return "Sweep '" + std::string(sweep) + "': model '" + std::string(model) + "' not found."; }
END_ERRORCLASS(SweepModelNotFound);

ERRORCLASS(SweepModelParameterMissing)
    Id sweep;
    SweepModelParameterMissing(Id sweep) : sweep(sweep) {}
    std::string format() const { return "Sweep '" + std::string(sweep) + "': model parameter name not given."; }
END_ERRORCLASS(SweepModelParameterMissing);

ERRORCLASS(SweepModelParameterNotFound)
    Id sweep;
    Id model;
    Id parameter;
    SweepModelParameterNotFound(Id sweep, Id model, Id parameter) : sweep(sweep), model(model), parameter(parameter) {}
    std::string format() const {
        return "Sweep '" + std::string(sweep) + "': parameter '" + std::string(parameter) + "' of model '" + std::string(model) + "' not found.";
    }
END_ERRORCLASS(SweepModelParameterNotFound);

ERRORCLASS(SweepModelParameterBound)
    Id sweep;
    Id model;
    Id parameter;
    SweepModelParameterBound(Id sweep, Id model, Id parameter) : sweep(sweep), model(model), parameter(parameter) {}
    std::string format() const {
        return "Sweep '" + std::string(sweep) + "': parameter '" + std::string(parameter) + "' of model '" + std::string(model) + "' is bound to an expression.";
    }
END_ERRORCLASS(SweepModelParameterBound);

ERRORCLASS(SweepInstanceNotFound)
    Id sweep;
    Id instance;
    SweepInstanceNotFound(Id sweep, Id instance) : sweep(sweep), instance(instance) {}
    std::string format() const { return "Sweep '" + std::string(sweep) + "': instance '" + std::string(instance) + "' not found."; }
END_ERRORCLASS(SweepInstanceNotFound);

ERRORCLASS(SweepNoPrincipalParameter)
    Id sweep;
    Id instance;
    SweepNoPrincipalParameter(Id sweep, Id instance) : sweep(sweep), instance(instance) {}
    std::string format() const { return "Sweep '" + std::string(sweep) + "': instance '" + std::string(instance) + "' has no principal parameter."; }
END_ERRORCLASS(SweepNoPrincipalParameter);

ERRORCLASS(SweepInstanceParameterNotFound)
    Id sweep;
    Id instance;
    Id parameter;
    SweepInstanceParameterNotFound(Id sweep, Id instance, Id parameter) : sweep(sweep), instance(instance), parameter(parameter) {}
    std::string format() const {
        return "Sweep '" + std::string(sweep) + "': parameter '" + std::string(parameter) + "' of instance '" + std::string(instance) + "' not found.";
    }
END_ERRORCLASS(SweepInstanceParameterNotFound);

ERRORCLASS(SweepInstanceParameterBound)
    Id sweep;
    Id instance;
    Id parameter;
    SweepInstanceParameterBound(Id sweep, Id instance, Id parameter) : sweep(sweep), instance(instance), parameter(parameter) {}
    std::string format() const {
        return "Sweep '" + std::string(sweep) + "': parameter '" + std::string(parameter) + "' of instance '" + std::string(instance) + "' is bound to an expression.";
    }
END_ERRORCLASS(SweepInstanceParameterBound);

ERRORCLASS(SweepParameterReadFailed)
    Id sweep;
    SweepParameterReadFailed(Id sweep) : sweep(sweep) {}
    std::string format() const { return "Sweep '" + std::string(sweep) + "': failed to read parameter value."; }
END_ERRORCLASS(SweepParameterReadFailed);

// Details forwarded from a Status-based API called during sweep processing.
ERRORCLASS(SweepValueReadDetail)
    std::string message;
    SweepValueReadDetail(std::string message) : message(std::move(message)) {}
    std::string format() const { return message; }
END_ERRORCLASS(SweepValueReadDetail);

ERRORCLASS(SweepSettingsEvalDetail)
    std::string message;
    SweepSettingsEvalDetail(std::string message) : message(std::move(message)) {}
    std::string format() const { return message; }
END_ERRORCLASS(SweepSettingsEvalDetail);

ERRORCLASS(SweepVariableWriteDetail)
    std::string message;
    SweepVariableWriteDetail(std::string message) : message(std::move(message)) {}
    std::string format() const { return message; }
END_ERRORCLASS(SweepVariableWriteDetail);

ERRORCLASS(SweepParameterWriteDetail)
    std::string message;
    SweepParameterWriteDetail(std::string message) : message(std::move(message)) {}
    std::string format() const { return message; }
END_ERRORCLASS(SweepParameterWriteDetail);


// Sweep settings
typedef struct SweepSettings  {
    Id name {Id()};          // Name of sweep, will appear as a vector 
                             // holding the swept values in the raw file
    Loc location;            // Not exposed 
    Id instance {Id()};      // Name of the instance if sweeping an instance parameter
    Id model {Id()};         // Name of the model if sweeping a model parameter
    Id parameter {Id()};     // Name of the parameter to sweep
                             // if instance/model are not given, sweeps a toplevel
                             // instance parameter
    Id option {Id()};        // Name of simulator option to sweep
    Id variable {Id()};      // Name of circuit variable to sweep
    Int component {-1};      // Not supported yet. 
    Real from {0};           // Starting point
    Real to {0};             // End point
    Real step {0};           // Step size for linear stepped sweep (when mode not given)
    Id mode {Id()};          // Sweep with given number of points 
                             // can be lin (linear), dec (points per decade), 
                             // or oct (points per octave)
    Int points {0};          // Number of sweep points when mode is given
    Value values {0};        // Sweep given values (must be a vector or a list), 
                             // used when from, to, step, mode, and points are not given
    Int continuation {1};    // Use continuation mode for speeding up the sweep 
                             // (1=enabled, 0=disabled), enabled by default
    
    SweepSettings();
    
} SweepSettings;


class ScalarSweep {
public:
    enum class SweepType {
        Stepped, Lin, Log, Value
    };

    ScalarSweep();

    // Index corresponding to point at which the sweep is now (0-based)
    Int valueIndex() const;

    // Reset index to 0
    void reset();

    // Return position (0-based)
    Int at() const;

    // Return number of values
    Int count() const;

    // Advance index by 1, returns true when sweep is exhausted
    bool advance();

    // Computes the sweep value
    bool compute(Value& v, ErrorConsumer& errors) const;

    // Format progress
    std::string progress() const;

    // Set up a sweep
    bool setupSteppedSweep(Real from_, Real to_, Real step_, ErrorConsumer& errors);
    bool setupValueSweep(const Value& values, ErrorConsumer& errors);
    bool setupLinearSweep(Real from_, Real to_, Int points, ErrorConsumer& errors);
    bool setupLogSweep(Real from_, Real to_, Real factor_, Int pointsPerFactor, ErrorConsumer& errors);

    // Set up a scalar sweep based on settings structure
    template<typename A> bool setup(const A& settings, ErrorConsumer& errors);

protected: 
    // Common fields
    Int at_;
    Int end;
    SweepType sweepType;

    // For range sweep (stepped, lin, dec, oct)
    Real from;
    Real to;

    // For stepped sweep
    Real step;

    // For value sweep
    const Value* vals;

    // For log sweep
    Real factor;

private:
    static Id modeLin;
    static Id modeDec;
    static Id modeOct;
};

template<typename A> bool ScalarSweep::setup(const A& settings, ErrorConsumer& errors) {
    // Check if any sweep is pecified
    int specCount=0;
    if (settings.values.isVector()) {
        specCount++;
    }
    if (settings.mode) {
        specCount++;
    }
    if (settings.step!=0) {
        specCount++;
    }
    if (specCount>1) {
        errors.push(SweepMultipleSpecs{});
        return false;
    }

    if (settings.values.isVector()) {
        return setupValueSweep(settings.values, errors);
    } else if (settings.mode) {
        if (settings.mode==ScalarSweep::modeLin) {
            return setupLinearSweep(settings.from, settings.to, settings.points, errors);
        } else if (settings.mode==ScalarSweep::modeDec) {
            return setupLogSweep(settings.from, settings.to, 10, settings.points, errors);
        } else if (settings.mode==ScalarSweep::modeOct) {
            return setupLogSweep(settings.from, settings.to, 2, settings.points, errors);
        } else {
            errors.push(SweepUnknownMode{});
            return false;
        }
    } else if (settings.step!=0) {
        return setupSteppedSweep(settings.from, settings.to, settings.step, errors);
    }
    errors.push(SweepNoSpec{});
    return false;
}


// Order method invocation:
//   bind()
//   storeState()
//   reset()
//   repeat
//     write() sweep
//     invoke analysis
//     advance()
//   write() stored state

class Circuit;

class ParameterSweeper : public ProgressTracker {
public:
    enum class WriteValues { StoredState, Sweep };
    enum class ParameterFamily { Instance=1<<0, Model=1<<1, Option=1<<2, Variable=1<<3 };
    
    ParameterSweeper(Circuit& circuit, const std::vector<PTSweep>& ptSweeps);

    // Setup sweeper (evaluate expressions, fill settings structures)
    bool setup(ErrorConsumer& errors);

    // Update
    bool update(int advancedSweepIndex, ErrorConsumer& errors);

    // Number of sweeps
    int count() const { return settings.size(); };

    // Bind (lookup instances, models, and simulator options)
    bool bind(Circuit& circuit, IStruct<SimulatorOptions>& opt, ErrorConsumer& errors);

    // Store parameters corresponding to current circuit state
    bool storeState(ErrorConsumer& errors);

    // Reset
    void reset();

    // Does i-th sweep use continuation
    bool continuation(int i) { return settings[i].continuation; };

    // Advance, return value: sweep done, index of sweep that was incremented (resets do not count)
    std::tuple<bool, Int> advance();

    // Position of the innermost sweep
    Int innermostSweepPosition() const { return scalarSweeps.back().at(); }; 

    // Format progress
    std::string progress() const;

    // Write swept parameters or stored parameters to circuit
    // Options are written to opt structure
    // Return value: ok, at least one instance or model parameter changed
    std::tuple<bool, bool> write(ParameterFamily types, WriteValues what, ErrorConsumer& errors);

    // Sweep name
    Id sweepName(Int ndx) const;
    
    // Return current index
    Int valueIndex(Int ndx) const;

    // Compute current value
    bool compute(Int ndx, Value& v, ErrorConsumer& errors) const;

private:
    Circuit& circuit;
    const std::vector<PTSweep>& ptSweeps;
    std::vector<SweepSettings> settings;
    std::vector<ScalarSweep> scalarSweeps;
    std::vector<ParameterFamily> parameterFamily;
    std::vector<Parameterized*> parameterizedObject;
    std::vector<ParameterIndex> parameterIndex;
    std::vector<Value> storedValues;
    Int incrementedSweepIndex;
    Circuit* circuit_;
    size_t sweepPos;
};
DEFINE_FLAG_OPERATORS(ParameterSweeper::ParameterFamily);

}

#endif
