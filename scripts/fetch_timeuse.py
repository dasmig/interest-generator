"""Fetch Eurostat HETUS participation rates and write a raw TSV.

Dataset tus_00age: participation rate (% of people doing the activity on a
diary day) by sex, age group and main activity, 22 countries, waves 2000 and
2010. Eurostat's reuse policy allows reuse with attribution (about CC BY 4.0).

Output (resources/raw/timeuse_eurostat.tsv): country_code (ISO alpha-2),
wave, sex (T, M, F), age (Eurostat age group), activity (Eurostat code), rate.

Usage: python fetch_timeuse.py OUT_TSV
"""
import csv
import json
import sys
import urllib.request

URL = ("https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data/"
       "tus_00age?format=JSON&lang=EN&unit=PTP_RT")
ISO = {"EL": "GR", "UK": "GB"}  # Eurostat codes that differ from ISO alpha-2


def main(out_tsv):
    with urllib.request.urlopen(URL, timeout=120) as r:
        d = json.load(r)
    ids, sizes = d["id"], d["size"]
    cats = {k: sorted(d["dimension"][k]["category"]["index"].items(), key=lambda kv: kv[1])
            for k in ids}
    strides, s = {}, 1
    for k, n in zip(reversed(ids), reversed(sizes)):
        strides[k] = s
        s *= n
    rows = []
    for flat_s, value in d["value"].items():
        flat = int(flat_s)
        c = {}
        for k in ids:
            pos = (flat // strides[k]) % sizes[ids.index(k)]
            c[k] = cats[k][pos][0]
        rows.append((ISO.get(c["geo"], c["geo"]), c["time"], c["sex"], c["age"], c["acl00"], value))
    rows.sort()
    with open(out_tsv, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["country_code", "wave", "sex", "age", "activity", "rate"])
        w.writerows(rows)
    print(f"{len(rows)} rows, {len({r[0] for r in rows})} countries -> {out_tsv}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
