import sys
sys.path.insert(0, 'build/lib/vacask/python')
from rawfile import rawread
import numpy as np
import matplotlib.pyplot as plt

r = rawread('pss1_stabtran.raw').get()
print('Variables:', r.names)
print('Points:   ', r.data.shape[0])

t  = np.real(r['time'])
v1 = np.real(r['1'])

V_exact = np.sqrt(4.0 * 0.01 / (3.0 * 0.01))
print(f"Final envelope ~{np.max(np.abs(v1[-200:])):.4f} V  (expected {V_exact:.4f} V)")

plt.figure(figsize=(10, 4))
plt.title('PSS stabilisation transient — V(1)')
plt.xlabel('time [µs]')
plt.ylabel('V(1) [V]')
plt.plot(t * 1e6, v1, lw=0.7)
plt.axhline( V_exact, color='r', ls='--', label=f'±{V_exact:.3f} V')
plt.axhline(-V_exact, color='r', ls='--')
plt.legend()
plt.tight_layout()
plt.show()
