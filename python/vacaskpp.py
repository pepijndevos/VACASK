import numpy as np

__all__ = [ 'errorWrtRef', 'logBinMean' ]

# Compute error in the range given by fref, frequency values taken from fsim
def errorWrtRef(fref, Sref, fsim, Ssim, mode="db10"):
    fmin = fref.min()
    fmax = fref.max()
    f = fsim[np.where((fsim>=fmin)*(fsim<=fmax))]
    if f[0]>fmin:
        f = np.concatenate((np.array([fmin]), f))
    if f[-1]<fmax:
        f = np.concatenate((f, np.array([fmax])))
    Ssimi = np.interp(f, fsim, Ssim)
    Srefi = np.interp(f, fref, Sref)
    if mode=="db10":
        err = 10*np.log10(Ssimi/Srefi)
    elif mode=="db20":
        err = 20*np.log10(Ssimi/Srefi)
    elif mode=="lin":
        err = Ssimi-Srefi
    return f, err

# Log bin mean
def logBinMean(f, Pxx, binsPerDecade=10):
    f = np.asarray(f)
    Pxx = np.asarray(Pxx)

    m = (f > 0) & np.isfinite(Pxx)
    f, Pxx = f[m], Pxx[m]

    decades = np.log10(f.max()) - np.log10(f.min())
    nbins = int(np.ceil(decades * binsPerDecade))

    edges = np.logspace(np.log10(f.min()), np.log10(f.max()), nbins + 1)
    idx = np.digitize(f, edges) - 1

    fb = np.array([np.sqrt(edges[i] * edges[i+1]) for i in range(nbins)])
    Pb = np.array([Pxx[idx == i].mean() if np.any(idx == i) else np.nan for i in range(nbins)])
    
    # Find last nan, get first frequency after last nan
    idx = np.where(np.isnan(Pb))[0]
    if idx.size>0:
        # Have nan, get index of first frequency after nan
        ifirst = idx[-1]+1
        # Find last original frequency that is less than fb[ifirst]
        ilast = np.where(f<fb[ifirst])[0][-1]

        # Up to ifirst copy original f and Pxx
        fret = np.concatenate((f[:ilast+1], fb[ifirst:]))
        Pret = np.concatenate((Pxx[:ilast+1], Pb[ifirst:]))
    else:
        # No nan bins, return binned arrays directly
        fret = fb
        Pret = Pb

    return fret, Pret
    