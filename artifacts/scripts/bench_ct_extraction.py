#!/usr/bin/env python3
"""Wall-clock comparison for the constant-time extraction query.

Both tools extract every value change of the four marker signals (clock,
address, isWrite, valid) from the same constant-time probe waveform:
  WAL++       wal --fast-values-signals signals/sailor_rtl_markers.txt <vcd>
  Python WAL  python3 -m wal latency_signals.wal -l <vcd>   (run inside an
              SCA-WAL checkout, https://github.com/iti-luebeck/sca-wal)
Times include loading and parsing the waveform.

Example:
  bench_ct_extraction.py --vcd ct_probe.vcd --sca-wal-dir ~/src/sca-wal
"""
import argparse
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent


def timed(cmd, cwd=None):
    start = time.monotonic()
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    elapsed = time.monotonic() - start
    if result.returncode != 0:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise RuntimeError(f"command failed: {' '.join(map(str, cmd))}")
    return elapsed, result


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--vcd", required=True, type=Path)
    ap.add_argument("--wal-bin", default="wal", help="WAL++ executable (default: wal on PATH)")
    ap.add_argument("--sca-wal-dir", type=Path, help="SCA-WAL checkout (needed unless --skip-python-wal)")
    ap.add_argument("--signals", type=Path, default=HERE / "signals" / "sailor_rtl_markers.txt")
    ap.add_argument("--skip-python-wal", action="store_true")
    args = ap.parse_args()

    print(f"VCD: {args.vcd} ({args.vcd.stat().st_size / 1e6:.1f} MB)", file=sys.stderr)

    fd, out = tempfile.mkstemp(suffix=".csv")
    os.close(fd)
    try:
        t_walpp, _ = timed([args.wal_bin, "--fast-values-signals", args.signals, args.vcd, "-o", out])
        with open(out) as f:
            rows_walpp = sum(1 for line in f if line.strip() and not line.startswith("#"))
    finally:
        os.unlink(out)
    print(f"WAL++      : {t_walpp:.2f} s ({rows_walpp} rows)")

    if args.skip_python_wal:
        return
    if args.sca_wal_dir is None:
        raise SystemExit("--sca-wal-dir is required for the Python WAL run")
    t_py, res = timed(["python3", "-m", "wal", HERE / "latency_signals.wal", "-l", args.vcd.resolve()],
                      cwd=args.sca_wal_dir)
    rows_py = sum(1 for line in res.stdout.splitlines() if line.strip())
    print(f"Python WAL : {t_py:.2f} s ({rows_py} rows)")
    print(f"Speedup    : {t_py / t_walpp:.1f}x")
    if rows_py != rows_walpp:
        print(f"WARNING: row counts differ (WAL++ {rows_walpp}, Python WAL {rows_py})", file=sys.stderr)


if __name__ == "__main__":
    main()
