#!/usr/bin/env python3
"""Finds the documentation's WAL snippets that actually run, for the playground buttons.

Every ```scheme block in the docs is evaluated with the wal CLI, first without a
trace and then against each playground trace. The ones that come back without an
error are written to docs/playground/runnable.json, and docs/javascripts/playground.js
puts a "Run in playground" button on exactly those.

The point is that a button which opens the playground only to show an error is worse
than no button. A good share of the snippets do not evaluate: they use operators WAL++ does not
implement, or they are abstract templates with placeholders.
Those are listed in the report this prints, which is worth reading now and then.

Usage: tools/mark-runnable-snippets.py [path/to/wal]
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
# Every Markdown page the site builds from, discovered rather than listed. A fixed
# list went stale the moment the README was split into docs/guides and
# docs/reference: the three files named here still existed, so the tool reported
# 0/0 runnable and wrote an empty catalogue instead of failing.
DOC_ROOTS = ["README.md", "docs"]
SKIP_DIRS = {"playground", "images", "stylesheets", "javascripts", "overrides"}


def find_docs(root):
    """Markdown pages that can hold snippets, in a stable order."""
    found = []
    for entry in DOC_ROOTS:
        path = os.path.join(root, entry)
        if os.path.isfile(path):
            found.append(entry)
            continue
        for dirpath, dirnames, filenames in os.walk(path):
            dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
            for name in sorted(filenames):
                if name.endswith(".md"):
                    found.append(os.path.relpath(os.path.join(dirpath, name), root))
    return found
TRACE_DIR = os.path.join("docs", "playground", "traces")
OUT = os.path.join(ROOT, "docs", "playground", "runnable.json")


def split_forms(source):
    """Splits WAL source into top level forms. Mirrors splitForms() in playground.js."""
    forms, depth, start, in_string, in_comment = [], 0, -1, False, False
    i = 0
    while i < len(source):
        c = source[i]
        if in_comment:
            if c == "\n":
                in_comment = False
            i += 1
            continue
        if in_string:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_string = False
            i += 1
            continue
        if c == ";":
            in_comment = True
            i += 1
            continue
        if c == '"':
            if start < 0:
                start = i
            in_string = True
            i += 1
            continue
        if c == "(":
            if depth == 0:
                start = i
            depth += 1
            i += 1
            continue
        if c == ")":
            depth -= 1
            if depth == 0 and start >= 0:
                forms.append(source[start:i + 1])
                start = -1
            i += 1
            continue
        if depth == 0 and start < 0 and not c.isspace():
            start = i
        if depth == 0 and start >= 0 and c.isspace():
            forms.append(source[start:i])
            start = -1
        i += 1
    if start >= 0 and depth == 0:
        forms.append(source[start:])
    return [f for f in forms if f.strip()]


def normalise(code):
    """The key both sides agree on: trailing space off each line, no blank edges."""
    return "\n".join(line.rstrip() for line in code.strip().splitlines())


def evaluate(wal, forms, trace):
    args = [wal] + (["-l", trace] if trace else [])
    for form in forms:
        args += ["-e", form]
    try:
        done = subprocess.run(args, capture_output=True, text=True, timeout=120)
    except Exception as exc:  # a hang or a missing binary is a failure like any other
        return [f"could not run wal: {exc}"]
    output = done.stdout + done.stderr
    return [l for l in output.splitlines() if "Error" in l or "error:" in l]


def main():
    wal = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "wal")
    if not os.path.exists(wal):
        print(f"error: no wal binary at {wal}; build it first or pass the path", file=sys.stderr)
        return 2

    traces = [None]
    trace_path = os.path.join(ROOT, TRACE_DIR)
    if os.path.isdir(trace_path):
        traces += [os.path.join(TRACE_DIR, f)
                   for f in sorted(os.listdir(trace_path)) if f.endswith(".vcd")]

    runnable, rejected = [], []
    for doc in find_docs(ROOT):
        path = os.path.join(ROOT, doc)
        if not os.path.exists(path):
            continue
        for match in re.finditer(r"^```scheme\n(.*?)^```", open(path).read(), re.S | re.M):
            code = normalise(match.group(1))
            forms = split_forms(code)
            if not forms:
                continue
            reason = None
            for trace in traces:
                errors = evaluate(wal, forms, os.path.join(ROOT, trace) if trace else None)
                if not errors:
                    runnable.append({
                        "code": code,
                        "trace": os.path.basename(trace) if trace else None,
                    })
                    break
                reason = reason or errors[0]
            else:
                rejected.append((doc, code.splitlines()[0][:52], reason))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as handle:
        json.dump({"snippets": runnable}, handle, indent=1)

    total = len(runnable) + len(rejected)
    print(f"{len(runnable)}/{total} documentation snippets are runnable; wrote {OUT}")
    if rejected:
        print("\nno button on these, they do not evaluate:")
        for doc, first, reason in rejected:
            print(f"  {doc}: {first:<54} {(reason or '').strip()[:60]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
