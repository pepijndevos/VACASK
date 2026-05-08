import numpy as np
import matplotlib.pyplot as plt

# Paste the output from the C++ dump here

# Block (2, 2) FD jacobians
f = np.array([-2.000000000000000e+05, -1.000000000000000e+05, 0.000000000000000e+00, 1.000000000000000e+05, 2.000000000000000e+05])
GCol = np.array([-1.633084675898337e-01+1.440253009531086e-02j, -1.443657546831249e-02+2.869646154904137e-01j, 3.220769265842021e-01+0.000000000000000e+00j, -1.443657546831249e-02+-2.869646154904137e-01j, -1.633084675898337e-01+-1.440253009531086e-02j])
CCol = np.array([0.000000000000000e+00+-0.000000000000000e+00j, 0.000000000000000e+00+-0.000000000000000e+00j, 0.000000000000000e+00+0.000000000000000e+00j, 0.000000000000000e+00+0.000000000000000e+00j, 0.000000000000000e+00+0.000000000000000e+00j])


# --- inverse DFT via explicit Fourier synthesis ---
# Works for any (non-uniform) frequency set.
# GCol/CCol are the two-sided complex spectra; result is real for physical signals.

df = f[1] - f[0]          # frequency step
T  = 1.0 / df             # period

t_td = np.linspace(0, T, 1000, endpoint=False)
E = np.exp(2j * np.pi * f[np.newaxis, :] * t_td[:, np.newaxis])  # (n_t, n_f)
g_td = np.real(E @ GCol)
c_td = np.real(E @ CCol)

# --- plot ---

fig, axes = plt.subplots(2, 2, figsize=(14, 7))
(ax_Gf, ax_Cf), (ax_Gt, ax_Ct) = axes

# frequency domain — magnitude spectra
ax_Gf.stem(f, np.abs(GCol), markerfmt='C0o', linefmt='C0-', basefmt='k-')
ax_Gf.set_ylabel('|G|')
ax_Gf.set_title('G — frequency domain')
ax_Gf.set_xlabel('Frequency (Hz)')
ax_Gf.grid(True)

ax_Cf.stem(f, np.abs(CCol), markerfmt='C1o', linefmt='C1-', basefmt='k-')
ax_Cf.set_ylabel('|C|')
ax_Cf.set_title('C — frequency domain')
ax_Cf.set_xlabel('Frequency (Hz)')
ax_Cf.grid(True)

# time domain — reconstructed waveforms
ax_Gt.plot(t_td * 1e6, g_td, 'C0')
ax_Gt.set_ylabel('G(t)')
ax_Gt.set_title('G — time domain')
ax_Gt.set_xlabel('Time (µs)')
ax_Gt.grid(True)

ax_Ct.plot(t_td * 1e6, c_td, 'C1')
ax_Ct.set_ylabel('C(t)')
ax_Ct.set_title('C — time domain')
ax_Ct.set_xlabel('Time (µs)')
ax_Ct.grid(True)

fig.suptitle('FD Jacobian column — frequency and time domain')
plt.tight_layout()
plt.show()
