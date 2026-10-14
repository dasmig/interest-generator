# Usage Guide

This guide covers every feature of the interest-generator library. For a quick overview, see the [README](../README.md). For the full API reference, run `doxygen Doxyfile` from the repository root and open `doc/api/html/index.html`.

[TOC]

<!-- GitHub-rendered TOC (Doxygen uses [TOC] above instead) -->
<ul>
<li><a href="#quick-start">Quick Start</a></li>
<li><a href="#installation">Installation</a></li>
<li><a href="#loading-resources">Loading Resources</a></li>
<li><a href="#generating">Generating</a></li>
<li><a href="#seeding-and-deterministic-generation">Seeding and Deterministic Generation</a></li>
<li><a href="#extending-data">Extending Data</a></li>
<li><a href="#replay-and-json">Replay and JSON</a></li>
<li><a href="#compiled-in-data">Compiled-in Data</a></li>
<li><a href="#thread-safety">Thread Safety</a></li>
<li><a href="#error-reference">Error Reference</a></li>
</ul>

## Quick Start

```cpp
#include <dasmig/interestgen.hpp>
#include <iostream>

int main()
{
    auto& gen = dasmig::intg::instance();
        const dasmig::interest_request request{.country = "BR", .age = 34, .person_sex = dasmig::sex::female};
        auto v = gen.get_interest(request);
    std::cout << v.religion << ", " << v.hobbies.front().label << '\n';
}
```

## Installation

Copy `dasmig/interestgen.hpp`, `dasmig/core.hpp` and the `dasmig/core/` directory into your include path, keeping them together. Supply `resources/` next to the program, or define `DASMIG_INTEREST_EMBED` to compile the `lite` tier in.

## Loading Resources

`instance()` loads the `lite` tier on first use (or `full` if there is no `lite`), from the compiled-in copy or from the first of `resources/`, `../resources`, `interest-generator/resources` and `../interest-generator/resources` that has it.

```cpp
dasmig::intg gen;                       // an empty, independent instance
bool ok = gen.load(dasmig::dataset::lite); // false when the tier is not found
gen.load(std::filesystem::path{"path/to/resources/lite"}); // a directory
gen.has_data();       // religion data is loaded
gen.country_count();  // countries with religion shares
gen.item_count();     // hobbies and genres
gen.shares("US");     // the loaded religion shares of a country
```

Each `load` replaces the data and every layer loaded before. A tier is five tables (`religion`, `interests`, `affinity`, `age_bands`, `regions`); a directory with only `religion.tsv` still loads and then draws no hobbies or media. The `lite` tier has 195 countries and 104 hobbies and genres; `full` has 248 and 188, with the detailed religions. See [Data and Method](data.md).

## Generating

```cpp
const dasmig::interest_request request{.country = "BR", .age = 34, .person_sex = dasmig::sex::female};
auto v = gen.get_interest(request);

v.country_code;   // "BR"
v.religion;       // "Christian" (lite) or "Roman Catholic" (full)
v.religion_group; // "christian": comparable between tiers
v.hobbies;        // 3 items, each with key, label and category
v.films;          // 2 genres; likewise v.games, v.books, v.music
```

Generation has exactly two overloads: `get_interest(request)` and `get_interest(request, seed)`. Every per-call choice lives in `dasmig::interest_request`; it does not change the instance's configuration. Every field has a default:

