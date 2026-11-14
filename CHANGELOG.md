# Changelog

All notable changes to WAL++ are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- **The language reference covers the WAL manual's chapters.** Groups and scopes,
  waveform handling, signal access and bit slicing, arrays as accumulators,
  printing, type conversions, and running other files were all absent. Each new
  section has runnable examples and three of them have animated diagrams, and the
  documentation snippet count went from 19 to 35, all of them runnable.
- **`fabric.vcd` in the playground**, a scope with three ready/valid interfaces
  sharing a naming convention, so `groups` has something to find. The groups
  examples were meaningless against a trace with a single interface.
- **Five playground examples** under "One analysis, every interface": groups,
  scopes, bit fields, `timeframe`, and building a histogram with `geta/default`.

### Fixed

- **`-e` and `print` rendered an array two different ways** once `print` was
  aligned with upstream in 1.6.4. Both now give `{("x" 10)}`, which is what
  upstream prints and what the manual shows.
- **`groups` returned symbols where upstream returns strings**, so
  `(string? (first (groups "valid")))` disagreed and a prefix could not be used in
  string work under the strict `+`.
- The stale `__version__` fallback in `src/walpp/__init__.py`, which said 1.2.2.

## [1.6.4] - 2026-11-14

Checked against the [WAL Programmer Manual](https://wal-lang.org/documentation/core)
form by form, with every expectation taken from upstream `wal-lang` 0.9.1.
`test_wal_manual_forms` in `tests/test_wal_executable.cpp` now holds each one.

### Breaking

- **A trace loaded with `-l` is `t0`**, then `t1`, which is the scheme the manual
  documents and the one an upstream script names. It was `DEFAULT`, so `t0^tb.clk`
  did not resolve. A script that spelled out `DEFAULT^...` or passed `"DEFAULT"` to
  `dump-trace` or `trim-trace` has to use `t0`. The C++ and Python `load()` default
  is unchanged.
- **`print` renders as upstream renders.** Arguments run together with no separator,
  a top-level string is bare where a nested one is quoted, symbols are bare, arrays
  are `{("x" 10)}`, booleans are `true`/`false` rather than `#t`/`#f`, and a float
  carries the shortest decimal that reads back as the same value, keeping `.0` when
  whole. `#t` and `#f` are unchanged as input.

### Fixed

- **`case` took `default` positionally.** Written before a matching clause it
  swallowed every clause after it and returned that branch with no error:
  `(case 2 [default 99] [2 2])` gave 99 instead of 2. `default` is now a fallback,
  checked once every explicit label has failed. The upstream assertion covering
  this had gone missing from the vendored `tests/original/test_eval.py`. It is back,
  and that directory is byte-identical to upstream again.
- **`let` did not bind in order**, so `(let ([x 10] [y x]) (+ x y))` raised instead
  of giving 20.
- **`slice` could not slice a number.** `(slice 12 3 2)` is the bit field 3..2, and
  `(slice 12 2)` is bit 2. Strings now index and slice as lists do, and a two-argument
  list slice returns the element rather than the tail.
- **`in` rejected an array**, which made `geta/default` unusable even though it ships
  in upstream's own `lib/std/std.wal`.
- **`+` refused to concatenate strings**, which broke `ALL-INDICES`, `trace-index`
  and `timeframe` in that same stdlib. All-string `+` now joins. A string mixed with
  a number is still an error, which is the case the strictness exists for.
- **`expr@off` disagreed with `reval`** although the manual defines the first as the
  second. An offset past either end of the trace now reads as `#f` instead of
  aborting the run, so `(whenever (!= sig sig@-1) ...)` works at index 0.
- **A trace id had to be a string.** The manual types every one as `symbol?`, so
  `(load "t.vcd" 'tr)`, `(unload 'tr)` and `(step 'tr 1)` now work.
- **`step` took its arguments in the opposite order** to the manual's `(step id
  amount)`. Both orders work now, since a count is an integer and an id never is.
- **`eval-file` did not add `.wal`**, so the documented `(eval-file "module")` form
  failed.
- **A script echoed the value of its last top-level form**, putting a stray line on
  the stdout of anything whose job was to write a report or a CSV. An interactive
  prompt still echoes.
- **`true` and `false` were unbound.** Upstream binds them as ordinary globals.
- **`(** 2 10)` returned 1024.0.** Integer operands with a non-negative exponent
  give an integer.
- **`loaded-traces` came back in `unordered_map` order**, so it could differ between
  runs.

## [1.6.3] - 2026-11-14

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
