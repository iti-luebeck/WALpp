#!/usr/bin/env python3
"""Per-instruction cycle latency from a constant-time probe waveform.

The probe program executes each AES instruction under test between two stores
to the `.timestamp` register (0x80003000). Both stores are data-bus
transactions in the waveform. WAL++ (`--fast-values-signals`) exports the
value changes of clock, address, isWrite and valid; the latency of a trial is
the number of rising clock edges between its two marker stores, so it
includes a constant store overhead.

Trials are labelled from the probe's static program layout: instructions
aes32esi, aes32esmi, aes32dsi, aes32dsmi (outer loop), byte-select 0..3, then
N trials each, i.e. 16 x N trials and 32 x N markers in one waveform.

Example:
  extract_latency.py --vcd ct_probe.vcd --n-trials 1000 -o latency.csv
"""
import argparse
import csv
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from walpp_tools import SIGNALS_DIR, bus_events, cycle_edges_and_markers, time_to_cycle  # noqa: E402

INSTRUCTIONS = ["aes32esi", "aes32esmi", "aes32dsi", "aes32dsmi"]
BS_VALUES = [0, 1, 2, 3]


def label_trials(marker_cycles, n_trials):
    expected = len(INSTRUCTIONS) * len(BS_VALUES) * n_trials * 2
    if len(marker_cycles) != expected:
        raise ValueError(
            f"expected {expected} marker stores ({len(INSTRUCTIONS)} instructions x "
            f"{len(BS_VALUES)} bs x {n_trials} trials x 2), found {len(marker_cycles)}; "
            f"does --n-trials match the probe binary?")
    rows = []
    i = 0
    for instr in INSTRUCTIONS:
        for bs in BS_VALUES:
            for trial in range(n_trials):
                start, end = marker_cycles[i], marker_cycles[i + 1]
                i += 2
                rows.append({"instruction": instr, "bs": bs, "trial": trial,
                             "start_cycle": start, "end_cycle": end,
                             "latency_cycles": end - start})
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--vcd", required=True, type=Path)
    ap.add_argument("--n-trials", required=True, type=int, help="trials per (instruction, bs)")
    ap.add_argument("--signals", type=Path, default=SIGNALS_DIR / "sailor_rtl_markers.txt",
                    help="marker signal list: clock, address, isWrite, valid (default: Sailor RTL)")
    ap.add_argument("--debounce-cycles", type=int, default=0,
                    help="merge marker detections closer than this (use 20 for netlist traces)")
    ap.add_argument("--wal-bin", default="wal", help="WAL++ executable (default: wal on PATH)")
    ap.add_argument("-o", "--output", required=True, type=Path)
    args = ap.parse_args()

    edges, markers = cycle_edges_and_markers(bus_events(args.wal_bin, args.vcd, args.signals),
                                             debounce_cycles=args.debounce_cycles)
    # Cycle index of a marker = rising edges at or before it, counted from 0.
    marker_cycles = [time_to_cycle(edges, t) - 1 for t in markers]
    rows = label_trials(marker_cycles, args.n_trials)

    with open(args.output, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["instruction", "bs", "trial", "start_cycle",
                                          "end_cycle", "latency_cycles"])
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {len(rows)} trials to {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
