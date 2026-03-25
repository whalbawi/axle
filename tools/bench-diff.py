#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.9"
# dependencies = [
#     "scipy",
#     "numpy",
# ]
# ///
"""Diff all metrics between two Google Benchmark runs."""

import argparse
import json
import logging
import math
import os
import re
import subprocess
import sys
import tempfile
from collections import defaultdict

log = logging.getLogger("bench-diff")

# Fields that are metadata, not metrics.
_SKIP_FIELDS = frozenset({
    "name", "run_name", "run_type", "aggregate_name", "aggregate_unit",
    "family_index", "per_family_instance_index", "repetitions",
    "repetition_index", "threads", "iterations", "time_unit",
})

# Ordering for well-known fields; everything else sorts alphabetically after.
_FIELD_ORDER = {"real_time": 0, "cpu_time": 1}


def _field_sort_key(field):
    return (_FIELD_ORDER.get(field, 100), field)


def _higher_is_better(field):
    """Return True if a higher value means better performance."""
    return "per_second" in field or field.endswith("_rate")


def load_benchmark_json(path, label, extra_args=None):
    """Load benchmark results from a JSON file or by running a binary.

    If the path points to a valid JSON file, parse it directly.
    Otherwise, treat it as a benchmark binary and execute it.
    """
    # Try parsing as JSON first.
    try:
        with open(path, "r") as f:
            data = json.load(f)
        if "benchmarks" in data:
            log.info("Loading %s from JSON file: %s (%d benchmarks)",
                     label, path, len(data["benchmarks"]))
            return data
    except (json.JSONDecodeError, UnicodeDecodeError):
        pass

    # Not JSON -- treat as a binary.
    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
        tmp_path = tmp.name
    try:
        cmd = [os.path.abspath(path), f"--benchmark_out={tmp_path}",
               "--benchmark_out_format=json"]
        if extra_args:
            cmd.extend(extra_args)
        log.info("Running %s benchmark binary: %s", label, " ".join(cmd))
        subprocess.run(cmd, check=True)
        with open(tmp_path, "r") as f:
            data = json.load(f)
    finally:
        os.unlink(tmp_path)
    log.info("%s benchmark finished: %d results collected",
             label.capitalize(), len(data.get("benchmarks", [])))
    return data


def collect_benchmarks(data):
    """Group benchmark entries by name, collecting all numeric field values.

    Returns (groups, time_units) where groups maps benchmark name to a dict of
    field -> list of values, and time_units maps benchmark name to its time_unit.
    """
    groups = defaultdict(lambda: defaultdict(list))
    time_units = {}
    skipped_aggregate = 0
    for entry in data.get("benchmarks", []):
        if entry.get("run_type") == "aggregate":
            skipped_aggregate += 1
            continue
        name = entry["name"]
        if "time_unit" in entry:
            time_units[name] = entry["time_unit"]
        for field, value in entry.items():
            if field in _SKIP_FIELDS or not isinstance(value, (int, float)):
                continue
            groups[name][field].append(value)
    log.info(
        "Collected %d unique benchmarks (%d aggregate skipped)",
        len(groups), skipped_aggregate,
    )
    return groups, time_units


def format_value(value, field, time_unit=None):
    """Format a benchmark value with appropriate units."""
    if "bytes_per_second" in field:
        units = ["B/s", "KB/s", "MB/s", "GB/s", "TB/s"]
        v = float(value)
        for unit in units:
            if abs(v) < 1024.0 or unit == units[-1]:
                return f"{v:.2f} {unit}"
            v /= 1024.0
    if field in ("real_time", "cpu_time") and time_unit:
        return f"{value:.2f} {time_unit}"
    if "items_per_second" in field:
        units = [("", "/s"), ("k", "/s"), ("M", "/s"), ("G", "/s"), ("T", "/s")]
        v = float(value)
        for prefix, suffix in units:
            if abs(v) < 1000.0 or prefix == "T":
                return f"{v:.2f}{prefix}{suffix}"
            v /= 1000.0
    abs_v = abs(value)
    if abs_v >= 1e9:
        return f"{value / 1e9:.2f}G"
    if abs_v >= 1e6:
        return f"{value / 1e6:.2f}M"
    if abs_v >= 1e3:
        return f"{value / 1e3:.2f}k"
    return f"{value:.2f}"


def mann_whitney_u_test(baseline_values, contender_values, alpha):
    """Perform the Mann-Whitney U test.

    Returns (is_significant, p_value) or (None, None) if not enough data.
    """
    if len(baseline_values) < 2 or len(contender_values) < 2:
        return None, None
    try:
        from scipy.stats import mannwhitneyu
        _, p_value = mannwhitneyu(
            baseline_values, contender_values, alternative="two-sided"
        )
        return p_value < alpha, p_value
    except ImportError:
        log.warning("scipy not found; skipping statistical significance test")
        return None, None


