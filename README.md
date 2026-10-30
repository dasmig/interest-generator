# Interest Generator for C++

> **Requires C++23** (e.g., `-std=c++23` for GCC/Clang, `/std:c++latest` for MSVC).

[![Interest Generator for C++](https://raw.githubusercontent.com/dasmig/interest-generator/main/doc/interest-generator.png)](https://github.com/dasmig/interest-generator/releases)

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/dasmig/interest-generator/main/LICENSE.MIT)
[![CI](https://github.com/dasmig/interest-generator/actions/workflows/ci.yml/badge.svg)](https://github.com/dasmig/interest-generator/actions/workflows/ci.yml)
[![GitHub Releases](https://img.shields.io/github/release/dasmig/interest-generator.svg)](https://github.com/dasmig/interest-generator/releases)
[![C++23](https://img.shields.io/badge/standard-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Header-only](https://img.shields.io/badge/type-header--only-green.svg)](https://github.com/dasmig/interest-generator#integration)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue.svg)](https://dasmig.github.io/interest-generator/)

**[API Reference](https://dasmig.github.io/interest-generator/)** · **[Usage Guide](doc/usage.md)** · **[Data and Method](doc/data.md)** · **[Releases](https://github.com/dasmig/interest-generator/releases)**

Religion, hobbies and favourite media by country, age and sex. A call draws a **religion** from the shares of a country, a few **hobbies**, and favourite **film, game, book and music genres**, shaped by the person's sex, age, country and religion (195 countries in `lite`, 248 with detail in `full`).

## Features

- **Country-based Religion**. Each country's shares come from the CIA World Factbook, with hand-written rows where it has none (see [Data and Method](doc/data.md) for what is estimated).

- **Hobbies and Media by Sex, Age and Country**. An item's weight is multiplied by factors for the person's sex, age band, country or region, and religion. The sex, age and country differences in activities are measured (Eurostat time-use surveys, Europe), the catalogue and the genre tastes are curated, and each factor row says which.

- **Request-First Generation**. All per-call choices live in `interest_request`; use `get_interest(request)` for random generation or `get_interest(request, seed)` for reproducible results. `interest::seed()` records the seed for replay.

- **Deterministic Seeding**. `get_interest(request, seed)` is a pure function of the request, the seed and the data, and `interest::seed()` replays a previous result.

- **Batch Generation**. Generate thousands of results from one seed, on several threads with the same output for any thread count, optionally unique by a key, and stream them as JSON Lines or CSV (see the usage guide).

- **Identical Everywhere**. A seed gives the same result on every compiler, standard library and CPU (GCC, Clang, MSVC, Emscripten; x86-64 and ARM64), checked by known-answer tests.

- **Extensible Data**. Load your own TSV layers on top of the shipped data to add, change or remove entries (`extend()`), checked before they are applied.

- **JSON and Replay**. `to_json()` on every result, and a `signature()` recording the seed and the data it came from.

- **Compiled-in Data**. Define `DASMIG_INTEREST_EMBED` to build the `lite` tier into the program, for WebAssembly and other builds without files.

- **Multi-Instance Support**. Construct independent `intg` instances with their own data and random engine.

## Integration

[`interestgen.hpp`](https://github.com/dasmig/interest-generator/blob/main/dasmig/interestgen.hpp) is the library [released here](https://github.com/dasmig/interest-generator/releases). Keep [`core.hpp`](https://github.com/dasmig/interest-generator/blob/main/dasmig/core.hpp) and the [`core/`](https://github.com/dasmig/interest-generator/tree/main/dasmig/core) directory ([dasmig-core](https://github.com/dasmig/dasmig-core)) in the same directory. Add

```cpp
#include <dasmig/interestgen.hpp>
```

to the files you want to use it in, and supply the [`resources`](https://github.com/dasmig/interest-generator/tree/main/resources) folder (or define `DASMIG_INTEREST_EMBED`).

## Usage

```cpp
#include <dasmig/interestgen.hpp>
#include <iostream>

auto& gen = dasmig::intg::instance(); // loads the lite tier

const dasmig::interest_request request{.country = "BR", .age = 34, .person_sex = dasmig::sex::female};
auto v = gen.get_interest(request);
std::cout << v << '\n';           // the religion, e.g. "Christian"
for (const auto& h : v.hobbies) std::cout << h.label << '\n'; // also films, games, books, music
std::cout << v.to_json() << '\n'; // {"country_code":"BR","religion":...,"hobbies":[...],...,"signature":{...}}

// The same seed and request give the same result everywhere.
auto again = gen.get_interest(request, v.seed());
```

Use `get_interest({})` for a random country, unknown age and sex, three hobbies and two genres per medium. Generation has no mutable instance defaults or generator-level seeding controls; data loading and layers remain instance-level. The old country-only, seed-only and no-argument overloads have been removed.

See the [usage guide](doc/usage.md) for everything else.

## Building

```bash
make            # example
make test       # tests
make bench      # benchmarks, results in build/bench.tsv
make test-embed # tests with the lite tier compiled in
make coverage   # code coverage
make lint       # clang-tidy
make docs       # API docs
make embed      # regenerate dasmig/interestgen_data.hpp from resources/lite
```

## Compiler Support

Tested with:
- Clang 18+ (`-std=c++23`)
- GCC 14+ (`-std=c++23`)
- MSVC 19.38+ (`/std:c++latest`)
- Emscripten 3.1.74

## Dependencies

| Dependency | Version | Bundled | Purpose |
|-----------|---------|---------|---------|
| [dasmig-core](https://github.com/dasmig/dasmig-core) | 1.4 | Yes (`core.hpp`, `core/`) | Platform-independent random engine and distributions, data layers, JSON |
| [Catch2](https://github.com/catchorg/Catch2) | 3.x | Yes (amalgamated) | Unit testing |

## Related Libraries

| Library | Description |
|---------|-------------|
| [address-generator](https://github.com/dasmig/address-generator) | Postal addresses with real streets by place |
| [biodata-generator](https://github.com/dasmig/biodata-generator) | Procedural human physical characteristics |
| [birth-generator](https://github.com/dasmig/birth-generator) | Demographically plausible birthdays |
| [city-generator](https://github.com/dasmig/city-generator) | Weighted city selection by population |
| [country-generator](https://github.com/dasmig/country-generator) | Weighted country selection by population |
| [device-generator](https://github.com/dasmig/device-generator) | Devices, browsers, user agents and IP addresses that fit together |
| [email-generator](https://github.com/dasmig/email-generator) | Email addresses in each country's style |
| [entity-generator](https://github.com/dasmig/entity-generator) | ECS-based entity generation |
| [name-generator](https://github.com/dasmig/name-generator) | Culturally appropriate full names |
| [nickname-generator](https://github.com/dasmig/nickname-generator) | Gamer-style nicknames |
| [occupation-generator](https://github.com/dasmig/occupation-generator) | Employment status, education and occupation by country, age and sex |
| [persona-generator](https://github.com/dasmig/persona-generator) | Complete characters composed from the other generators |
| [phone-generator](https://github.com/dasmig/phone-generator) | Phone numbers valid in each country's numbering plan |
| [relationship-generator](https://github.com/dasmig/relationship-generator) | Families around a person: parents, siblings, partners and children |
| [trait-generator](https://github.com/dasmig/trait-generator) | Personality traits with conflicts, synergies and Big Five axes |
| [user-generator](https://github.com/dasmig/user-generator) | Complete user records: account, username, email, phone and address |

## License

This library is released under the [MIT License](https://opensource.org/licenses/MIT).

```
MIT License

Copyright (c) 2026 Diego Dasso Migotto
```
