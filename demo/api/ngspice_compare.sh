#!/usr/bin/env bash
# ngspice_compare.sh <spice_cir>
# Run ngspice on the given SPICE RC netlist, parse the binary raw file,
# and verify the RC time constant τ = R*C = 2ms.
# Expected circuit: V1 PULSE(0 5 1m 1u 1u 4m 10m), R1=1k, C1=2u  → τ=2ms.
# Exits 77 (SKIP) if ngspice is not found, so CI without ngspice still builds.
set -euo pipefail

CIR="${1:-spice_rc.cir}"
NGSPICE="${NGSPICE_BIN:-/usr/local/bin/ngspice}"

if [ ! -x "$NGSPICE" ]; then
    echo "SKIP: ngspice not found at $NGSPICE"
    exit 77
fi

if [ ! -f "$CIR" ]; then
    echo "ERROR: circuit file not found: $CIR"
    exit 1
fi

WORK=/home/pepijn/ngspice_cmp_$$
mkdir -p "$WORK"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT

RAW="$WORK/out.raw"

"$NGSPICE" -b -r "$RAW" "$CIR" > "$WORK/ngspice.log" 2>&1 || {
    echo "ERROR: ngspice failed:"
    cat "$WORK/ngspice.log"
    exit 1
}

echo "ngspice log:"
cat "$WORK/ngspice.log"

python3 - "$RAW" <<'PYEOF'
import sys, struct, math

rawfile = sys.argv[1]
with open(rawfile, 'rb') as f:
    raw = f.read()

# Locate "Binary:\n".
binary_marker = b'Binary:\n'
pos = raw.find(binary_marker)
if pos < 0:
    print("ERROR: 'Binary:' not found in raw file"); sys.exit(1)

header_text = raw[:pos].decode('ascii', errors='replace')
data_bytes   = raw[pos + len(binary_marker):]

# Parse header into dict. ngspice format:
#   Key: value\n           (normal field)
#   Key:\n                 (section header, value is empty, continuations follow)
#   \tcontents\n           (tab-indented continuation of previous key)
fields = {}
cur_key = None
for line in header_text.splitlines():
    if line.startswith('\t') and cur_key is not None:
        fields[cur_key] = fields.get(cur_key, '') + '\n' + line
    elif line.endswith(':') and ' ' not in line:
        # Section header with no value (e.g. "Variables:")
        cur_key = line[:-1]
        fields[cur_key] = ''
    elif ': ' in line:
        cur_key, v = line.split(': ', 1)
        fields[cur_key] = v.strip()
    else:
        cur_key = None

nvars   = int(fields.get('No. Variables', '0').strip())
npoints = int(fields.get('No. Points',    '0').strip())
flags   = fields.get('Flags', '').lower()
print(f"Variables: {nvars}, Points: {npoints}, Flags: {flags}")

# Parse variable table: each continuation line is "\tidx\tname\ttype"
var_names = []
for line in fields.get('Variables', '').splitlines():
    parts = line.strip().split('\t')
    if len(parts) >= 2:
        var_names.append(parts[1].lower())   # "v(2)", "time", ...
print(f"Var names: {var_names}")

# Read binary doubles.
is_complex = 'complex' in flags
skip = 8 if is_complex else 0   # imaginary part to skip per value

avail = len(data_bytes) // (nvars * (8 + skip))
if avail < npoints:
    print(f"WARNING: expected {npoints} pts, only {avail} fit; truncating")
    npoints = avail

data = {n: [] for n in var_names}
offset = 0
for _ in range(npoints):
    for name in var_names:
        val = struct.unpack_from('d', data_bytes, offset)[0]
        data[name].append(val)
        offset += 8 + skip

time  = data.get('time', [])
node2 = data.get('v(2)', data.get('2', None))
if node2 is None:
    non_t = [k for k in var_names if k != 'time']
    node2 = data[non_t[0]] if non_t else None

if not time or node2 is None:
    print(f"ERROR: could not extract node 2; keys={list(data.keys())}"); sys.exit(1)

print(f"Time range: {time[0]:.3e} .. {time[-1]:.3e} s  ({len(time)} points)")
print(f"Node 2 range: {min(node2):.4f} V .. {max(node2):.4f} V")

# Analytic RC check: τ = 1kΩ * 2μF = 2ms.
# PULSE delay=1ms. At t=3ms: V(2) = 5*(1-e^-1) ≈ 3.161 V.
tau     = 2e-3
V_final = 5.0
t_rise  = 1e-3

t_target = t_rise + tau
V_expect = V_final * (1.0 - math.exp(-1.0))

idx = min(range(len(time)), key=lambda i: abs(time[i] - t_target))
t_act = time[idx];  v_act = node2[idx]

print(f"\nRC τ={tau*1e3:.0f}ms check: at t={t_target*1e3:.1f}ms (actual={t_act*1e3:.3f}ms)")
print(f"  Expected V(2) = {V_expect:.4f} V  (= V_final*(1-1/e))")
print(f"  Actual   V(2) = {v_act:.4f} V")

tol_abs = 0.15   # 150 mV
tol_rel = 0.05   # 5%
err = abs(v_act - V_expect)
rel = err / V_expect

if err > tol_abs and rel > tol_rel:
    print(f"FAIL: error {err:.4f} V ({rel*100:.1f}%) exceeds tolerances"); sys.exit(1)

print(f"PASS: error {err:.4f} V ({rel*100:.2f}%) within tolerance")

print("\nSample node-2 voltages:")
for ts in [1e-3, 2e-3, 3e-3, 5e-3, 9e-3]:
    i = min(range(len(time)), key=lambda j: abs(time[j] - ts))
    print(f"  t={time[i]*1e3:.2f}ms  V(2)={node2[i]:.4f} V")
PYEOF
