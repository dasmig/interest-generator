// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Diego Dasso Migotto

#ifndef DASMIG_INTERESTGEN_HPP
#define DASMIG_INTERESTGEN_HPP

#include "core.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef DASMIG_INTEREST_EMBED
#include "interestgen_data.hpp"
#endif

/// @file interestgen.hpp
/// @brief Interest Generator library — religion, hobbies and favourite media by country, age and sex for C++23.
/// @author Diego Dasso Migotto (diegomigotto at hotmail dot com)
/// @see See doc/usage.md for the narrative tutorial.

DASMIG_CORE_REQUIRE(1, 0);

namespace dasmig
{

#ifndef DASMIG_SEX_DEFINED
#define DASMIG_SEX_DEFINED
/// @brief Biological sex.
enum class sex : std::uint8_t
{
    male,  ///< Male.
    female ///< Female.
};
#endif

/// @brief One hobby or favourite genre in a result.
struct interest_item
{
    std::string key;      ///< Catalogue key (the `key` column of `interests.tsv`).
    std::string label;    ///< Display label.
    std::string category; ///< Grouping such as "sport" or "film"; may be empty.
};

/// @brief Per-call generation choices. Every field has a default.
///
/// Pass an empty request for a random country, unknown age and sex,
/// three hobbies and two genres per medium. Requests do not change
/// the generator's configuration.
struct interest_request
{
    std::string_view country{};        ///< ISO 3166-1 alpha-2 code; empty draws a country (uniform over those loaded).
    std::optional<std::uint8_t> age{}; ///< Age in completed years; unknown applies no age factor or age limit.
    std::optional<sex> person_sex{};   ///< Sex; unknown applies no sex factor.
    std::size_t hobbies{3};            ///< How many hobbies to draw.
    std::size_t media{2};              ///< How many genres to draw in each of film, games, books and music.
};

/// @brief Return type for interest generation.
///
/// Supports implicit conversion to std::string (the religion), streaming
/// via operator<< and JSON output via to_json().
class interest
{
  public:
    std::string country_code;   ///< ISO 3166-1 alpha-2 code of the country drawn from.
    std::string religion;       ///< Religion label (the `religion` column).
    std::string religion_group; ///< Major group of the religion (the `group` column).
    std::vector<interest_item> hobbies; ///< Hobbies, in the order they were drawn.
    std::vector<interest_item> films;   ///< Favourite film genres.
    std::vector<interest_item> games;   ///< Favourite game genres.
    std::vector<interest_item> books;   ///< Favourite book genres.
    std::vector<interest_item> music;   ///< Favourite music genres.

    /// @brief Retrieve the random seed used to generate this interest.
    /// @return The call seed; pass it, with the same request, to the seeded
    ///         get_interest() overload to generate the same interest again.
    [[nodiscard]] std::uint64_t seed() const { return _signature.seed; }

    /// @brief The seed and the data this interest was generated from.
    /// @return The replay signature; `data` is intg::data_hash() at the
    ///         time of generation.
    [[nodiscard]] const core::seed_signature& signature() const
    {
        return _signature;
    }

    /// @brief Human-readable summary string.
    /// @return The religion.
    [[nodiscard]] std::string to_string() const { return religion; }

    /// @brief Write this interest as a JSON object.
    /// @param w Writer positioned where a value is allowed.
    void to_json(core::json_writer& w) const
    {
        w.begin_object()
            .field("country", country_code)
            .field("religion", religion)
            .field("religion_group", religion_group);
        write_items_(w, "hobbies", hobbies);
        write_items_(w, "films", films);
        write_items_(w, "games", games);
        write_items_(w, "books", books);
        write_items_(w, "music", music);
        w.key("signature");
        _signature.to_json(w);
        w.end_object();
    }

