#ifndef __ANSOLUTION_DEFINED
#define __ANSOLUTION_DEFINED

#include "ansupport.h"
#include "node.h"
#include "value.h"
#include "spurs.h"
#include "common.h"

// OP (real vector, names aux real vector)
// - real vector (node values)
// - names vector
// - aux real vector - states
//
// HB (complex vector, names, spurs, aux real vector)
// - complex vector (spectrum values for each node)
// - names vector
// - spurs (spectrum information)
// - aux real vector - timepoints
//
// PSS (real vector, names, real scalar)
// - real vector (node values)
// - names vector
// - aux real scalar (period), <=0 means no period given

namespace NAMESPACE {

class Circuit;

class AnnotatedSolution {
public:
    AnnotatedSolution();

    AnnotatedSolution           (const AnnotatedSolution&)  = delete;
    AnnotatedSolution           (      AnnotatedSolution&&) = default;
    AnnotatedSolution& operator=(const AnnotatedSolution&)  = delete;
    AnnotatedSolution& operator=(      AnnotatedSolution&&) = default;

    // Clear
    void clear() { typeTag_ = Id(); values_ = std::monostate{}; names_.clear(); realVec_.clear(); auxReal_ = 0.0; };

    // Type tag
    Id typeTag() const { return typeTag_; };
    void setTypeTag(Id tag) { typeTag_ = tag; };

    // Actual data
    const Vector<double>& values() const { return std::get<std::vector<double>>(values_); };
    const Vector<Complex>& cxValues() const { return std::get<std::vector<Complex>>(values_); };
    void setValues(const Vector<double>& vec) { values_ = vec; };
    void setCxValues(const Vector<Complex>& vec) { values_ = vec; };
    
    // Names of unknowns
    const std::vector<Id>& names() const { return names_; };
    void setNames(Circuit& circuit);
    
    // AUX data
    void setAuxRealVector(const Vector<double>& vec) { realVec_ = vec; };
    void setSpurs(const Spurs& spurs) { Spurs tmp(spurs); spurs_ = std::move(tmp); };
    void setAuxReal(double r) { auxReal_ = r; };
    const Vector<double>& auxRealVector() const { return realVec_; };
    const Spurs& spurs() const { return spurs_; };
    double auxReal() const { return auxReal_; };
    
private:
    typedef std::variant<std::monostate, Vector<double>, Vector<Complex>> VectorVariant;

    // Tag
    Id typeTag_;

    // Solution vector
    // - dc: one real component per unknown, index 0 is ground (bucket)
    // - hb: nf complex components per unknown. 
    //       no bucket - index 0 is first unknown
    VectorVariant values_;
    
    // Names of unknowns for cross matching across slightly different circuits
    std::vector<Id> names_;

    // Vector of auxiliary data
    // - op: states
    // - hb: timepoints
    Vector<double> realVec_;

    // Spurs (HB)
    Spurs spurs_;

    // Aux real scalar
    double auxReal_;
};

}

#endif
