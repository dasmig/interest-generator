"""Cross-check Factbook religion shares against OWID (Pew 2020).

OWID's data is Pew's, which has no open licence, so it is used only to flag
Factbook rows worth a look and is never copied into resources/.

Usage: python compare_religion.py RELIGION_TSV OWID_CSV COUNTRIES_TSV [THRESHOLD]
Prints countries where a major group differs by more than THRESHOLD points
(default 20), worst first.
"""
import csv
import sys
from collections import defaultdict

from religion_groups import GROUPS, group_of

OWID_COL = {
    "christian": "share__religion_christians", "muslim": "share__religion_muslims",
    "hindu": "share__religion_hindus", "buddhist": "share__religion_buddhists",
    "jewish": "share__religion_jews", "unaffiliated": "share__religion_religiously_unaffiliated",
}


def main(fb_tsv, owid_csv, countries_tsv, threshold=20.0):
    iso3 = {}
    with open(countries_tsv, encoding="utf-8", newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            iso3[r["cca3"]] = r["cca2"]
    owid = {}
    with open(owid_csv, encoding="utf-8", newline="") as f:
        for r in csv.DictReader(f):
            if r["year"] == "2020" and r["code"] in iso3:
                owid[iso3[r["code"]]] = {g: float(r[c] or 0) for g, c in OWID_COL.items()}
    fb = defaultdict(lambda: defaultdict(float))
    year = {}
    with open(fb_tsv, encoding="utf-8", newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            fb[r["cca2"]][group_of(r["religion"])] += float(r["share"])
            year[r["cca2"]] = r["year"]
    flags = []
    for cc, shares in fb.items():
        if cc not in owid:
            continue
        for g in GROUPS:
            d = shares.get(g, 0) - owid[cc][g]
            if abs(d) > threshold:
                flags.append((abs(d), cc, g, shares.get(g, 0), owid[cc][g], year[cc]))
    flags.sort(reverse=True)
    print(f"{len(fb)} Factbook countries, {len(owid)} in OWID, {len(flags)} flagged (> {threshold} pts)")
    print("cc  group         factbook  owid2020  fb_year")
    for _, cc, g, a, b, y in flags:
        print(f"{cc}  {g:<12} {a:8.1f} {b:9.1f}  {y}")


if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4], *(float(x) for x in sys.argv[4:5]))
