#!/usr/bin/env bash
# ngspice_compare.sh <spice_cir> <vacask_raw> [sample_times_csv] [tol_abs_v] [tol_rel_frac]
# Run ngspice on the given SPICE netlist, parse both the VACASK raw and the
# ngspice raw, sample V(2) (node 2) at several matching time points, and compare
# VACASK-vs-ngspice at each point.  Exits nonzero if ANY point exceeds tolerance.
# Exits 77 (SKIP) if ngspice is not found, so CI without ngspice still builds.
#
# Optional third argument: comma-separated sample times in seconds, e.g.
#   "0.25e-3,1.25e-3,2.25e-3"
# When omitted, defaults to [2,4,6,8,9] ms.
#
# Optional fourth/fifth arguments: absolute tolerance (V) and relative tolerance
# (fraction).  When omitted, defaults to 5e-3 V and 0.02 (2%).
set -euo pipefail

CIR="${1:-spice_rc.cir}"
VACASK_RAW="${2:-}"
SAMPLE_TS_CSV="${3:-}"
TOL_ABS="${4:-5e-3}"
TOL_REL="${5:-0.02}"

NGSPICE="${NGSPICE_BIN:-/usr/local/bin/ngspice}"

if [ ! -x "$NGSPICE" ]; then
    echo "SKIP: ngspice not found at $NGSPICE"
    exit 77
fi

if [ ! -f "$CIR" ]; then
    echo "ERROR: circuit file not found: $CIR"
    exit 1
fi

if [ -z "$VACASK_RAW" ] || [ ! -f "$VACASK_RAW" ]; then
    echo "ERROR: VACASK raw file not found: ${VACASK_RAW:-<not specified>}"
    exit 1
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

NGSPICE_RAW="$WORK/out.raw"

"$NGSPICE" -b -r "$NGSPICE_RAW" "$CIR" > "$WORK/ngspice.log" 2>&1 || {
    echo "ERROR: ngspice failed:"
    cat "$WORK/ngspice.log"
    exit 1
}

echo "ngspice log:"
cat "$WORK/ngspice.log"

python3 - "$VACASK_RAW" "$NGSPICE_RAW" "$SAMPLE_TS_CSV" "$TOL_ABS" "$TOL_REL" <<'PYEOF'
import sys, struct, math

def parse_raw(rawfile, node2_candidates):
    """Parse a SPICE-format binary or ASCII raw file.
    Returns (times, node2_vals) using the first matching name from node2_candidates."""
    with open(rawfile, 'rb') as f:
        raw = f.read()

    binary_marker = b'Binary:\n'
    values_marker = b'Values:\n'
    bpos = raw.find(binary_marker)
    vpos = raw.find(values_marker)

    is_binary = (bpos >= 0) and (vpos < 0 or bpos < vpos)
    if is_binary:
        header_text = raw[:bpos].decode('ascii', errors='replace')
        data_bytes   = raw[bpos + len(binary_marker):]
    elif vpos >= 0:
        header_text = raw[:vpos].decode('ascii', errors='replace')
        data_bytes   = raw[vpos + len(values_marker):]
    else:
        print(f"ERROR: no 'Binary:' or 'Values:' in {rawfile}"); sys.exit(1)

    # Parse header into dict.
    fields = {}
    cur_key = None
    for line in header_text.splitlines():
        if line.startswith('\t') and cur_key is not None:
            fields[cur_key] = fields.get(cur_key, '') + '\n' + line
        elif line.endswith(':') and ' ' not in line:
            cur_key = line[:-1]; fields[cur_key] = ''
        elif ': ' in line:
            cur_key, v = line.split(': ', 1); fields[cur_key] = v.strip()
        else:
            cur_key = None

    nvars   = int(fields.get('No. Variables', '0').strip())
    npoints = int(fields.get('No. Points',    '0').strip())
    flags   = fields.get('Flags', '').lower()
    is_complex = 'complex' in flags

    var_names = []
    for line in fields.get('Variables', '').splitlines():
        parts = line.strip().split('\t')
        if len(parts) >= 2:
            var_names.append(parts[1].lower())

    print(f"  {rawfile}:")
    print(f"    {nvars} vars, {npoints} pts, flags='{flags}'")
    print(f"    vars: {var_names}")

    if is_binary:
        bytes_per_val = 16 if is_complex else 8
        avail = len(data_bytes) // (nvars * bytes_per_val)
        npoints = min(npoints, avail)
        times, node2 = [], []
        for i in range(npoints):
            row_off = i * nvars * bytes_per_val
            t = struct.unpack_from('d', data_bytes, row_off)[0]
            times.append(t)
        # Find node2 column index
        n2_idx = None
        for cand in node2_candidates:
            if cand in var_names:
                n2_idx = var_names.index(cand)
                break
        if n2_idx is None:
            return times, None
        for i in range(npoints):
            row_off = i * nvars * bytes_per_val + n2_idx * bytes_per_val
            v = struct.unpack_from('d', data_bytes, row_off)[0]
            node2.append(v)
        return times, node2
    else:
        # ASCII: " idx\tval\n\tval\n..."
        n2_idx = None
        for cand in node2_candidates:
            if cand in var_names:
                n2_idx = var_names.index(cand)
                break
        if n2_idx is None:
            return [], None
        lines = data_bytes.decode('ascii', errors='replace').splitlines()
        times, node2 = [], []
        i = 0
        pt = 0
        while i < len(lines) and pt < npoints:
            line = lines[i].strip()
            if not line:
                i += 1; continue
            parts = line.split()
            if len(parts) >= 2:
                try:
                    int(parts[0])
                    row = [float(parts[1])]
                    i += 1
                    for vi in range(1, nvars):
                        if i < len(lines):
                            row.append(float(lines[i].strip()))
                            i += 1
                    times.append(row[0])
                    node2.append(row[n2_idx])
                    pt += 1
                except ValueError:
                    i += 1
            else:
                i += 1
        return times, node2


