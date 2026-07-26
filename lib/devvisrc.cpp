#include <numbers>
#include <algorithm>
#include <optional>
#include "devvisrc.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

// Signed index type
using signed_size_t = typename std::make_signed<size_t>::type;

static const double PI = std::numbers::pi;

template<> int Introspection<DevSourceModelParams>::setup() {
    return 0;
}
instantiateIntrospection(DevSourceModelParams);


DevSourceModelParams::DevSourceModelParams() {
}

template<> int Introspection<DevSourceInstanceParams>::setup() {
    registerNamedMember(mfactor, "$mfactor");
    registerMember(type);
    registerMember(delay);
    
    registerMember(dc);
    
    registerMember(val0);
    registerMember(val1);
    registerMember(period);
    registerMember(rise);
    registerMember(fall);
    registerMember(width);
    
    registerMember(sinedc);
    registerMember(ampl);
    registerMember(freq);
    registerMember(tdphase);
    registerMember(theta);

    registerMember(td2);
    registerMember(tau1);
    registerMember(tau2);
    
    registerMember(wave);
    registerMember(offset);
    registerMember(scale);
    registerMember(stretch);
    registerNamedMember(breakpt, "break");
    registerMember(slopetol);
    registerMember(sloperel);
    registerMember(slopeglob);
    
    registerMember(modfreq);
    registerMember(modphase);
    registerMember(modindex);
    
    registerMember(mag);
    registerMember(phase);

    registerMember(spur);
    registerMember(smag);
    registerMember(sphase);

    return 0;
}
instantiateIntrospection(DevSourceInstanceParams);

static Id typeSine = Id::createStatic("sine");
static Id typePulse = Id::createStatic("pulse");
static Id typeExp = Id::createStatic("exp");
static Id typeDc = Id::createStatic("dc");
static Id typePwl = Id::createStatic("pwl");
static Id typeAm = Id::createStatic("am");
static Id typeFm = Id::createStatic("fm");

static Id valAll = Id::createStatic("all");
static Id valNone = Id::createStatic("none");
static Id valAuto = Id::createStatic("auto");

DevSourceInstanceParams::DevSourceInstanceParams() {
    mfactor = 1;

    type = "dc";
    delay = 0.0;
    tdphase = 0.0;
    period = 0.0;

    // type="dc"
    dc = 0.0;

    // type="pulse"
    val0 = 0.0;
    val1 = 1.0;
    // delay
    rise = 1e-9;
    fall = 0.0;
    width = 0.0;
    // period
    
    // type="sine"
    // delay
    sinedc = 0.0;
    ampl = 1.0;
    freq = 1e3;
    // tdphase
    theta = 0.0;

    // type="exp"
    // val0, val1, delay
    td2 = 0.0;
    tau1 = 0.0;
    tau2 = 0.0;

    // type="pwl"
    wave = RealVector();
    offset = 0.0;
    scale = 1.0;
    stretch = 1.0;
    // delay, period, tdphase
    breakpt = valAll;
    slopetol = 0.0;
    sloperel = 0.0;
    slopeglob = 0.01;

    // type="am" or "fm"
    // sinedc, ampl, freq, tdphase
    modfreq = 1e3;
    modphase = 0;
    modindex = 0.5; 
    
    // small signal parameters
    mag = 0.0;
    phase = 0.0;

    // (quasi)periodic small-signal excitation
    spur = ValueVector({});
    smag = RealVector({});
    sphase = RealVector({});
}


template<> int Introspection<DevVSourceInstanceData>::setup() {
    registerMember(v);
    registerMember(i);
    return 0;
}
instantiateIntrospection(DevVSourceInstanceData);

DevVSourceInstanceData::DevVSourceInstanceData() {
}

template<> int Introspection<DevISourceInstanceData>::setup() {
    registerMember(v);
    registerMember(i);
    return 0;
}
instantiateIntrospection(DevISourceInstanceData);

DevISourceInstanceData::DevISourceInstanceData() {
}

