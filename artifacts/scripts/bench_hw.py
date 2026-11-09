#!/usr/bin/env python3
"""Wall-clock comparison for the all-signals Hamming-weight query
("Hamming-Weight micro-benchmark" and "campaign trace" rows of Table II).

  WAL++       wal --fast-hw-all <vcd> -o <csv>
  Python WAL  python3 -m wal experiment-scripts/hwALL.wal -l <vcd>
              (inside an SCA-WAL checkout, https://github.com/iti-luebeck/sca-wal;
              the operator hamming-weight-all-signals-all-timestamps is an
              SCA-WAL extension and does not exist in upstream WAL)
Times include loading and parsing the waveform.

Example:
  bench_hw.py --vcd trace.vcd --sca-wal-dir ~/src/sca-wal
"""
import argparse
import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bench_ct_extraction import timed  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--vcd", required=True, type=Path)
    ap.add_argument("--wal-bin", default="wal", help="WAL++ executable (default: wal on PATH)")
    ap.add_argument("--sca-wal-dir", type=Path, help="SCA-WAL checkout (needed unless --skip-python-wal)")
    ap.add_argument("--skip-python-wal", action="store_true")
    args = ap.parse_args()

    print(f"VCD: {args.vcd} ({args.vcd.stat().st_size / 1e6:.1f} MB)", file=sys.stderr)

    fd, out = tempfile.mkstemp(suffix=".csv")
    os.close(fd)
    try:
        t_walpp, _ = timed([args.wal_bin, "--fast-hw-all", args.vcd, "-o", out])
    finally:
        os.unlink(out)
    print(f"WAL++      : {t_walpp:.3f} s")

    if args.skip_python_wal:
        return
    if args.sca_wal_dir is None:
        raise SystemExit("--sca-wal-dir is required for the Python WAL run")
    t_py, _ = timed(["python3", "-m", "wal", "experiment-scripts/hwALL.wal", "-l", args.vcd.resolve()],
                    cwd=args.sca_wal_dir)
    print(f"Python WAL : {t_py:.2f} s")
    print(f"Speedup    : {t_py / t_walpp:.0f}x")


if __name__ == "__main__":
    main()