def geometric_mean(ratios):
    """Compute the geometric mean of a list of positive ratios."""
    if not ratios:
        return 1.0
    log_sum = sum(math.log(r) for r in ratios)
    return math.exp(log_sum / len(ratios))


# ANSI color helpers -----------------------------------------------------------

COLOR_GREEN = "\033[32m"
COLOR_RED = "\033[91m"
COLOR_YELLOW = "\033[93m"
COLOR_RESET = "\033[0m"
COLOR_BOLD = "\033[1m"


def colorize(text, color, use_color):
    if not use_color:
        return text
    return f"{color}{text}{COLOR_RESET}"


# Table rendering --------------------------------------------------------------


def _get_verdict_and_color(row):
    """Return (verdict, color) for a benchmark row."""
    delta_pct = row["delta_pct"]
    higher_better = _higher_is_better(row["field"])
    improved = (delta_pct > 0) if higher_better else (delta_pct < 0)
    regressed = (delta_pct < 0) if higher_better else (delta_pct > 0)

    if row["has_utest"]:
        if not row["is_significant"]:
            return "SAME", COLOR_YELLOW
        elif improved:
            return "FASTER", COLOR_GREEN
        else:
            return "SLOWER", COLOR_RED
    else:
        if improved:
            return "FASTER", COLOR_GREEN
        elif regressed:
            return "SLOWER", COLOR_RED
        else:
            return "", COLOR_RESET


def build_table(rows, use_color, alpha):
    """Build and print the comparison table.

    Rows are grouped by benchmark name.  The name is printed only on the first
    field of each group; subsequent fields show a blank name column.
    """
    # Pre-format cells.
    formatted = []
    # Track ratios per field for per-field geometric mean.
    field_ratios = defaultdict(list)
    prev_name = None
    for row in rows:
        baseline_str = format_value(row["baseline_val"], row["field"],
                                    row.get("time_unit"))
        contender_str = format_value(row["contender_val"], row["field"],
                                     row.get("time_unit"))
        delta_pct = row["delta_pct"]
        sign = "+" if delta_pct >= 0 else ""
        delta_str = f"{sign}{delta_pct:.1f}%"

        verdict, color = _get_verdict_and_color(row)

        display_name = row["name"] if row["name"] != prev_name else ""
        prev_name = row["name"]

        formatted.append({
            "name": display_name,
            "field": row["field"],
            "baseline": baseline_str,
            "contender": contender_str,
            "delta": delta_str,
            "verdict": verdict,
            "color": color,
        })
        if row["baseline_val"] != 0:
            field_ratios[row["field"]].append(
                row["contender_val"] / row["baseline_val"]
            )

    # Compute column widths.
    show_verdict = not use_color
    headers = ["Benchmark", "Field", "Baseline", "Contender", "Delta"]
    if show_verdict:
        headers.append("Verdict")

    name_w = max(len(headers[0]), *(len(r["name"]) for r in formatted))
    field_w = max(len(headers[1]), *(len(r["field"]) for r in formatted))
    base_w = max(len(headers[2]), *(len(r["baseline"]) for r in formatted))
    cont_w = max(len(headers[3]), *(len(r["contender"]) for r in formatted))
    delta_w = max(len(headers[4]), *(len(r["delta"]) for r in formatted))
    verdict_w = (max(len("Verdict"), *(len(r["verdict"]) for r in formatted))
                 if show_verdict else 0)

    def fmt_row(name, field, baseline, contender, delta,
                verdict="", color=None):
        line = (
            f"  {name:<{name_w}}  "
            f"{field:<{field_w}}  "
            f"{baseline:>{base_w}}  "
            f"{contender:>{cont_w}}  "
        )
        if use_color:
            delta_part = (colorize(f"{delta:>{delta_w}}", color, use_color)
                          if color else f"{delta:>{delta_w}}")
            line += delta_part
        else:
            line += f"{delta:>{delta_w}}  {verdict:<{verdict_w}}"
        return line

    total_w = name_w + field_w + base_w + cont_w + delta_w + 10
    if show_verdict:
        total_w += verdict_w + 2

    # Header.
    header_line = fmt_row(
        *headers[:5],
        verdict=headers[5] if show_verdict else "",
    )
    sep_line = "  " + "-" * total_w

    print()
    if use_color:
        print(f"{COLOR_BOLD}{header_line}{COLOR_RESET}")
    else:
        print(header_line)
    print(sep_line)

    for r in formatted:
        print(fmt_row(r["name"], r["field"], r["baseline"], r["contender"],
                       r["delta"], verdict=r["verdict"], color=r["color"]))

    # Per-field geometric mean summary.
    print(sep_line)
    for field in sorted(field_ratios, key=_field_sort_key):
        ratios = field_ratios[field]
        gmean = geometric_mean(ratios)
        gmean_pct = (gmean - 1.0) * 100.0
        sign = "+" if gmean_pct >= 0 else ""
        summary_delta = f"{sign}{gmean_pct:.1f}%"
        higher_better = _higher_is_better(field)
        improved = (gmean_pct > 0) if higher_better else (gmean_pct < 0)
        regressed = (gmean_pct < 0) if higher_better else (gmean_pct > 0)
        if improved:
            color = COLOR_GREEN
        elif regressed:
            color = COLOR_RED
        else:
            color = COLOR_RESET
        print(fmt_row("Geometric mean", field, "", "", summary_delta,
                       color=color))
    print()


