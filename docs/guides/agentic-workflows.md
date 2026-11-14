# Driving WAL++ from an agent

Wiring WAL++ into a program, a pipeline, or an assistant that writes its own
queries. Every command and every payload on this page came out of a real run.

A person reading a terminal can tell a failure from an empty answer. A program
cannot, unless the output says so in a fixed place. That is the whole reason these
modes exist.

![The loop: inspect the trace, write a query against the names it reports, lint it,
run it, read the envelope. A wrong name caught at inspect costs nothing, the same
wrong name caught after the run costs a plausible number nobody
questions.](../images/agent-loop.svg)

## Start by asking what is in the trace

The first failure in an automated analysis is almost never a crash. It is a signal
name that was guessed, resolved to nothing, and produced a number that looked
reasonable.

```bash
wal inspect trace.vcd
```

```json
{
  "envelope_version": 1,
  "tool": "walpp",
  "tool_version": "1.6.3",
  "command": "inspect",
  "generated_at": "2026-11-14T13:49:54Z",
  "source": {"path": "counter.vcd","modified_at": "2026-11-13T23:06:12Z","age_seconds": 53021},
  "ok": true,
  "warnings": [],
  "data": {
    "max_index": 80,
    "index_count": 81,
    "signal_count": 7,
    "scope_count": 2,
    "hierarchy_separator": ".",
    "trace_id_separator": "^",
    "scopes": ["tb", "tb.dut"],
    "signals": [
      {"name": "tb.overflow", "leaf": "overflow", "scope": "tb", "width": 1},
      {"name": "tb.clk", "leaf": "clk", "scope": "tb", "width": 1}
    ]
  }
}
```

`name` is the exact spelling the engine accepts. Use it verbatim. Do not rebuild it
from `scope` and `leaf`, and do not assume the separator: `hierarchy_separator` and
`trace_id_separator` are reported because they are the two characters a generated
query gets wrong.

`max_index` is the last valid index, so a trace with `index_count` 81 runs 0 to 80.

It reads only the header where the format allows, so it stays cheap on a file too
large to load.

### A plain list, when JSON is more than you need

```bash
wal inspect trace.vcd --format=names 2>/dev/null
```

```
# timescale 1s
1 tb.overflow
1 tb.clk
1 tb.reset
```

Each line is `width name`. Comments start with `#`. The progress line goes to stderr,
which is what the redirect is for. This is the format `--signals` reads, so it feeds
straight back into an extraction run.

## The envelope

Every JSON answer has the same outer shape, whichever mode produced it. It is the
envelope the ITI `ekumen` tool uses, so an assistant that has learned one can read
the other.

| Field | |
|---|---|
| `envelope_version` | Bumped only when this shape changes, never for payload additions |
| `tool`, `tool_version` | `"walpp"` and the binary's version |
| `command` | Which mode produced it: `inspect`, `lint`, `eval` |
| `generated_at` | When the answer was produced, UTC |
| `source` | The file it came from, with `modified_at` and `age_seconds` |
| `ok` | Always present. The only field worth branching on |
| `warnings` | Reserved. See below |
| `data` | The payload, or `null` on failure |
| `error` | Present only when `ok` is false |

Two properties of this are safety properties rather than conveniences.

**Failure has the same shape as success.** A consumer that has to tell "the command
failed" from "the command returned nothing" by reading prose will get it wrong
eventually, and in the direction that matters. `ok` is always there.

**Age is materialised.** A trace is a file, and an automated run can easily be reading
one produced before the change it is meant to evaluate. Every payload carries the
source's modification time and its age in seconds, so a stale answer cannot be skimmed
as a current one. No freshness budget is imposed, because WAL++ has no way to know
what the trace ought to be newer *than*. The facts are reported and the caller decides.

!!! note "`warnings` is reserved"

    It is emitted on every envelope and is always `[]`. Nothing populates it today.
    Parse it if you like, but do not treat an empty array as "nothing went wrong",
    because it would be empty either way.

## Failures

```bash
wal inspect /nope.vcd
```

```json
{
  "envelope_version": 1,
  "tool": "walpp",
  "command": "inspect",
  "source": {"path": "/nope.vcd"},
  "ok": false,
  "warnings": [],
  "error": {
    "code": "walpp.trace.unreadable",
    "message": "Failed to load trace: File not found: /nope.vcd",
    "subject": "/nope.vcd",
    "hint": "check the path and that the format is VCD, FST or CSV"
  },
  "data": null
}
```

`code` is the stable, machine-matchable part. Match on that, not on `message`, which
is written for a person and may be reworded. `subject` is what the failure was about,
and `hint` is what to do about it when there is something useful to say.

On failure `source` carries the path but no `modified_at` or `age_seconds`, because
there was no file to stat.

**Exit codes** agree with `ok`: 0 when it is true, 1 when it is false. A caller that
only needs to know whether something worked does not have to parse anything.

## Check a script before you run it

A mistyped operator is indistinguishable from a signal name until the moment it is
evaluated, and by then the first half of the script has already run and written
whatever it writes.

```bash
wal lint analysis.wal
```

```
analysis.wal: error: call to 'cuont', which is not an operator, a macro, or defined in this file [unknown-operator]
    check the spelling, or define it before use
1 finding(s), 1 error(s)
```

