#!/usr/bin/env python3
"""Run upstream WAL's own test suite against WAL++ and report the parity gap.

The test files in `tests/original/` are upstream ics-jku/wal's suite, vendored
verbatim (`counter.vcd` there is byte-identical to upstream's). They were never run:
they are not registered with CTest, and `tests/original/wal/` is not a shim onto WAL++
but a ~690-line Python interpreter of its own, so passing them proved nothing about
WAL++.

This runner executes the same test files against the real engine, by pre-importing the
adapter in `tests/conformance/wal/` so upstream's `from wal.core import Wal` resolves
to WAL++'s extension module.

Failures are the point. Each one names a feature WAL++ does not yet match, so the
summary doubles as the parity to-do list.

Usage:
    python3 tests/conformance/run_conformance.py [-v] [--file test_eval.py]

Exit status is 0 unless --strict is given, because failures are expected while parity
work is in progress. With --strict, any failure fails the run.
"""
import argparse
import importlib.util
import os
import sys
import unittest
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
ORIGINAL = REPO / "tests" / "original"

# Upstream files that assert on upstream's *internal* representation rather than on
# observable behavior. WAL++ has a different parser and no Python-visible AST, so these
# can never pass and are not parity gaps.
EXCLUDED = {
    "test_parser.py": "asserts on upstream's lark parse tree",
    "test_varresolve.py": "asserts on upstream's variable-resolution AST pass",
}


def load_module(path):
    """Import an upstream test file as a member of the `original` package.

    Two of them do `from .test_eval import OpTest`, so they must be imported as
    package members rather than standalone files.
    """
    sys.path.insert(0, str(REPO / "tests"))
    return importlib.import_module(f"original.{path.stem}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("--file", help="run only this upstream test file")
    parser.add_argument("--strict", action="store_true",
                        help="exit non-zero if any upstream test fails")
    parser.add_argument("--min-pass", type=int, default=None,
                        help="ratchet: exit non-zero if fewer than this many upstream "
                             "tests pass. Raise it as parity work lands, so the number "
                             "can only go up.")
    args = parser.parse_args()

    # The adapter must win over tests/original/wal/, so put it first and import it now.
    sys.path.insert(0, str(HERE))
    try:
        import wal.core  # noqa: F401
    except ImportError as exc:
        print(f"SKIP: {exc}")
        return 0

    # Upstream's tests use paths relative to the repo root (tests/traces/...).
    os.chdir(REPO)
    traces = REPO / "tests" / "traces"
    if not traces.exists():
        original_traces = ORIGINAL / "traces"
        if original_traces.exists():
            try:
                traces.symlink_to(original_traces, target_is_directory=True)
            except OSError:
                print(f"note: upstream tests expect {traces}; create it to run them")

    if args.file:
        files = [ORIGINAL / args.file]
    else:
        files = sorted(ORIGINAL.glob("test_*.py"))

    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    skipped = []
    for path in files:
        if path.name in EXCLUDED:
            skipped.append((path.name, EXCLUDED[path.name]))
            continue
        try:
            suite.addTests(loader.loadTestsFromModule(load_module(path)))
        except Exception as exc:  # noqa: BLE001
            skipped.append((path.name, f"could not load: {exc}"))

    runner = unittest.TextTestRunner(verbosity=2 if args.verbose else 1,
                                     stream=sys.stdout)
    result = runner.run(suite)

    total = result.testsRun
    failed = len(result.failures) + len(result.errors)
    print()
    print("=" * 72)
    print(f"Upstream conformance: {total - failed}/{total} upstream tests pass "
          f"against WAL++")
    print("=" * 72)

    for name, why in skipped:
        print(f"  excluded  {name}: {why}")

    # Group failures by their first line, so the output is a to-do list rather than a
    # wall of tracebacks.
    reasons = Counter()
    for _test, trace in list(result.failures) + list(result.errors):
        line = next((ln.strip() for ln in reversed(trace.strip().splitlines())
                     if ln.strip()), "unknown")
        reasons[line[:110]] += 1

    if reasons:
        print(f"\n  {failed} failing, grouped by cause (the parity to-do list):\n")
        for reason, count in reasons.most_common():
            print(f"  {count:4d}x  {reason}")

    passed = total - failed
    if args.min_pass is not None and passed < args.min_pass:
        print(f"\nRATCHET FAILED: {passed} passing, expected at least "
              f"{args.min_pass}. Parity regressed.")
        return 1
    if args.min_pass is not None and passed > args.min_pass:
        print(f"\nRatchet can be raised: {passed} now pass (baseline "
              f"{args.min_pass}). Update it in tests/CMakeLists.txt.")
    return 1 if (args.strict and failed) else 0


if __name__ == "__main__":
    sys.exit(main())
