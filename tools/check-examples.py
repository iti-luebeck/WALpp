#!/usr/bin/env python3
"""Runs every playground example through the wal CLI and reports failures.

The playground is documentation: an example that errors in the browser is a broken
doc page. This checks them against the same engine the wasm build is compiled from,
so breakage shows up in CI rather than on the published site.

Usage: tools/check-examples.py [path/to/wal]
"""
import json
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
CATALOGUE = os.path.join(ROOT, "docs", "playground", "examples.json")
TRACES = os.path.join(ROOT, "docs", "playground", "traces")


def run(wal, trace, code_lines):
    """Evaluates the forms in one session, exactly as the playground does."""
    args = [wal]
    if trace:
        args += ["-l", os.path.join(TRACES, trace)]
    for line in code_lines:
        args += ["-e", line]
    done = subprocess.run(args, capture_output=True, text=True, timeout=120)
    return done.stdout + done.stderr


def main():
    wal = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "wal")
    if not os.path.exists(wal):
        print(f"error: no wal binary at {wal}; build it first or pass the path", file=sys.stderr)
        return 2

    catalogue = json.load(open(CATALOGUE))
    failures = []
    total = 0
    for group in catalogue["groups"]:
        print(f"\n{group['title']}")
        for example in group["examples"]:
            total += 1
            output = run(wal, example.get("trace"), example["code"])
            bad = [l for l in output.splitlines() if "Error" in l or "error:" in l]
            if bad:
                failures.append((example["id"], bad[0]))
                print(f"  FAIL  {example['id']}: {bad[0][:100]}")
            else:
                first = next((l for l in output.splitlines() if l.strip()), "")
                print(f"  ok    {example['id']}: {first[:80]}")

    print(f"\n{total - len(failures)}/{total} examples pass")
    if failures:
        print("\nfailing examples:")
        for name, message in failures:
            print(f"  {name}: {message}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
