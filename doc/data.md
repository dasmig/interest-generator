# Data and Method

The data in `resources/` and how interest-generator uses it (rule 6 of the dasmig standard). For using the library, see the [usage guide](usage.md).

[TOC]

There are five tables. `religion.tsv` gives each country's religion shares. The other four describe hobbies and favourite film, game, book and music genres, and how likely each is for a person of a given sex, age and country.

**What is measured and what is curated.** The religion shares are measured (CIA World Factbook), with some estimates marked below. For hobbies and media, only the *sex, age and country differences in how much people do an activity class* are measured (Eurostat time-use surveys, Europe only). The catalogue itself (which hobbies and genres exist, their base popularity) and every genre-level preference are curated: no open dataset gives them. Each factor row says which in its `basis` column.

## Tiers

| Tier | Countries | Hobbies and genres | Affinity rows | Notes |
| --- | --- | --- | --- | --- |
| `lite` | 195 independent states | 56 hobbies, 12 genres per medium (104) | 651 | Religion by major group. Sex and age factors, and region factors for Europe |
| `full` | 248 countries and territories (all but Antarctica and the US Minor Outlying Islands) | 106 hobbies, 20-22 genres per medium (188) | 1,433 | Detailed religions, plus country factors and curated regional tastes |

## Religion

### `religion.tsv`, keyed by `country_code` and `religion`

| Column | Tier | Meaning | Source |
| --- | --- | --- | --- |
| `country_code` | both | ISO 3166-1 alpha-2 code (the key used by country-generator and biodata-generator) | Matched to the Factbook's GEC codes, see below |
| `religion` | both | Label. In `lite` it is the group name | World Factbook text, cleaned; curated rows as listed below |
| `group` | both | Major group: `christian`, `muslim`, `hindu`, `buddhist`, `jewish`, `unaffiliated` or `other` | Assigned by keyword from the label (`scripts/religion_groups.py`) |
| `weight` | both | Share of the population in percent; only the ratios matter | World Factbook, or curated |
| `year` | full | Survey or estimate year the Factbook gives; 0 when unknown | World Factbook |
| `basis` | full | `factbook`, `parent: XX`, or a note on a curated row | See below |

