# Changelog

All notable changes to WAL++ are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

## [1.0.0] - TBD

First public release.

- WAL interpreter in C++20 with the WAL standard library and 130+ operators
- VCD, FST and CSV readers and writers, streaming mode for large traces
- Fast Hamming weight / Hamming distance operators (`--fast-hw-all`, `--fast-hd-all`),
  including signals wider than 64 bits
- Python bindings (`walpp` package) and the `wal` command-line tool with REPL
- Paper evaluation traces in `artifacts/`
