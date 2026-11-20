# Paper artifacts

This directory holds the workload descriptions, analysis scripts, results and
traces behind the evaluation of

> A. Nešković, A. Kabour, C. Ewert, M. Berekovic, R. Meyer, S. Mulhem:
> *Security-Driven Verification of RISC-V Crypto Extensions: A Scalable
> Framework for Timing and Power Side-Channel Analysis.*
> IEEE Embedded Systems Letters, 2026 (early access),
> doi:[10.1109/LES.2026.3753937](https://doi.org/10.1109/LES.2026.3753937).
> BibTeX: [`CITATION.bib`](../CITATION.bib).

The Sailor simulation waveforms are tens to hundreds of GB per campaign and
are not distributed; they can be regenerated from the workload descriptions
below. The per-cycle activity traces derived from them (the input to CPA and
TVLA) are in [`traces/`](traces/), together with three example waveforms
described in [Waveform traces](#waveform-traces). All traces are stored
xz-compressed; `SHA256SUMS` holds the checksums of the uncompressed files.

```bash
xz -dk artifacts/traces/*.xz
(cd artifacts/traces && shasum -a 256 -c SHA256SUMS)
```

```
artifacts/
├── scripts/                 WAL++ post-processing used in the paper
│   ├── walpp_tools.py         shared helpers (wal CLI calls, clock edges, markers, resampling)
│   ├── extract_latency.py     per-instruction latency from a constant-time probe waveform
│   ├── check_constant_time.py zero-variance PASS/FAIL per (instruction, byte-select)
│   ├── extract_campaign.py    per-cycle activity traces, one encryption per waveform
│   ├── bench_ct_extraction.py Table II row 1 (WAL++ vs Python WAL, value changes)
│   ├── latency_signals.wal    Python WAL query used by bench_ct_extraction.py
│   ├── bench_hw.py            Table II rows 2-3 (WAL++ vs Python WAL, Hamming weight)
│   ├── make_figure2.py        Fig. 2
│   └── signals/               marker signal lists for the RTL and netlist testbenches
├── data/
│   ├── inputs/                AES key, plaintexts, constant-time operands
│   ├── constant_time/         Table III latencies and summaries
│   ├── power/                 CPA summaries and TVLA t-values (Fig. 2, Sec. III)
│   └── performance/           re-run logs of the Table II measurements
└── traces/
    ├── traces_{plain,accel}_n500_{fixed,random}.npy.xz     RTL activity traces (Fig. 2)
    ├── traces_netlist_accel_n500_{fixed,random}.npy.xz     netlist activity traces (Sec. III)
    ├── netlist_aes_trace.vcd.xz                            Table II row 2 input
    ├── rtl_aes_trace.vcd.xz, rv64ui-p-simple.vcd.xz        example waveforms
    └── SHA256SUMS
```

Requirements: WAL++ (`wal` on `PATH`, or pass `--wal-bin`), Python 3.9+ with
numpy (matplotlib for Fig. 2), and for CPA/TVLA and the Python WAL baseline
[SCA-WAL](https://github.com/iti-luebeck/sca-wal) at commit `94eea44`.

## Target: Sailor

- Core: Sailor, a bit-serial RISC-V core with scalar-crypto hardware
  ([arXiv:2602.24166](https://arxiv.org/abs/2602.24166),
  [github.com/iti-luebeck/SailChip](https://github.com/iti-luebeck/SailChip)).
  RTL revision used: SailChip commit `ac8496e` (2026-07-13).
- Configuration: `serialWidth = 32`, `ChiselSyncMem` register file, no branch
  predictor. Two cores: `rv32i` (plain software) and `rv32izktzkn`
  (Zkn + Zkt, crypto hardware).
- RTL simulation: Verilator through chiseltest 0.5.4. The testbench maps
  program memory, a `.tohost` exit register and a `.timestamp` register at
  `0x80003000`; memory responds without wait states.
- Netlist: the `rv32izktzkn` core implemented with LibreLane (Yosys, OpenROAD)
  on IHP SG13G2 (130 nm, 50 ns clock constraint; 10,849 cells, DRC/LVS clean),
  simulated with Icarus Verilog using the post-route SDF of the
  `nom_typ_1p20V_25C` corner.
- Programs: RISC-V GCC, `-O3 -mabi=ilp32`, with `-march=rv32i` (plain) or
  `-march=rv32izkn` (accelerated).

## Workload A: constant-time verification (Table III)

| | |
|---|---|
| Instructions | `aes32esi`, `aes32esmi`, `aes32dsi`, `aes32dsmi`, byte-select 0..3 |
| Trials | 1000 random operand pairs per (instruction, byte-select), 16,000 executions in a single simulation of the `rv32izktzkn` core |
| Operands | [`data/inputs/ct_operands.csv`](data/inputs/ct_operands.csv): Python `random.seed(42)`, 1024 × `getrandbits(32)` for `rs1`, then 1024 for `rs2`; the first 1000 are used |
| Order | instruction-major, then byte-select, then trial |
| Bracketing | `timestamp = 1` store → one AES instruction (inline asm, `volatile`, with a compiler barrier) → `timestamp = 1` store |
| Latency | rising clock edges between the two stores, so a constant store overhead is included |

**Bug injection.** One line of the Sailor control logic is changed so that
the middle-round encryption instruction takes one extra cycle when bit 0 of
the S-box input is set. In `src/main/scala/Core/SerialTopModule.scala`
(SailChip `ac8496e`), in the 32-bit control block after the comment
"Don't add code above this line for 32 bit version!!!":

```scala
// baseline (line 626)
optionalSubInstrCounterIO.subInstructionCount := Mux(optionalAesEncIO.aesEncOp.asUInt.asBool, 4.U, 3.U)
// bug injection
optionalSubInstrCounterIO.subInstructionCount := Mux(optionalAesEncIO.aesEncOp.asUInt.asBool,
                                                     Mux(optionalAesSboxIO.i_data(0), 5.U, 4.U), 3.U)
```

The same assignment near line 333 belongs to the other serial widths and has
no effect for `serialWidth = 32`.

## Workload B: power side-channel analysis (Fig. 2, Sec. III)

| | |
|---|---|
| Program | one AES-128 key schedule, then `timestamp` store → one encryption → `timestamp` store; one binary and one fresh simulation per trace |
| Plain software | byte-oriented reference AES with table S-box (riscv-crypto benchmark code) |
| Accelerated | assembly using `aes32esi`/`aes32esmi` (riscv-crypto benchmark code) |
| Key | NIST SP 800-38A F.1.1: `2B7E151628AED2A6ABF7158809CF4F3C` ([`data/inputs/key.npy`](data/inputs/key.npy)) |
| Fixed plaintext (TVLA) | `6BC1BEE22E409F96E93D7E117393172A` |
| Random plaintexts | Python `random.seed(1337)`, 16 × `getrandbits(8)` per block ([`data/inputs/plaintexts_random.npy`](data/inputs/plaintexts_random.npy), 1000 × 16; the first 500 are used) |
| Traces | 500 fixed + 500 random per configuration |
| Activity model | per-cycle Hamming weight of all signals of the core, without testbench and memories: scope `TOP.SAILORBlackBoxAdapter.core` (RTL, settled value per cycle) or `sailor_tb.dut` (netlist, sum of all events in the cycle, so glitches are included) |
| Window | between the two markers: 4,512 cycles (accelerated, RTL and netlist); 8,778–8,850 cycles (plain), truncated to the shortest |
| CPA | first-round S-box output, Hamming-weight model, 256 guesses per key byte, random set |
| TVLA | fixed vs. random Welch's t-test, threshold \|t\| > 4.5, traces centred per trace |

## Where each result comes from

| Paper | Value | Data | Produced by |
|---|---|---|---|
| Table I | 0.54 s, 2 m 27 s | cited from SCA-WAL | — |
| Table II, constant-time extraction | 307.8 s vs. 4.4 s | Workload A waveform (1.08 GB) | `bench_ct_extraction.py` |
| Table II, HW micro-benchmark | 51.84 s vs. 0.053 s | [`traces/netlist_aes_trace.vcd.xz`](traces/) | `bench_hw.py` |
| Table II, HW campaign trace | 46.52 s vs. 0.051 s | one Workload B waveform (plain, random #0) | `bench_hw.py` |
| Table II, full N=500 campaign | ~12.9 h vs. 172 s | 1000 Workload B waveforms (plain) | `extract_campaign.py --jobs 1`; Python WAL extrapolated as 1000 × the single-trace time |
| Table III | 32 / 33 / 32 / 35 cycles; bug injection 33–34 for `aes32esmi` | [`data/constant_time/`](data/constant_time/) | `extract_latency.py`, `check_constant_time.py` |
| Fig. 2, Sec. III RTL | 16/16 key bytes; peaks 0.79–0.99 (plain), 0.75–0.92 (accel.); max \|t\| 151.9 / 75.2 | [`traces/`](traces/) `traces_{plain,accel}_n500_*`, [`data/power/`](data/power/) | `extract_campaign.py --preset sailor-rtl`, SCA-WAL `cpa_aes.py` / `tvla_from_npy.py`, `make_figure2.py` |
| Sec. III netlist, constant time | no variation, 4,512 cycles | netlist Workload B | `extract_campaign.py --preset sailor-netlist` (window length) |
| Sec. III netlist, power | 16/16 key bytes; peaks 0.72–0.78; max \|t\| 90.7 | [`traces/`](traces/) `traces_netlist_accel_n500_*`, [`data/power/`](data/power/) | `extract_campaign.py --preset sailor-netlist`, SCA-WAL scripts |

Table II was measured on an Apple M4 Max (16 cores, 64 GB RAM); all times
include loading and parsing the waveform. A re-run with the release build on
the same machine is in [`data/performance/`](data/performance/)
([`table2_measurements.csv`](data/performance/table2_measurements.csv)): rows
2–4 agree with the paper within a few percent; row 1 is faster for WAL++
(2.49 s instead of 4.4 s), and Python WAL reports 4 fewer value changes than
WAL++ on that waveform.

## Waveform traces

| File | Uncompressed | Content | Used for |
|---|---|---|---|
| `netlist_aes_trace.vcd.xz` | 5.7 MB | gate-level simulation of one AES-128 encryption from the SCA-WAL case study (Nešković et al., IEEE ESL 2025): OpenCores [tiny_aes](https://opencores.org/projects/tiny_aes), synthesized with the Synopsys 14 nm educational design kit, simulated with Synopsys VCS T-2022.06 (10 ns clock); 147,400 signals, 383 timestamps | Table II, HW micro-benchmark |
| `rtl_aes_trace.vcd.xz` | 0.3 MB | RTL simulation of the same design and testbench (one encryption, 2,452 signals) | `examples/hw_module.wal` (scope `test_aes_128`); not used in the paper |
| `rv64ui-p-simple.vcd.xz` | 69.8 MB | Chipyard SoC with one Rocket core (FPU, L1 caches, L2) running the `rv64ui-p-simple` test from riscv-tests, simulated with Verilator | `examples/hd_module.wal` (Rocket core scope); not used in the paper |

Both AES traces use the fixed key `7369C667EC4AFF51ABBACD2946E3FBF2` of the
SCA-WAL test patterns. The value of the testbench's `filename` register (the
test-pattern file it read) has been shortened to a relative path; the traces
are otherwise unchanged. Their testbench contains wide file-I/O registers
(`filename`, `dumpfile`, `line`) that dominate an all-signals Hamming weight;
use a scope such as `test_aes_128.uut` to restrict the sum to the AES core.

## Reproducing

### From the shipped activity traces (no simulation)

```bash
xz -dk artifacts/traces/*.npy.xz
(cd artifacts/traces && shasum -a 256 -c SHA256SUMS)

git clone https://github.com/iti-luebeck/sca-wal && git -C sca-wal checkout 94eea44
X=sca-wal/experiment-scripts
A=artifacts/traces
python3 -c "import numpy as np; np.save('pt500.npy', np.load('artifacts/data/inputs/plaintexts_random.npy')[:500])"

for c in plain accel netlist_accel; do
  python3 $X/cpa_aes.py --traces $A/traces_${c}_n500_random.npy --plaintexts pt500.npy \
      --csv --outdir cpa_$c                                    # -> cpa_$c/cpa_summary.csv
  python3 $X/tvla_from_npy.py --fixed $A/traces_${c}_n500_fixed.npy \
      --random $A/traces_${c}_n500_random.npy --center per-trace --save-csv tvla_$c.csv
done
# The outputs are identical to data/power/{cpa_summary,tvla}_*_n500.csv.

# Fig. 2
for c in plain accel; do
  python3 $X/cpa_aes.py --traces $A/traces_${c}_n500_random.npy --plaintexts pt500.npy \
      --byte 0 --save_heatmaps --outdir hm_$c
done
python3 artifacts/scripts/make_figure2.py --cpa-plain hm_plain/R_byte00.npy --cpa-accel hm_accel/R_byte00.npy \
    --tvla-plain tvla_plain.csv --tvla-accel tvla_accel.csv -o figure2_cpa_tvla.pdf
```

### From your own simulation

The scripts only use these WAL++ calls:

```bash
wal --fast-list <vcd>                                  # find the hierarchy and signal names
wal --fast-values-signals <siglist.txt> <vcd> -o v.csv # value changes: time,signal_index,value
wal --fast-hw-scope <scope> <vcd> -o hw.csv            # Hamming weight below <scope>: time,value
```

`siglist.txt` lists clock, data-bus address, isWrite and valid, one per line,
dot-separated (see [`scripts/signals/`](scripts/signals/)). For another design,
pass `--signals` and `--scope`.

```bash
# Workload A
scripts/extract_latency.py --vcd ct_probe.vcd --n-trials 1000 -o latency.csv
scripts/check_constant_time.py --csv latency.csv --label baseline --out-csv summary.csv

# Workload B: waveform paths must contain r0_i<k> (fixed) or r1_i<k> (random)
scripts/extract_campaign.py --preset sailor-rtl --jobs 16 --out-prefix traces_accel_n500 runs/*r[01]_i*/*.vcd
scripts/extract_campaign.py --preset sailor-netlist --jobs 16 --out-prefix traces_netlist_accel_n500 vcd/*_r[01]_i*.vcd

# Table II
scripts/bench_ct_extraction.py --vcd ct_probe.vcd --sca-wal-dir sca-wal
scripts/bench_hw.py --vcd traces/netlist_aes_trace.vcd --sca-wal-dir sca-wal   # row 2
scripts/bench_hw.py --vcd <one Workload B waveform> --sca-wal-dir sca-wal      # row 3
```