def interpolate(times, vals, t_target):
    """Linear interpolation of vals at t_target."""
    if not times: return None
    if t_target <= times[0]: return vals[0]
    if t_target >= times[-1]: return vals[-1]
    for i in range(len(times) - 1):
        if times[i] <= t_target <= times[i + 1]:
            frac = (t_target - times[i]) / (times[i + 1] - times[i])
            return vals[i] + frac * (vals[i + 1] - vals[i])
    return vals[-1]


vacask_raw      = sys.argv[1]
ngspice_raw     = sys.argv[2]
sample_ts_csv   = sys.argv[3] if len(sys.argv) > 3 else ""
tol_abs         = float(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4] else 5e-3
tol_rel         = float(sys.argv[5]) if len(sys.argv) > 5 and sys.argv[5] else 0.02

print("\nParsing VACASK raw:")
v_times, v_node2 = parse_raw(vacask_raw,  ['2', 'v(2)'])
print("\nParsing ngspice raw:")
n_times, n_node2 = parse_raw(ngspice_raw, ['v(2)', '2'])

if v_node2 is None:
    print(f"ERROR: could not find node-2 variable in VACASK raw {vacask_raw}")
    sys.exit(1)
if n_node2 is None:
    print(f"ERROR: could not find v(2) variable in ngspice raw {ngspice_raw}")
    sys.exit(1)

print(f"\nVACASK time range: {v_times[0]:.3e}..{v_times[-1]:.3e} s ({len(v_times)} pts)")
print(f"ngspice time range: {n_times[0]:.3e}..{n_times[-1]:.3e} s ({len(n_times)} pts)")

# Sample at several time points and compare VACASK vs ngspice using interpolation.
if sample_ts_csv:
    sample_ts = [float(x) for x in sample_ts_csv.split(',')]
else:
    sample_ts = [2e-3, 4e-3, 6e-3, 8e-3, 9e-3]
# tol_abs and tol_rel are set from command-line args above.

print("\nVACASK vs ngspice — V(node 2) comparison:")
print(f"{'t(ms)':>8}  {'VACASK(V)':>12}  {'ngspice(V)':>12}  "
      f"{'abs_err(V)':>12}  {'rel_err(%)':>10}  {'result':>6}")
print("-" * 76)

any_fail = False
for ts in sample_ts:
    vv = interpolate(v_times, v_node2, ts)
    nv = interpolate(n_times, n_node2, ts)
    if vv is None or nv is None:
        print(f"{ts*1e3:8.1f}  {'N/A':>12}  {'N/A':>12}  {'N/A':>12}  {'N/A':>10}  {'SKIP':>6}")
        continue
    err = abs(vv - nv)
    ref = abs(nv) if abs(nv) > 1e-9 else 1e-9
    rel = err / ref
    # Fail if EITHER threshold exceeded (OR gate — one failure is enough).
    fail = (err > tol_abs) or (rel > tol_rel)
    result = "FAIL" if fail else "PASS"
    if fail:
        any_fail = True
    print(f"{ts*1e3:8.1f}  {vv:12.6f}  {nv:12.6f}  {err:12.6f}  {rel*100:10.3f}  {result:>6}")

if any_fail:
    print(f"\nFAIL: VACASK-vs-ngspice difference exceeded "
          f"tol_abs={tol_abs*1e3:.0f}mV or tol_rel={tol_rel*100:.0f}%")
    sys.exit(1)

print(f"\nPASS: all sample points within "
      f"tol_abs={tol_abs*1e3:.0f}mV and tol_rel={tol_rel*100:.0f}%")
PYEOF
