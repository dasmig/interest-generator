# Design: Hobbies and Favourite Media

Status: implemented (2026-10-12); the data and the code are described in [data.md](data.md) and [usage.md](usage.md). Where they differ from this page, they are right: this page records the reasoning. It extends the religion model in [data.md](data.md) and follows the same rules: layered TSV tables, one random stream per field, every table described in `data.md`, derived data rebuilt by a script in `scripts/`.

## What it must do

Given a person's **country, age and sex**, draw:

- a few **hobbies** (default 3),
- a few **favourite genres** in each of **film, games, books and music** (default 2 each).

The persona pipeline gets country, age and sex from country-generator, birth-generator and biodata-generator, and passes them in. All three are optional, as the country already is: a missing one draws from the unconditioned weights.

## Inputs and result

```cpp
struct interest_request
{
    std::string_view country;       // ISO alpha-2; empty = random country, as today
    std::optional<std::uint8_t> age; // completed years
    std::optional<sex> person_sex;   // dasmig::sex, shared with biodata and birth (DASMIG_SEX_DEFINED)
    std::size_t hobbies = 3;
    std::size_t media = 2;           // genres per medium
};
```

The result gains `std::vector<item> hobbies` and `film`, `game`, `book`, `music` (each `item` is `key`, `label`, `category`). JSON adds the same five arrays. Existing religion fields and their random streams do not change, so religion known answers stay valid.

## The problem with one big table

Country × age band × sex × item would be about 200 × 8 × 2 × 250 rows, and nobody can source or curate that. No open dataset gives genre preference by age, sex and country. So the preference is **factorised**: an item's weight for a person is

> base weight × sex factor × age-band factor × (country factor, or else region factor) × religion factor

with a missing factor meaning 1. This is the same shape as `trait-generator` (a base weight multiplied by adjustments), so the core's weighted choice and without-replacement draw apply unchanged.

## Tables

All keyed and layerable like the rest (`extend`, `disabled`, hashes in the signature).

**`interests.tsv`**, key `kind` + `key`:

| Column | Meaning |
| --- | --- |
| `kind` | `hobby`, `film`, `game`, `book` or `music` |
| `key`, `label` | Stable identifier and display name |
| `weight` | Base popularity, relative within its kind |
| `class` | The activity class used by the time-use factors (`sport`, `reading`, `gaming`, `crafts`, `outdoors`, `socialising`, ...). Optional for media |
| `category` | Grouping for display and for other generators |
| `tags` | `;`-separated themes, for the game and for trait links |
| `min_age`, `max_age` | Hard limits (no weight for a seven-year-old's gambling); optional |

**`affinity.tsv`**, key `target` + `dimension` + `value` (a class target is written `class:<name>`, so it cannot be mistaken for an item with the same name):

| Column | Meaning |
| --- | --- |
| `target` | An item `key`, or a `class` (a class factor applies to every item of that class) |
| `dimension` | `sex` (`male`, `female`), `age_band`, `region`, `country` or `religion` (a religion group) |
| `value` | The dimension's value, such as `female`, `18-24`, `Southern Europe`, `JP`, `muslim` |
| `factor` | Multiplier `>= 0`; 1 is neutral |
| `basis` | `atus`, `hetus`, `eurostat`, `curated: <what it is based on>` |

An item's factor in a dimension is its own row times its class row. A `country` row replaces the `region` row for that country; the rest multiply.

**`age_bands.tsv`**, key `band`, with `min_age` and `max_age`: 5-12, 13-17, 18-24, 25-34, 35-49, 50-64, 65+. Ages under 5 draw nothing. Children use the same factors where the data allows (see decision 3). Bands are data, so a game can change them.

**`regions.tsv`**, key `country_code`, with `region`: the UN subregion from country-generator (22 values), built by a script. A country with no row has no region factor.

## Generation

Per kind, a stream of its own (`"hobby"`, `"film"`, ...): compute each eligible item's weight for the person, then draw `n` without replacement (the trait-generator pattern). Eligible means within `min_age`/`max_age`. If fewer than `n` items have a positive weight, return those. Adding a medium later does not move any existing draw.

Religion as a dimension lets the pipeline pass the religion group already drawn (see decision 4 for what ships).

## Where the numbers come from

| Layer | Source | Quality |
| --- | --- | --- |
| Item lists (about 100 hobbies, 20-30 genres per medium) | Curated. Genre names can be checked against Wikidata (CC0) for coverage; hobbies have no taxonomy | Design choice |
| Sex and age factors for activity classes | US ATUS (public domain): minutes and participation by sex and age; Eurostat HETUS (Eurostat reuse, about CC BY 4.0): the same for about 15 European countries. A factor is the group's rate divided by the overall rate, clipped to 0.2-5 | Measured for the class, then assigned to a hobby by hand |
| Country and region factors | HETUS and Eurostat cultural participation for Europe; elsewhere curated at region level | Europe measured, the rest curated |
| Media genre factors | Curated from published surveys (Ofcom, Nielsen and similar), each row's `basis` says which | The weakest layer: stated as curated |
| Item-specific overrides | Curated | Design choice |

Excluded for licence reasons: IMDb datasets, TMDB, IGDB, Steam, Last.fm and Spotify; MusicBrainz tags and ratings.

Because the class factors come from time-use data, the work is: about 15 classes × 2 sexes × 7 age bands from ATUS, a mapping of each hobby to a class, and the curated media rows. That is a few hundred measured rows plus a few hundred curated ones, not tens of thousands.

## Tiers

`lite`: about 60 hobbies, 12 genres per medium, factors for sex and age band and for region only. `full`: the whole catalogue, with country rows where HETUS provides them.

## Decisions (2026-10-12)

1. **One `interests.tsv` for all five kinds.**
2. **Class factors with item overrides.**
3. **Children get hobbies and media.** The bands stay 5-12, 13-17, and so on. ATUS only covers ages 15 and up, so the 13-17 band uses its 15-19 group, and the 5-12 band is **curated** (`basis` says so), as are all media factors for under-18s. Eurostat HETUS covers ages 10 and up in some countries and can replace the 5-12 guesses for Europe.
4. **Religion is a dimension in the schema and code, but the shipped factors are few.** No open source breaks hobbies down by religion, a character's religion here is only a country-level share (not how observant they are), and curated "group X likes Y" factors tend to be stereotypes. The base data therefore carries religion factors only for the religious-practice class (ATUS "religious and spiritual activities": high for any religious group, near zero for the unaffiliated). Further factors are the job of layers (`extend()`) from the game or a mod. The library uses the religion group it drew itself; the request does not take one.
5. **Defaults:** 3 hobbies and 2 genres per medium.

## Build order

1. Get the ATUS tables and write the class factors (`scripts/prepare_timeuse.py`).
2. Write the hobby catalogue and the class mapping, then the genre catalogues.
3. Implement `interests.tsv`, `affinity.tsv`, `age_bands.tsv` and `regions.tsv` in the header, with tests on small in-memory worlds first (as for religion), then the known answers.
4. Add HETUS for Europe and the curated media factors.
5. Document in `data.md`, including every curated row's basis.
