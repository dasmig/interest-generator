"""Build the hobby and media tables of resources/{lite,full}/.

Inputs (resources/raw/):
  interests_curated.tsv   the catalogue (kind, key, label, class, ..., tier)
  affinity_curated.tsv    hand-written factors (target, dimension, value, factor, basis, tier)
  timeuse_eurostat.tsv    Eurostat HETUS participation rates (fetch_timeuse.py)
and country-generator's countries.tsv for the regions.

Outputs, per tier: interests.tsv, affinity.tsv, age_bands.tsv, regions.tsv.

Measured factors (basis "eurostat"), per activity class:
  sex       mean participation of men (or women) / mean participation of all
  age_band  mean participation of the age group / mean participation of all
  country   (full) the country's participation / the mean of the countries
  region    the mean of the country factors in a UN subregion
Rates come from each country's latest wave (2010, else 2000). Factors are
clipped (sex, age 0.2-5; country, region 0.4-2.5), and those within 5% of 1
are left out. A country gets a row wherever its region has one, so a country
row always replaces the region row.

Usage: python prepare_interests.py COUNTRIES_TSV [RESOURCES_DIR]
"""
import csv
import math
import sys
from collections import defaultdict
from pathlib import Path

CLASS_ACTIVITY = {
    "sport": "AC6A", "outdoors": "AC611", "handicraft": "AC33A", "gardening": "AC34A", "pets": "AC343",
    "diy": "AC35", "volunteering": "AC41", "religious": "AC43", "socialising": "AC51A", "culture": "AC52",
    "computing": "AC7A", "hobbyist": "AC7B", "gaming": "AC733", "reading": "AC812", "screen": "AC82",
    "music": "AC83", "study": "AC22",
}
# (band, min_age, max_age, Eurostat age groups in order of preference; none = curated)
BANDS = [
    ("5-12", 5, 12, None), ("13-17", 13, 17, ["Y15-20"]), ("18-24", 18, 24, ["Y20-24"]),
    ("25-34", 25, 34, ["Y25-44"]), ("35-49", 35, 49, ["Y25-44"]), ("50-64", 50, 64, ["Y45-64"]),
    ("65+", 65, 120, ["Y_GE65", "Y65-74"]),
]
MIN_TOTAL_RATE = 1.0  # percent; below this the ratios are noise


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def write(path, header, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(header)
        w.writerows(rows)


def clip(x, lo, hi):
    return max(lo, min(hi, x))


def neutral(x, tol):
    return abs(x - 1.0) <= tol


def measured(timeuse, subregion):
    """Return (sex_rows, age_rows, country_rows, region_rows) as (target, dim, value, factor)."""
    latest = {}  # (cc, activity, sex, age) -> rate, from the latest wave
    for r in sorted(timeuse, key=lambda r: r["wave"]):
        latest[(r["country_code"], r["activity"], r["sex"], r["age"])] = float(r["rate"])
    countries = sorted({k[0] for k in latest})

    def mean_ratio(act, num, den):
        """Ratio of means over countries that have both rates."""
        a = b = n = 0
        for cc in countries:
            x, y = latest.get((cc, act) + num), latest.get((cc, act) + den)
            if x is not None and y is not None:
                a, b, n = a + x, b + y, n + 1
        return (a / b, b / n) if n and b > 0 else (None, 0)

    sex_rows, age_rows, country_rows, region_rows = [], [], [], []
    for cls, act in CLASS_ACTIVITY.items():
        total_ratio, total_rate = mean_ratio(act, ("T", "TOTAL"), ("T", "TOTAL"))
        if total_rate < MIN_TOTAL_RATE:
            print(f"skip {cls}: mean rate {total_rate:.2f}% is too low")
            continue
        for sex, name in (("M", "male"), ("F", "female")):
            ratio, _ = mean_ratio(act, (sex, "TOTAL"), ("T", "TOTAL"))
            if ratio is not None:
                sex_rows.append((cls, "sex", name, clip(ratio, 0.2, 5.0)))
        for band, _, _, groups in BANDS:
            for g in groups or []:
                ratio, _ = mean_ratio(act, ("T", g), ("T", "TOTAL"))
                if ratio is not None:
                    age_rows.append((cls, "age_band", band, clip(ratio, 0.2, 5.0)))
                    break
        mean_total = total_rate
        by_region = defaultdict(list)
        cfac = {}
        for cc in countries:
            rate = latest.get((cc, act, "T", "TOTAL"))
            if rate is None:
                continue
            cfac[cc] = clip(rate / mean_total, 0.4, 2.5)
            if subregion.get(cc):
                by_region[subregion[cc]].append(cfac[cc])
        regions = {r: sum(v) / len(v) for r, v in by_region.items()}
        for r, f in regions.items():
            if not neutral(f, 0.05):
                region_rows.append((cls, "region", r, f))
        for cc, f in cfac.items():
            reg = regions.get(subregion.get(cc))
            if not neutral(f, 0.1) or (reg is not None and not neutral(reg, 0.05)):
                country_rows.append((cls, "country", cc, f))
    return sex_rows, age_rows, country_rows, region_rows


def main(countries_tsv, out="resources"):
    raw = Path(out) / "raw"
    countries = read(countries_tsv)
    subregion = {r["cca2"]: r["subregion"] for r in countries}
    independent = {r["cca2"] for r in countries if r["independent"] == "1"}
    catalogue = read(raw / "interests_curated.tsv")
    curated = read(raw / "affinity_curated.tsv")
    sex_rows, age_rows, country_rows, region_rows = measured(read(raw / "timeuse_eurostat.tsv"), subregion)

    keys = {r["key"] for r in catalogue}
    classes = {r["class"] for r in catalogue if r["class"]}

    def target_of(t):
        """An item key, or a class written as class:<name>; a bare name that is not an item key is a class."""
        if t.startswith("class:"):
            return t if t[6:] in classes else None
        return t if t in keys else ("class:" + t if t in classes else None)

    regions = set(subregion.values()) - {""}
    ccs = set(subregion)
    bands = {b[0] for b in BANDS}
    for r in curated:
        t, d, v = r["target"], r["dimension"], r["value"]
        problems = []
        if target_of(t) is None:
            problems.append("unknown target")
        if d == "region" and v not in regions:
            problems.append("unknown region")
        if d == "country" and v not in ccs:
            problems.append("unknown country")
        if d == "age_band" and v not in bands:
            problems.append("unknown age band")
        if d == "sex" and v not in ("male", "female"):
            problems.append("unknown sex")
        if problems:
            raise SystemExit(f"affinity_curated.tsv: {r}: {', '.join(problems)}")

    basis = "eurostat HETUS 2000/2010"
    for tier in ("lite", "full"):
        use = ("lite",) if tier == "lite" else ("lite", "full")
        items = [r for r in catalogue if r["tier"] in use]
        write(Path(out) / tier / "interests.tsv",
              ["kind", "key", "label", "weight", "class", "category", "tags", "min_age", "max_age"],
              [(r["kind"], r["key"], r["label"], r["weight"], r["class"], r["category"], r["tags"],
                r["min_age"], r["max_age"]) for r in items])
        used_classes = {r["class"] for r in items if r["class"]}
        used = {r["key"] for r in items} | {"class:" + c for c in used_classes}
        aff = [("class:" + t, d, v, f"{f:.2f}", basis) for t, d, v, f in sex_rows + age_rows + region_rows
               if t in used_classes]
        if tier == "full":
            aff += [("class:" + t, d, v, f"{f:.2f}", basis) for t, d, v, f in country_rows if t in used_classes]
        aff += [(target_of(r["target"]), r["dimension"], r["value"], r["factor"], r["basis"])
                for r in curated if r["tier"] in use and target_of(r["target"]) in used]
        dupes = [k for k, n in __import__("collections").Counter((a[0], a[1], a[2]) for a in aff).items() if n > 1]
        if dupes:
            raise SystemExit(f"{tier}: affinity rows defined twice: {dupes[:5]}")
        aff.sort(key=lambda r: (r[0], r[1], r[2]))
        write(Path(out) / tier / "affinity.tsv", ["target", "dimension", "value", "factor", "basis"], aff)
        write(Path(out) / tier / "age_bands.tsv", ["band", "min_age", "max_age"],
              [(b, lo, hi) for b, lo, hi, _ in BANDS])
        cc_set = independent if tier == "lite" else ccs
        write(Path(out) / tier / "regions.tsv", ["country_code", "region"],
              sorted((cc, subregion[cc]) for cc in cc_set if subregion.get(cc)))
        print(f"{tier}: {len(items)} items, {len(aff)} factors")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(*sys.argv[1:3])