    /// @brief This interest as a JSON document.
    /// @return Compact JSON, for example
    ///   `{"country":"BR","religion":"...","religion_group":"...","hobbies":[{"key":"...","label":"..."}],...,"signature":{...}}`.
    [[nodiscard]] std::string to_json() const
    {
        core::json_writer w;
        to_json(w);
        return w.str();
    }

    /// @brief Implicit conversion to std::string.
    /// @return Summary string.
    operator std::string() const // NOLINT(hicpp-explicit-conversions)
    {
        return to_string();
    }

    /// @brief Stream the summary to an output stream.
    /// @param os Output stream.
    /// @param v The interest to stream.
    /// @return The stream.
    friend std::ostream& operator<<(std::ostream& os, const interest& v)
    {
        os << v.to_string();
        return os;
    }

  private:
    static void write_items_(core::json_writer& w, std::string_view name,
                             const std::vector<interest_item>& items)
    {
        w.key(name).begin_array();
        for (const auto& i : items)
        {
            w.begin_object().field("key", i.key).field("label", i.label).end_object();
        }
        w.end_array();
    }

    core::seed_signature _signature;
    friend class intg;
};

/// @brief One religion's share of a country, as loaded.
struct religion_share
{
    std::string religion; ///< Label, for example "Roman Catholic" (`full`) or "Christian" (`lite`).
    std::string group;    ///< Major group: christian, muslim, hindu, buddhist, jewish, unaffiliated, other, or a group a layer adds.
    double weight{0.0};   ///< Share of the population in percent; only the ratios matter.
    int year{0};          ///< Survey year, or 0 when unknown.
};

/// @brief Interest Generator: draws a person's religion, hobbies and
///        favourite film, game, book and music genres.
///
/// The country (when not given) comes from the `"country"` stream of the
/// call seed, the religion from `"religion"`, and each kind of item from a
/// stream of its own (`"hobby"`, `"film"`, `"game"`, `"book"`, `"music"`),
/// so fields added later leave these unchanged. Output is identical on
/// every compiler and platform (dasmig-core).
///
/// An item's weight for a person is its base `weight` multiplied by factors
/// for the person's sex, age band, country (or else region) and religion
/// group, for the item and for its activity class; a missing factor is 1.
/// Items are drawn without replacement.
///
/// A country chosen at random is uniform over the countries loaded; the
/// data has no population column. Pass a country chosen by country-generator
/// for a population-weighted world.
///
/// Can be used as a singleton via instance() or constructed
/// independently.
///
/// @par Thread safety
/// Each instance is independent.  Concurrent calls to get_interest() on the
/// **same** instance require external synchronisation.
class intg
{
  public:
    /// @brief The data tables.
    enum class table : std::uint8_t
    {
        religion,  ///< `religion.tsv`: key `country_code` and `religion`; group, weight, year, basis.
        interests, ///< `interests.tsv`: key `kind` and `key`; label, weight, class, category, tags, min_age, max_age.
        affinity,  ///< `affinity.tsv`: key `target` (an item key, or `class:` and a class), `dimension` and `value`; factor, basis.
        age_bands, ///< `age_bands.tsv`: key `band`; min_age, max_age.
        regions    ///< `regions.tsv`: key `country_code`; region.
    };

    /// @brief Default constructor — creates an empty generator with no
    ///        data.
    intg() : _tables{make_tables_()} {}

    intg(const intg&) = delete;            ///< Not copyable.
    intg& operator=(const intg&) = delete; ///< Not copyable.
    intg(intg&&) noexcept = default;       ///< Movable.
    intg& operator=(intg&&) noexcept = default; ///< Movable.
    ~intg() = default;                       ///< Destructor.

    /// @brief Access the global singleton instance.
    ///
    /// Loads the `lite` tier (or `full` if `lite` is missing) on first
    /// access, from compiled-in data or an auto-probed `resources/`.
    /// @return The singleton.
    static intg& instance()
    {
        static intg inst{auto_probe_tag{}};
        return inst;
    }

    // -- Generation -------------------------------------------------------

