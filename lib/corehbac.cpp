#include <numbers>
#include <complex>
#include "corehbac.h"
#include "spurs.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

// Default parameters
HBACParameters::HBACParameters() {
    hbParams.write = 0;
}

template<> int Introspection<HBACParameters>::setup() {
    registerMember(from);
    registerMember(to);
    registerMember(step);
    registerMember(mode);
    registerMember(points);
    registerMember(values);
    registerMember(writehb);
    registerMember(outspur);
    registerMember(write);
    registerNamedMember(hbParams.freq, "freq");
    registerNamedMember(hbParams.nharm, "nharm");
    registerNamedMember(hbParams.immax, "immax");
    registerNamedMember(hbParams.truncate, "truncate");
    registerNamedMember(hbParams.samplefac, "samplefac");
    registerNamedMember(hbParams.nper, "nper");
    registerNamedMember(hbParams.sample, "sample");
    registerNamedMember(hbParams.shift, "shift");
    registerNamedMember(hbParams.nodeset, "nodeset");
    registerNamedMember(hbParams.store, "store");
    
    return 0;
}
instantiateIntrospection(HBACParameters);

HBACCore::HBACCore(
    OutputDescriptorResolver& parentResolver, HBACParameters& params, HBCore& hbCore, 
    Circuit& circuit, CommonData& commons, 
    KluBlockSparseComplexMatrix& jacSpec, 
    VectorRepository<Complex>& hbSolution, 
    KluBlockSparseComplexMatrix& acMatrix, Vector<Complex>& acSolution
) : AnalysisCore(parentResolver, circuit, commons), params(params), outfile(nullptr), hbCore_(hbCore), 
    jacSpec(jacSpec), 
    hbSolution(hbSolution), 
    acMatrix(acMatrix), acSolution(acSolution) {
}

HBACCore::~HBACCore() {
    delete outfile;
}

// TODO
bool HBACCore::resolveOutputDescriptors(bool strict, Status& s) {
    // Clear output sources
    outputSources.clear();
    // Resolve output descriptors
    bool ok = true; 
    auto nStoredSpurs = spurIndices.size();
    auto nSpurs = hbCore_.spurs().smsigFreq().size();

    for (auto it = outputDescriptors.cbegin(); it != outputDescriptors.cend(); ++it) {
        Node *node;
        Instance *inst;
        switch (it->type) {
        case OutdSolComponent:
            for(decltype(nSpurs) i=0; i<nStoredSpurs; i++) {
                Id name = std::string(it->id)+";"+suffixes[i];
                ok = addComplexVarOutputSource(strict, it->id, acSolution, nSpurs, spurIndices[i], name, s);
            }
            break;
        case OutdFrequency:
            outputSources.emplace_back(&frequency, it->name);
            break;
        default:
            // Delegate to parent
            ok = parentResolver.resolveOutputDescriptor(*it, outputSources, strict, s);
            break;
        }
        if (!ok) {
            break;
        }
    }
    return ok;
}

bool HBACCore::addCoreOutputDescriptors(Status& s) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (!addOutputDescriptor(OutputDescriptor(OutdFrequency, "frequency"))) {
        s.set(Status::Analysis, std::string("Failed to add output descriptor for frequency."));
        return false;
    }
    return true;
}

bool HBACCore::addDefaultOutputDescriptors(Status& s) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (savesCount==0) {
        return addAllUnknowns(PTSave("default", Id(), Id()), s);
    }
    return true;
}

bool HBACCore::initializeOutputs(Id name, Status& s) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    // Create output file if not created yet
    if (!outfile) {
        outfile = new OutputRawfile(
            name, outputDescriptors, outputSources,
            (circuit.simulatorOptions().core().rawfile==SimulatorOptions::rawfileBinary ? OutputRawfile::Flags::Binary : OutputRawfile::Flags::None) |
                OutputRawfile::Flags::Padded | OutputRawfile::Flags::Complex);
        outfile->setTitle(circuit.title());
        outfile->setPlotname("HBAC Small Signal Analysis");
    }
    outfile->prologue();

    return true;
}

