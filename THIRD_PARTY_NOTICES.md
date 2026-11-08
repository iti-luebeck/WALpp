# Third-party notices

WAL++ is licensed under the BSD 3-Clause License (see [LICENSE](LICENSE)).
This repository also contains code and data from the projects below, each under
its own license. The files are unchanged copies unless stated otherwise.

## WAL — Waveform Analysis Language

- Source: <https://github.com/ics-jku/wal> (project site: <https://wal-lang.org>)
- Version: commit [`a8bd583`](https://github.com/ics-jku/wal/commit/a8bd58362bec2e3a434317783f02214aab396a0f)
  (2025-10-21, after v0.8.5)
- License: BSD 3-Clause, Copyright (c) 2020, Lucas Klemmer

WAL++ is a C++ reimplementation of WAL. It reuses WAL's language design and
the following files verbatim:

| Path in WAL++ | Path in WAL |
|---|---|
| `lib/std/std.wal`, `lib/std/module.wal` | `wal/libs/std/` |
| `tests/examples/` (all files) | `examples/` |
| `tests/original/*.py`, `tests/original/files/` | `tests/` |
| `tests/original/traces/counter.vcd`, `sv_struct_array.{vcd,fst}` | `tests/traces/` |
| `tests/original/traces/counter.fst` | `examples/basics/counter.fst` |

The modules in `tests/original/wal/` are WAL++'s own shims that let the
original WAL test suite run against WAL++; they are not WAL code.

```
BSD 3-Clause License

Copyright (c) 2020, Lucas Klemmer
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## libfst (FST waveform format API)

- Files: `third_party/fstapi/fstapi.c`, `fstapi.h`, `fst_config.h`, `wavealloca.h`
- Source: GTKWave, <https://github.com/gtkwave/gtkwave> (`lib/libfst`)
- License: MIT, Copyright (c) 2009-2018 Tony Bybell; `wavealloca.h` Copyright (c) 1999 Tony Bybell.
  The full license text is in the header of each file.

## FastLZ

- Files: `third_party/fstapi/fastlz.c`, `fastlz.h`
- Source: <https://github.com/ariya/FastLZ>, as bundled with GTKWave
- License: MIT, Copyright (C) 2005-2007 Ariya Hidayat.
  The full license text is in the header of each file.

## LZ4

- Files: `third_party/fstapi/lz4.c`, `lz4.h`
- Source: <https://github.com/lz4/lz4>, as bundled with GTKWave
- License: BSD 2-Clause, Copyright (C) 2011-2015 Yann Collet.
  The full license text is in the header of each file.

## Build-time dependencies (not included)

These are fetched or found at build time and are not part of this repository:
[pybind11](https://github.com/pybind/pybind11) (BSD 3-Clause, Python bindings),
zlib, libedit (BSD-3-Clause; optional, line editing in the interactive REPL),
and for the documentation MkDocs and Material for MkDocs.