    /// @brief Generate a random interest for a request.
    /// @param request Country, age, sex and counts. Use `{}` for defaults.
    /// @return The interest.
    /// @throws std::runtime_error If no data has been loaded.
    /// @throws std::invalid_argument If the country has no data, or every
    ///         share of it has weight 0 (or, with no country, no country has
    ///         a positive weight).
    [[nodiscard]] interest get_interest(const interest_request& request)
    {
        return generate_(request, draw_seed_());
    }

    /// @brief Generate a deterministic interest for a request.
    /// @param request Country, age, sex and counts. Use `{}` for defaults.
    /// @param call_seed Seed; the same seed, request and data give the same
    ///        interest, independent of prior calls. Does not touch the
    ///        instance's random engine.
    /// @return The interest.
    /// @throws std::runtime_error If no data has been loaded.
    /// @throws std::invalid_argument As for the unseeded overload.
    [[nodiscard]] interest get_interest(const interest_request& request,
                                        std::uint64_t call_seed) const
    {
        return generate_(request, call_seed);
    }

    // -- Queries ----------------------------------------------------------

    /// @brief Check whether religion data has been loaded.
    /// @return True when at least one religion row is loaded.
    [[nodiscard]] bool has_data() const { return _tables.at(index_(table::religion)).size() > 0; }

    /// @brief Return the number of countries with religion data.
    /// @return The count.
    [[nodiscard]] std::size_t country_count() const { return _model.countries.size(); }

    /// @brief Return the number of hobbies and genres loaded, all kinds.
    /// @return The count.
    [[nodiscard]] std::size_t item_count() const
    {
        std::size_t n = 0;
        for (const auto& v : _model.items) n += v.size();
        return n;
    }

    /// @brief The religion shares loaded for a country.
    /// @param cca2 ISO 3166-1 alpha-2 country code.
    /// @return The shares in table order; empty when the country has none.
    [[nodiscard]] std::span<const religion_share> shares(std::string_view cca2) const
    {
        const auto it = _model.countries.find(cca2);
        if (it == _model.countries.end()) return {};
        return it->second.shares;
    }

    /// @brief Affinity rows whose target is neither an item key nor
    ///        `class:` and an activity class that is loaded. They are ignored, so removing an
    ///        item needs no other change; this lists them to catch misspelt
    ///        names.
    /// @return One description per ignored row, such as
    ///         `affinity.tsv:12: unknown target 'footbal'`.
    [[nodiscard]] const std::vector<std::string>& unresolved() const
    {
        return _model.unresolved;
    }

    // -- Data management --------------------------------------------------

