#!/usr/bin/env python3
"""wal-time.py - cycle/edge timing from a trace, driven by the WAL++ `wal` binary.

Positional arguments only (no S-expressions, no shell quoting). A signal is named
by any unique **substring** of its full hierarchical name; timing is reported in
nanoseconds. All trace I/O is done by `wal`, through its fast paths:

    wal --fast-list            -> signal names, widths, timescale
    wal --fast-values-signals  -> raw value-change rows (1-bit signals AND buses)

so this reads every format wal reads (VCD / FST / CSV); the Python layer only does
the timing arithmetic.

Subcommands (signal = any unique substring; ambiguous -> candidates are listed):

  wal-time.py <trace> signals [pattern]        list signal names (optionally filtered)
  wal-time.py <trace> edges  <signal>          value-change times: "<t_ns> <value>"
  wal-time.py <trace> pulses <signal>          high pulses of a 1-bit signal
  wal-time.py <trace> at     <signal> <t_ns>   value of <signal> at time <t_ns>
  wal-time.py <trace> find   <signal> <value>  times where <signal> == <value>
  wal-time.py <trace> latency <sigA> <valA> <sigB> <valB>
                                               ns from first A==valA to next B==valB

Values: 1-bit prints 0/1, buses print 0x-hex; <value> arguments accept 12 or 0xC.
Times are ns, derived from the trace timescale.

Read-only. Finding the `wal` binary: $WAL, else `wal` on PATH, else ./build/wal
relative to the repository root.

Limits inherited from wal's fast values path: x/z bits are not represented (defined
values only), and vectors wider than 64 bits are truncated on this path. Hamming
weight/distance handle >64-bit signals exactly; only the value export truncates.
"""
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def find_wal():
    """Locate the wal binary: $WAL, then PATH, then the in-repo build directory."""
    env = os.environ.get("WAL")
    if env:
        return env
    found = shutil.which("wal")
    if found:
        return found
    # scripts/ -> skill dir -> skills -> .claude -> repo root
    repo_build = Path(__file__).resolve().parents[4] / "build" / "wal"
    if repo_build.is_file() and os.access(repo_build, os.X_OK):
        return str(repo_build)
    sys.exit(
        "cannot find the `wal` binary. Set $WAL to its path, put it on PATH, "
        "or build it in this repository (cmake --build build)."
    )


WAL = find_wal()


def _timescale_ns(text):
    text = text.strip()
    num = "".join(c for c in text if (c.isdigit() or c == ".")) or "1"
    unit = "".join(c for c in text if c.isalpha()).lower()
    scale = {"s": 1e9, "ms": 1e6, "us": 1e3, "ns": 1.0, "ps": 1e-3, "fs": 1e-6}.get(unit, 1.0)
    return float(num) * scale


def _run(args):
    out = tempfile.NamedTemporaryFile(mode="r", suffix=".csv", delete=False)
    out.close()
    try:
        r = subprocess.run([WAL] + args + ["-o", out.name],
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        if r.returncode != 0:
            sys.exit(f"wal failed: {r.stderr.strip()}")
        with open(out.name) as f:
            return f.read().splitlines()
    finally:
        os.unlink(out.name)


def list_signals(trace):
    """Return (ns_per_tick, [(width, name)])."""
    ns = 1.0
    sigs = []
    for line in _run(["--fast-list", trace]):
        if line.startswith("# timescale"):
            ns = _timescale_ns(line.split()[-1])
        elif line:
            w, _, name = line.partition(" ")
            sigs.append((int(w), name))
    if not sigs:
        sys.exit(f"no signals (is '{trace}' a trace wal can read?)")
    return ns, sigs


def resolve(sigs, query):
    """Return (width, name) for the unique signal whose full name contains query."""
    q = query.lower()
    exact = [(w, n) for w, n in sigs if n.lower() == q]
    if exact:
        return exact[0]
    hits = [(w, n) for w, n in sigs if q in n.lower()]
    if not hits:
        sys.exit(f"no signal matches '{query}'")
    if len(hits) > 1:
        names = "\n".join(f"  {n}" for _, n in sorted(hits, key=lambda x: x[1]))
        sys.exit(f"'{query}' is ambiguous ({len(hits)} matches):\n{names}")
    return hits[0]


def values(trace, names):
    """Run wal's value export for `names`; return (ns_per_tick, rows[(ticks, idx, val)])."""
    lf = tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False)
    lf.write("\n".join(names) + "\n")
    lf.close()
    try:
        out = _run(["--fast-values-signals", lf.name, trace])
    finally:
        os.unlink(lf.name)
    ns = 1.0
    rows = []
    for line in out:
        if line.startswith("# timescale"):
            ns = _timescale_ns(line.split()[-1])
        elif line:
            t, idx, val = line.split(",")
            rows.append((int(t), int(idx), int(val)))
    rows.sort()  # stable by timestamp
    return ns, rows