bool HBACCore::finalizeOutputs(Status& s) {
    if (outfile) {
        outfile->epilogue();
        delete outfile;
        outfile = nullptr;
    }
    return true;
}

bool HBACCore::deleteOutputs(Id name, Status& s) {
    if (!params.write || Simulator::noOutput()) {
        return true;
    }

    // Cannot assume outfile is available
    auto fname = std::string(name)+".raw";
    if (std::filesystem::exists(fname)) {
        std::filesystem::remove(fname);
    }
    return true;
}
  

// TODO: make list of sources every time core is invoked
//       somebody might sweep cs* parameters of sources


void HBACCore::constructSuffixes() {
    auto& spurs = hbCore_.spurs();
    auto nf = spurs.smsigFreq().size();
    suffixes.resize(nf);
    for (auto i : spurIndices) {
        auto w = spurs.smsigFreqWeights(i);
        std::string s;
        for (size_t k = 0; k < w.n(); k++) {
            if (k > 0) s += ',';
            s += std::to_string(w[k]);
        }
        suffixes[i] = std::move(s);
    }
}


// Construct omega vector: omega[n] = 2*pi*(f + f_n)
// where f_n = smsigFreq[n] is the signed spur frequency from the HB operating point.
// f    - small-signal input frequency (Hz)
// omega - output vector, resized to nf (number of spurs)
void HBACCore::computeOmega(Vector<Real>& omega, Real f) {
    auto& smsigFreq = hbCore_.spurs().smsigFreq();
    auto nf = smsigFreq.size();
    for (size_t n = 0; n < nf; n++) {
        omega[n] = 2.0 * std::numbers::pi * (f + smsigFreq[n]);
    }
}

// Fill one (p,q) subblock of the conversion matrix H(omega).
//
// The (n,m) entry of the subblock is:
//   h_nm = G[k] + j*(omega+omega_n)*C[k]
// where k is the Jacobian harmonic index determined by the mixing stencil,
// and omega is the small-signal frequency for output row n.
//
// Parameters:
//   G    - Fourier coefficients [G_k]_pq of the resistive Jacobian,
//          indexed by 0-based Jacobian frequency index
//          only positive part of the spectrum
//   C    - Fourier coefficients [C_k]_pq of the reactive Jacobian,
//          indexed by 0-based Jacobian frequency index
//          only positive part of the spectrum
//   omega - small-signal frequencies 2 pi (f+f_n), one per output row n
//   block - (p,q) subblock of H(omega) to fill, size nf x nf
//           column-major assumed
void HBACCore::fillDenseBlock(
    const VectorView<Complex>& G,
    const VectorView<Complex>& C,
    const Vector<Real>& omega,
    DenseMatrixView<Complex>& block
) {
    auto& spurs = hbCore_.spurs();
    auto& stencil = spurs.mixingStencil();
    auto nf = stencil.nRows();

    // Outer loop over columns (assume column major matrix)
    auto* p = &block.at(0, 0);
    auto* jacIndex = &stencil.at(0, 0);
    for (size_t m = 0; m < nf; m++) {
        // Omega is common for the whole row
        auto* om = &omega.at(0);
        auto [start, end] = spurs.rowRange(m);
        auto p1 = p + start;
        for(size_t n = start; n < end; n++) {
            if (*jacIndex != Spurs::noJacIndex) {
                bool conjugated = (*jacIndex < 0);
                auto k = (conjugated ? (-*jacIndex) : *jacIndex) - 1;
                Complex g = conjugated ? std::conj(G[k]) : G[k];
                Complex c = conjugated ? std::conj(C[k]) : C[k];
                *p1 = g + Complex(0.0, *om) * c;
                p1++;
                jacIndex++;
                om++;
            }
        }
        p += nf;
    }
}

}