    /// @brief Load a resource directory, replacing all data and layers
    ///        loaded before.
    ///
    /// The directory must contain `religion.tsv`; the other tables are read
    /// when present (a missing one leaves its table empty, so a directory
    /// with only `religion.tsv` draws no hobbies or media). A path without
    /// `religion.tsv` is ignored.
    ///
    /// @param dir Directory such as `resources/lite`.
    /// @throws core::tsv_error If a file is malformed; the data is then
    ///         unchanged.
    void load(const std::filesystem::path& dir)
    {
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec) ||
            !std::filesystem::is_regular_file(dir / file_name_(table::religion), ec))
        {
            return;
        }
        apply_(read_directory_(dir), core::layer_mode::replace, true);
    }

    /// @brief Load a dataset tier, replacing all data and layers loaded
    ///        before.
    ///
    /// Uses the compiled-in copy when the library is built with
    /// `DASMIG_INTEREST_EMBED`, otherwise searches `resources/`,
    /// `../resources`, `interest-generator/resources` and
    /// `../interest-generator/resources`.
    ///
    /// @param tier The tier.
    /// @return False when the tier was not found.
    /// @throws core::tsv_error If a file is malformed.
    [[nodiscard]] bool load(dataset tier)
    {
        const std::string tier_dir{core::tier_name(tier)};
        const auto marker = tier_dir + "/" + std::string{file_name_(table::religion)};
        const auto root = core::find_resources(library_name_, marker);
        documents docs;
        for (const auto t : all_tables_)
        {
            docs.at(index_(t)) = core::load_resource(tier_dir + "/" + std::string{file_name_(t)},
                                                     embedded_files_(), root);
        }
        if (!docs.at(index_(table::religion))) return false;
        apply_(std::move(docs), core::layer_mode::replace, true);
        return true;
    }

    /// @brief Add data layers from a directory or a file.
    ///
    /// A directory may hold any of `religion.tsv`, `interests.tsv`,
    /// `affinity.tsv`, `age_bands.tsv` and `regions.tsv`; all are applied
    /// together or, if any fails, none. A file must have one of those
    /// names.
    ///
    /// @param path Directory or file.
    /// @param mode How the layers combine with the data loaded before.
    /// @throws core::tsv_error If a file cannot be read or a row is
    ///         invalid; the data is then unchanged.
    /// @throws std::invalid_argument If @p path is a file with another name.
    void extend(const std::filesystem::path& path, core::layer_mode mode = core::layer_mode::override)
    {
        std::error_code ec;
        if (std::filesystem::is_directory(path, ec))
        {
            apply_(read_directory_(path), mode, false);
            return;
        }
        for (const auto t : all_tables_)
        {
            if (path.filename() == file_name_(t))
            {
                (void)extend(t, core::tsv_document::load(path), mode);
                return;
            }
        }
        throw std::invalid_argument("extend: expected religion.tsv, interests.tsv, affinity.tsv, "
                                    "age_bands.tsv or regions.tsv, got " + path.filename().string());
    }

    /// @brief Add a data layer to one table.
    ///
    /// Rows are keyed by the table's key columns (see table). In the default
    /// override mode a row replaces the loaded row with the same key and new
    /// keys are added; a row with `disabled` set to 1 removes its key.
    ///
    /// @param which The table.
    /// @param doc TSV with that table's columns.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did (rows added, replaced, removed, warnings).
    /// @throws core::tsv_error If a row is invalid; the data is unchanged.
    const core::layer_info& extend(table which, core::tsv_document doc,
                                   core::layer_mode mode = core::layer_mode::override)
    {
        documents docs;
        docs.at(index_(which)).emplace(std::move(doc));
        apply_(std::move(docs), mode, false);
        return _tables.at(index_(which)).layers().back();
    }

    /// @brief Add a data layer to one table from TSV text (for example
    ///        from a WebAssembly host).
    /// @param which The table.
    /// @param text TSV with that table's columns.
    /// @param source Name used in errors.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did.
    /// @throws core::tsv_error If a row is invalid; the data is unchanged.
    const core::layer_info& extend_from_string(table which, std::string_view text, std::string source,
                                               core::layer_mode mode = core::layer_mode::override)
    {
        return extend(which, core::tsv_document::parse(text, std::move(source)), mode);
    }

    /// @brief The layers in effect for one table, in order.
    /// @param which The table.
    /// @return The layer records.
    [[nodiscard]] const std::vector<core::layer_info>& layers(table which) const
    {
        return _tables.at(index_(which)).layers();
    }

    /// @brief Hash of the loaded data (all tables), recorded in each
    ///        result's signature().
    /// @return The hash; 0 when nothing is loaded.
    [[nodiscard]] std::uint64_t data_hash() const
    {
        std::uint64_t h = 0;
        bool any = false;
        for (const auto& t : _tables)
        {
            any = any || !t.layers().empty();
            h = core::hash_combine(h, t.data_hash());
        }
        return any ? h : 0;
    }

  private:
    // -- Tables -------------------------------------------------------------

    static constexpr std::string_view library_name_ = "interest-generator";
    static constexpr std::size_t table_count_ = 5;
    static constexpr std::array<table, table_count_> all_tables_{
        table::religion, table::interests, table::affinity, table::age_bands, table::regions};

    using documents = std::array<std::optional<core::tsv_document>, table_count_>;
    using tables = std::array<core::layered_table, table_count_>;

    static constexpr std::size_t index_(table t) { return static_cast<std::size_t>(t); }

    static constexpr std::string_view file_name_(table t)
    {
        switch (t)
        {
        case table::religion: return "religion.tsv";
        case table::interests: return "interests.tsv";
        case table::affinity: return "affinity.tsv";
        case table::age_bands: return "age_bands.tsv";
        case table::regions: return "regions.tsv";
        }
        return "";
    }

    static core::table_schema schema_(table t)
    {
        switch (t)
        {
        case table::religion:
            return {.key = {"country_code", "religion"}, .required = {"group", "weight"}, .optional = {"year", "basis"}};
        case table::interests:
            return {.key = {"kind", "key"},
                    .required = {"label", "weight"},
                    .optional = {"class", "category", "tags", "min_age", "max_age"}};
        case table::affinity:
            return {.key = {"target", "dimension", "value"}, .required = {"factor"}, .optional = {"basis"}};
        case table::age_bands: return {.key = {"band"}, .required = {"min_age", "max_age"}, .optional = {}};
        case table::regions: return {.key = {"country_code"}, .required = {"region"}, .optional = {}};
        }
        return {};
    }

    static tables make_tables_()
    {
        return {core::layered_table{schema_(table::religion)}, core::layered_table{schema_(table::interests)},
                core::layered_table{schema_(table::affinity)}, core::layered_table{schema_(table::age_bands)},
                core::layered_table{schema_(table::regions)}};
    }

    // -- Parsed rows --------------------------------------------------------

    static constexpr std::size_t kind_count_ = 5;
    static constexpr std::array<std::string_view, kind_count_> kind_names_{"hobby", "film", "game", "book", "music"};
    static constexpr std::array<std::string_view, kind_count_> kind_streams_{"hobby", "film", "game", "book", "music"};

    enum class dimension : std::uint8_t { sex, age_band, region, country, religion };
    static constexpr std::array<std::string_view, 5> dimension_names_{"sex", "age_band", "region", "country", "religion"};

    struct item_data
    {
        std::string key;
        std::string label;
        std::string cls;
        std::string category;
        double weight{0.0};
        int min_age{0};
        int max_age{255};
    };

    struct band_data
    {
        std::string band;
        int min_age{0};
        int max_age{0};
    };

    struct country_data
    {
        std::vector<religion_share> shares; // in table row order
        core::weighted_table<std::size_t> weights; // share index, by weight
    };

    struct model
    {
        std::map<std::string, country_data, std::less<>> countries;
        std::vector<std::string> codes; // countries with a positive total, sorted
        std::array<std::vector<item_data>, kind_count_> items;
        std::unordered_map<std::string, double> factors; // target, dimension, value -> factor
        std::vector<band_data> bands;                    // in table row order
        std::map<std::string, std::string, std::less<>> region_of;
        std::vector<std::string> unresolved;
    };

    static std::string factor_key_(std::string_view target, dimension d, std::string_view value)
    {
        std::string k;
        k.reserve(target.size() + value.size() + 12);
        k.append(target).push_back('\x1f');
        k.append(dimension_names_[static_cast<std::size_t>(d)]).push_back('\x1f');
        k.append(value);
        return k;
    }

    static std::size_t kind_of_(const core::tsv_row& row)
    {
        const auto name = row["kind"];
        for (std::size_t i = 0; i < kind_count_; ++i)
        {
            if (kind_names_[i] == name) return i;
        }
        throw row.error("kind", std::format("expected hobby, film, game, book or music, got '{}'", name));
    }

    static dimension dimension_of_(const core::tsv_row& row)
    {
        const auto name = row["dimension"];
        for (std::size_t i = 0; i < dimension_names_.size(); ++i)
        {
            if (dimension_names_[i] == name) return static_cast<dimension>(i);
        }
        throw row.error("dimension", std::format("expected sex, age_band, region, country or religion, got '{}'", name));
    }

    // Each parse_ function checks a row and throws core::tsv_error located at
    // it; the same function validates a layer and builds the model.
    static std::pair<std::string, religion_share> parse_share_(const core::tsv_row& row)
    {
        std::string cca2 = row.get<std::string>("country_code");
        const bool code_ok = cca2.size() == 2 && cca2[0] >= 'A' && cca2[0] <= 'Z' &&
                             cca2[1] >= 'A' && cca2[1] <= 'Z';
        if (!code_ok) throw row.error("country_code", "expected two capital letters (ISO 3166-1 alpha-2)");
        religion_share s;
        s.religion = row.get<std::string>("religion");
        s.group = row.get<std::string>("group");
        s.weight = row.get<double>("weight");
        if (!(s.weight >= 0.0)) throw row.error("weight", "must not be negative");
        s.year = row.get_or<int>("year", 0);
        return {std::move(cca2), std::move(s)};
    }

    static std::pair<std::size_t, item_data> parse_item_(const core::tsv_row& row)
    {
        const std::size_t kind = kind_of_(row);
        item_data i;
        i.key = row.get<std::string>("key");
        i.label = row.get<std::string>("label");
        i.weight = row.get<double>("weight");
        if (!(i.weight >= 0.0)) throw row.error("weight", "must not be negative");
        i.cls = row.get_or<std::string>("class", "");
        i.category = row.get_or<std::string>("category", "");
        i.min_age = row.get_or<int>("min_age", 0);
        i.max_age = row.get_or<int>("max_age", 255);
        if (i.min_age < 0) throw row.error("min_age", "must not be negative");
        if (i.max_age < i.min_age) throw row.error("max_age", "must not be below min_age");
        return {kind, std::move(i)};
    }

    struct affinity_data
    {
        std::string target;
        dimension dim{dimension::sex};
        std::string value;
        double factor{1.0};
    };

    static affinity_data parse_affinity_(const core::tsv_row& row)
    {
        affinity_data a;
        a.target = row.get<std::string>("target");
        a.dim = dimension_of_(row);
        a.value = row.get<std::string>("value");
        if (a.dim == dimension::sex && a.value != "male" && a.value != "female")
        {
            throw row.error("value", std::format("expected male or female, got '{}'", a.value));
        }
        a.factor = row.get<double>("factor");
        if (!(a.factor >= 0.0)) throw row.error("factor", "must not be negative");
        return a;
    }

    static band_data parse_band_(const core::tsv_row& row)
    {
        band_data b;
        b.band = row.get<std::string>("band");
        b.min_age = row.get<int>("min_age");
        b.max_age = row.get<int>("max_age");
        if (b.min_age < 0) throw row.error("min_age", "must not be negative");
        if (b.max_age < b.min_age) throw row.error("max_age", "must not be below min_age");
        return b;
    }

    static std::pair<std::string, std::string> parse_region_(const core::tsv_row& row)
    {
        return {row.get<std::string>("country_code"), row.get<std::string>("region")};
    }

    static void validate_(table t, const core::tsv_row& row)
    {
        switch (t)
        {
        case table::religion: (void)parse_share_(row); break;
        case table::interests: (void)parse_item_(row); break;
        case table::affinity: (void)parse_affinity_(row); break;
        case table::age_bands: (void)parse_band_(row); break;
        case table::regions: (void)parse_region_(row); break;
        }
    }

    // -- State ----------------------------------------------------------------

    tables _tables;
    model _model;
    core::rng _engine{core::rng::from_entropy()};

    struct auto_probe_tag {};

    explicit intg(auto_probe_tag /*tag*/) : _tables{make_tables_()}
    {
        if (!load(dataset::lite)) (void)load(dataset::full);
    }

    // -- Helpers ----------------------------------------------------------

    static std::span<const core::embedded_file> embedded_files_()
    {
#ifdef DASMIG_INTEREST_EMBED
        return embedded::interestgen::files;
#else
        return {};
#endif
    }

    std::uint64_t draw_seed_() { return _engine(); }

    static documents read_directory_(const std::filesystem::path& dir)
    {
        documents docs;
        for (const auto t : all_tables_)
        {
            const auto file = dir / file_name_(t);
            std::error_code ec;
            if (std::filesystem::is_regular_file(file, ec))
            {
                docs.at(index_(t)) = core::tsv_document::load(file);
            }
        }
        return docs;
    }

    // Apply layers to copies of the tables, then commit: all or nothing.
    // With @p reset (load), tables without a document are emptied.
    void apply_(documents docs, core::layer_mode mode, bool reset)
    {
        tables next = _tables;
        for (const auto t : all_tables_)
        {
            auto& doc = docs.at(index_(t));
            auto& tbl = next.at(index_(t));
            if (!doc)
            {
                if (reset) tbl.clear();
                continue;
            }
            (void)tbl.extend(std::move(*doc), mode, [t](const core::tsv_row& row) { validate_(t, row); });
        }
        auto built = build_model_(next);
        _tables = std::move(next);
        _model = std::move(built);
    }

    static model build_model_(const tables& tbls)
    {
        model m;
        for (const auto& row : tbls.at(index_(table::religion)).rows())
        {
            auto [cca2, share] = parse_share_(row);
            auto& c = m.countries[cca2];
            c.weights.add(c.shares.size(), share.weight);
            c.shares.push_back(std::move(share));
        }
        for (const auto& [cca2, c] : m.countries)
        {
            if (c.weights.total_weight() > 0.0) m.codes.push_back(cca2);
        }

        std::map<std::string, bool, std::less<>> known; // item keys and classes
        for (const auto& row : tbls.at(index_(table::interests)).rows())
        {
            auto [kind, item] = parse_item_(row);
            known[item.key] = true;
            if (!item.cls.empty()) known["class:" + item.cls] = true;
            m.items.at(kind).push_back(std::move(item));
        }
        for (const auto& row : tbls.at(index_(table::affinity)).rows())
        {
            const auto a = parse_affinity_(row);
            if (!known.contains(a.target))
            {
                m.unresolved.push_back(std::format("{}:{}: unknown target '{}'", row.source(), row.line(), a.target));
                continue;
            }
            m.factors[factor_key_(a.target, a.dim, a.value)] = a.factor;
        }
        for (const auto& row : tbls.at(index_(table::age_bands)).rows()) m.bands.push_back(parse_band_(row));
        for (const auto& row : tbls.at(index_(table::regions)).rows())
        {
            auto [cca2, region] = parse_region_(row);
            m.region_of[cca2] = std::move(region);
        }
        return m;
    }

    void require_data_() const
    {
        if (!has_data())
        {
            throw std::runtime_error("No interest data loaded. Call load() first.");
        }
    }

    // The person an item weight is computed for.
    struct person
    {
        std::string cca2;
        std::string region;       // empty when unknown
        std::string sex;          // "male", "female" or empty
        std::string band;         // empty when unknown
        std::string religion;     // group of the drawn religion
        std::optional<int> age;
    };

    [[nodiscard]] double factor_(std::string_view target, dimension d, std::string_view value) const
    {
        if (value.empty()) return 1.0;
        const auto it = _model.factors.find(factor_key_(target, d, value));
        return it == _model.factors.end() ? 1.0 : it->second;
    }

    // A country row replaces the region row of the same target.
    [[nodiscard]] double geo_factor_(std::string_view target, const person& p) const
    {
        const auto c = _model.factors.find(factor_key_(target, dimension::country, p.cca2));
        if (c != _model.factors.end()) return c->second;
        return factor_(target, dimension::region, p.region);
    }

    [[nodiscard]] double item_weight_(const item_data& i, const person& p) const
    {
        double w = i.weight;
        const std::string class_target = i.cls.empty() ? std::string{} : "class:" + i.cls;
        for (const std::string_view target : {std::string_view{i.key}, std::string_view{class_target}})
        {
            if (target.empty()) continue;
            w *= factor_(target, dimension::sex, p.sex);
            w *= factor_(target, dimension::age_band, p.band);
            w *= factor_(target, dimension::religion, p.religion);
            w *= geo_factor_(target, p);
        }
        return w;
    }

    // Draw up to n items of one kind without replacement.
    void draw_items_(std::size_t kind, std::size_t n, const person& p, core::rng& r,
                     std::vector<interest_item>& out) const
    {
        const auto& items = _model.items.at(kind);
        core::weighted_table<std::size_t> table;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const auto& item = items[i];
            if (p.age && (*p.age < item.min_age || *p.age > item.max_age)) continue;
            table.add(i, item_weight_(item, p));
        }
        std::vector<bool> taken(items.size(), false);
        const auto adjust = [&taken](const std::size_t& i, double base) { return taken[i] ? 0.0 : base; };
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto picked = table.try_pick_index(r, adjust);
            if (!picked) break;
            const std::size_t i = table.entries()[*picked].value;
            taken[i] = true;
            out.push_back({.key = items[i].key, .label = items[i].label, .category = items[i].category});
        }
    }

    [[nodiscard]] interest generate_(const interest_request& req, std::uint64_t call_seed) const
    {
        require_data_();
        const core::rng call{call_seed};

        std::string cca2{req.country};
        if (cca2.empty())
        {
            if (_model.codes.empty())
            {
                throw std::invalid_argument("No country has a religion share above 0.");
            }
            auto cr = call.split("country");
            cca2 = _model.codes[static_cast<std::size_t>(core::bounded(cr, _model.codes.size()))];
        }
        const auto it = _model.countries.find(cca2);
        if (it == _model.countries.end())
        {
            throw std::invalid_argument(std::format("Unknown country code: {}", cca2));
        }
        const auto& c = it->second;
        if (!(c.weights.total_weight() > 0.0))
        {
            throw std::invalid_argument(std::format("Every religion share of {} has weight 0.", cca2));
        }

        auto rr = call.split("religion");
        const auto& share = c.shares[c.weights.pick(rr)];

        interest v;
        v._signature = {.seed = call_seed, .data = data_hash()};
        v.country_code = cca2;
        v.religion = share.religion;
        v.religion_group = share.group;

        person p;
        p.cca2 = cca2;
        p.religion = share.group;
        p.age = req.age ? std::optional<int>{*req.age} : std::nullopt;
        if (const auto rg = _model.region_of.find(cca2); rg != _model.region_of.end()) p.region = rg->second;
        if (req.person_sex) p.sex = *req.person_sex == sex::male ? "male" : "female";
        if (p.age && !_model.bands.empty())
        {
            const auto b = std::ranges::find_if(_model.bands, [&](const band_data& x) {
                return *p.age >= x.min_age && *p.age <= x.max_age;
            });
            if (b != _model.bands.end()) p.band = b->band;
            // Younger than every band: too young for hobbies and media.
            else if (std::ranges::all_of(_model.bands, [&](const band_data& x) { return *p.age < x.min_age; }))
            {
                return v;
            }
        }

        std::array<std::vector<interest_item>*, kind_count_> out{&v.hobbies, &v.films, &v.games, &v.books, &v.music};
        for (std::size_t k = 0; k < kind_count_; ++k)
        {
            auto r = call.split(kind_streams_[k]);
            draw_items_(k, k == 0 ? req.hobbies : req.media, p, r, *out.at(k));
        }
        return v;
    }
};

} // namespace dasmig

#endif // DASMIG_INTERESTGEN_HPP