def fmt_val(v, width):
    return str(v) if width == 1 else hex(v & ((1 << width) - 1))


def parse_val(text, width):
    n = int(text, 16) if text.lower().startswith("0x") else int(text)
    return n & ((1 << width) - 1) if width > 1 else (1 if n else 0)


def main():
    a = sys.argv[1:]
    if len(a) < 2 or a[0] in ("-h", "--help"):
        sys.exit(__doc__)
    trace, cmd = a[0], a[1]
    npt, sigs = list_signals(trace)

    if cmd == "signals":
        pat = a[2].lower() if len(a) > 2 else None
        for _, n in sorted(sigs, key=lambda x: x[1]):
            if pat is None or pat in n.lower():
                print(n)
        return

    def ns(t):
        v = t * npt
        return int(v) if abs(v - round(v)) < 1e-6 else round(v, 3)

    if cmd in ("edges", "pulses", "at", "find"):
        if len(a) < 3:
            sys.exit(f"{cmd} needs a signal argument")
        w, name = resolve(sigs, a[2])
        _, rows = values(trace, [name])
        series = [(t, v) for (t, i, v) in rows if i == 0]

        if cmd == "edges":
            print(f"# {name} (width {w}); times in ns", file=sys.stderr)
            last = None
            for t, v in series:
                if v != last:
                    print(f"{ns(t)} {fmt_val(v, w)}")
                    last = v
            return
        if cmd == "pulses":
            if w != 1:
                sys.exit("pulses expects a 1-bit signal")
            print(f"# {name} high pulses; times in ns", file=sys.stderr)
            rise = None
            for t, v in series:
                if v and rise is None:
                    rise = t
                elif not v and rise is not None:
                    print(f"rise={ns(rise)} fall={ns(t)} high={ns(t - rise)}")
                    rise = None
            if rise is not None:
                print(f"rise={ns(rise)} fall=<still high at end of trace>")
            return
        if cmd == "at":
            if len(a) < 4:
                sys.exit("at needs a time in ns")
            target = float(a[3])
            cur = None
            for t, v in series:
                if ns(t) > target:
                    break
                cur = v
            print(fmt_val(cur, w) if cur is not None else "(no value before t)")
            return
        if cmd == "find":
            if len(a) < 4:
                sys.exit("find needs a value")
            want = parse_val(a[3], w)
            for t, v in series:
                if v == want:
                    print(ns(t))
            return

    if cmd == "latency":
        if len(a) < 6:
            sys.exit("latency needs <sigA> <valA> <sigB> <valB>")
        wa, na = resolve(sigs, a[2])
        wb, nb = resolve(sigs, a[4])
        names = [na] if na == nb else [na, nb]
        _, rows = values(trace, names)
        ia, ib = 0, (0 if na == nb else 1)
        va, vb = parse_val(a[3], wa), parse_val(a[5], wb)
        t_a = None
        for t, i, v in rows:
            if i == ia and v == va and t_a is None:
                t_a = t
            elif i == ib and v == vb and t_a is not None and t >= t_a:
                print(f"{na}=={a[3]} @ {ns(t_a)} ns -> {nb}=={a[5]} @ {ns(t)} ns  => {ns(t - t_a)} ns")
                return
        sys.exit("latency: event pair not found")

    sys.exit(__doc__)


if __name__ == "__main__":
    main()