Compiler-shaped output, non-zero exit, so it drops into a Makefile or a pre-commit
hook without a wrapper. With `--json` it comes back in the envelope:

```json
"data": {
  "file": "analysis.wal",
  "error_count": 1,
  "finding_count": 1,
  "findings": [
    {
      "severity": "error",
      "code": "walpp.lint.unknown-operator",
      "line": 0,
      "column": 0,
      "message": "call to 'cuont', which is not an operator, a macro, or defined in this file",
      "hint": "check the spelling, or define it before use"
    }
  ]
}
```

Two things to know about the positions:

- A **parse** error carries a real `line` and `column`, 1-based. Unterminated list,
  stray bracket, bad string.
- An **unknown operator** reports `0, 0`. The symbol's position is not tracked that
  far, so the finding names the operator rather than pointing at it. Search for the
  name.

Position decides whether an unknown symbol is reported at all. In **head** position it
is a call to something that does not exist, so it is an error. In **argument**
position it is not reported, because that is how a signal is read and `lint` loads no
trace, so every bare name there is potentially legitimate.

```bash
wal lint head.wal    # (zzz 1)              -> error, exit 1
wal lint arg.wal     # (printf "%d\n" zzz)  -> no problems found, exit 0
```

So `lint` tells you the script will *start*. It cannot tell you the names in it exist.
That is what `inspect` is for, and why the loop runs it first.

!!! note "`severity` is always `error`"

    The field exists and `Severity::Warning` is declared, but nothing in the binary
    emits one today. Branch on `error_count`, which is the number you actually have.

Then run it:

```bash
wal run analysis.wal -l trace.vcd
```

## One-off questions

```bash
wal eval '(count (rising tb.clk))' -l trace.vcd --json
```

```json
"ok": true,
"data": {
  "results": [40]
}
```

`results` is an array because `-e` may be given more than once, in order. An
expression that fails stops the run, and the envelope says so while keeping what
already succeeded:

```json
"ok": false,
"error": {
  "code": "walpp.eval.failed",
  "message": "Eval error: Undefined variable: bogus",
  "subject": "(bogus)",
  "hint": "the expressions before this one are in data.results"
}
```

`--compact-json` is the same content on one line, for a log file or a pipe where the
indentation is noise:

```bash
wal eval '(count (rising tb.clk))' -l trace.vcd --compact-json
```

```json
{"envelope_version":1,"tool":"walpp","tool_version":"1.6.3","command":"eval","generated_at":"2026-11-14T13:51:49Z","source":{"path":"counter.vcd","modified_at":"2026-11-13T23:06:12Z","age_seconds":53136},"ok":true,"warnings":[],"data":{"results":[40]}}
```

## A whole run

```bash
#!/usr/bin/env bash
set -euo pipefail

trace=$1
script=$2

# 1. What is in it, and is it recent enough for what we are about to claim?
meta=$(wal inspect "$trace")
age=$(jq -r '.source.age_seconds' <<<"$meta")
[ "$age" -lt 86400 ] || echo "warning: trace is $age seconds old" >&2

# 2. The names, verbatim. Never rebuilt from scope and leaf.
jq -r '.data.signals[].name' <<<"$meta" > names.txt

# 3. Will it even run?
wal lint "$script"

# 4. Run it.
wal run "$script" -l "$trace"
```

Steps 1 and 3 are the two that earn their place. Step 1 stops a guessed name from
becoming a plausible number, and step 3 stops a typo on line 90 from being discovered
after lines 1 to 89 have already written their output.

## Verbs and flags are the same command

`wal inspect` and `wal --inspect` are one command. The verb is rewritten into the flag
before anything is parsed, so there is no second code path and no behavior that
differs between them.

| Verb | Flag |
|---|---|
| `wal inspect FILE` | `wal --inspect FILE` |
| `wal inspect FILE --format=names` | `wal --fast-list FILE -o /dev/stdout` |
| `wal lint FILE.wal` | `wal --lint FILE.wal` |
| `wal eval EXPR` | `wal -e EXPR` |
| `wal run FILE.wal` | `wal -f FILE.wal` |

Use whichever reads better. A word is only taken as a verb when no file of that name
exists, so `wal inspect` in a directory containing a file called `inspect` still opens
the file.

## The skill

The repository ships a Claude skill at
[`.claude/skills/walpp-waveform-analysis/`](https://github.com/iti-luebeck/WALpp/blob/main/.claude/skills/walpp-waveform-analysis/SKILL.md),
written from the WAL++ usage behind the paper. It carries the signal-naming rules, a
CLI cheat sheet, the analysis operators worth knowing, and `scripts/wal-time.py` for
edge and cycle timing without shell quoting.

It also carries its own `verify.sh`, which checks every claim it makes against the
binary you actually have, and that script is wired into CTest. A change that makes the
skill wrong fails the build rather than misleading an agent months later.

## See also

- [Command-line reference](../reference/cli.md#agent-and-scripting-modes) for every
  flag these modes accept
- [Cookbook](cookbook.md) for the same commands from a person's side
- [Embedding WAL++ in a C++ program](cpp-api.md) to skip the process boundary entirely
- [Python API](../reference/python-api.md) to drive the engine in process from Python
