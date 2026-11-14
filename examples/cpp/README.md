# C++ examples

Four programs, each the worked example from one section of the C++ guides. They
build against an **installed** WAL++, the way a consumer would:

```bash
cmake --install <walpp-build> --prefix /opt/walpp
cmake -S examples/cpp -B build -DCMAKE_PREFIX_PATH=/opt/walpp
cmake --build build
```

| | |
|---|---|
| `edge_count.cpp` | Load a trace, run a query, read the result. [Guide](../../docs/guides/cpp-api.md) |
| `custom_operator.cpp` | Register an operator written in C++. [Guide](../../docs/guides/cpp-api.md#adding-an-operator) |
| `toggles.cpp` | A one-pass extraction tool. [Guide](../../docs/guides/writing-fast-tools.md) |
| `reuse_kernel.cpp` | Reuse the kernels the CLI uses. [Guide](../../docs/guides/writing-fast-tools.md#if-you-need-the-state-not-a-scalar) |

Each produces a result you can check against the CLI:

```bash
./build/edge_count     counter.vcd tb.clk     # 40 rising edges
./build/custom_operator counter.vcd           # 0.493827
./build/toggles        counter.vcd tb         # 150 bit toggles over 81 timestamps
./build/reuse_kernel   counter.vcd tb         # same rows as --fast-hd-scope tb
```
