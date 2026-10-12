"""Fetch religion shares from the World Factbook archive and write a raw TSV.

The CIA closed the Factbook in Feb 2026; factbook/factbook.json (CC0 mirror of
the public-domain text) is the frozen copy. Profiles use GEC codes, so each is
matched to ISO by internet TLD, then by name, against country-generator's
countries.tsv. Unmatched profiles are reported, not guessed.

Output (resources/raw/religion_factbook.tsv): cca2, religion, share (percent),
year, raw (the original text). Parsing is deliberately conservative: entries
that are not "<name> <number>%" (e.g. "less than 1%", "NA") are kept in `raw`
only and listed in the report for curation.

Usage: python fetch_religion.py COUNTRIES_TSV OUT_TSV
"""
import csv
import html
import json
import re
import sys
import urllib.request

API = "https://api.github.com/repos/factbook/factbook.json/git/trees/master?recursive=1"
RAW = "https://raw.githubusercontent.com/factbook/factbook.json/master/"
# GEC profiles whose name/TLD match the wrong country
FORCE = {"vq": "VI"}
SKIP = {"meta", "world", "oceans", "antarctica"}
ENTRY = re.compile(r"^(.*?)\s+(<)?\s*(\d*\.?\d+)(?:\s*-\s*(\d*\.?\d+))?\s*%?\)?$")
YEAR = re.compile(r"\((\d{4})(?:[^)]*)\)")


def get(url):
    with urllib.request.urlopen(url, timeout=60) as r:
        return r.read()


def load_countries(path):
    by_tld, by_name = {}, {}
    with open(path, encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            by_tld[row["tld"].lower()] = row["cca2"]
            by_name[row["name_common"].lower()] = row["cca2"]
            # official names are only a fallback: BVI's is "Virgin Islands"
            by_name.setdefault(row["name_official"].lower(), row["cca2"])
    return by_tld, by_name


def parse(text):
    text = html.unescape(text).replace(" ", " ")
    text = text.split("<br>")[0]  # some profiles list a second survey; keep the first
    year = YEAR.search(text)
    body = re.sub(r"\([^)]*\)", "", text)  # drop "(2022 est.)" and notes
    body = re.sub(r"([A-Za-z]),\s*(?=<?\s*\d)", r"\1 ", body)  # "Muslim, 1.3%" -> "Muslim 1.3%"
    body = re.sub(r"%\.?\s+(?=[A-Za-z])", "%, ", body)  # "58.6% Roman Catholic" lacks a comma
    body = re.split(r";\s*(?:note|less than)", body)[0]  # drop trailing notes
    rows, rest = [], []
    for part in body.split(","):
        m = ENTRY.match(part.strip())
        if m:
            share = float(m.group(3))
            if m.group(4):  # range "15-20%" -> midpoint
                share = (share + float(m.group(4))) / 2
            rows.append((m.group(1).strip(), share / 2 if m.group(2) else share))  # "<1%" -> half
        elif part.strip():
            rest.append(part.strip())
    return rows, rest, year.group(1) if year else ""


def main(countries_tsv, out_tsv):
    by_tld, by_name = load_countries(countries_tsv)
    tree = json.loads(get(API))["tree"]
    paths = [t["path"] for t in tree
             if t["path"].endswith(".json") and "/" in t["path"]
             and t["path"].split("/")[0] not in SKIP]
    out, unmatched, unparsed, dupes, matched = [], [], [], [], {}
    for p in paths:
        d = json.loads(get(RAW + p))
        rel = d.get("People and Society", {}).get("Religions", {}).get("text")
        if not rel:
            continue
        tld = d.get("Communications", {}).get("Internet country code", {}).get("text", "")
        name = d.get("Government", {}).get("Country name", {}).get("conventional short form", {}).get("text", "")
        # name first: territories share TLDs (.us is the US and its outlying islands)
        cca2 = FORCE.get(p.rsplit("/", 1)[1][:-5]) or by_name.get(name.lower()) or by_tld.get(tld.split()[0].lower() if tld else "")
        if not cca2:
            unmatched.append((p, name, tld))
            continue
        rows, rest, year = parse(rel)
        if cca2 in matched:
            dupes.append((cca2, matched[cca2], p))
        matched[cca2] = p
        if not rows or rest:
            unparsed.append((cca2, rest or [rel]))
        for r, s in rows:
            out.append((cca2, r, s, year, rel))
    out.sort()
    with open(out_tsv, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["cca2", "religion", "share", "year", "raw"])
        w.writerows(out)
    print(f"{len({o[0] for o in out})} countries, {len(out)} rows -> {out_tsv}")
    print(f"unmatched profiles ({len(unmatched)}):", *unmatched, sep="\n  ")
    print(f"two profiles for one country ({len(dupes)}):", *dupes, sep=chr(10) + "  ")
    print(f"needs curation ({len(unparsed)}):", *unparsed, sep="\n  ")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(*sys.argv[1:])
