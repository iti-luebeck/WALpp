# Changelog

All notable changes to WAL++ are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

## [1.6.3] - TBD

Upstream WAL compatibility, the agent-facing modes, and the documentation site.

### Breaking

- **Signal names use upstream's separators.** `.` joins a hierarchy and `^` prefixes
  a trace id, where WAL++ previously used `^` for the hierarchy. Scripts written for
  the old spelling still resolve, but `signal-names` and `inspect` now report the
  upstream form.
- **Types no longer coerce.** `(+ 1 "hi")` is an error rather than `"1hi"`, and
  `(= 0 #f)`, `(= 1 #t)` and `(= 1 1.0)` are false. A signal wider than 64 bits reads
  as a bit string, so a coercing `+` or `=` would return a plausible wrong answer on
  exactly the signals that matter. `--compat-types` restores upstream's behavior.
- **Integers are 64-bit.** A literal that does not fit is an error naming `--bigint`,
  which gives arbitrary precision at the cost of speed.
- **`append` is upstream's `append`.** WAL++'s list concatenation, which shared the
  name and did something else, is now `extend`.
- **`mod` is Euclidean**, so the result is the least non-negative residue. This agrees
  with upstream for every positive divisor.

### Added

- **Upstream compatibility.** 71 of upstream's 77 tests pass against the WAL++ engine,
  and 0 of its 106 operators are missing. The six that do not pass assert on upstream's
  internal enum or capture stdout from Python. A ratchet in `tests/CMakeLists.txt`
  keeps the number from falling. Upstream's own `lib/std/std.wal` is loaded unchanged.
- **Verbs.** `wal inspect`, `lint`, `eval`, `extract`, `run` and `repl`, as sugar
  rewritten into the existing flags before anything is parsed. A word is only a verb
  when no file of that name exists.
- **Agent modes.** `inspect` and `lint` answer in a JSON envelope with a stable `ok`
  field, a machine-matchable `error.code`, and the source's modification time and age,
  so a stale trace cannot be read as a current one. `--json` and `--compact-json` put
  `eval` in the same envelope. Shipped with a Claude skill whose `verify.sh` checks
  every claim against the binary and runs under CTest.
- **C++ package export.** `find_package(walpp)` and `walpp::walpp`, with guides for
  embedding the library and for writing extraction kernels of your own.
- **Documentation site.** Guides for each of the paper's use cases, a cookbook, a
  compatibility reference, an agent guide, and the interactive playground.

### Changed

- **Extraction streams.** Peak memory for the `--fast-*` modes went from about 2.7x
  the trace size to roughly the file itself, and no longer grows with it.
- **Separate `Wal` instances are safe to use concurrently.** Fixes a process-wide
  `require` registry, colliding gensyms and non-atomic mode flags.

### Fixed

- Zero is false. `if`, `!`, `&&` and `||` treated `0` as true.
- `-e` lost list results and ran only the last expression when given more than once.
- The streaming reader returned end-of-file values for indexed reads.
- `-fcoroutines-ts` broke every real-Clang build since Clang 17.
- A static build could not be generated: `fstapi` was missing from the export set.

## [1.2.2] - TBD

First public release.

- WAL interpreter in C++20 with the WAL standard library and 130+ operators
- VCD, FST and CSV readers and writers, streaming mode for large traces
- Fast Hamming weight / Hamming distance operators (`--fast-hw-all`, `--fast-hd-all`),
  including signals wider than 64 bits
- Python bindings (`walpp` package) and the `wal` command-line tool with REPL
- Paper evaluation workloads, scripts, results and traces in `artifacts/`
