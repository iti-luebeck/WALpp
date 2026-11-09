#!/usr/bin/env python3
"""Zero-variance constant-time check over extracted latencies.

For every (instruction, bs) group the verdict is PASS if all trials have the
same latency and FAIL otherwise. Because RTL and gate-level simulation are
deterministic, any variation is a real data dependency. A PASS only means
that no variation was found in the tested cases; it is not a formal proof.

Input: CSV from extract_latency.py. Exit code 0 if all groups pass, else 1.

Example:
  check_constant_time.py --csv latency.csv --label baseline --out-csv summary.csv
"""
import argparse
import csv
import sys
from collections import defaultdict
from pathlib import Path


def load_latencies(path):
    groups = defaultdict(list)
    with open(path) as f:
        for row in csv.DictReader(f):
            groups[(row["instruction"], int(row["bs"]))].append(int(row["latency_cycles"]))
    return groups


def check(groups, label=""):
    rows = []
    all_pass = True
    for instr, bs in sorted(groups):
        vals = groups[(instr, bs)]
        n = len(vals)
        lo, hi = min(vals), max(vals)
        mean = sum(vals) / n
        variance = sum((v - mean) ** 2 for v in vals) / n
        verdict = "PASS" if lo == hi else "FAIL"
        all_pass &= verdict == "PASS"
        rows.append({"label": label, "instruction": instr, "bs": bs, "n": n,
                     "min": lo, "max": hi, "variance": variance, "verdict": verdict})
    return rows, all_pass


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--csv", required=True, type=Path)
    ap.add_argument("--label", default="")
    ap.add_argument("--out-csv", type=Path, help="write the summary table as CSV")
    args = ap.parse_args()

    rows, all_pass = check(load_latencies(args.csv), args.label)
    hdr = f"{'label':<10} {'instruction':<11} {'bs':<3} {'n':<6} {'min':<6} {'max':<6} {'variance':<10} verdict"
    print(hdr)
    print("-" * len(hdr))
    for r in rows:
        print(f"{r['label']:<10} {r['instruction']:<11} {r['bs']:<3} {r['n']:<6} {r['min']:<6} "
              f"{r['max']:<6} {r['variance']:<10.3f} {r['verdict']}")

    if args.out_csv:
        with open(args.out_csv, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=["label", "instruction", "bs", "n", "min", "max",
                                              "variance", "verdict"])
            w.writeheader()
            w.writerows(rows)

    print(f"\nOverall: {'ALL PASS' if all_pass else 'AT LEAST ONE FAIL'}")
    sys.exit(0 if all_pass else 1)


if __name__ == "__main__":
    main()
