#ifndef DASMIG_INTERESTGEN_HPP
#define DASMIG_INTERESTGEN_HPP

#include "core.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
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

    /// @brief Retrieve the random seed used to generate this interest.
    /// @return The call seed; pass it, with the country, to the seeded
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
            .field("religion_group", religion_group)
            .key("signature");
        _signature.to_json(w);
        w.end_object();
    }

    /// @brief This interest as a JSON document.
    /// @return Compact JSON, for example
    ///   `{"country":"BR","religion":"...","religion_group":"...","signature":{...}}`.
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

/// @brief Interest Generator: draws a person's religion from the shares of
///        their country.
///
/// The country (when not given) comes from the `"country"` stream of the
/// call seed and the religion from the `"religion"` stream, so fields added
/// later draw from streams of their own and leave these unchanged. Output
/// is identical on every compiler and platform (dasmig-core).
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
    /// @brief Default constructor — creates an empty generator with no
    ///        data.
    intg() = default;

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

    /// @brief Generate a random interest for a specific country.
    /// @param cca2 ISO 3166-1 alpha-2 country code (e.g. "US", "BR").
    /// @return The interest.
    /// @throws std::runtime_error If no data has been loaded.
    /// @throws std::invalid_argument If the country has no data, or every
    ///         share of it has weight 0.
    [[nodiscard]] interest get_interest(std::string_view cca2)
    {
        return generate_(lookup_(cca2), cca2, draw_seed_());
    }

    /// @brief Generate a deterministic interest for a specific country.
    /// @param cca2 ISO 3166-1 alpha-2 country code.
    /// @param call_seed Seed; the same seed, country and data give the same
    ///        interest.
    /// @return The interest.
    /// @throws std::runtime_error If no data has been loaded.
    /// @throws std::invalid_argument If the country has no data, or every
    ///         share of it has weight 0.
    [[nodiscard]] interest get_interest(std::string_view cca2,
                                        std::uint64_t call_seed) const
    {
        return generate_(lookup_(cca2), cca2, call_seed);
    }

    /// @brief Generate a random interest from a random country.
    /// @return The interest.
    /// @throws std::runtime_error If no data has been loaded.
    /// @throws std::invalid_argument If no country has a positive weight.
    [[nodiscard]] interest get_interest()
    {
        return get_interest(draw_seed_());
    }

    /// @brief Generate a deterministic interest from a random country.
    /// @param call_seed Seed.
    /// @return The interest.
    /// @throws std::runtime_error If no data has been loaded.
    /// @throws std::invalid_argument If no country has a positive weight.
    [[nodiscard]] interest get_interest(std::uint64_t call_seed) const
    {
        require_data_();
        if (_codes.empty())
        {
            throw std::invalid_argument("No country has a religion share above 0.");
        }
        auto r = core::rng{call_seed}.split("country");
        const auto& cca2 = _codes[static_cast<std::size_t>(core::bounded(r, _codes.size()))];
        return generate_(lookup_(cca2), cca2, call_seed);
    }

    // -- Seeding ----------------------------------------------------------

    /// @brief Seed the internal random engine for deterministic
    ///        sequences.
    /// @param seed_value Seed.
    /// @return `*this`.
    intg& seed(std::uint64_t seed_value)
    {
        _engine.seed(seed_value);
        return *this;
    }

    /// @brief Reseed the engine with a non-deterministic source.
    /// @return `*this`.
    intg& unseed()
    {
        _engine.unseed();
        return *this;
    }

    // -- Data management --------------------------------------------------

    /// @brief Check whether any data has been loaded.
    /// @return True when at least one religion row is loaded.
    [[nodiscard]] bool has_data() const { return _table.size() > 0; }

    /// @brief Return the number of countries with religion data.
    /// @return The count.
    [[nodiscard]] std::size_t country_count() const { return _countries.size(); }

    /// @brief The religion shares loaded for a country.
    /// @param cca2 ISO 3166-1 alpha-2 country code.
    /// @return The shares in table order; empty when the country has none.
    [[nodiscard]] std::span<const religion_share> shares(std::string_view cca2) const
    {
        const auto it = _countries.find(cca2);
        if (it == _countries.end()) return {};
        return it->second.shares;
    }

    /// @brief Load data from a resource directory, replacing all data and
    ///        layers loaded before.
    ///
    /// Expects the directory to contain `religion.tsv`. A path that is not
    /// a directory, or a directory without the file, is ignored.
    ///
    /// @param dir Directory such as `resources/lite`.
    /// @throws core::tsv_error If the file is malformed.
    void load(const std::filesystem::path& dir)
    {
        std::error_code ec;
        const auto file = dir / data_file_;
        if (!std::filesystem::is_directory(dir, ec) ||
            !std::filesystem::is_regular_file(file, ec))
        {
            return;
        }
        (void)extend(core::tsv_document::load(file), core::layer_mode::replace);
    }

    /// @brief Load a dataset tier, replacing all data and layers loaded
    ///        before.
    ///
    /// Uses the compiled-in copy when the library is built with
    /// `DASMIG_INTEREST_EMBED`, otherwise searches `resources/`, `../resources`,
    /// `interest-generator/resources` and `../interest-generator/resources`.
    ///
    /// @param tier The tier.
    /// @return False when the tier was not found.
    /// @throws core::tsv_error If the file is malformed.
    [[nodiscard]] bool load(dataset tier)
    {
        const std::string path = std::string{core::tier_name(tier)} + "/" + std::string{data_file_};
        auto doc = core::load_resource(path, embedded_files_(),
                                       core::find_resources(library_name_, path));
        if (!doc) return false;
        (void)extend(std::move(*doc), core::layer_mode::replace);
        return true;
    }

    /// @brief Add a data layer from a TSV file with the `religion.tsv`
    ///        columns.
    ///
    /// Rows are keyed by `country_code` and `religion`. In the default
    /// override mode a row replaces the loaded row with the same key and new
    /// keys are added; a row with `disabled` set to 1 removes its key.
    ///
    /// @param file The layer file.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did (rows added, replaced, removed, warnings).
    /// @throws core::tsv_error If the file cannot be read or a row is
    ///         invalid; the data is then unchanged.
    const core::layer_info& extend(const std::filesystem::path& file,
                                   core::layer_mode mode = core::layer_mode::override)
    {
        return extend(core::tsv_document::load(file), mode);
    }

    /// @brief Add a data layer from TSV text (for example from a
    ///        WebAssembly host).
    /// @param text TSV with the `religion.tsv` columns.
    /// @param source Name used in errors.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did.
    /// @throws core::tsv_error If a row is invalid; the data is unchanged.
    const core::layer_info& extend_from_string(std::string_view text, std::string source,
                                               core::layer_mode mode = core::layer_mode::override)
    {
        return extend(core::tsv_document::parse(text, std::move(source)), mode);
    }

    /// @brief Add a data layer.
    /// @param doc TSV with the `religion.tsv` columns.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did.
    /// @throws core::tsv_error If a row is invalid; the data is unchanged.
    const core::layer_info& extend(core::tsv_document doc,
                                   core::layer_mode mode = core::layer_mode::override)
    {
        const auto& info = _table.extend(std::move(doc), mode, [](const core::tsv_row& row) {
            (void)parse_share_(row);
        });
        rebuild_();
        return info;
    }

    /// @brief The data layers loaded, in order.
    /// @return The layer records.
    [[nodiscard]] const std::vector<core::layer_info>& layers() const
    {
        return _table.layers();
    }

    /// @brief Hash of the loaded data, recorded in each result's
    ///        signature().
    /// @return The hash; 0 when nothing is loaded.
    [[nodiscard]] std::uint64_t data_hash() const { return _table.data_hash(); }

  private:
    // -- Internal data structures -----------------------------------------

    static constexpr std::string_view library_name_ = "interest-generator";
    static constexpr std::string_view data_file_ = "religion.tsv";

    static core::table_schema schema_()
    {
        return {.key = {"country_code", "religion"},
                .required = {"group", "weight"},
                .optional = {"year", "basis"}};
    }

    // Parse and check a row; throws core::tsv_error located at the row.
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

    struct country_data
    {
        std::vector<religion_share> shares; // in table row order
        core::weighted_table<std::size_t> weights; // share index, by weight
    };

    core::layered_table _table{schema_()};
    std::map<std::string, country_data, std::less<>> _countries;
    std::vector<std::string> _codes; // countries with a positive total, sorted

    core::rng _engine{core::rng::from_entropy()};

    struct auto_probe_tag {};

    explicit intg(auto_probe_tag /*tag*/)
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

    void rebuild_()
    {
        std::map<std::string, country_data, std::less<>> countries;
        std::vector<std::string> codes;
        for (const auto& row : _table.rows())
        {
            auto [cca2, share] = parse_share_(row);
            auto& c = countries[cca2];
            c.weights.add(c.shares.size(), share.weight);
            c.shares.push_back(std::move(share));
        }
        for (const auto& [cca2, c] : countries)
        {
            if (c.weights.total_weight() > 0.0) codes.push_back(cca2);
        }
        _countries = std::move(countries);
        _codes = std::move(codes);
    }

    void require_data_() const
    {
        if (!has_data())
        {
            throw std::runtime_error("No interest data loaded. Call load() first.");
        }
    }

    [[nodiscard]] const country_data& lookup_(std::string_view cca2) const
    {
        require_data_();
        const auto it = _countries.find(cca2);
        if (it == _countries.end())
        {
            throw std::invalid_argument(std::format("Unknown country code: {}", cca2));
        }
        return it->second;
    }

    [[nodiscard]] interest generate_(const country_data& c, std::string_view cca2,
                                     std::uint64_t call_seed) const
    {
        const core::rng call{call_seed};
        auto r = call.split("religion");
        if (!(c.weights.total_weight() > 0.0))
        {
            throw std::invalid_argument(std::format("Every religion share of {} has weight 0.", cca2));
        }
        const auto& s = c.shares[c.weights.pick(r)];

        interest v;
        v._signature = {.seed = call_seed, .data = data_hash()};
        v.country_code = std::string{cca2};
        v.religion = s.religion;
        v.religion_group = s.group;
        return v;
    }
};

} // namespace dasmig

#endif // DASMIG_INTERESTGEN_HPP
