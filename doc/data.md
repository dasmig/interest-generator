# Data and Method

The data in `resources/` and how interest-generator uses it (rule 6 of the dasmig standard). For using the library, see the [usage guide](usage.md).

[TOC]

This version covers **religion**. Hobbies and favourite media (by age, sex and country) are not in the library yet.

## Tiers

| Tier | Countries | Rows | What it holds |
| --- | --- | --- | --- |
| `lite` | 195 | 695 | Independent states; the shares added up per major group (Christian, Muslim, Hindu, Buddhist, Jewish, Unaffiliated, Other) |
| `full` | 248 | 1,402 | Every country and territory except Antarctica and the US Minor Outlying Islands (no permanent population); the detailed religions (Roman Catholic, Sunni Muslim, Folk religion, ...) with survey year and basis |

## Columns

`religion.tsv`, keyed by `country_code` and `religion`:

| Column | Tier | Meaning | Source |
| --- | --- | --- | --- |
| `country_code` | both | ISO 3166-1 alpha-2 code (the key used by country-generator and biodata-generator) | Matched to the Factbook's GEC codes, see below |
| `religion` | both | Label. In `lite` it is the group name | World Factbook text, cleaned; curated rows as listed below |
| `group` | both | Major group: `christian`, `muslim`, `hindu`, `buddhist`, `jewish`, `unaffiliated` or `other` | Assigned by keyword from the label (`scripts/religion_groups.py`) |
| `weight` | both | Share of the population in percent; only the ratios matter | World Factbook, or curated |
| `year` | full | Survey or estimate year the Factbook gives; 0 when unknown | World Factbook |
| `basis` | full | `factbook`, `parent: XX`, or a note on a curated row | See below |

## Sources

| Data | Source | Reliability |
| --- | --- | --- |
| Shares for 220 countries and territories | [CIA World Factbook](https://www.cia.gov/the-world-factbook/), "People and Society: Religions", as archived in [factbook/factbook.json](https://github.com/factbook/factbook.json) (CC0). The Factbook is a US government work and is in the public domain. The CIA closed the Factbook in February 2026, so this is a frozen copy | Survey years run from 2006 to 2024 and the sources differ by country (censuses, surveys, estimates). Some countries count affiliation by registration (Japan: Shinto 48.6% and Buddhism 46.4% add to 95%, with no "unaffiliated" row) |
| Curated rows (about 25 countries) | Hand-written in `resources/raw/religion_curated.tsv` | See below |
| 14 territories with no shares | The parent country's rows (`basis` = `parent: XX`) | A guess: Mayotte is about 98% Muslim, not like France. Antarctica and the US Minor Outlying Islands have no population and are left out |

### Curated rows

`basis` says what each curated row is based on:

- `factbook` (CD, ZA, CN, PS, LB, VU): the Factbook gives numbers, but the text could not be parsed, or needed a fix. Palestine combines the West Bank and Gaza profiles by population (3.1 and 2.1 million). Lebanon splits the Druze out of the Muslim total. Vanuatu's Protestant share is 67.1%, the sum of the Factbook's own denominations, because its 39.9% total contradicts them.
- `approximate` (Eritrea, Sudan, Western Sahara, Saudi Arabia, Ukraine, Russia, North Korea, Guernsey, Greenland, Northern Mariana Islands, Saint-Martin, San Marino, Saint Kitts and Nevis): the Factbook lists groups without shares, or its Russian figures (2006) are incomplete. **These shares are rough estimates from general knowledge and public polling, not citations. They have not been checked against a source.** Replace them with a sourced figure when one is at hand.

### Matching to ISO codes

The Factbook uses GEC (formerly FIPS) codes. `scripts/fetch_religion.py` matches each profile to country-generator's list by the country's common name, then its official name, then its internet domain. The US Virgin Islands are forced (`vq`), because they share a name with the British ones. The European Union profile is skipped.

## Generation Model

1. The country is the one given, or one drawn uniformly from the countries with a positive total weight (`"country"` stream of the call seed). The data has no population, so pass a country chosen by country-generator to weight by population.
2. The religion is drawn by `weight` from that country's rows (`"religion"` stream). A row with weight 0 is loaded but never drawn.

A country's weights need not add up to 100: they are ratios.

## Rebuilding the Data

```bash
python scripts/fetch_religion.py ../country-generator/resources/full/countries.tsv resources/raw/religion_factbook.tsv
python scripts/merge_religion.py resources/raw/religion_factbook.tsv resources/raw/religion_curated.tsv resources/raw/religion.tsv
python scripts/prepare_religion.py ../country-generator/resources/full/countries.tsv
make embed
```

- `fetch_religion.py` downloads the Factbook profiles and parses the shares. It prints what it could not parse (these are the countries that need curated rows). Ranges such as "15-20%" use the midpoint, and "<1%" counts as half a percent.
- `merge_religion.py` applies the curated rows over the Factbook rows, fills territories from their parent, assigns groups and reports any country whose shares do not add up to between 90 and 110.
- `prepare_religion.py` writes both tiers. `lite` takes the independent states from country-generator's table.
- `compare_religion.py` checks the shares against the Pew Research Center's 2020 estimates as republished by Our World in Data. Pew's data has no open licence, so it is only used to flag rows to look at; it is not part of the repository.

## Known Limitations

- **One snapshot.** Survey years differ by country and the Factbook will not be updated.
- **Religion only, and not by age or sex.** The Factbook has no age or sex split, so a young person and an old person of the same country draw from the same shares.
- **Classification differs by country.** The Factbook counts what each census counts: "unaffiliated" is 52% in China and 0% in Japan, where affiliation is by registration. Laos and Macau treat folk religion and "none" differently from other sources.
- **Small territories use guesses.** See the curated and parent rows above.
- **The detailed `full` religions are free text.** Labels are kept as the Factbook has them ("Roman Catholic", "Church of Norway", "Kimbanguist"), so the same church can appear under different names in different countries. Use `group` to compare.
- **Religion is independent of everything else.** It does not depend on ethnicity, region within a country, or the person's other traits.