template<typename InstanceParams, typename InstanceData> 
std::tuple<bool, bool, bool> sourceSetup(InstanceParams& params, InstanceData& data, Loc loc, Circuit& circuit, Status& s) { 
    auto& p = params.core();
    auto& d = data.core();

    // Store quick access code, check parameters
    if (p.type == typeSine) {
        d.typeCode = IndependentSourceType::Sine;
        if (p.freq<=0) {
            s.set(Status::BadArguments, "Frequency of sinusoidal transient must be greater than 0.");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }
    } else if (p.type == typePulse) {
        d.typeCode = IndependentSourceType::Pulse;
        if (p.rise<=0) {
            s.set(Status::BadArguments, "Rise time of pulse transient must be grater than 0.");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }
        
        // fall<=0 generates a step

        // width<=0 generates
        // - a step if fall<=0
        // - a triangle if fall>0

        // period<=0 generates a single pulse

        // If greater than 0, period must be greater than rise+fall+width
        if (p.period>0 && p.period<=p.rise+p.fall+p.width) {
            s.set(Status::BadArguments, "Period of pulse transient must be greater than rise+fall+width.");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }
    } else if (p.type == typePwl) {
        d.typeCode = IndependentSourceType::Pwl;
        // Not enough points .. error
        if (p.wave.size()<4) {
            s.set(Status::BadArguments, "Pwl waveform needs at least two points.");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }

        // Too many points
        if (p.wave.size()>=SIM_SIZE_T_MAX/2-1) {
            s.set(Status::BadArguments, "Pwl waveform too long.");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }
        
        // Need pairs of values
        if (p.wave.size()%2 == 1) {
            s.set(Status::BadArguments, "Pwl waveform needs an even number of values.");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }
        
        // Check stretch
        if (p.stretch<=0) {
            s.set(Status::BadArguments, "Waveform stretch must be >0.");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }

        // Check breakpoint mechanism
        if (p.breakpt!=valAll && p.breakpt!=valNone && p.breakpt!=valAuto) {
            s.set(Status::BadArguments, "parameter break must be all, none, or auto.");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }

        // Check slopetol
        if (p.slopetol<0) {
            s.set(Status::BadArguments, "Parameter slopetol must be >=0.");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }

        // Check sloperel
        if (p.sloperel<0) {
            s.set(Status::BadArguments, "Parameter sloperel must be >=0.");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }

        // Check slopeglob
        if (p.slopeglob<0) {
            s.set(Status::BadArguments, "Parameter slopeglob must be >=0.");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }

        // First point x must be >=0
        if (p.wave[0]<0) {
            s.set(Status::BadArguments, "First timepoint must be >=0");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }

        // Check period sanity, detect if the last point is equivalent to the first point
        auto n = p.wave.size();
        d.npts = n/2;
        if (p.period>0) {
            auto tfirst = p.wave[0];
            auto tlast = p.wave[2*(d.npts-1)];
            if (tlast>p.period) {
                s.set(Status::BadArguments, "Period ends before last point.");
                s.extend(loc);
                return std::make_tuple(false, false, false);
            } else if (tlast==p.period) {
                if (tfirst==0.0) {
                    // If last point is equivalent to first point, last point is ignored
                    if (p.wave[1]!=p.wave[2*(d.npts-1)+1]) {
                        s.set(Status::BadArguments, "First and last timepoint must match if period transition time is 0.");
                        s.extend(loc);
                        return std::make_tuple(false, false, false);
                    }
                    d.npts = n/2-1;
                }
            }
        }

        // Not enough points .. error
        if (d.npts<2) {
            s.set(Status::BadArguments, "Pwl waveform needs at least two points.");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }

        // Check time scale strict monotonicity, compute slopes, extract maximal slope
        double maxSlope = 0;
        double oldX = p.wave[0];
        double oldY = p.wave[1];
        d.slopes.resize(d.npts);
        d.breakEnabled.resize(d.npts);
        d.nextBreakIndex.resize(d.npts);
        // Iterate through second points of intervals
        for(decltype(n) i=1; i<d.npts; i++) {
            auto x = p.wave[2*i];
            auto y = p.wave[2*i+1];
            if (x<=oldX) {
                s.set(Status::BadArguments, "Pwl scale is not strictly monotonic.");
                s.extend(loc);
                return std::make_tuple(false, false, false);
            }
            auto slope = (y-oldY)/(x-oldX);
            if (std::abs(slope)>maxSlope) {
                maxSlope = std::abs(slope);
            }
            d.slopes[i-1] = slope;
            oldX = x;
            oldY = y;
        }
        // Last slope is special
        if (p.period>0) {
            // If periodic, last slope (beyond last point, index d.npts-1) is the transition slope 
            // between the last point of a period and the first point of the next period
            auto dx = p.period - (oldX - p.wave[0]) ;
            // dx must be >0 (guaranteed by the earlier period/monotonicity checks,
            // but guard against a division by zero/negative in release builds)
            if (dx<=0) {
                s.set(Status::BadArguments, "Pwl period is too short for the given waveform.");
                s.extend(loc);
                return std::make_tuple(false, false, false);
            }
            // Slope
            auto slope = (oldY - p.wave[1])/dx;
            d.slopes.back() = slope;
        } else {
            // Not periodic, last slope (beyond last point) is 0
            d.slopes.back() = 0.0;
        }
        // Slope tolerance base (common part)
        double slopeTolBase = 0.0;
        // Absolute slope tolerance
        if (p.slopetol>0) {
            slopeTolBase = std::max(slopeTolBase, p.slopetol);
        }
        // Global relative slope tolerance
        if (p.slopeglob>0) {
            slopeTolBase = std::max(slopeTolBase, p.slopeglob*maxSlope);
        }        
        // Do we need a breakpoint? 
        auto needsBreakpoint = false; // for "none"
        size_t firstBreakIndex;
        size_t lastBreakIndex;
        bool hasBreakpoints;
        if (p.breakpt==valAll) {
            // For "all"
            for(decltype(n) i=0; i<d.npts; i++) {
                d.breakEnabled[i] = true;
            }
            hasBreakpoints = true;
            firstBreakIndex = 0;
            lastBreakIndex = d.npts-1;
        } else if (p.breakpt==valNone) {
            // For "none"
            for(decltype(n) i=0; i<d.npts; i++) {
                d.breakEnabled[i] = false;
            }
            hasBreakpoints = false;
        } else {
            // For "auto", determine if a breakpoint is needed at each point
            // Initialize hasBreakpoints flag
            hasBreakpoints = false;
            for(decltype(n) i=0; i<d.npts; i++) {
                // Decide when break=auto
                double slopeBefore; 
                auto slopeAfter = d.slopes[i];
                // Slope before first point
                if (i==0) {
                    slopeBefore = p.period>0 ? d.slopes.back() : 0.0;
                } else {
                    slopeBefore = d.slopes[i-1];
                }
                // Local relative slope tolerance
                auto slopeTolLocal = slopeTolBase;
                if (p.sloperel>0) {
                    slopeTolLocal = std::max(
                        slopeTolLocal, 
                        std::max(std::abs(slopeBefore), std::abs(slopeAfter))*p.sloperel
                    );
                }
                // Do we need a breakpoint here (based on slope tolerance)
                needsBreakpoint = std::abs(slopeAfter-slopeBefore)>slopeTolLocal;
                // Handle special case (not periodic requires breakpoints at first and last point)
                if (p.period<=0 && (i==0 || i==d.npts-1)) {
                    needsBreakpoint = true;
                }
                if (needsBreakpoint) {
                    // Set first breakpoint
                    if (!hasBreakpoints) {
                        firstBreakIndex = i;
                        hasBreakpoints = true;
                    }
                    // Set last breakpoint
                    lastBreakIndex = i;
                }
                // Store 
                d.breakEnabled[i] = needsBreakpoint;
                
                // By default set the next breakpoint index to self... this is the correct value for 
                // points at and beyond last breakpoint in aperiodic case. 
                d.nextBreakIndex[i] = i;
            }
        }
        // For each point set point index of next breakpoint via reverse iteration. 
        // For aperiodic waveforms the last breakpoint and points following it have no next breakpoint. 
        // For periodic waveforms the the last breakpoint and the points following it have a next breakpoint - 
        // the first breakpoint. 
        // Nothing to do if there are no breakpoints. 
        if (hasBreakpoints) {
            // Initial value of next breakpoint index, applied to tail waveform points
            signed_size_t nextBreakIndex;
            if (p.period>0) {
                // For periodic waveforms this is the first breakpoint
                nextBreakIndex = firstBreakIndex;
            } else {
                // For aperiodic waveforms this is the last breakpoint
                nextBreakIndex = lastBreakIndex;
            }
            // j is the index, i is the counter
            auto j = d.npts;
            for(decltype(n) i=0; i<d.npts; i++) {
                j--;
                d.nextBreakIndex[j] = nextBreakIndex;
                if (d.breakEnabled[j]) {
                    nextBreakIndex = j;
                }
            }
        }
        // Initialize lastPointIndex
        d.lastPointIndex = 0;
    } else if (p.type == typeExp) {
        d.typeCode = IndependentSourceType::Exp;
        if (p.td2<=0) {
            s.set(Status::BadArguments, "Parameter td2 of exponential transient must be greater than 0.");
            s.extend(loc);
            return std::make_tuple(false, false, false);
        }
    } else if (p.type == typeAm) {
        d.typeCode = IndependentSourceType::Am;
    } else if (p.type == typeFm) {
        d.typeCode = IndependentSourceType::Fm;
    } else if (p.type == typeDc) {
        d.typeCode = IndependentSourceType::Dc;
    } else {
        s.set(Status::BadArguments, "Unknown transient waveform type.");
        s.extend(loc);
        return std::make_tuple(false, false, false);
    }

    return std::make_tuple(true, false, false); 
}

