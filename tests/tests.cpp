#include "catch_amalgamated.hpp"
#include "../dasmig/interestgen.hpp"

#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static dasmig::intg make_loaded_generator()
{
    dasmig::intg gen;
    if (!gen.load(dasmig::dataset::lite))
    {
        for (const auto* candidate : {"resources/lite", "../resources/lite"})
        {
            if (std::filesystem::is_directory(candidate))
            {
                gen.load(std::filesystem::path{candidate});
                break;
            }
        }
    }
    REQUIRE(gen.has_data());
    return gen;
}

// A small world for exact statistics: XA is split 3 : 1 : 0, XB has one
// religion, XC has only weight 0.
static dasmig::intg make_small_generator()
{
    dasmig::intg gen;
    (void)gen.extend_from_string("country_code\treligion\tgroup\tweight\n"
                                 "XA\tAlpha\tchristian\t3\n"
                                 "XA\tBravo\tmuslim\t1\n"
                                 "XA\tCharlie\tother\t0\n"
                                 "XB\tDelta\tother\t10\n"
                                 "XC\tEcho\tother\t0\n",
                                 "small.tsv");
    return gen;
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

TEST_CASE("load(dataset::lite) loads the religion shares", "[loading]")
{
    const auto gen = make_loaded_generator();
    REQUIRE(gen.country_count() == 195);
    REQUIRE(gen.layers().size() == 1);
}

TEST_CASE("every lite country has shares that add up to about 100", "[loading][data]")
{
    const auto gen = make_loaded_generator();
    std::size_t checked = 0;
    for (const auto* cca2 : {"US", "BR", "IN", "NG", "JP", "DE", "SA", "ID", "RU", "CN"})
    {
        double total = 0;
        for (const auto& s : gen.shares(cca2)) total += s.weight;
        REQUIRE(total > 90.0);
        REQUIRE(total < 110.0);
        ++checked;
    }
    REQUIRE(checked == 10);
}

TEST_CASE("load - non-existent path is a no-op", "[loading]")
{
    dasmig::intg gen;
    gen.load(std::filesystem::path{"no/such/directory"});
    REQUIRE_FALSE(gen.has_data());
    REQUIRE(gen.data_hash() == 0);
}

#ifndef DASMIG_INTEREST_EMBED // compiled-in data is never missing
TEST_CASE("load(dataset) returns false for a missing tier", "[loading]")
{
    const auto old_cwd = std::filesystem::current_path();
    const auto tmp = std::filesystem::temp_directory_path() / "intg_empty_dir";
    std::filesystem::create_directories(tmp);
    std::filesystem::current_path(tmp);
    dasmig::intg gen;
    const bool loaded = gen.load(dasmig::dataset::full);
    std::filesystem::current_path(old_cwd);
    std::filesystem::remove_all(tmp);
    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(gen.has_data());
}

TEST_CASE("the full tier has more countries and detailed religions", "[loading]")
{
    dasmig::intg gen;
    for (const auto* candidate : {"resources/full", "../resources/full"})
    {
        if (std::filesystem::is_directory(candidate))
        {
            gen.load(std::filesystem::path{candidate});
            break;
        }
    }
    REQUIRE(gen.has_data());
    REQUIRE(gen.country_count() > 195);
    std::set<std::string> us;
    for (const auto& s : gen.shares("US")) us.insert(s.religion);
    REQUIRE(us.contains("Protestant"));
    REQUIRE(us.contains("Roman Catholic"));
}
#endif

TEST_CASE("the singleton loads data on first use", "[loading]")
{
    REQUIRE(dasmig::intg::instance().has_data());
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

TEST_CASE("get_interest throws when no data is loaded", "[generation]")
{
    dasmig::intg gen;
    REQUIRE_THROWS_AS(gen.get_interest(), std::runtime_error);
    REQUIRE_THROWS_AS(gen.get_interest(std::uint64_t{1}), std::runtime_error);
    REQUIRE_THROWS_AS(gen.get_interest("US"), std::runtime_error);
}

TEST_CASE("an unknown country is rejected", "[generation][errors]")
{
    const auto gen = make_loaded_generator();
    REQUIRE_THROWS_AS(gen.get_interest("ZZ", std::uint64_t{1}), std::invalid_argument);
    REQUIRE_THROWS_AS(gen.get_interest("us", std::uint64_t{1}), std::invalid_argument); // codes are upper case
    REQUIRE_THROWS_AS(gen.get_interest("", std::uint64_t{1}), std::invalid_argument);
}

TEST_CASE("the country is reported", "[generation]")
{
    const auto gen = make_loaded_generator();
    REQUIRE(gen.get_interest("BR", std::uint64_t{1}).country_code == "BR");
    // Without a country, the drawn one is reported and replays.
    const auto v = gen.get_interest(std::uint64_t{77});
    REQUIRE(gen.get_interest(v.country_code, 77).to_json() != "");
    REQUIRE(gen.shares(v.country_code).size() > 0);
}

TEST_CASE("get_interest with a seed is deterministic", "[generation][seed]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t s = 0; s < 100; ++s)
    {
        REQUIRE(gen.get_interest(s).to_json() == gen.get_interest(s).to_json());
        REQUIRE(gen.get_interest("FR", s).to_json() == gen.get_interest("FR", s).to_json());
    }
}

TEST_CASE("a result replays from its seed", "[generation][seed]")
{
    auto gen = make_loaded_generator();
    const auto v = gen.get_interest("IN");
    REQUIRE(gen.get_interest("IN", v.seed()).to_json() == v.to_json());
    const auto w = gen.get_interest();
    REQUIRE(gen.get_interest(w.seed()).to_json() == w.to_json());
}

TEST_CASE("seed and unseed control the instance engine", "[seed]")
{
    auto gen = make_loaded_generator();
    gen.seed(42);
    const auto a = gen.get_interest().to_json();
    gen.seed(42);
    REQUIRE(gen.get_interest().to_json() == a);
    REQUIRE(&gen.unseed() == &gen);
}

TEST_CASE("the religion follows the country's shares", "[generation][stats]")
{
    const auto gen = make_small_generator();
    std::map<std::string, int> counts;
    constexpr int draws = 20000;
    for (std::uint64_t s = 0; s < draws; ++s) ++counts[gen.get_interest("XA", s).religion];
    REQUIRE(counts["Charlie"] == 0); // weight 0 is never drawn
    REQUIRE(counts["Alpha"] == Catch::Approx(draws * 0.75).margin(draws * 0.02));
    REQUIRE(counts["Bravo"] == Catch::Approx(draws * 0.25).margin(draws * 0.02));
    REQUIRE(gen.get_interest("XA", std::uint64_t{1}).religion_group != "");
}

TEST_CASE("a random country is uniform and skips countries with no weight", "[generation][stats]")
{
    const auto gen = make_small_generator();
    std::map<std::string, int> counts;
    constexpr int draws = 10000;
    for (std::uint64_t s = 0; s < draws; ++s) ++counts[gen.get_interest(s).country_code];
    REQUIRE(counts["XC"] == 0); // every share is 0
    REQUIRE(counts["XA"] == Catch::Approx(draws * 0.5).margin(draws * 0.03));
    REQUIRE(counts["XB"] == Catch::Approx(draws * 0.5).margin(draws * 0.03));
    REQUIRE_THROWS_AS(gen.get_interest("XC", std::uint64_t{1}), std::invalid_argument);
}

TEST_CASE("the religion does not depend on how the country was chosen", "[generation][seed]")
{
    // The religion has a stream of its own: asking for the country the seed
    // would have drawn gives the same religion.
    const auto gen = make_loaded_generator();
    for (std::uint64_t s = 0; s < 50; ++s)
    {
        const auto a = gen.get_interest(s);
        REQUIRE(gen.get_interest(a.country_code, s).to_json() == a.to_json());
    }
}

// ---------------------------------------------------------------------------
// Conversion
// ---------------------------------------------------------------------------

TEST_CASE("string conversion and streaming give the religion", "[conversion]")
{
    const auto gen = make_loaded_generator();
    const auto v = gen.get_interest(std::uint64_t{5});
    const std::string s = v;
    std::ostringstream os;
    os << v;
    REQUIRE(s == v.religion);
    REQUIRE(os.str() == v.religion);
}

// ---------------------------------------------------------------------------
// Known answers (rule 2): the same seed and data give these exact results on
// every compiler and platform. A change here is a change of output and needs
// a major version.
// ---------------------------------------------------------------------------

TEST_CASE("known answers for fixed seeds", "[kat]")
{
    dasmig::intg gen;
    gen.load(std::filesystem::path{"resources/lite"});
    if (!gen.has_data()) gen.load(std::filesystem::path{"../resources/lite"});
    REQUIRE(gen.has_data());
    REQUIRE(gen.data_hash() == 0xa42bc4b2ab5bd900ULL);

    REQUIRE(gen.get_interest("US", std::uint64_t{1}).to_json() == R"({"country":"US","religion":"Unaffiliated","religion_group":"unaffiliated","signature":{"seed":"0000000000000001","data":"a42bc4b2ab5bd900"}})");
    REQUIRE(gen.get_interest("IN", std::uint64_t{2}).to_json() == R"({"country":"IN","religion":"Hindu","religion_group":"hindu","signature":{"seed":"0000000000000002","data":"a42bc4b2ab5bd900"}})");
    REQUIRE(gen.get_interest(std::uint64_t{3}).to_json() == R"({"country":"KP","religion":"Unaffiliated","religion_group":"unaffiliated","signature":{"seed":"0000000000000003","data":"a42bc4b2ab5bd900"}})");
    REQUIRE(gen.get_interest(std::uint64_t{4}).to_json() == R"({"country":"MZ","religion":"Other","religion_group":"other","signature":{"seed":"0000000000000004","data":"a42bc4b2ab5bd900"}})");

    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::uint64_t s = 0; s < 10000; ++s)
    {
        for (const char c : gen.get_interest(s).to_json())
        {
            h ^= static_cast<unsigned char>(c);
            h *= 0x100000001b3ULL;
        }
    }
    REQUIRE(h == 0x8be1a34f88562a4aULL);
}

// ---------------------------------------------------------------------------
// JSON and signatures
// ---------------------------------------------------------------------------

TEST_CASE("to_json and signature", "[json]")
{
    const auto gen = make_loaded_generator();
    const auto v = gen.get_interest("DE", std::uint64_t{99});
    REQUIRE(v.signature().seed == 99);
    REQUIRE(v.seed() == 99);
    REQUIRE(v.signature().data == gen.data_hash());
    const auto json = v.to_json();
    REQUIRE(json.starts_with(R"({"country":"DE","religion":")"));
    REQUIRE(json.find(R"("signature":{"seed":"0000000000000063")") != std::string::npos);

    dasmig::core::json_writer w;
    w.begin_array();
    v.to_json(w);
    w.end_array();
    REQUIRE(w.str() == "[" + json + "]");
}

// ---------------------------------------------------------------------------
// Extending data (rule 8)
// ---------------------------------------------------------------------------

TEST_CASE("extend overrides, adds and removes rows", "[extend]")
{
    auto gen = make_loaded_generator();
    const auto base_countries = gen.country_count();
    const auto base_hash = gen.data_hash();
    const auto before = gen.get_interest("US", std::uint64_t{7});

    // A layer that makes the US all "Pastafarian" and adds a new country.
    std::string layer = "country_code\treligion\tgroup\tweight\n";
    for (const auto& s : gen.shares("US"))
    {
        layer += "US\t" + s.religion + "\t" + s.group + "\t0\n";
    }
    layer += "US\tPastafarian\tother\t1\nXX\tFlying Spaghetti\tother\t5\n";
    const auto& info = gen.extend_from_string(layer, "mod.tsv");
    REQUIRE(info.added == 2);
    REQUIRE(info.replaced == gen.shares("US").size() - 1);
    REQUIRE(gen.country_count() == base_countries + 1);
    REQUIRE(gen.data_hash() != base_hash);
    REQUIRE(gen.layers().size() == 2);
    const auto after = gen.get_interest("US", std::uint64_t{7});
    REQUIRE(after.religion == "Pastafarian");
    REQUIRE(after.signature().data == gen.data_hash());
    REQUIRE(gen.get_interest("XX", std::uint64_t{7}).religion == "Flying Spaghetti");

    gen.extend_from_string("country_code\treligion\tdisabled\nXX\tFlying Spaghetti\t1\n", "remove.tsv");
    REQUIRE(gen.country_count() == base_countries);
    REQUIRE_THROWS_AS(gen.get_interest("XX", std::uint64_t{7}), std::invalid_argument);

    // Reloading the base drops every layer.
    REQUIRE(gen.load(dasmig::dataset::lite));
    REQUIRE(gen.data_hash() == base_hash);
    REQUIRE(gen.get_interest("US", std::uint64_t{7}).to_json() == before.to_json());
}

TEST_CASE("layer order matters: later layers win", "[extend]")
{
    auto gen = make_small_generator();
    gen.extend_from_string("country_code\treligion\tgroup\tweight\nXB\tDelta\tother\t0\nXB\tFoxtrot\tother\t1\n", "one.tsv");
    REQUIRE(gen.get_interest("XB", std::uint64_t{1}).religion == "Foxtrot");
    gen.extend_from_string("country_code\treligion\tgroup\tweight\nXB\tDelta\tother\t1\nXB\tFoxtrot\tother\t0\n", "two.tsv");
    REQUIRE(gen.get_interest("XB", std::uint64_t{1}).religion == "Delta");
}

TEST_CASE("a bad layer is rejected and leaves the data unchanged", "[extend][errors]")
{
    auto gen = make_loaded_generator();
    const auto hash = gen.data_hash();
    const auto count = gen.country_count();
    const auto header = std::string{"country_code\treligion\tgroup\tweight\n"};

    REQUIRE_THROWS_WITH(gen.extend_from_string(header + "XX\tZulu\tother\theavy\n", "bad.tsv"),
                        "bad.tsv:2: column 'weight': expected a number, got 'heavy'");
    REQUIRE_THROWS_AS(gen.extend_from_string(header + "XX\tZulu\tother\t-1\n", "negative.tsv"),
                      dasmig::core::tsv_error);
    REQUIRE_THROWS_AS(gen.extend_from_string(header + "xx\tZulu\tother\t1\n", "lower.tsv"),
                      dasmig::core::tsv_error); // not two capital letters
    REQUIRE_THROWS_AS(gen.extend_from_string(header + "XXX\tZulu\tother\t1\n", "long.tsv"),
                      dasmig::core::tsv_error);
    REQUIRE_THROWS_AS(gen.extend_from_string("country_code\treligion\tgroup\nXX\tZulu\tother\n", "partial.tsv"),
                      dasmig::core::tsv_error); // required column missing
    REQUIRE_THROWS_AS(gen.extend_from_string(header + "US\tChristian\tchristian\t1\n", "clash.tsv",
                                             dasmig::core::layer_mode::add),
                      dasmig::core::tsv_error); // US Christian already loaded

    REQUIRE(gen.data_hash() == hash);
    REQUIRE(gen.country_count() == count);
    REQUIRE(gen.layers().size() == 1);
}

#ifdef DASMIG_INTEREST_EMBED
TEST_CASE("compiled-in data needs no files and matches them", "[embed]")
{
    const auto old_cwd = std::filesystem::current_path();
    const auto tmp = std::filesystem::temp_directory_path() / "intg_embed_dir";
    std::filesystem::create_directories(tmp);
    std::filesystem::current_path(tmp);
    dasmig::intg gen;
    const bool loaded = gen.load(dasmig::dataset::lite);
    std::filesystem::current_path(old_cwd);
    std::filesystem::remove_all(tmp);

    REQUIRE(loaded);
    REQUIRE(gen.layers().front().source == "embedded:lite/religion.tsv");
    REQUIRE(gen.data_hash() == 0xa42bc4b2ab5bd900ULL); // same as the file
    REQUIRE(gen.get_interest("US", std::uint64_t{1}).to_json() ==
            R"({"country":"US","religion":"Unaffiliated","religion_group":"unaffiliated","signature":{"seed":"0000000000000001","data":"a42bc4b2ab5bd900"}})");
}
#endif
