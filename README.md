# Interest Generator for C++

> **Requires C++23** (e.g., `-std=c++23` for GCC/Clang, `/std:c++latest` for MSVC).

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/dasmig/interest-generator/main/LICENSE.MIT)
[![CI](https://github.com/dasmig/interest-generator/actions/workflows/ci.yml/badge.svg)](https://github.com/dasmig/interest-generator/actions/workflows/ci.yml)
[![GitHub Releases](https://img.shields.io/github/release/dasmig/interest-generator.svg)](https://github.com/dasmig/interest-generator/releases)
[![C++23](https://img.shields.io/badge/standard-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Header-only](https://img.shields.io/badge/type-header--only-green.svg)](https://github.com/dasmig/interest-generator#integration)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue.svg)](https://dasmig.github.io/interest-generator/)

**[API Reference](https://dasmig.github.io/interest-generator/)** · **[Usage Guide](doc/usage.md)** · **[Data and Method](doc/data.md)** · **[Releases](https://github.com/dasmig/interest-generator/releases)**

Religion, hobbies and favourite media by country, age and sex. This version draws a **religion** from the shares of a country (195 countries in `lite`, 248 with detail in `full`); hobbies and media are planned.

## Features

- **Country-based Religion**. Each country's shares come from the CIA World Factbook, with hand-written rows where it has none (see [Data and Method](doc/data.md) for what is estimated).

- **Deterministic Seeding**. Per-call `get_interest(seed)` for reproducible results, generator-level `seed()` / `unseed()` for deterministic sequences, and `interest::seed()` for replaying a previous generation.

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

auto v = gen.get_interest("BR");  // a Brazilian; omit the code for a random country
std::cout << v << '\n';           // the religion, e.g. "Christian"
std::cout << v.to_json() << '\n'; // {"country":"BR","religion":...,"religion_group":...,"signature":{...}}

// The same seed and country give the same result everywhere.
auto again = gen.get_interest("BR", v.seed());
```

See the [usage guide](doc/usage.md) for everything else.

## Building

```bash
make            # example
make test       # tests
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
| [dasmig-core](https://github.com/dasmig/dasmig-core) | 1.0 | Yes (`core.hpp`, `core/`) | Platform-independent random engine and distributions, data layers, JSON |
| [Catch2](https://github.com/catchorg/Catch2) | 3.x | Yes (amalgamated) | Unit testing |

## License

<img align="right" src="https://opensource.org/trademarks/opensource/OSI-Approved-License-100x137.png">

This library is released under the [MIT License](https://opensource.org/licenses/MIT).

```
MIT License

Copyright (c) 2026 Diego Dasso Migotto
```