// Search through a vector of PWL values for a timepoint, start at pwl point index start, 
// look for first pwl timepoint where timepoint<=target. 
// Assume target>=t[0]
// Returns point index
// Careful, n can be <=wave length (if last wave point is ignored in periodic case)
size_t pwlIndexLookup(const RealVector& wave, size_t n, size_t start, double target) {
    // Value at position start
    auto atTime = wave[2*start];
    // Step and direction
    if (target==atTime) {
        // Already there
        return start;
    } else if (target>=wave[2*(n-1)]) {
        // It is at or after the last timepoint
        return n-1;
    }
    // We need to look it up
    size_t step = 1;
    bool forward = true;
    if (target<atTime) {
        // Backward
        forward = false;
    }
    // At this point we are sure t[0]<=target<=t[n-1]
    // Expand exponentially, until you find the bracket
    // This catches small changes in index
    signed_size_t atIndex = start;
    while (true) {
        // Compute trial point
        signed_size_t tryIndex = forward ? atIndex+step : atIndex-step;
        // Limit to 0<=tryIndex<n-1
        if (tryIndex<0) {
            tryIndex = 0;
        } else if (tryIndex>=n) {
            tryIndex = n-1;
        }
        // Get time
        auto tryTime = wave[2*tryIndex];
        // Check it
        if (forward && tryTime>target) {
            // Going forward, tryTime>target
            atIndex = tryIndex;
            break;
        } else if (!forward && tryTime<=target) {
            // Going backward, tryTime<=target
            atIndex = tryIndex;
            break;
        }
        // Increase step
        step *= 2;
    }
    // Are we done (atIndex==start)
    if (atIndex==start) {
        return start;
    }
    // Bisection boundaries i1<i2
    signed_size_t i1, i2, imid;
    if (forward) {
        i1 = start;
        i2 = atIndex;
    } else {
        i1 = atIndex;
        i2 = start;
    }
    // Bisect to target
    while (true) {
        imid = (i1+i2)/2;
        auto midTime = wave[imid*2];
        if (midTime>target) {
            // Take left interval
            i2 = imid;
        } else {
            // Take right interval
            i1 = imid;
        }
        if (i2-i1<=1) {
            break;
        }
    }
    // i1 is the point
    return i1;
}

