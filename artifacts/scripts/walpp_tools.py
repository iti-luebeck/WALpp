"""Shared helpers for the paper's WAL++ post-processing scripts.

All waveform access goes through the WAL++ command-line tool (`wal`):

* ``wal --fast-values-signals <siglist> <vcd> -o <csv>`` returns every value
  change of the listed signals as ``time,signal_index,value`` rows. The signal
  index is the line number in the signal-list file (0-based).
* ``wal --fast-hw-scope <scope> <vcd> -o <csv>`` (or ``--fast-hd-scope``)
  returns the Hamming weight (distance) summed over all signals below
  ``<scope>`` at every timestamp, as ``time,value`` rows.

Everything else (clock-edge counting, marker detection, per-cycle resampling)
is plain Python/numpy on top of those two outputs.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

# Address of the memory-mapped `.timestamp` register in the Sailor testbench
# memory map. The test programs write it right before and right after the
# operation under test; the write is an ordinary data-bus transaction.
TIMESTAMP_ADDR = 0x80003000

# Signal-list order expected by the marker detection (one name per line in the
# signal-list file): clock, data-bus address, data-bus isWrite, data-bus valid.
IDX_CLOCK, IDX_ADDR, IDX_ISWRITE, IDX_VALID = range(4)

SIGNALS_DIR = Path(__file__).resolve().parent / "signals"


def run_wal(wal_bin, args):
    """Run `wal <args> -o <tmp.csv>` and return the CSV rows as lists of ints.

    Lines that are empty or start with '#' are skipped.
    """
    fd, out_name = tempfile.mkstemp(suffix=".csv")
    os.close(fd)
    out_path = Path(out_name)
    try:
        cmd = [str(wal_bin)] + [str(a) for a in args] + ["-o", str(out_path)]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(result.stdout, file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            raise RuntimeError(f"wal {' '.join(str(a) for a in args)} failed")
        rows = []
        with open(out_path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                rows.append([int(x) for x in line.split(",")])
        return rows
    finally:
        out_path.unlink(missing_ok=True)


def bus_events(wal_bin, vcd, signals_file):
    """Value changes of the four marker signals, grouped by timestamp.

    Returns {time: [(signal_index, value), ...]} with the per-timestamp order
    as emitted by WAL++.
    """
    events_by_time = {}
    for t, idx, val in run_wal(wal_bin, ["--fast-values-signals", signals_file, vcd]):
        events_by_time.setdefault(t, []).append((idx, val))
    return events_by_time


def cycle_edges_and_markers(events_by_time, marker_addr=TIMESTAMP_ADDR, debounce_cycles=0):
    """Replay the bus value-change stream.

    Returns (cycle_edges, marker_times):
      cycle_edges[k]  time of the k-th rising clock edge
      marker_times    times at which (address == marker_addr & isWrite & valid)
                      becomes true

    With debounce_cycles > 0, a marker detected within that many clock periods
    after the previous one is dropped. On SDF-annotated gate-level traces the
    address/isWrite/valid nets do not switch atomically, so one store can show
    up as two detections a few cycles apart; real markers are thousands of
    cycles apart. RTL traces need no debounce (debounce_cycles=0).
    """
    state = {IDX_CLOCK: 0, IDX_ADDR: -1, IDX_ISWRITE: 0, IDX_VALID: 0}
    match_prev = False
    cycle_edges = []
    raw_markers = []
    clock_period = None

    for t in sorted(events_by_time):
        rising = False
        for idx, val in events_by_time[t]:
            if idx == IDX_CLOCK and state[IDX_CLOCK] == 0 and val == 1:
                rising = True
            state[idx] = val
        if rising:
            if cycle_edges and clock_period is None:
                clock_period = t - cycle_edges[-1]
            cycle_edges.append(t)
        match_now = (state[IDX_ADDR] == marker_addr and state[IDX_ISWRITE] == 1
                     and state[IDX_VALID] == 1)
        if match_now and not match_prev:
            raw_markers.append(t)
        match_prev = match_now

    debounce = debounce_cycles * (clock_period or 1)
    markers = []
    for t in raw_markers:
        if markers and (t - markers[-1]) <= debounce:
            continue
        markers.append(t)
    return np.array(cycle_edges), markers


def time_to_cycle(cycle_edges, t):
    """Number of rising clock edges at or before time t."""
    return int(np.searchsorted(cycle_edges, t, side="right"))


def activity_series(wal_bin, vcd, scope, use_hd=False):
    """Hamming-weight (or -distance) time series over all signals below scope."""
    flag = "--fast-hd-scope" if use_hd else "--fast-hw-scope"
    rows = run_wal(wal_bin, [flag, scope, vcd])
    t = np.array([r[0] for r in rows])
    v = np.array([r[1] for r in rows])
    return t, v


def resample_per_cycle(cycle_edges, series_t, series_v, method):
    """Reduce the activity series to one value per clock cycle.

    method="last":       value just before the next rising edge (the settled
                         value). Used for zero-delay RTL traces, where a cycle
                         has a single meaningful value.
    method="accumulate": sum of all samples within the cycle. Used for
                         SDF-annotated netlist traces, so intra-cycle switching
                         (including glitches) contributes to the trace.
    Result length is len(cycle_edges) - 1.
    """
    boundaries = np.searchsorted(series_t, cycle_edges, side="left")
    if method == "last":
        idx = np.clip(boundaries[1:] - 1, 0, len(series_v) - 1)
        return series_v[idx]
    if method == "accumulate":
        cumsum = np.concatenate(([0], np.cumsum(series_v, dtype=np.int64)))
        sums = cumsum[boundaries[1:]] - cumsum[boundaries[:-1]]
        # A cycle without any sample carries the last known value forward.
        empty = boundaries[1:] == boundaries[:-1]
        if empty.any():
            last_idx = np.clip(boundaries[:-1] - 1, 0, len(series_v) - 1)
            sums = np.where(empty, series_v[last_idx], sums)
        return sums
    raise ValueError(f"unknown resample method: {method}")
