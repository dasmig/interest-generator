"""Build resources/{lite,full}/religion.tsv from resources/raw/religion.tsv.

raw/religion.tsv is the output of merge_religion.py (Factbook shares plus the
curated overrides; see doc/data.md). This script only reshapes it:

  full  every country in the raw file, the detailed rows
        (country_code, religion, group, weight, year, basis)
  lite  only the countries listed in COUNTRIES_TSV with independent = 1, with
        the shares added up per major group (country_code, religion, group, weight)
        and rounded to 0.1

Usage: python prepare_religion.py COUNTRIES_TSV [RAW_TSV] [RESOURCES_DIR]
  COUNTRIES_TSV  country-generator's resources/full/countries.tsv
"""
import csv
import sys
from collections import defaultdict
from pathlib import Path

GROUP_LABEL = {
    "christian": "Christian", "muslim": "Muslim", "hindu": "Hindu",
    "buddhist": "Buddhist", "jewish": "Jewish", "unaffiliated": "Unaffiliated",
    "other": "Other",
}


def write(path, header, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(header)
        w.writerows(rows)


def main(countries_tsv, raw_tsv="resources/raw/religion.tsv", out="resources"):
    with open(countries_tsv, encoding="utf-8", newline="") as f:
        independent = {r["cca2"] for r in csv.DictReader(f, delimiter="\t") if r["independent"] == "1"}
    with open(raw_tsv, encoding="utf-8", newline="") as f:
        raw = list(csv.DictReader(f, delimiter="\t"))

    out = Path(out)
    full = sorted((r["cca2"], r["religion"], r["group"], r["share"], r["year"], r["basis"]) for r in raw)
    write(out / "full" / "religion.tsv",
          ["country_code", "religion", "group", "weight", "year", "basis"], full)

    by_group = defaultdict(lambda: defaultdict(float))
    for r in raw:
        if r["cca2"] in independent:
            by_group[r["cca2"]][r["group"]] += float(r["share"])
    lite = []
    for cc in sorted(by_group):
        for g, share in sorted(by_group[cc].items(), key=lambda kv: -kv[1]):
            if round(share, 1) > 0:
                lite.append((cc, GROUP_LABEL[g], g, f"{share:.1f}"))
    write(out / "lite" / "religion.tsv", ["country_code", "religion", "group", "weight"], lite)
    print(f"full: {len(full)} rows, {len({r[0] for r in full})} countries")
    print(f"lite: {len(lite)} rows, {len(by_group)} countries")
    missing = independent - set(by_group)
    if missing:
        print("independent countries without data:", sorted(missing))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