// Interpolate from pwl, return interpolated value, breakpoint time, and next breakpoint time
template<typename InstanceParams, typename InstanceData> 
std::tuple<double, double, double> pwlValue(InstanceParams& p, InstanceData& d, double tposper, double tposabs, double origin) {
    auto n = d.npts;
    double y;
    std::optional<size_t> i1 = std::nullopt;
    std::optional<size_t> i2 = std::nullopt;
    size_t index;
    const auto inf = std::numeric_limits<double>::infinity();
    if (p.period<=0) {
        // Aperiodic
        if (tposper<p.wave[0]) {
            // Before first
            y = p.wave[1];
        } else if (tposper>=p.wave[2*(n-1)]) {
            // At or after last
            y = p.wave[2*(n-1)+1];
        } else {
            // In between
            index = pwlIndexLookup(p.wave, n, d.lastPointIndex, tposper);
            d.lastPointIndex = index;
            y = p.wave[2*index+1]+(tposper-p.wave[2*index])*d.slopes[index];
        }
        // Breakpoint handling
        if (tposabs<0) {
            // Waveform not started yet
            if (tposper>=p.wave[2*(n-1)]) {
                // At or after last, no breakpoint
                i1 = std::nullopt;
                i2 = std::nullopt;
            } else if (tposper<=p.wave[0]) {
                // Before or at first
                if (d.breakEnabled[0]) {
                    // First waveform point is a breakpoint, break there
                    i1 = 0;
                    i2 = d.nextBreakIndex[0];
                } else {
                    // First waveform point is not a breakpoint, look for first breakpoint
                    i1 = d.nextBreakIndex[0];
                    i2 = d.nextBreakIndex[i1.value()];
                }
            } else {
                // All others (had index lookup), look for first breakpoint
                i1 = d.nextBreakIndex[index];
                i2 = d.nextBreakIndex[i1.value()];
            }
        } else {
            // Waveform started
            if (tposper>=p.wave[2*(n-1)]) {
                // At or after last, no breakpoint
                i1 = std::nullopt;
                i2 = std::nullopt;
            } else if (tposper<p.wave[0]) {
                // Before first
                if (d.breakEnabled[0]) {
                    // First waveform point is a breakpoint, break there
                    i1 = 0;
                    i2 = d.nextBreakIndex[i1.value()];
                } else {
                    // First waveform point is not a breakpoint, look for first breakpoint
                    i1 = d.nextBreakIndex[0];
                    i2 = d.nextBreakIndex[i1.value()];
                }
            } else {
                // All others (had index lookup), look for first breakpoint
                i1 = d.nextBreakIndex[index];
                i2 = d.nextBreakIndex[i1.value()];
            }
        }
        // Breakpoint times
        if (!i1.has_value()) {
            return std::make_tuple(y, inf, inf);
        } else {
            double t1, t2;
            if (d.breakEnabled[i1.value()]) {
                t1 = origin + p.wave[2*i1.value()]*p.stretch;
                t2 = origin + p.wave[2*i2.value()]*p.stretch;
                return std::make_tuple(y, t1, t2);
            } else {
                return std::make_tuple(y, inf, inf);
            }
        }
    } else {
        // Periodic
        if (tposper<p.wave[0] || tposper>=p.wave[2*(n-1)]) {
            // Before/at first or at/after last
            index = n-1;
        } else {
            // In between
            index = pwlIndexLookup(p.wave, n, d.lastPointIndex, tposper);
        }
        d.lastPointIndex = index;
        y = p.wave[2*index+1]+(tposper-p.wave[2*index])*d.slopes[index];
        // Breakpoint handling
        int i1Per = 0;
        int i2Per = 0;
        if (tposabs<0) {
            // Waveform not started yet
            if (tposper>=p.wave[2*(n-1)]) {
                // At or after last
                i1 = d.nextBreakIndex[n-1];
                i1Per = i1.value() < n-1 ? 1 : 0;
                i2 = d.nextBreakIndex[i1.value()];
                i2Per = i1Per + (i2.value() < i1.value() ? 1 : 0);
            } else if (tposper<=p.wave[0]) {
                // Before or at first
                i1 = d.nextBreakIndex[n-1];
                i1Per = 0;
                i2 = d.nextBreakIndex[i1.value()];
                i2Per = i1Per + (i2.value() < i1.value() ? 1 : 0);
            } else {
                // All others (had index lookup), look for first breakpoint
                i1 = d.nextBreakIndex[index];
                i1Per = i1Per + (i1.value() < index ? 1 : 0);
                i2 = d.nextBreakIndex[i1.value()];
                i2Per = i1Per + (i2.value() < i1.value() ? 1 : 0);
            }
        } else {
            // Waveform started
            if (tposper>=p.wave[2*(n-1)]) {
                // At or after last
                i1 = d.nextBreakIndex[n-1];
                i1Per = i1.value() < n-1 ? 1 : 0;
                i2 = d.nextBreakIndex[i1.value()];
                i2Per = i1Per + (i2.value() < i1.value() ? 1 : 0);
            } else if (tposper<p.wave[0]) {
                // Before first
                i1 = d.nextBreakIndex[n-1];
                i1Per = 0;
                i2 = d.nextBreakIndex[i1.value()];
                i2Per = i1Per + (i2.value() < i1.value() ? 1 : 0);
            } else {
                // All others (had index lookup), look for first breakpoint
                i1 = d.nextBreakIndex[index];
                i1Per = i1Per + (i1.value() < index ? 1 : 0);
                i2 = d.nextBreakIndex[i1.value()];
                i2Per = i1Per + (i2.value() < i1.value() ? 1 : 0);
            }
        }
        // Breakpoint times
        if (!i1.has_value()) {
            // This should never happen
            return std::make_tuple(y, inf, inf);
        } else {
            double t1, t2;
            if (d.breakEnabled[i1.value()]) {
                t1 = origin + (i1Per*p.period+p.wave[2*i1.value()])*p.stretch;
                t2 = origin + (i2Per*p.period+p.wave[2*i2.value()])*p.stretch;
                return std::make_tuple(y, t1, t2);
            } else {
                return std::make_tuple(y, inf, inf);
            }
        }
    }
}