# Main -------------------------------------------------------------------------


def main():
    parser = argparse.ArgumentParser(
        description="Diff all metrics between two Google Benchmark runs."
    )
    parser.add_argument("baseline", help="Baseline JSON file or benchmark binary")
    parser.add_argument("contender", help="Contender JSON file or benchmark binary")
    parser.add_argument(
        "--alpha",
        type=float,
        default=0.05,
        help="Significance level for the Mann-Whitney U test (default: 0.05)",
    )
    parser.add_argument(
        "--filter",
        default=None,
        help="Regex to filter benchmark names",
    )
    parser.add_argument(
        "--fields",
        default=None,
        help="Comma-separated list of fields to include (default: all)",
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="Disable ANSI color output",
    )
    parser.add_argument(
        "--benchmark-args",
        nargs=argparse.REMAINDER,
        default=None,
        help="Extra arguments to pass when running benchmark binaries",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose info logging",
    )
    args = parser.parse_args()

    logging.basicConfig(
        format="%(name)s: %(message)s",
        level=logging.INFO if args.verbose else logging.WARNING,
    )

    use_color = not args.no_color and sys.stdout.isatty()
    field_filter = set(args.fields.split(",")) if args.fields else None

    baseline_data = load_benchmark_json(args.baseline, "baseline",
                                        extra_args=args.benchmark_args)
    contender_data = load_benchmark_json(args.contender, "contender",
                                         extra_args=args.benchmark_args)

    baseline_benchmarks, baseline_time_units = collect_benchmarks(baseline_data)
    contender_benchmarks, contender_time_units = collect_benchmarks(contender_data)

    # Intersection of benchmark names, preserving baseline order.
    contender_set = set(contender_benchmarks.keys())
    common_names = [n for n in baseline_benchmarks if n in contender_set]
    log.info("Matched %d benchmarks between baseline and contender",
             len(common_names))

    # Apply name filter if provided.
    if args.filter:
        pattern = re.compile(args.filter)
        before = len(common_names)
        common_names = [n for n in common_names if pattern.search(n)]
        log.info("Filter /%s/ kept %d of %d benchmarks",
                 args.filter, len(common_names), before)

    if not common_names:
        print("No matching benchmarks found.", file=sys.stderr)
        sys.exit(1)

    rows = []
    utest_skipped = 0
    utest_performed = 0
    for name in common_names:
        b_fields = baseline_benchmarks[name]
        c_fields = contender_benchmarks[name]
        common_fields = sorted(
            [f for f in b_fields if f in c_fields],
            key=_field_sort_key,
        )
        if field_filter:
            common_fields = [f for f in common_fields if f in field_filter]

        time_unit = (baseline_time_units.get(name)
                     or contender_time_units.get(name))

        for field in common_fields:
            b_vals = b_fields[field]
            c_vals = c_fields[field]
            b_avg = sum(b_vals) / len(b_vals)
            c_avg = sum(c_vals) / len(c_vals)

            if b_avg == 0:
                continue

            delta_pct = (c_avg - b_avg) / b_avg * 100.0

            is_significant, p_value = mann_whitney_u_test(
                b_vals, c_vals, args.alpha,
            )
            if is_significant is None:
                utest_skipped += 1
            else:
                utest_performed += 1

            rows.append({
                "name": name,
                "field": field,
                "baseline_val": b_avg,
                "contender_val": c_avg,
                "delta_pct": delta_pct,
                "is_significant": is_significant,
                "p_value": p_value,
                "has_utest": is_significant is not None,
                "time_unit": time_unit,
            })

    if utest_performed:
        log.info("U-test performed on %d comparisons (alpha=%.2f)",
                 utest_performed, args.alpha)
    if utest_skipped:
        log.info("U-test skipped for %d comparisons (need >= 2 repetitions)",
                 utest_skipped)

    if not rows:
        print("No benchmarks with comparable fields found.", file=sys.stderr)
        sys.exit(1)

    build_table(rows, use_color, args.alpha)


if __name__ == "__main__":
    main()
