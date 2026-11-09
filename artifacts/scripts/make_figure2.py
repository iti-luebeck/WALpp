#!/usr/bin/env python3
"""Fig. 2 of the paper: CPA (key byte 0) and TVLA, plain software vs. Zkne/Zknd.

Inputs:
  CPA   R_byte00.npy (256 key guesses x samples) from SCA-WAL's
        `cpa_aes.py --byte 0 --save_heatmaps`
  TVLA  per-sample t-values (columns: sample,t) from SCA-WAL's
        `tvla_from_npy.py --save-csv`

Example:
  make_figure2.py --cpa-plain cpa_plain/R_byte00.npy --cpa-accel cpa_accel/R_byte00.npy \
      --tvla-plain ../data/power/tvla_plain_n500.csv --tvla-accel ../data/power/tvla_accel_n500.csv
"""
import argparse
import csv
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use("pdf")
import matplotlib.pyplot as plt  # noqa: E402

# --- IEEE-compatible vector PDF settings ---
matplotlib.rcParams["pdf.fonttype"] = 42  # TrueType, not Type 3
matplotlib.rcParams["ps.fonttype"] = 42
matplotlib.rcParams["font.family"] = "serif"
matplotlib.rcParams["font.serif"] = ["Times New Roman", "Nimbus Roman", "Times", "DejaVu Serif"]
matplotlib.rcParams["mathtext.fontset"] = "stix"
matplotlib.rcParams["axes.linewidth"] = 0.6
matplotlib.rcParams["font.size"] = 8

TRUE_KEY_BYTE0 = 0x2B

# Consistent palette (Okabe-Ito colorblind-safe): plain-software = blue, accelerated = orange.
COLOR_PLAIN = "#0072B2"
COLOR_ACCEL = "#D55E00"
COLOR_OTHER = "0.75"       # light gray for wrong-key CPA hypotheses
COLOR_THRESH = "red"


def load_tvla_csv(path: Path):
    samples, tvals = [], []
    with open(path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            samples.append(int(row["sample"]))
            tvals.append(float(row["t"]))
    return np.array(samples), np.abs(np.array(tvals))


def plot_cpa(ax, R, color, label):
    n_hyp, n_samples = R.shape
    t = np.arange(n_samples)
    absR = np.abs(R)
    # Rasterize the dense "wrong hypothesis" cloud only -- keeps the vector PDF
    # small while the correct-key trace and axes stay crisp vector paths.
    other_mask = np.ones(n_hyp, dtype=bool)
    other_mask[TRUE_KEY_BYTE0] = False
    ax.plot(t, absR[other_mask].T, color=COLOR_OTHER, linewidth=0.3,
             alpha=0.6, rasterized=True, zorder=1)
    peak_t = int(np.argmax(absR[TRUE_KEY_BYTE0]))
    peak_v = absR[TRUE_KEY_BYTE0, peak_t]
    ax.plot(t, absR[TRUE_KEY_BYTE0], color=color, linewidth=1.1, zorder=3,
             label=f"correct key (0x{TRUE_KEY_BYTE0:02X})")
    ax.plot(peak_t, peak_v, marker="o", color=color, markersize=3, zorder=4)
    ax.annotate(f"{peak_v:.2f}", (peak_t, peak_v), textcoords="offset points",
                xytext=(4, 4), fontsize=6.5, color=color)
    ax.set_ylim(0, 1.02)
    ax.set_xlim(0, n_samples)


def plot_tvla(ax, samples, tvals, color, shared_ymax):
    ax.plot(samples, tvals, color=color, linewidth=0.5, rasterized=True, zorder=2)
    ax.axhline(4.5, color=COLOR_THRESH, linestyle="--", linewidth=0.8, zorder=3,
               label=r"threshold $|t|=4.5$")
    ax.set_xlim(0, samples.max())
    # Same y-limit for both configurations so the magnitudes stay comparable.
    ax.set_ylim(0, shared_ymax)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cpa-plain", required=True, type=Path, help="R_byte00.npy, plain software")
    ap.add_argument("--cpa-accel", required=True, type=Path, help="R_byte00.npy, accelerated")
    ap.add_argument("--tvla-plain", required=True, type=Path, help="TVLA CSV, plain software")
    ap.add_argument("--tvla-accel", required=True, type=Path, help="TVLA CSV, accelerated")
    ap.add_argument("-o", "--output", type=Path, default=Path("figure2_cpa_tvla.pdf"))
    args = ap.parse_args()

    r_plain = np.load(args.cpa_plain)
    r_accel = np.load(args.cpa_accel)
    tvla_plain_t, tvla_plain_v = load_tvla_csv(args.tvla_plain)
    tvla_accel_t, tvla_accel_v = load_tvla_csv(args.tvla_accel)

    fig, axes = plt.subplots(2, 2, figsize=(7.16, 4.0), sharex="col")
    (ax_cpa_p, ax_cpa_a), (ax_tvla_p, ax_tvla_a) = axes

    plot_cpa(ax_cpa_p, r_plain, COLOR_PLAIN, "plain-software")
    plot_cpa(ax_cpa_a, r_accel, COLOR_ACCEL, "accelerated")
    tvla_ymax = max(tvla_plain_v.max(), tvla_accel_v.max()) * 1.08
    plot_tvla(ax_tvla_p, tvla_plain_t, tvla_plain_v, COLOR_PLAIN, tvla_ymax)
    plot_tvla(ax_tvla_a, tvla_accel_t, tvla_accel_v, COLOR_ACCEL, tvla_ymax)

    ax_cpa_p.set_ylabel(r"CPA $|\rho|$ (byte 0)")
    ax_tvla_p.set_ylabel(r"TVLA $|t|$")
    ax_tvla_p.set_xlabel("time sample (cycle)")
    ax_tvla_a.set_xlabel("time sample (cycle)")

    ax_cpa_p.set_title("Plain-software AES-128", fontsize=9, color=COLOR_PLAIN)
    ax_cpa_a.set_title("Zkne/Zknd-accelerated AES-128", fontsize=9, color=COLOR_ACCEL)

    for ax in (ax_cpa_p, ax_cpa_a, ax_tvla_p, ax_tvla_a):
        ax.tick_params(labelsize=7)
        ax.grid(True, linewidth=0.3, alpha=0.4)
        for spine in ("top", "right"):
            ax.spines[spine].set_visible(False)

    for ax in (ax_cpa_p, ax_cpa_a):
        ax.legend(loc="upper right", fontsize=6, frameon=False)
    for ax in (ax_tvla_p, ax_tvla_a):
        ax.legend(loc="upper right", fontsize=6, frameon=False)

    fig.align_ylabels([ax_cpa_p, ax_tvla_p])
    fig.tight_layout(pad=0.6, h_pad=1.0, w_pad=1.2)
    fig.savefig(args.output)
    print(f"wrote {args.output}")


if __name__ == "__main__":
    main()