// A device model should not rely on tolerances. 
// Its only job is to produce consistent reponses, i.e. 
// in this case t5 should match t1 in the next period. 
template<typename InstanceParams, typename InstanceData> 
std::tuple<double, double> sourceCompute(const InstanceParams& params, InstanceData& data, double time) {
    double val;
    double nextBreak = std::numeric_limits<double>::infinity();

    switch (data.typeCode) {
    case IndependentSourceType::Dc:
        val = params.dc;
        break;
    case IndependentSourceType::Sine:
        if (time<params.delay) {
            // For t < delay the value is equal to value at t=delay
            val = params.sinedc+params.ampl*std::sin(params.tdphase*PI/180);
            nextBreak = params.delay;
        } else {
            // For t >= delay start sine at given phase
            // Use two-product via fma to reduce freq*td to its fractional part
            // with full precision, avoiding range reduction error for large t
            auto td = time-params.delay;
            auto prod = params.freq*td;
            auto lo = std::fma(params.freq, td, -prod);
            auto intpart = std::trunc(prod);
            auto frac = (prod - intpart) + lo;
            val = params.sinedc+
                  params.ampl
                    *std::sin(2*PI*frac+params.tdphase*PI/180)
                    *std::exp(-params.theta*td);
        }
        break;
    case IndependentSourceType::Exp:
        if (time<params.delay) {
            // For t < delay the value is equal to value at t=delay
            val = params.val0;
            nextBreak = params.delay;
        } else {
            auto t1 = params.delay + params.td2; // start of fall
            if (time<t1) {
                // Rising exponential
                val = params.val0 
                      + (params.val1-params.val0)*(1-std::exp(-(time-params.delay)/params.tau1));
                nextBreak = t1;
            } else {
                // Falling exponential
                val = params.val0 
                      + (params.val1-params.val0)*(1-std::exp(-(time-params.delay)/params.tau1))
                      + (params.val0-params.val1)*(1-std::exp(-(time-t1)/params.tau2));
            }
        }
        break;
    case IndependentSourceType::Am:
        if (time<params.delay) {
            // For t < delay the value is equal to value at t=delay
            val = params.sinedc+params.ampl*std::sin(params.tdphase*PI/180)*(
                1+params.modindex*std::sin(params.modphase*PI/180)
            );
            nextBreak = params.delay;
        } else {
            // For t >= delay start sine at given phase
            val = params.sinedc+params.ampl*std::sin(2*PI*params.freq*(time-params.delay)+params.tdphase*PI/180)*(
                1+params.modindex*std::sin(2*PI*params.modfreq*(time-params.delay)+params.modphase*PI/180)
            );
        }
        break;
    case IndependentSourceType::Fm:
        if (time<params.delay) {
            // For t < delay the value is equal to value at t=delay
            val = params.sinedc+params.ampl*std::sin(
                params.tdphase*PI/180+
                params.modindex*std::sin(params.modphase*PI/180)
            );
            nextBreak = params.delay;
        } else {
            // For t >= delay start sine at given phase
            val = params.sinedc+params.ampl*std::sin(
                2*PI*params.freq*(time-params.delay)+params.tdphase*PI/180+
                params.modindex*std::sin(2*PI*params.modfreq*(time-params.delay)+params.modphase*PI/180)
            );
        }
        break;
    case IndependentSourceType::Pulse:
        if (time<params.delay) {
            // Before waveform starts
            val = params.val0;
            nextBreak = params.delay;
        } else {
            // Waveform started, see where we are
            // Time since start of this repetition
            double basetime = params.delay;
            int64_t cycle = 0; 
            // Start of current period
            if (params.period<=0) {
                // Not periodic
                basetime = params.delay;
            } else {
                // Periodic
                cycle = static_cast<int64_t>(std::floor((time-params.delay)/params.period));
                basetime = params.delay + cycle*params.period;
            }
            // Significant time points
            auto t0 = basetime; // start of rise
            auto t1 = t0 + params.rise; // end of rise, start of top
            auto t2 = t1 + params.width; // start of fall
            auto t3 = t2 + params.fall; // end of fall
            auto t4 = t0 + params.period; // end of period
            auto t5 = params.delay + (cycle+1)*params.period + params.rise; // next period, end of rise
            // Relative time since start of period
            double reltime = time - basetime;
            // Simulator::dbg().setf(std::ios::scientific, std::ios::floatfield);
            if (time<t1) {
                // Rising
                val = (time-t0)/params.rise*(params.val1-params.val0)+params.val0;
                nextBreak = t1;
            } else if (time<t2) {
                // On top
                val = params.val1;
                // Set next break only if width>0
                if (params.width>0) {
                    nextBreak = t2;
                }
            } else if (params.fall<=0) {
                // Fall not set, stay on top, no breakpoint
                val = params.val1;
            } else if (time<t3) {
                // Falling
                val = (time-t2)/params.fall*(params.val0-params.val1)+params.val1;
                nextBreak = t3;
            } else if (time<t4) {
                // After fall, back on base level
                val = params.val0;
                // Beakpoint only if period is set
                if (params.period>0) {
                    nextBreak = t4;
                }
            } else {
                if (params.period>0) {
                    // Periodic waveform, rising flank of next period
                    // We may end up here for the first point of next period due to tolerances, 
                    // Compute rising flank with origin at t4
                    val = (time-t4)/params.rise*(params.val1-params.val0)+params.val0;
                    nextBreak = t5;
                } else {
                    // Not periodic, back on base level after fall
                    val = params.val0;
                }
            }
        }
        break;
    case IndependentSourceType::Pwl: 
        // Determine where in the waveform we are in terms of 
        // unstretched time relative to period start (periodic) or 
        // waveform start (aperiodic)
        
        // Stretched time within waveform, zero at time=delay
        double tposabs = time - params.delay;

        // Stretched time within period (for aperiodic signals equals tposabs)
        double tposper = tposabs;

        // Origin for breakpoints (tpos=0 corresponds to delay)
        double origin = params.delay;

        // Handle periodic pwl
        if (params.period>0) { 
            // Relative phase [0, 1) corresponds to [0, 360)
            auto relphase = params.tdphase/360;

            // Force into [0, 1)
            relphase = relphase - floor(relphase);

            auto stretchedPeriod = params.period*params.stretch;
            if (tposabs<0) {
                // Before waveform starts we are at the point of initial phase
                tposper = stretchedPeriod*relphase;
            } else {
                // Waveform started, advance tposabs by phase to get tposper
                tposper = tposabs + relphase*stretchedPeriod;
                
                // Remove integer number of stretched periods
                auto nper = signed_size_t(std::floor(tposper/stretchedPeriod));
                tposper -= nper*stretchedPeriod;

                // Make sure it is within stretched period 0<=tposper<stretchedPeriod (numerical errors can move it out)
                if (tposper>=stretchedPeriod) {
                    tposper -= stretchedPeriod;
                }
                if (tposper<0) {
                    tposper = 0;
                }

                // Move origin
                origin += nper*stretchedPeriod;
            }
            // Breakpoint origin concides with period start for phase=0
            // Otherwise it is behind by initial phase
            origin -= stretchedPeriod*relphase;
        } else {
            // For aperiodic signals make sure tposper>=0 
            if (tposabs<0) {
                tposper = 0;
            }
        }

        // Unstretch to original time
        tposabs /= params.stretch;
        tposper /= params.stretch;

        // Get value, breakpoint, and next breakpoint
        auto [y, tbr1, tbr2] = pwlValue(params, data, tposper, tposabs, origin);

        // Apply scale, offset
        val = y*params.scale + params.offset;

        // If tbr1 is within tolerance of current time use tbr2
        auto tol = time*timeRelativeTolerance;
        if (tbr1>=time-tol && tbr1<=time+tol) {
            nextBreak = tbr2;
        } else {
            nextBreak = tbr1;
        }
        break;
    }
    return std::make_tuple(val, nextBreak);
}

