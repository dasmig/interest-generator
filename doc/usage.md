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
    std::cout << gen.get_interest("BR") << '\n'; // a Brazilian's religion
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
gen.has_data();
gen.country_count();
gen.shares("US"); // the loaded religion shares of a country
```

Each `load` replaces the data and every layer loaded before. The `lite` tier has 195 countries with seven major groups each; `full` has 248 with the detailed religions. See [Data and Method](data.md).

## Generating

```cpp
auto v = gen.get_interest("BR");                 // a country, seed from the instance engine
auto w = gen.get_interest("BR", std::uint64_t{1234}); // deterministic; does not touch the engine
auto x = gen.get_interest();                     // a random country
auto y = gen.get_interest(std::uint64_t{1234});  // a random country, deterministic
v.country_code;   // "BR"
v.religion;       // "Christian" (lite) or "Roman Catholic" (full)
v.religion_group; // "christian": comparable between tiers
```

- The country is an ISO 3166-1 alpha-2 code in capitals, the same key as country-generator.
- A religion is drawn by the `weight` column of the country's rows. A row with weight 0 is loaded but never drawn.
- Without a country, one is drawn uniformly from the countries loaded. The data has no population, so for a population-weighted world pass the country that country-generator chose.
- The country and the religion use separate random streams. Asking for the country a seed would have drawn gives the same religion as asking for no country.

## Seeding and Deterministic Generation

- `get_interest(seed)` and `get_interest(country, seed)` give the same result for the same seed, country and data, on every platform.
- `seed(s)` makes the instance engine deterministic, so a sequence of `get_interest()` calls repeats; `unseed()` reseeds it from the system.
- `v.seed()` is the seed a result was generated from: `get_interest(v.country_code, v.seed())` replays it.

## Extending Data

The shipped data is a base layer. Files with the same columns as `religion.tsv` can be loaded on top of it to add countries or religions, change shares or remove rows, without editing the shipped file. Rows are keyed by `country_code` and `religion`; `group` and `weight` are required, `year` and `basis` are optional.

```cpp
// Override mode (the default): a row replaces the loaded row with its key;
// new keys are added.
gen.extend("mods/extra.tsv");

// From text, for example supplied by a WebAssembly host: a made-up country
// (the code is two capital letters) with its own religion.
gen.extend_from_string("country_code\treligion\tgroup\tweight\nXX\tSun Worship\tother\t2\n", "scenario");

// To change a country's mix, set the shares you want. Rows you leave out stay.
// Remove rows: a layer needs only the keys and a disabled column.
gen.extend_from_string("country_code\treligion\tdisabled\nXX\tSun Worship\t1\n", "cleanup");

// Add mode refuses to touch keys that are already loaded.
gen.extend("mods/new.tsv", dasmig::core::layer_mode::add);
```

- Every value in a layer is checked before anything changes. A bad layer throws `dasmig::core::tsv_error` with the file, line and column, and the data stays as it was.
- `layers()` lists what is loaded: source, mode, rows added, replaced and removed, and warnings such as unknown columns.
- `load(...)` replaces the base and drops every layer.

## Replay and JSON

Every result records the seed and the data it came from:

```cpp
auto v = gen.get_interest();
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
| `std::invalid_argument` | The country code has no data (codes are two capital letters), or every share of the country has weight 0, or no country has a positive weight (random country) |
| `dasmig::core::tsv_error` | A malformed `religion.tsv` or layer (message gives file, line and column); the data is unchanged |
