#!/usr/bin/env python3
"""Per-cycle activity traces for a CPA/TVLA campaign (one encryption per VCD).

For every waveform, two WAL++ passes are made:
  1. `--fast-values-signals` on clock + data bus: clock edges and the two
     `.timestamp` marker stores around the encryption;
  2. `--fast-hw-scope <scope>` (or `--fast-hd-scope`): Hamming weight over all
     signals below <scope>, e.g. the whole Sailor core without testbench and
     memories.
The activity series is reduced to one sample per clock cycle and cut to the
marker window, giving one trace per VCD.

Trial labels are taken from the path: `r0`/`r1` = fixed/random plaintext and
`i<k>` = plaintext index, matched by --label-regex (default `r(\\d)_i(\\d+)`).
Output rows are sorted by index, so row k corresponds to plaintext k.

Presets (the paper's settings):
  sailor-rtl      scope TOP.SAILORBlackBoxAdapter.core, resample "last",
                  no marker debounce (Verilator VCDs, chiseltest testbench)
  sailor-netlist  scope sailor_tb.dut, resample "accumulate", debounce 20
                  cycles (Icarus VCDs of the SDF-annotated SG13G2 netlist)

Example:
  extract_campaign.py --preset sailor-rtl --jobs 16 \\
      --out-prefix traces_accel_n500 runs/*rv32izktzkn*cpa_single_r*_i*/*.vcd
Writes <prefix>_fixed.npy and <prefix>_random.npy (shape: traces x cycles).
"""
import argparse
import os
import re
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from walpp_tools import (SIGNALS_DIR, activity_series, bus_events,  # noqa: E402
                         cycle_edges_and_markers, resample_per_cycle, time_to_cycle)

PRESETS = {
    "sailor-rtl": {"signals": SIGNALS_DIR / "sailor_rtl_markers.txt",
                   "scope": "TOP.SAILORBlackBoxAdapter.core",
                   "resample": "last", "debounce": 0},
    "sailor-netlist": {"signals": SIGNALS_DIR / "sailor_netlist_markers.txt",
                       "scope": "sailor_tb.dut",
                       "resample": "accumulate", "debounce": 20},
}


def extract_one(wal_bin, vcd, signals, scope, resample, debounce, use_hd):
    edges, markers = cycle_edges_and_markers(bus_events(wal_bin, vcd, signals),
                                             debounce_cycles=debounce)
    if len(markers) != 2:
        raise ValueError(f"{vcd}: expected 2 marker stores, found {len(markers)}")
    t, v = activity_series(wal_bin, vcd, scope, use_hd)
    per_cycle = resample_per_cycle(edges, t, v, resample)
    return per_cycle[time_to_cycle(edges, markers[0]):time_to_cycle(edges, markers[1])]


def stack(traces):
    if not traces:
        return np.zeros((0, 0), dtype=int)
    lo, hi = min(len(t) for t in traces), max(len(t) for t in traces)
    if lo != hi:
        print(f"note: trace lengths vary {lo}..{hi} cycles, truncating to {lo}", file=sys.stderr)
    return np.stack([t[:lo] for t in traces])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("vcds", nargs="+", type=Path, help="one VCD per trial")
    ap.add_argument("--preset", choices=sorted(PRESETS), default="sailor-rtl")
    ap.add_argument("--signals", type=Path, help="override the preset's marker signal list")
    ap.add_argument("--scope", help="override the preset's activity scope")
    ap.add_argument("--resample-method", choices=["last", "accumulate"], help="override the preset")
    ap.add_argument("--debounce-cycles", type=int, help="override the preset")
    ap.add_argument("--use-hd", action="store_true", help="Hamming distance instead of weight")
    ap.add_argument("--label-regex", default=r"r(\d)_i(\d+)")
    ap.add_argument("--min-index", type=int, default=0)
    ap.add_argument("--max-index", type=int, help="only trials with index < max-index")
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    ap.add_argument("--wal-bin", default="wal", help="WAL++ executable (default: wal on PATH)")
    ap.add_argument("--out-prefix", required=True)
    args = ap.parse_args()

    p = PRESETS[args.preset]
    signals = args.signals or p["signals"]
    scope = args.scope or p["scope"]
    resample = args.resample_method or p["resample"]
    debounce = p["debounce"] if args.debounce_cycles is None else args.debounce_cycles

    label_re = re.compile(args.label_regex)
    jobs = []
    for vcd in args.vcds:
        matches = label_re.findall(str(vcd))
        if not matches:
            print(f"skipping {vcd}: no trial label", file=sys.stderr)
            continue
        r, i = matches[-1]
        idx = int(i)
        if idx < args.min_index or (args.max_index is not None and idx >= args.max_index):
            continue
        jobs.append((vcd, r == "1", idx))

    fixed, rand = {}, {}
    start = time.monotonic()
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futs = {pool.submit(extract_one, args.wal_bin, vcd, signals, scope, resample,
                            debounce, args.use_hd): (vcd, is_random, idx)
                for vcd, is_random, idx in jobs}
        for n, fut in enumerate(as_completed(futs), 1):
            vcd, is_random, idx = futs[fut]
            trace = fut.result()
            (rand if is_random else fixed)[idx] = trace
            print(f"[{n}/{len(jobs)} {time.monotonic() - start:.0f}s] {vcd}: {len(trace)} cycles",
                  file=sys.stderr)

    fixed_arr = stack([fixed[k] for k in sorted(fixed)])
    rand_arr = stack([rand[k] for k in sorted(rand)])
    np.save(f"{args.out_prefix}_fixed.npy", fixed_arr)
    np.save(f"{args.out_prefix}_random.npy", rand_arr)
    print(f"wrote {fixed_arr.shape} fixed, {rand_arr.shape} random traces "
          f"in {time.monotonic() - start:.1f}s", file=sys.stderr)
    if fixed_arr.shape[0] > 1:
        print(f"sanity check, all fixed-plaintext traces identical: "
              f"{bool(np.all(fixed_arr == fixed_arr[0]))}", file=sys.stderr)


if __name__ == "__main__":
    main()