template<> void BuiltinVSource::defineInternals() {
    nodeIds = { "p", "n", "flow(br)" };
    terminalCount = 2;
}

template<> void BuiltinISource::defineInternals() {
    nodeIds = { "p", "n" };
    terminalCount = 2;
}

template<> bool BuiltinVSource::isSource() const {
    return true;
}

template<> bool BuiltinISource::isSource() const {
    return true;
}

template<> bool BuiltinVSource::isVoltageSource() const {
    return true;
}


template<> const Device::Flags BuiltinVSource::extraFlags = 
    Device::Flags::GeneratesAC | Device::Flags::GeneratesDCIncremental;

template<> const Device::Flags BuiltinISource::extraFlags = 
    Device::Flags::GeneratesAC | Device::Flags::GeneratesDCIncremental;

template<> std::tuple<ParameterIndex, bool> BuiltinVSourceInstance::principalParameterIndex() const {
    static const auto pair = Introspection<DevSourceInstanceParams>::index("dc");
    return pair;
}

template<> std::tuple<ParameterIndex, bool> BuiltinISourceInstance::principalParameterIndex() const {
    static const auto pair = Introspection<DevSourceInstanceParams>::index("dc");
    return pair;
}

template<> bool BuiltinVSourceInstance::deleteHierarchy(Circuit& circuit, Status& s) { 
    return unbindInternalNodes(circuit);
} 

template<> bool BuiltinVSourceInstance::buildHierarchy(Circuit& circuit, RpnEvaluator& evaluator, InstantiationData& idata, Status& s) { 
    // If we want to leave unconnected terminals hanging, do this 
    // // Create internal nodes for unconnected terminals
    // createNodesForUnconnectedTerminals();
    
    // If we require all terminals to be connected, do this
    if (!verifyTerminalsConnected(s)) { 
        return false;
    }
    
    // Create internal static flow node
    auto node = getInternalNode(circuit, nodeName(2), Node::Flags::FlowNode, s);
    if (!node) {
        return false;
    }
    
    // Bind static flow node
    nodes_[2] = node;

    return true; 
};  

template<> bool BuiltinISourceInstance::buildHierarchy(Circuit& circuit, RpnEvaluator& evaluator, InstantiationData& idata, Status& s) { 
    // If we want to leave unconnected terminals hanging, do this 
    // // Create internal nodes for unconnected terminals
    // createNodesForUnconnectedTerminals();

    // If we require all terminals to be connected, do this
    if (!verifyTerminalsConnected(s)) { 
        return false;
    }
    return true;
}

template<> std::tuple<EquationIndex,EquationIndex> BuiltinVSourceInstance::sourceExcitation(Circuit& circuit) const { 
    return std::make_tuple(0, nodes_[2]->unknownIndex()); 
}

template<> std::tuple<UnknownIndex,UnknownIndex> BuiltinVSourceInstance::sourceResponse(Circuit& circuit) const { 
    return std::make_tuple(nodes_[2]->unknownIndex(), 0); 
}

template<> double BuiltinVSourceInstance::scaledUnityExcitation() const { 
    // mag=1 produces a voltage of 1V, regardless of $mfactor. 
    return 1.0; 
}

template<> double BuiltinVSourceInstance::responseScalingFactor() const { 
    // Because the computed response (branch current) is 1/$mfactor times 
    // the total current of all parallel instances combined, the 
    // scaling factor must be $mfactor. 
    return params.core().mfactor; 
}

template<> std::tuple<const ValueVector&, const RealVector&, const RealVector&> BuiltinVSourceInstance::spur() const {
    return { params.core().spur, params.core().smag, params.core().sphase };
}

template<> std::tuple<EquationIndex,EquationIndex> BuiltinISourceInstance::sourceExcitation(Circuit& circuit) const { 
    return std::make_tuple(nodes_[1]->unknownIndex(), nodes_[0]->unknownIndex()); 
}

template<> std::tuple<UnknownIndex,UnknownIndex> BuiltinISourceInstance::sourceResponse(Circuit& circuit) const { 
    return std::make_tuple(nodes_[0]->unknownIndex(), nodes_[1]->unknownIndex()); 
}

template<> double BuiltinISourceInstance::scaledUnityExcitation() const { 
    // mag=1 produces a current of $mfactor A. 
    return params.core().mfactor; 
}

template<> double BuiltinISourceInstance::responseScalingFactor() const { 
    // Because the computed response (branch voltage) is the same for 
    // all parallel instances the scaling factor must be 1. 
    return 1.0; 
}

template<> std::tuple<const ValueVector&, const RealVector&, const RealVector&> BuiltinISourceInstance::spur() const {
    return { params.core().spur, params.core().smag, params.core().sphase };
}

template<> bool BuiltinVSourceInstance::getOutvar(ParameterIndex ndx, Value& v, Status& s) const { 
    switch (ndx) {
    case 0:
        v = data.core().v;
        break;
    case 1:
        v = data.core().i;
        break;
    default:
        s.set(Status::Range, std::string("Output variable index id=")+std::to_string(ndx)+" out of range.");
        return false;
    }
    return true;
}

template<> bool BuiltinISourceInstance::getOutvar(ParameterIndex ndx, Value& v, Status& s) const { 
    switch (ndx) {
    case 0:
        v = data.core().v;
        break;
    case 1:
        v = data.core().i;
        break;
    default:
        s.set(Status::Range, std::string("Output variable index id=")+std::to_string(ndx)+" out of range.");
        return false;
    }
    return true;
}

template<> std::tuple<bool, OutputSource> BuiltinVSourceInstance::outvarOutputSource(ParameterIndex ndx) const { 
    switch (ndx) {
    case 0:
        return std::make_tuple(true, OutputSource(&data.core().v));
    case 1: 
        return std::make_tuple(true, OutputSource(&data.core().i));
    default: 
        return std::make_tuple(false, OutputSource());
    }
}

