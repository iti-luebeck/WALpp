# Paper artifacts

Waveform traces used in the evaluation of the WAL++ paper
(see [CITATION.cff](../CITATION.cff) for the reference).
They are stored xz-compressed to keep the repository small.

| File | Uncompressed | Source |
|---|---|---|
| `traces/rv64ui-p-simple.vcd.xz` | 69.8 MB | Rocket Chip core running the `rv64ui-p-simple` test from riscv-tests, simulated with Verilator |
| `traces/netlist_aes_trace.vcd.xz` | 5.7 MB | AES-128 gate-level netlist simulation, Synopsys VCS T-2022.06 |
| `traces/rtl_aes_trace.vcd.xz` | 0.3 MB | AES-128 RTL simulation, same testbench |

<!-- TODO(Andrija): name the AES design and the Rocket configuration, and which
     table/figure of the paper each trace belongs to; add the WAL++ commands or
     scripts that reproduce the paper's numbers. -->

## Unpack

```bash
xz -dk artifacts/traces/*.vcd.xz
shasum -a 256 -c artifacts/traces/SHA256SUMS
```

`SHA256SUMS` holds the checksums of the uncompressed VCD files.

## Example

```bash
# Hamming distance of all signals at every timestamp, as CSV
wal --fast-hd-all artifacts/traces/netlist_aes_trace.vcd -o netlist_aes_hd.csv
```

<!-- TODO(Andrija): replace with the real evaluation commands. -->
