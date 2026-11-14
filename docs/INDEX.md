# WAL++ documentation

WAL++ analyzes digital simulation waveforms (VCD, FST, CSV) with an S-expression
language. It is a C++20 reimplementation of
[WAL](https://wal-lang.org) ([ics-jku/wal](https://github.com/ics-jku/wal)), written
for throughput on traces too large for the original to handle.

## Start here

| | |
|---|---|
| [Installation](guides/installation.md) | Building from source, and the Python module |
| [Cookbook](guides/cookbook.md) | "I want X, what do I type" — start here if you are new |
| [Getting started](guides/getting-started.md) | A first trace, a first query |
| [Troubleshooting](guides/troubleshooting.md) | What the common failures mean |

## Guides

### Security analysis

These describe the two analyses in
[Nešković et al., *Security-Driven Verification of RISC-V Crypto Extensions*](#citing),
applied to the Zkn scalar-crypto instructions of the Sailor core. Neither is specific
to that core.

| | |
|---|---|
| [Constant-time verification](guides/constant-time-verification.md) | Show that an instruction's latency does not depend on its data, with a deterministic pass/fail criterion and a bug-injection check that the criterion works |
| [Power side-channel analysis](guides/power-side-channel.md) | Derive a per-cycle activity trace and run CPA and TVLA against it, at RTL and at netlist level |

### Working at scale

| | |
|---|---|
| [Scaling to full-core traces](guides/scaling-extraction.md) | The `--fast-*` extraction modes, their measured throughput, and when to use each |
| [Working with very large traces](guides/large-traces.md) | Streaming, full load, extraction: what each keeps and how to choose |

### Building on WAL++

| | |
|---|---|
| [Embedding WAL++ in a C++ program](guides/cpp-api.md) | Link the library, run queries, add operators of your own |
| [Writing your own extraction tool](guides/writing-fast-tools.md) | Build a single-pass kernel the way `--fast-hw-*` is built, when no existing mode fits |

Both have working programs in [`examples/cpp/`](../examples/cpp/README.md), which
build against an installed WAL++ the way a consumer would.

### Driving WAL++ from an agent

The repository ships a Claude skill at
[`.claude/skills/walpp-waveform-analysis/`](https://github.com/iti-luebeck/WALpp/blob/main/.claude/skills/walpp-waveform-analysis/SKILL.md),
written from the WAL++ usage behind the paper. It carries its own `verify.sh`, which
checks every claim it makes against the binary you have and is wired into CTest, so a
change that makes the skill wrong fails the build instead of misleading an agent later.

The machine-readable CLI modes it builds on, `--inspect`, `--lint` and `--json`, are
in the [command-line reference](reference/cli.md#agent-and-scripting-modes).

### Coming from upstream WAL

| | |
|---|---|
| [Migrating from WAL](guides/migrating-from-wal.md) | Moving an existing script across |
| [Compatibility with upstream WAL](reference/wal-compatibility.md) | What matches, what differs on purpose, and the flags that switch the differences off |

## Reference

| | |
|---|---|
| [Language reference](reference/language.md) | Values, operators, signal access, the standard library |
| [Command-line reference](reference/cli.md) | Every flag |
| [Python API](reference/python-api.md) | Driving WAL++ from Python |
| [API quick reference](API_QUICK_REFERENCE.md) | C++ entry points, with code samples |

## Generated API documentation

[**API reference**](../api/index.md) is generated from the headers by
[doxide](https://doxide.org) and covers the C++ internals:

| | |
|---|---|
| `wal` | The namespace index, everything below in one place |
| `Value` | The tagged union every expression evaluates to |
| `Symbol`, `WList` | The reader's output types |
| `Closure`, `Macro` | User-defined functions and macros |
| `Environment` | Scope chain and variable lookup |
| `UserOperator` | The interface an operator of your own implements |
| `fast_vcd` | The extraction reader, which bypasses the interpreter |

Each of those is a page under the API tab.

For the entry points you actually call from a program, with code, start at the
[C++ API quick reference](API_QUICK_REFERENCE.md) and
[Embedding WAL++ in a C++ program](guides/cpp-api.md).

## Two things worth knowing up front

**There are two engines in this binary.** The S-expression interpreter answers
arbitrary questions. The `--fast-*` extraction modes answer four shaped questions
with no interpreter involved, which is where the published throughput figures come
from. Neither one's behavior affects the other.

**Types do not coerce, and integers are 64-bit.** Both differ from upstream, both for
reasons specific to waveform data, and both can be switched back with
`--compat-types` and `--bigint`. See
[Compatibility with upstream WAL](reference/wal-compatibility.md).

## Citing

If WAL++ contributes to published work, please cite the paper in
[`CITATION.bib`](../CITATION.bib), and consider citing upstream WAL as well: the
language, its operator library and its standard library are theirs.