template<> std::tuple<bool, OutputSource> BuiltinISourceInstance::outvarOutputSource(ParameterIndex ndx) const { 
    switch (ndx) {
    case 0:
        return std::make_tuple(true, OutputSource(&data.core().v));
    case 1: 
        return std::make_tuple(true, OutputSource(&data.core().i));
    default: 
        return std::make_tuple(false, OutputSource());
    }
}

template<> std::tuple<bool, bool, bool> BuiltinVSourceInstance::setupCore(Circuit& circuit, CommonData& commons, DeviceRequests* devReq, Status& s) {
    clearFlags(Flags::NeedsSetup); 
    return sourceSetup(params, data, location(), circuit, s);
}; 

template<> std::tuple<bool, bool, bool> BuiltinISourceInstance::setupCore(Circuit& circuit, CommonData& commons, DeviceRequests* devReq, Status& s) { 
    clearFlags(Flags::NeedsSetup);
    return sourceSetup(params, data, location(), circuit, s);
}; 

template<> bool BuiltinVSourceInstance::setStaticTolerancesCore(Circuit& circuit, CommonData& commons, Status& s) {
    // Always use spice tolerance mode
    
    // Options
    auto& options = circuit.simulatorOptions().core();

    // Unknowns
    auto pn = nodes_[0]->unknownIndex();
    auto nn = nodes_[1]->unknownIndex();
    auto in = nodes_[2]->unknownIndex();

    // Tolerances
    updatePotentialNodeSpiceTolerances(options, commons, pn);
    updatePotentialNodeSpiceTolerances(options, commons, nn);
    updateFlowNodeSpiceTolerances(options, commons, in);
    
    return true;
}

template<> bool BuiltinISourceInstance::setStaticTolerancesCore(Circuit& circuit, CommonData& commons, Status& s) {
    // Always use spice tolerance mode
    
    // Options
    auto& options = circuit.simulatorOptions().core();

    // Unknowns
    auto pn = nodes_[0]->unknownIndex();
    auto nn = nodes_[1]->unknownIndex();
    
    // Tolerances
    updatePotentialNodeSpiceTolerances(options, commons, pn);
    updatePotentialNodeSpiceTolerances(options, commons, nn);
    
    return true;
}

template<> bool BuiltinVSourceInstance::populateStructuresCore(Circuit& circuit, Status& s) {
    // Create Jacobian entries
    if (auto [_, ok] = circuit.createJacobianEntry(nodes_[0], nodes_[2], EntryFlags::Resistive, s); !ok) {
        return false;
    }
    if (auto [_, ok] = circuit.createJacobianEntry(nodes_[1], nodes_[2], EntryFlags::Resistive, s); !ok) {
        return false;
    }
    if (auto [_, ok] = circuit.createJacobianEntry(nodes_[2], nodes_[0], EntryFlags::Resistive, s); !ok) {
        return false;
    }
    if (auto [_, ok] = circuit.createJacobianEntry(nodes_[2], nodes_[1], EntryFlags::Resistive, s); !ok) {
        return false;
    }
    
    circuit.newResistiveContribution(nodes_[0]);
    circuit.newResistiveContribution(nodes_[1]);
    circuit.newResistiveContribution(nodes_[2]);
    
    // No states to reserve
    return true;
}

template<> bool BuiltinISourceInstance::populateStructuresCore(Circuit& circuit, Status& s) {
    // No Jacobian entries
    // No states to reserve

    circuit.newResistiveContribution(nodes_[0]);
    circuit.newResistiveContribution(nodes_[1]);
    
    return true;
}

template<> bool BuiltinVSourceInstance::bindCore(
    Circuit& circuit, 
    KluMatrixAccess* matResist, Component compResist, const std::optional<MatrixEntryPosition>& mepResist, 
    KluMatrixAccess* matReact, Component compReact, const std::optional<MatrixEntryPosition>& mepReact, 
    Status& s
) {
    auto& d = data.core();

    // Unknown indices
    d.uP = nodes_[0]->unknownIndex();
    d.uN = nodes_[1]->unknownIndex();
    d.uFlow = nodes_[2]->unknownIndex();

    // Resistive Jacobian entry pointers
    if (matResist) {
        jacEntryPtr(d.jacPFlow, d.uP, d.uFlow, matResist, compResist, mepResist);
        jacEntryPtr(d.jacNFlow, d.uN, d.uFlow, matResist, compResist, mepResist);
        jacEntryPtr(d.jacFlowP, d.uFlow, d.uP, matResist, compResist, mepResist);
        jacEntryPtr(d.jacFlowN, d.uFlow, d.uN, matResist, compResist, mepResist);
    }

    // No reactive Jacobian entries
    
    return true;
}

template<> bool BuiltinISourceInstance::bindCore(
    Circuit& circuit, 
    KluMatrixAccess* matResist, Component compResist, const std::optional<MatrixEntryPosition>& mepResist, 
    KluMatrixAccess* matReact, Component compReact, const std::optional<MatrixEntryPosition>& mepReact, 
    Status& s
) {
    auto& d = data.core();

    // Unknown indices
    d.uP = nodes_[0]->unknownIndex();
    d.uN = nodes_[1]->unknownIndex();
    
    // No Jacobian entries
    
    return true;
}

template<> bool BuiltinVSourceInstance::evalCore(Circuit& circuit, CommonData& commons, EvalSetup& evalSetup) {
    auto& p = params.core();
    auto& d = data.core();
    auto& options = circuit.simulatorOptions().core();
    auto sourceFactor = commons.sourcescalefactor*options.homotopy_sourcefactor;
    
    // Evaluate, placeholder for bypass implementation
    auto [val, nextBreakpoint] = sourceCompute(p, d, evalSetup.time);
    if (true) {
        if (evalSetup.evaluateResistiveResidual) {
            if (evalSetup.evaluateResistiveResidual) {
                d.flowResidual = p.mfactor*evalSetup.oldSolution[d.uFlow];
                d.eqResidual = -evalSetup.oldSolution[d.uP] + evalSetup.oldSolution[d.uN] + sourceFactor*val;
            }
            // Output variables
            d.v = sourceFactor*val; // mfactor does not affect voltage source value
            d.i = evalSetup.oldSolution[d.uFlow]; // flow across one parallel instance
        }
    }

    // Next breakpoint
    if (evalSetup.computeNextBreakpoint) {
        evalSetup.setBreakPoint(nextBreakpoint, commons); 
    }

    // Set maximal frequency
    if (evalSetup.computeMaxFreq) {
        if (d.typeCode==IndependentSourceType::Sine) {
            evalSetup.setMaxFreq(p.freq);
        }
    }

    return true;
}