| Field | Default | Meaning |
|-------|---------|---------|
| `country` | empty | ISO 3166-1 alpha-2 code in capitals (the key of country-generator). Empty draws a country uniformly from those loaded; the data has no population, so for a population-weighted world pass the country country-generator chose |
| `age` | unknown | Age in completed years (birth-generator's `age`). Unknown applies no age factor and no age limit. Younger than 5 draws no hobbies or media |
| `person_sex` | unknown | `dasmig::sex` (shared with biodata-generator and birth-generator). Unknown applies no sex factor |
| `hobbies` | 3 | How many hobbies |
| `media` | 2 | How many genres in each of film, games, books and music |

Use a request for every call, including the default case:

```cpp
auto a = gen.get_interest({.country = "BR"});            // age and sex unknown, default counts
auto b = gen.get_interest({});                          // a random country, default counts
auto c = gen.get_interest(request, std::uint64_t{1234}); // deterministic; does not touch the engine
auto d = gen.get_interest({.country = "BR"}, std::uint64_t{1234});
auto e = gen.get_interest({}, std::uint64_t{1234});
```

The country-only, seed-only and no-argument overloads have been removed. Data loading and layers still belong to the instance; generation options do not.

How a draw works:

- The religion comes from the country's shares, by the `weight` column; a row with weight 0 is loaded but never drawn.
- Each hobby and genre has a base `weight`, multiplied by factors for the person's sex, age band, country (or else region) and religion group, for the item and for its activity class. A missing factor is 1. Items are drawn without replacement, and only those the person is old enough (and young enough) for. If fewer than asked have a positive weight, fewer are returned.
- Country, religion and each kind of item use random streams of their own. Asking for more hobbies keeps the first ones the same; asking for no media leaves the hobbies unchanged; the religion does not depend on age, sex or counts.

## Seeding and Deterministic Generation

- `get_interest(request, seed)` gives the same result for the same seed, request and data, on every platform, independent of previous calls.
- `get_interest(request)` draws a fresh call seed from the instance's entropy-seeded random engine. There are no generator-level `seed()` or `unseed()` controls. For a reproducible sequence, supply an explicit seed for each request.
- `v.seed()` is the seed a result was generated from. Replay with `get_interest(request, v.seed())`, retaining the original request, including an empty country when one was drawn randomly.

## Extending Data

The shipped data is a base layer. Files with the same columns as the shipped tables can be loaded on top of it to add countries, religions, hobbies or genres, change shares or factors, or remove rows, without editing the shipped files. Each table has a key, and a row with the same key replaces the loaded row:

| `table` | File | Key | Required columns |
|---------|------|-----|------------------|
| `religion` | `religion.tsv` | `country_code`, `religion` | `group`, `weight` (optional: `year`, `basis`) |
| `interests` | `interests.tsv` | `kind`, `key` | `label`, `weight` (optional: `class`, `category`, `tags`, `min_age`, `max_age`) |
| `affinity` | `affinity.tsv` | `target`, `dimension`, `value` | `factor` (optional: `basis`) |
| `age_bands` | `age_bands.tsv` | `band` | `min_age`, `max_age` |
| `regions` | `regions.tsv` | `country_code` | `region` |

`kind` is `hobby`, `film`, `game`, `book` or `music`. An affinity `target` is an item `key`, or `class:` and an activity class (it then applies to every item of that class); `dimension` is `sex` (`male`, `female`), `age_band`, `region`, `country` or `religion` (a group such as `christian`).

```cpp
using table = dasmig::intg::table;

// Override mode (the default): a row replaces the loaded row with its key;
// new keys are added. A file or a directory with any of the table files.
gen.extend("mods/extra");
gen.extend("mods/interests.tsv");

// From text, for example supplied by a WebAssembly host: a new hobby,
// and a made-up country with its own religion.
gen.extend_from_string(table::interests,
    "kind\tkey\tlabel\tweight\nhobby\tfalconry\tFalconry\t0.3\n", "scenario");
gen.extend_from_string(table::religion,
    "country_code\treligion\tgroup\tweight\nXX\tSun Worship\tother\t2\n", "scenario");

// Make falconry popular with men in Kazakhstan.
gen.extend_from_string(table::affinity,
    "target\tdimension\tvalue\tfactor\nfalconry\tsex\tmale\t3\nfalconry\tcountry\tKZ\t8\n", "scenario");

// Remove rows: a layer needs only the keys and a disabled column.
gen.extend_from_string(table::religion, "country_code\treligion\tdisabled\nXX\tSun Worship\t1\n", "cleanup");

// Add mode refuses to touch keys that are already loaded.
gen.extend("mods/new.tsv", dasmig::core::layer_mode::add);
```

- Every value in a layer is checked before anything changes. A directory layer applies to all its tables or to none. A bad layer throws `dasmig::core::tsv_error` with the file, line and column, and the data stays as it was.
- Affinity rows whose target is not a loaded item or class are ignored, so removing an item needs no other change; `unresolved()` lists them to catch misspelt names.
- `layers(table)` lists what is loaded for a table: source, mode, rows added, replaced and removed, and warnings such as unknown columns.
- `load(...)` replaces the base and drops every layer.

## Replay and JSON

Every result records the seed and the data it came from:

```cpp
auto v = gen.get_interest({});
v.signature().seed; // the call seed
v.signature().data; // gen.data_hash() when it was generated
```

`to_json()` gives the result as JSON, the form the WebAssembly layer and the game consume. Seeds are strings because JavaScript numbers cannot hold every 64-bit value. `to_json(writer)` writes into an existing `dasmig::core::json_writer`, for composing larger documents.

## Compiled-in Data

Define `DASMIG_INTEREST_EMBED` before including the header to compile the `lite` tier into the program. `load(dataset::lite)` and `instance()` then use it and need no files. The compiled-in data has the same `data_hash()` as the file, so seeds replay the same either way.

`dasmig/interestgen_data.hpp` is generated from `resources/lite` by `make embed`; CI checks it is up to date.

## Thread Safety

Each `intg` instance is independent. Concurrent calls to `get_interest()` on the **same** instance require external synchronisation (e.g. a `std::mutex`).

## Error Reference

| Exception | When |
|-----------|------|
| `std::runtime_error` | Any generation call before data is loaded |
| `std::invalid_argument` | The country code has no data (codes are two capital letters), or every share of the country has weight 0, or no country has a positive weight (random country); `extend(path)` with a file that is not one of the table names |
| `dasmig::core::tsv_error` | A malformed table file or layer (message gives file, line and column); the data is unchanged |
