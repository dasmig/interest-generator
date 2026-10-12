"""Merge the parsed Factbook shares with the curated overrides.

A country present in the curated file takes all its rows from there (the
Factbook rows are dropped). Writes cca2, religion, group, share, year, basis.
Territories listed in PARENT inherit their parent country's rows.
Reports each country whose shares do not sum to 90-110.

Usage: python merge_religion.py FACTBOOK_TSV CURATED_TSV OUT_TSV
"""
import csv
import sys
from collections import defaultdict

from religion_groups import group_of


# Territories with no data of their own take their parent's shares. Antarctica
# has no parent and no population, so it stays without data.
PARENT = {"BV": "NO", "SJ": "NO", "HM": "AU", "GS": "GB", "IO": "GB", "AX": "FI", "BQ": "NL",
          "TF": "FR", "GF": "FR", "GP": "FR", "MQ": "FR", "YT": "FR", "RE": "FR", "BL": "FR"}


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def main(fb_tsv, curated_tsv, out_tsv):
    curated = read(curated_tsv)
    over = {r["cca2"] for r in curated}
    rows = [(r["cca2"], r["religion"], float(r["share"]), r["year"], "factbook")
            for r in read(fb_tsv) if r["cca2"] not in over]
    rows += [(r["cca2"], r["religion"], float(r["share"]), r["year"], r["basis"])
             for r in curated]
    have = {r[0] for r in rows}
    for child, parent in sorted(PARENT.items()):
        if child not in have:
            rows += [(child, r[1], r[2], r[3], f"parent: {parent}") for r in rows if r[0] == parent]
    rows.sort(key=lambda r: (r[0], -r[2]))
    with open(out_tsv, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["cca2", "religion", "group", "share", "year", "basis"])
        for cc, rel, share, year, basis in rows:
            w.writerow([cc, rel, group_of(rel), f"{share:g}", year, basis])
    total = defaultdict(float)
    for cc, _, share, _, _ in rows:
        total[cc] += share
    odd = {cc: round(t, 1) for cc, t in total.items() if not 90 <= t <= 110}
    print(f"{len(total)} countries, {len(rows)} rows -> {out_tsv}")
    print(f"sum outside 90-110 ({len(odd)}):", odd)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
