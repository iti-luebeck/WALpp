# Contributing to WAL++

Thanks for your interest. Bug reports, questions and pull requests are welcome.

## Reporting a bug

Open an issue at <https://github.com/iti-luebeck/WALpp/issues> with:

- the WAL++ commit or version and your OS and compiler,
- the command or WAL script you ran,
- a small trace that shows the problem, if you can share one,
- what you expected and what happened.

If WAL++ gives a different result than the original
[WAL](https://github.com/ics-jku/wal) for the same script and trace, say so;
compatibility with WAL is a goal.

## Building and testing

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Requirements: a C++20 compiler (GCC 10+, Clang 11+),
CMake 3.20+, zlib. libedit is optional and gives the REPL line editing and
history (macOS: built in; Debian/Ubuntu: `libedit-dev`; Fedora/RHEL: `libedit-devel`).
WAL++ deliberately does not use GNU Readline, which is GPL. For the Python
module add `-DBUILD_PYTHON_MODULE=ON` and install `pybind11`, or build the wheel
with `python -m build --wheel`.

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Keep a pull request to one topic. Add or update tests in `tests/` for
   behaviour you change.
3. Make sure `ctest` passes; the CI runs it on Linux and macOS.
4. Write commit messages that say what changed and why. We use short
   prefixes where they help, e.g. `fix(fast): ...`, `perf(vcd): ...`.

Files taken from WAL (`lib/std/`, `tests/examples/`, `tests/original/`) are
kept identical to upstream so the original test suite keeps working; please
change them only to follow upstream WAL. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## License

By contributing you agree that your contribution is licensed under the
BSD 3-Clause License of this project (see [LICENSE](LICENSE)).