template<> bool BuiltinVSourceInstance::loadCore(Circuit& circuit, CommonData& commons, LoadSetup& loadSetup) {
    auto& p = params.core();
    auto& d = data.core();
    auto& options = circuit.simulatorOptions().core();
    
    // Load resistive Jacobian, transient load is identical because there is no reactive component
    if (loadSetup.loadResistiveJacobian || loadSetup.loadTransientJacobian) {
        // KCL
        *(d.jacPFlow+loadSetup.jacobianLoadOffset) += p.mfactor;
        *(d.jacNFlow+loadSetup.jacobianLoadOffset) += -p.mfactor;
        // Extra equation
        *(d.jacFlowP+loadSetup.jacobianLoadOffset) += -1.0;
        *(d.jacFlowN+loadSetup.jacobianLoadOffset) += 1.0;
    }

    // Load resistive residual
    if (loadSetup.resistiveResidual) {
        loadSetup.resistiveResidual[d.uP] += d.flowResidual;
        loadSetup.resistiveResidual[d.uN] += -d.flowResidual;
        loadSetup.resistiveResidual[d.uFlow] += d.eqResidual;
    }

    // No limiting, so nothing to load for limited residual

    // Maximal residual contribution
    if (loadSetup.maxResistiveResidualContribution) {
        auto flowContrib = std::abs(d.flowResidual);
        auto eqContrib = std::abs(d.eqResidual);
        if (loadSetup.maxResistiveResidualContribution[d.uP]<flowContrib) {
            loadSetup.maxResistiveResidualContribution[d.uP] = flowContrib;
        }
        if (loadSetup.maxResistiveResidualContribution[d.uN]<flowContrib) {
            loadSetup.maxResistiveResidualContribution[d.uN] = flowContrib;
        }
        if (loadSetup.maxResistiveResidualContribution[d.uFlow]<eqContrib) {
            loadSetup.maxResistiveResidualContribution[d.uFlow] = eqContrib;
        }
    }

    // No reactive component, reactive residual derivative wrt. time is zero

    // DC increment residual
    if (loadSetup.dcIncrementResidual) { 
        loadSetup.dcIncrementResidual[d.uFlow] += p.mag;
    }

    // AC residual
    if (loadSetup.acResidual) {
        double re = p.mag*std::cos(p.phase*PI/180);
        double im = p.mag*std::sin(p.phase*PI/180);
        loadSetup.acResidual[d.uFlow] += Complex(re, im);
    }

    return true;
}

template<> bool BuiltinISourceInstance::evalCore(Circuit& circuit, CommonData& commons, EvalSetup& evalSetup) {
    auto& p = params.core();
    auto& d = data.core();
    auto sourceFactor = commons.sourcescalefactor;
    
    // Evaluate, placeholder for bypass implementation
    auto [val, nextBreakpoint] = sourceCompute(p, d, evalSetup.time);
    if (true) {
        if (evalSetup.evaluateResistiveResidual) {
            if (evalSetup.evaluateResistiveResidual) {
                d.flowResidual = sourceFactor*p.mfactor*val;
                // Ooutput variables
                d.i = sourceFactor*val; // current of one parallel instance
                d.v = evalSetup.oldSolution[d.uP] - evalSetup.oldSolution[d.uN]; // voltage across instance
            }  
        } 
    }

    // Next breakjpoint
    if (evalSetup.computeNextBreakpoint) {
        evalSetup.setBreakPoint(nextBreakpoint, commons); 
    }

    // Set maximal frequency
    if (evalSetup.computeMaxFreq) {
        if (d.typeCode==IndependentSourceType::Sine) {
            evalSetup.setMaxFreq(p.freq);
        }
    }

    return true;
}

template<> bool BuiltinISourceInstance::loadCore(Circuit& circuit, CommonData& commons, LoadSetup& loadSetup) {
    auto& p = params.core();
    auto& d = data.core();
    
    // Load resistive residual
    if (loadSetup.resistiveResidual) {
        loadSetup.resistiveResidual[d.uP] += d.flowResidual;
        loadSetup.resistiveResidual[d.uN] += -d.flowResidual;
    }

    // No limiting, so nothing to load for limited residual

    // Maximal residual contribution
    if (loadSetup.maxResistiveResidualContribution) {
        auto flowContrib = std::abs(d.flowResidual); 
        if (loadSetup.maxResistiveResidualContribution[d.uP]<flowContrib) {
            loadSetup.maxResistiveResidualContribution[d.uP] = flowContrib;
        }
        if (loadSetup.maxResistiveResidualContribution[d.uN]<flowContrib) {
            loadSetup.maxResistiveResidualContribution[d.uN] = flowContrib;
        }
    }

    // No reactive component, reactive residual derivative wrt. time is zero

    // DC increment residual
    if (loadSetup.dcIncrementResidual) { 
        loadSetup.dcIncrementResidual[d.uP] += p.mfactor*p.mag;
        loadSetup.dcIncrementResidual[d.uN] += -p.mfactor*p.mag;
    }

    // AC residual
    if (loadSetup.acResidual) {
        double re = p.mfactor*p.mag*std::cos(p.phase*PI/180);
        double im = p.mfactor*p.mag*std::sin(p.phase*PI/180);
        loadSetup.acResidual[d.uP] += Complex(re, im);
        loadSetup.acResidual[d.uN] += -Complex(re, im);
    }

    return true;
}

}
