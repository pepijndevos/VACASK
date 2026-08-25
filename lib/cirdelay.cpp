#include "cirdelay.h"


namespace NAMESPACE {

template<typename T> bool DelayLines::bindToMatrix(
    std::conditional_t<std::is_same_v<T, Complex*>, KluComplexMatrix, KluRealMatrix>& mat,
    const std::optional<MatrixEntryPosition>& mep,
    DelayMatrixBindings<T>& bindings,
    Status& s
) {
    // Loop through all delay lines, get both unknowns (in, out)
    auto n = static_cast<GlobalStorageIndex>(inputUnknown_.size());
    bindings.resize(n);
    for (GlobalStorageIndex slot=0; slot<n; slot++) {
        auto in = inputUnknown_[slot];
        auto out = outputUnknown_[slot];

        // For each delay line store matrix bindings to elements (out, in) and
        // (out, out) - the equation is the output unknown's own; mep
        // (if given) is passed through as blockMep, the same convention
        // OsdiInstance::bindCore uses for its Jacobian entries (compare
        // matResist->valuePtr(MatrixEntryPosition(e, u), comp, mepResist)).
        T outIn, outOut;
        if constexpr (std::is_same_v<T, Complex*>) {
            outIn = mat.cxValuePtr(MatrixEntryPosition(out, in), mep);
            outOut = mat.cxValuePtr(MatrixEntryPosition(out, out), mep);
        } else {
            outIn = mat.valuePtr(MatrixEntryPosition(out, in), Component::Real, mep);
            outOut = mat.valuePtr(MatrixEntryPosition(out, out), Component::Real, mep);
        }
        if (!outIn || !outOut) {
            s.set(Status::BadConversion, "Matrix entry not found for delay element.");
            return false;
        }
        bindings[slot] = std::make_tuple(outIn, outOut);
    }
    return true;
}

// Explicit instantiation: T selects both the pointer type stored in
// bindings and (via the conditional_t in the declaration) the matrix type
// - double*/KluRealMatrix for a real matrix, Complex*/KluComplexMatrix for
// a complex one.
template bool DelayLines::bindToMatrix<double*>(KluRealMatrix&, const std::optional<MatrixEntryPosition>&, DelayMatrixBindings<double*>&, Status&);
template bool DelayLines::bindToMatrix<Complex*>(KluComplexMatrix&, const std::optional<MatrixEntryPosition>&, DelayMatrixBindings<Complex*>&, Status&);

template<typename T> bool DelayLines::bindToMatrixBlock(
    std::conditional_t<std::is_same_v<T, DenseMatrixView<Complex>>, KluBlockSparseComplexMatrix, KluBlockSparseRealMatrix>* mat,
    DelayMatrixBindings<T>& bindings,
    Status& s
) {
    // Loop through all delay lines, get both unknowns (in, out)
    auto n = static_cast<GlobalStorageIndex>(inputUnknown_.size());
    bindings.clear();
    bindings.reserve(n);
    for (GlobalStorageIndex slot=0; slot<n; slot++) {
        auto in = inputUnknown_[slot];
        auto out = outputUnknown_[slot];

        // For each delay line store matrix block bindings to elements
        // (out, in) and (out, out) - block() returns the DenseMatrixView of
        // the whole dense block at that position, not a single scalar.
        auto [blockOutIn, foundOutIn] = mat->block(MatrixEntryPosition(out, in));
        auto [blockOutOut, foundOutOut] = mat->block(MatrixEntryPosition(out, out));
        if (!foundOutIn || !foundOutOut) {
            s.set(Status::BadConversion, "Matrix entry not found for delay element.");
            return false;
        }
        // Built via emplace_back (constructing the tuple's DenseMatrixViews
        // in place from blockOutIn/blockOutOut's copy constructor), not
        // resize()+operator= - DenseMatrixView::operator=() writes element
        // values through an already-bound view instead of rebinding it, so
        // assigning into a default-constructed (empty) slot would silently
        // do nothing rather than store the binding.
        bindings.emplace_back(blockOutIn, blockOutOut);
    }
    return true;
}

// Explicit instantiation: T selects both the DenseMatrixView value type
// stored in bindings and (via the conditional_t in the declaration) the
// matrix type - DenseMatrixView<double>/KluBlockSparseRealMatrix for a real
// matrix, DenseMatrixView<Complex>/KluBlockSparseComplexMatrix for a complex one.
template bool DelayLines::bindToMatrixBlock<DenseMatrixView<double>>(KluBlockSparseRealMatrix*, DelayMatrixBindings<DenseMatrixView<double>>&, Status&);
template bool DelayLines::bindToMatrixBlock<DenseMatrixView<Complex>>(KluBlockSparseComplexMatrix*, DelayMatrixBindings<DenseMatrixView<Complex>>&, Status&);

}
