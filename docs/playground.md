---
title: Playground
hide:
  - navigation
  - toc
---

# Playground

Both halves run in your browser. WAL++ is compiled to WebAssembly from the same C++20
sources as the `wal` command line tool, and the waveform viewer is
[Surfer](https://gitlab.com/surfer-project/surfer) driven over WCP, its Waveform Control
Protocol. Nothing is uploaded, and the traces never leave the page.

Pick an example on the left, press **Run**, and send a result to the viewer when it is a
list of `(time value)` pairs.

<div id="wal-playground" data-base="./"></div>

!!! note "If the viewer does not start"

    The viewer is a 15 MB WebAssembly bundle, so it loads only when you ask for it.
    The WAL++ console works without it.