| Data | Source | Reliability |
| --- | --- | --- |
| Shares for 220 countries and territories | [CIA World Factbook](https://www.cia.gov/the-world-factbook/), "People and Society: Religions", as archived in [factbook/factbook.json](https://github.com/factbook/factbook.json) (CC0). The Factbook is a US government work and is in the public domain. The CIA closed the Factbook in February 2026, so this is a frozen copy | Survey years run from 2006 to 2024 and the sources differ by country (censuses, surveys, estimates). Some countries count affiliation by registration (Japan: Shinto 48.6% and Buddhism 46.4% add to 95%, with no "unaffiliated" row) |
| Curated rows (about 25 countries) | Hand-written in `resources/raw/religion_curated.tsv` | See below |
| 14 territories with no shares | The parent country's rows (`basis` = `parent: XX`) | A guess: Mayotte is about 98% Muslim, not like France. Antarctica and the US Minor Outlying Islands have no population and are left out |

#### Curated religion rows

`basis` says what each curated row is based on:

- `factbook` (CD, ZA, CN, PS, LB, VU): the Factbook gives numbers, but the text could not be parsed, or needed a fix. Palestine combines the West Bank and Gaza profiles by population (3.1 and 2.1 million). Lebanon splits the Druze out of the Muslim total. Vanuatu's Protestant share is 67.1%, the sum of the Factbook's own denominations, because its 39.9% total contradicts them.
- `approximate` (Eritrea, Sudan, Western Sahara, Saudi Arabia, Ukraine, Russia, North Korea, Guernsey, Greenland, Northern Mariana Islands, Saint-Martin, San Marino, Saint Kitts and Nevis): the Factbook lists groups without shares, or its Russian figures (2006) are incomplete. **These shares are rough estimates from general knowledge and public polling, not citations. They have not been checked against a source.** Replace them with a sourced figure when one is at hand.

#### Matching to ISO codes

The Factbook uses GEC (formerly FIPS) codes. `scripts/fetch_religion.py` matches each profile to country-generator's list by the country's common name, then its official name, then its internet domain. The US Virgin Islands are forced (`vq`), because they share a name with the British ones. The European Union profile is skipped.

## Hobbies and media

### `interests.tsv`, keyed by `kind` and `key`

| Column | Meaning | Source |
| --- | --- | --- |
| `kind` | `hobby`, `film`, `game`, `book` or `music` | - |
| `key`, `label` | Stable identifier and display name | Curated |
| `weight` | Base popularity, relative within its kind (0.2 to 1.6) | Curated: a judgement of how common each is among people in general, not a measurement |
| `class` | The activity class whose measured factors apply (`sport`, `reading`, `gaming`, ...); empty for cooking, baking and home brewing, which no time-use category covers | Curated mapping |
| `category` | Grouping for display and for other generators | Curated |
| `tags` | `;`-separated themes (`physical`, `social`, `calm`, `creative`, ...) for the game and for trait links; not used for drawing | Curated |
| `min_age`, `max_age` | Hard limits, such as 18 for clubbing and 12 for children's songs | Curated |

The catalogue is in `resources/raw/interests_curated.tsv` with a `tier` column saying which tier it belongs to. The genres follow common usage (film: action, comedy, drama...), not a formal taxonomy; Wikidata's genre lists (CC0) were not imported.

### `affinity.tsv`, keyed by `target`, `dimension` and `value`

A row is a multiplier (`factor`, `>= 0`, 1 is neutral) on an item's weight.

| Column | Meaning |
| --- | --- |
| `target` | An item `key`, or `class:` and an activity class (applies to every item of that class) |
| `dimension` | `sex` (`male`, `female`), `age_band` (a band of `age_bands.tsv`), `region` (a UN subregion from `regions.tsv`), `country` (an alpha-2 code) or `religion` (a religion group) |
| `value` | The dimension's value |
| `factor` | The multiplier |
| `basis` | `eurostat HETUS 2000/2010` for measured rows, otherwise `curated` and a note |

An item's weight for a person is `weight` times, for the item and for its class, the factor for the person's sex, age band, religion group, and country (or, if the target has no country row for that country, region). A missing row is 1.

### `age_bands.tsv` and `regions.tsv`

`age_bands.tsv` (key `band`, `min_age`, `max_age`): 5-12, 13-17, 18-24, 25-34, 35-49, 50-64, 65+. A person younger than the first band draws no hobbies or media. `regions.tsv` (key `country_code`, `region`) is country-generator's UN subregion for every country in the tier, built by `prepare_interests.py`.

### Measured factors

Source: Eurostat, [Harmonised European Time Use Surveys](https://ec.europa.eu/eurostat/web/time-use-surveys), dataset `tus_00age` (participation rate by sex, age group and main activity), 22 countries, waves 2000 and 2010. Eurostat's reuse policy allows reuse with attribution. Participation rate is the percentage of people who did the activity on a diary day.

| Class | Eurostat activity | Class | Eurostat activity |
| --- | --- | --- | --- |
| `sport` | AC6A Sports and outdoor activities | `socialising` | AC51A Visiting and feasts |
| `outdoors` | AC611 Walking and hiking | `culture` | AC52 Entertainment and culture |
| `handicraft` | AC33A Handicraft and textiles | `computing` | AC7A Computing |
| `gardening` | AC34A Gardening | `hobbyist` | AC7B Hobbies and games (not computing) |
| `pets` | AC343 Caring for pets | `gaming` | AC733 Computer games |
| `diy` | AC35 Construction and repairs | `reading` | AC812 Reading books |
| `volunteering` | AC41 Organisational work | `screen` | AC82 Television and video |
| `religious` | AC43 Participatory activities (mostly religious) | `music` | AC83 Radio and music |
| `study` | AC22 Free time study | | |

Per class: the `sex` factor is the mean participation of men (or women) divided by the mean of all; the `age_band` factor is the age group's mean divided by the mean of all (13-17 uses the 15-20 group, 25-34 and 35-49 both use 25-44, 65+ uses 65 and over). The `country` factor (full) is a country's participation over the mean of the countries, and the `region` factor is the mean of its countries'. Each country's latest wave is used. Factors are clipped to 0.2-5 (sex, age) or 0.4-2.5 (country, region), and those within 5% of 1 are left out. A class with a mean participation under 1% is skipped.

### Curated factors

`resources/raw/affinity_curated.tsv` (written by hand; each row's `basis` is `curated`):

- **Children aged 5-12.** The surveys start at age 15 (the 13-17 band uses the 15-20 group), so the class factors for 5-12 are written by hand.
- **Genre and hobby tastes by sex and age**, for example romance books and K-pop leaning female, war films and metal leaning male, hip hop and anime leaning young, opera and westerns leaning old. These are generalisations from public surveys and common knowledge, not fitted to a dataset.
- **Religion** (`religion` dimension): the `religious` class is higher for any religion than for the unaffiliated (0.15), and Islam lowers pubs and bars, home brewing and betting (0.15), which its teaching forbids and many Muslims still do. Gospel music leans Christian. Nothing else depends on religion in the base data; a game or mod can add more through `extend()`.
- **Region and country** (`full` only): sports and music tied to places, for example baseball in the US, Japan, Korea and the Caribbean, cricket in South Asia and Australia, rugby in New Zealand and South Africa, K-pop in Korea, country music in the US, skiing nearly absent in the tropics.

Outside Europe the measured sex and age differences are assumed to hold as they are; country and region factors for the rest of the world come only from the curated list above.

## Generation Model

1. The country is the one given, or one drawn uniformly from the countries with a positive total weight (`"country"` stream of the call seed). The data has no population, so pass a country chosen by country-generator to weight by population.
2. The religion is drawn by `weight` from that country's rows (`"religion"` stream). A row with weight 0 is loaded but never drawn.
3. For each kind (`"hobby"`, `"film"`, `"game"`, `"book"`, `"music"` streams): every item the person is old enough (and young enough) for gets its weight as above, then items are drawn one at a time without replacement. Fewer are returned if fewer have a positive weight.

An unknown sex or age applies no factor and no age limit.

## Rebuilding the Data

```bash
# Religion
python scripts/fetch_religion.py ../country-generator/resources/full/countries.tsv resources/raw/religion_factbook.tsv
python scripts/merge_religion.py resources/raw/religion_factbook.tsv resources/raw/religion_curated.tsv resources/raw/religion.tsv
python scripts/prepare_religion.py ../country-generator/resources/full/countries.tsv

# Hobbies and media
python scripts/fetch_timeuse.py resources/raw/timeuse_eurostat.tsv
python scripts/prepare_interests.py ../country-generator/resources/full/countries.tsv

make embed
```

- `fetch_religion.py` downloads the Factbook profiles and parses the shares. It prints what it could not parse (these are the countries that need curated rows). Ranges such as "15-20%" use the midpoint, and "<1%" counts as half a percent.
- `merge_religion.py` applies the curated rows over the Factbook rows, fills territories from their parent, assigns groups and reports any country whose shares do not add up to between 90 and 110.
- `prepare_religion.py` writes both religion tiers. `lite` takes the independent states from country-generator's table.
- `compare_religion.py` checks the shares against the Pew Research Center's 2020 estimates as republished by Our World in Data. Pew's data has no open licence, so it is only used to flag rows to look at; it is not part of the repository.
- `fetch_timeuse.py` downloads the Eurostat rates.
- `prepare_interests.py` computes the measured factors, combines them with the curated catalogue and factors, checks every curated target, region, country and band exists, and writes both tiers.

## Known Limitations

- **One snapshot.** Religion survey years differ by country and the Factbook will not be updated. The time-use surveys are from 2000 and 2010, before streaming and smartphones changed how much people game, read and watch.
- **Europe measured, the world assumed.** The sex and age differences come from 22 European countries (and Türkiye) and are applied everywhere. US data (ATUS) would be a second source, but the Bureau of Labor Statistics blocks scripted downloads, so it is not included.
- **Participation is not preference.** The surveys measure whether people did something on a diary day, which favours frequent daily activities (television) over occasional ones (concerts). Item weights are curated to balance this.
- **Genre tastes are curated generalisations.** They give plausible differences, not measured ones, and they say nothing about an individual.
- **Religion has no age or sex split.** The Factbook has none, so a young person and an old person of the same country draw from the same shares.
- **Classification differs by country.** The Factbook counts what each census counts: "unaffiliated" is 52% in China and 0% in Japan, where affiliation is by registration. Laos and Macau treat folk religion and "none" differently from other sources.
- **Small territories use guesses.** See the curated and parent religion rows above.
- **The detailed `full` religions are free text.** Labels are kept as the Factbook has them ("Roman Catholic", "Church of Norway", "Kimbanguist"), so the same church can appear under different names in different countries. Use `group` to compare.
- **Items are independent.** A person's hobbies do not influence their favourite films, nor does religion beyond the factors above. Their personality, job and wealth are not inputs.
