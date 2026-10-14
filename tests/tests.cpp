#include "catch_amalgamated.hpp"
#include "../dasmig/interestgen.hpp"

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

using table = dasmig::intg::table;

template <typename Generator, typename... Args>
concept accepts_interest_arguments = requires(Generator& gen, Args&&... args) {
    { gen.get_interest(std::forward<Args>(args)...) } -> std::same_as<dasmig::interest>;
};

template <typename Generator>
concept has_generation_seed = requires(Generator& gen) { gen.seed(std::uint64_t{1}); };

template <typename Generator>
concept has_generation_unseed = requires(Generator& gen) { gen.unseed(); };

static_assert(accepts_interest_arguments<dasmig::intg, const dasmig::interest_request&>);
static_assert(accepts_interest_arguments<const dasmig::intg, const dasmig::interest_request&, std::uint64_t>);
static_assert(!accepts_interest_arguments<dasmig::intg>);
static_assert(!accepts_interest_arguments<dasmig::intg, std::string_view>);
static_assert(!accepts_interest_arguments<dasmig::intg, std::string_view, std::uint64_t>);
static_assert(!accepts_interest_arguments<dasmig::intg, const char*>);
static_assert(!accepts_interest_arguments<dasmig::intg, const char*, std::uint64_t>);
static_assert(!accepts_interest_arguments<dasmig::intg, std::uint64_t>);
static_assert(!accepts_interest_arguments<dasmig::intg, std::uint64_t, dasmig::interest_request>);
static_assert(!has_generation_seed<dasmig::intg>);
static_assert(!has_generation_unseed<dasmig::intg>);

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

static dasmig::interest_request request(std::string_view country, int age, dasmig::sex s)
{
    return {.country = country, .age = static_cast<std::uint8_t>(age), .person_sex = s};
}

// A small world for exact statistics.
//   Religion: XA is Christian 3 : Muslim 1 : Other 0, XB has one religion, XC has weight 0.
//   Hobbies:  a (3), b (1), c (1, from 18), d (0), e and f (class "cx", 1 each).
//   Films:    f1 and f2 (1 each).
static dasmig::intg make_small_generator()
{
    dasmig::intg gen;
    (void)gen.extend_from_string(table::religion, "country_code\treligion\tgroup\tweight\n"
                                 "XA\tAlpha\tchristian\t3\n"
                                 "XA\tBravo\tmuslim\t1\n"
                                 "XA\tCharlie\tother\t0\n"
                                 "XB\tDelta\tother\t10\n"
                                 "XC\tEcho\tother\t0\n",
                                 "small-religion.tsv");
    (void)gen.extend_from_string(table::interests, "kind\tkey\tlabel\tweight\tclass\tmin_age\n"
                                 "hobby\ta\tA\t3\t\t\n"
                                 "hobby\tb\tB\t1\t\t\n"
                                 "hobby\tc\tC\t1\t\t18\n"
                                 "hobby\td\tD\t0\t\t\n"
                                 "hobby\te\tE\t1\tcx\t\n"
                                 "hobby\tf\tF\t1\tcx\t\n"
                                 "film\tf1\tF1\t1\t\t\n"
                                 "film\tf2\tF2\t1\t\t\n",
                                 "small-interests.tsv");
    (void)gen.extend_from_string(table::age_bands, "band\tmin_age\tmax_age\n"
                                 "child\t5\t12\nteen\t13\t17\nadult\t18\t120\n",
                                 "small-bands.tsv");
    return gen;
}

static std::map<std::string, int> count_hobbies(const dasmig::intg& gen, const dasmig::interest_request& r,
                                                int draws = 20000)
{
    std::map<std::string, int> counts;
    for (std::uint64_t s = 0; s < static_cast<std::uint64_t>(draws); ++s)
    {
        for (const auto& h : gen.get_interest(r, s).hobbies) ++counts[h.key];
    }
    return counts;
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

TEST_CASE("load(dataset::lite) loads every table", "[loading]")
{
    const auto gen = make_loaded_generator();
    REQUIRE(gen.country_count() == 195);
    REQUIRE(gen.item_count() > 100);
    for (const auto t : {table::religion, table::interests, table::affinity, table::age_bands, table::regions})
    {
        REQUIRE(gen.layers(t).size() == 1);
    }
    REQUIRE(gen.unresolved().empty());
}

TEST_CASE("every lite country has shares that add up to about 100", "[loading][data]")
{
    const auto gen = make_loaded_generator();
    for (const auto* cca2 : {"US", "BR", "IN", "NG", "JP", "DE", "SA", "ID", "RU", "CN"})
    {
        double total = 0;
        for (const auto& s : gen.shares(cca2)) total += s.weight;
        REQUIRE(total > 90.0);
        REQUIRE(total < 110.0);
    }
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

TEST_CASE("the full tier has more countries, items and detail", "[loading]")
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
    REQUIRE(gen.item_count() > make_loaded_generator().item_count());
    REQUIRE(gen.unresolved().empty());
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
// Religion
// ---------------------------------------------------------------------------

TEST_CASE("get_interest throws when no data is loaded", "[generation]")
{
    dasmig::intg gen;
    REQUIRE_THROWS_AS(gen.get_interest({}), std::runtime_error);
    REQUIRE_THROWS_AS(gen.get_interest({}, std::uint64_t{1}), std::runtime_error);
    REQUIRE_THROWS_AS(gen.get_interest({.country = "US"}), std::runtime_error);
    REQUIRE_THROWS_AS(gen.get_interest({.country = "US"}, std::uint64_t{1}), std::runtime_error);
}

TEST_CASE("an unknown country is rejected", "[generation][errors]")
{
    const auto gen = make_loaded_generator();
    REQUIRE_THROWS_AS(gen.get_interest({.country = "ZZ"}, std::uint64_t{1}), std::invalid_argument);
    REQUIRE_THROWS_AS(gen.get_interest({.country = "us"}, std::uint64_t{1}), std::invalid_argument); // codes are upper case
    REQUIRE_THROWS_AS(gen.get_interest(request("ZZ", 30, dasmig::sex::male), 1), std::invalid_argument);
}

TEST_CASE("the country is reported", "[generation]")
{
    const auto gen = make_loaded_generator();
    REQUIRE(gen.get_interest({.country = "BR"}, std::uint64_t{1}).country_code == "BR");
    const auto v = gen.get_interest({}, std::uint64_t{77});
    REQUIRE(gen.get_interest({.country = v.country_code}, 77).to_json() == v.to_json()); // a random country replays
    REQUIRE(gen.shares(v.country_code).size() > 0);
}

TEST_CASE("the religion follows the country's shares", "[generation][stats]")
{
    const auto gen = make_small_generator();
    std::map<std::string, int> counts;
    constexpr int draws = 20000;
    for (std::uint64_t s = 0; s < draws; ++s) ++counts[gen.get_interest({.country = "XA"}, s).religion];
    REQUIRE(counts["Charlie"] == 0); // weight 0 is never drawn
    REQUIRE(counts["Alpha"] == Catch::Approx(draws * 0.75).margin(draws * 0.02));
    REQUIRE(counts["Bravo"] == Catch::Approx(draws * 0.25).margin(draws * 0.02));
    REQUIRE(gen.get_interest({.country = "XA"}, std::uint64_t{1}).religion_group != "");
}

TEST_CASE("a random country is uniform and skips countries with no weight", "[generation][stats]")
{
    const auto gen = make_small_generator();
    std::map<std::string, int> counts;
    constexpr int draws = 10000;
    for (std::uint64_t s = 0; s < draws; ++s) ++counts[gen.get_interest({}, s).country_code];
    REQUIRE(counts["XC"] == 0); // every share is 0
    REQUIRE(counts["XA"] == Catch::Approx(draws * 0.5).margin(draws * 0.03));
    REQUIRE(counts["XB"] == Catch::Approx(draws * 0.5).margin(draws * 0.03));
    REQUIRE_THROWS_AS(gen.get_interest({.country = "XC"}, std::uint64_t{1}), std::invalid_argument);
}

TEST_CASE("the religion does not depend on the other request fields", "[generation][seed]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t s = 0; s < 50; ++s)
    {
        const auto base = gen.get_interest({.country = "FR"}, s);
        const auto with = gen.get_interest(request("FR", 40, dasmig::sex::female), s);
        REQUIRE(base.religion == with.religion);
        auto many = request("FR", 40, dasmig::sex::female);
        many.hobbies = 8;
        many.media = 5;
        REQUIRE(gen.get_interest(many, s).religion == base.religion);
    }
}

// ---------------------------------------------------------------------------
// Determinism
// ---------------------------------------------------------------------------

TEST_CASE("get_interest with a seed is deterministic", "[generation][seed]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t s = 0; s < 100; ++s)
    {
        REQUIRE(gen.get_interest({}, s).to_json() == gen.get_interest({}, s).to_json());
        REQUIRE(gen.get_interest({.country = "FR"}, s).to_json() == gen.get_interest({.country = "FR"}, s).to_json());
        const auto r = request("DE", 33, dasmig::sex::male);
        REQUIRE(gen.get_interest(r, s).to_json() == gen.get_interest(r, s).to_json());
    }
}

TEST_CASE("a result replays from its seed", "[generation][seed]")
{
    auto gen = make_loaded_generator();
    const auto r = request("IN", 25, dasmig::sex::female);
    const auto v = gen.get_interest(r);
    REQUIRE(gen.get_interest(r, v.seed()).to_json() == v.to_json());
    const auto w = gen.get_interest({});
    REQUIRE(gen.get_interest({}, w.seed()).to_json() == w.to_json());
}

TEST_CASE("request defaults preserve the unconditioned generation behavior", "[generation][request]")
{
    const dasmig::interest_request defaults{};
    REQUIRE(defaults.country.empty());
    REQUIRE_FALSE(defaults.age.has_value());
    REQUIRE_FALSE(defaults.person_sex.has_value());
    REQUIRE(defaults.hobbies == 3);
    REQUIRE(defaults.media == 2);

    const auto gen = make_loaded_generator();
    const auto v = gen.get_interest(defaults, 42);
    REQUIRE(gen.shares(v.country_code).size() > 0);
    REQUIRE(v.hobbies.size() == 3);
    for (const auto* items : {&v.films, &v.games, &v.books, &v.music}) REQUIRE(items->size() == 2);
    REQUIRE(gen.get_interest({}, 42).to_json() == v.to_json());
    REQUIRE(gen.get_interest({.hobbies = 3, .media = 2}, 42).to_json() == v.to_json());
}

TEST_CASE("generation choices are per request and seeded calls ignore prior calls", "[generation][request][seed]")
{
    auto gen = make_loaded_generator();
    const auto other = make_loaded_generator();
    const dasmig::interest_request defaults{};
    const dasmig::interest_request custom{.country = "JP", .age = 16, .person_sex = dasmig::sex::female,
                                          .hobbies = 1, .media = 0};
    const auto hash = gen.data_hash();
    for (const std::uint64_t s : {0ULL, 42ULL, 0xffffffffffffffffULL})
    {
        const auto expected = other.get_interest(defaults, s).to_json();
        const auto limited = gen.get_interest(custom, s);
        REQUIRE(limited.country_code == "JP");
        REQUIRE(limited.hobbies.size() == 1);
        for (const auto* items : {&limited.films, &limited.games, &limited.books, &limited.music})
            REQUIRE(items->empty());

        const auto random = gen.get_interest(custom);
        REQUIRE(gen.get_interest(custom, random.seed()).to_json() == random.to_json());
        REQUIRE(gen.get_interest(defaults, s).to_json() == expected);
        REQUIRE(gen.data_hash() == hash);
    }
}

TEST_CASE("each kind has a stream of its own", "[generation][seed]")
{
    // Asking for fewer or more of one kind leaves the others unchanged, and
    // more hobbies extend the same sequence.
    const auto gen = make_loaded_generator();
    for (std::uint64_t s = 0; s < 50; ++s)
    {
        auto r = request("US", 30, dasmig::sex::male);
        const auto base = gen.get_interest(r, s);
        r.media = 0;
        const auto no_media = gen.get_interest(r, s);
        REQUIRE(no_media.hobbies.size() == base.hobbies.size());
        for (std::size_t i = 0; i < base.hobbies.size(); ++i) REQUIRE(no_media.hobbies[i].key == base.hobbies[i].key);
        REQUIRE(no_media.films.empty());
        r.media = 2;
        r.hobbies = 6;
        const auto more = gen.get_interest(r, s);
        REQUIRE(more.hobbies.size() == 6);
        for (std::size_t i = 0; i < base.hobbies.size(); ++i) REQUIRE(more.hobbies[i].key == base.hobbies[i].key);
        REQUIRE(more.music.size() == base.music.size());
        for (std::size_t i = 0; i < base.music.size(); ++i) REQUIRE(more.music[i].key == base.music[i].key);
    }
}

// ---------------------------------------------------------------------------
// Hobbies and media: rules on a small world
// ---------------------------------------------------------------------------

TEST_CASE("items are drawn without replacement and by weight", "[items][stats]")
{
    auto gen = make_small_generator();
    // One hobby: a : b : c : e : f = 3 : 1 : 1 : 1 : 1; d (weight 0) never.
    auto r = dasmig::interest_request{.country = "XB", .hobbies = 1, .media = 0};
    const auto counts = count_hobbies(gen, r);
    constexpr int draws = 20000;
    REQUIRE(counts.contains("d") == false);
    REQUIRE(counts.at("a") == Catch::Approx(draws * 3.0 / 7.0).margin(draws * 0.02));
    REQUIRE(counts.at("b") == Catch::Approx(draws / 7.0).margin(draws * 0.02));

    // Three hobbies are always three different ones.
    r.hobbies = 3;
    for (std::uint64_t s = 0; s < 500; ++s)
    {
        const auto v = gen.get_interest(r, s);
        REQUIRE(v.hobbies.size() == 3);
        std::set<std::string> keys;
        for (const auto& h : v.hobbies) keys.insert(h.key);
        REQUIRE(keys.size() == 3);
    }

    // Asking for more than there is returns what there is (5 have weight).
    r.hobbies = 20;
    r.media = 5;
    const auto all = gen.get_interest(r, 1);
    REQUIRE(all.hobbies.size() == 5);
    REQUIRE(all.films.size() == 2);
}

TEST_CASE("sex factors change what is drawn", "[items][stats]")
{
    auto gen = make_small_generator();
    (void)gen.extend_from_string(table::affinity, "target\tdimension\tvalue\tfactor\n"
                                 "a\tsex\tfemale\t0\n"
                                 "b\tsex\tmale\t0\n",
                                 "sex.tsv");
    const auto male = count_hobbies(gen, {.country = "XB", .person_sex = dasmig::sex::male, .hobbies = 2, .media = 0});
    const auto female = count_hobbies(gen, {.country = "XB", .person_sex = dasmig::sex::female, .hobbies = 2, .media = 0});
    const auto unknown = count_hobbies(gen, {.country = "XB", .hobbies = 2, .media = 0});
    REQUIRE(male.at("a") > 0);
    REQUIRE(male.contains("b") == false);
    REQUIRE(female.at("b") > 0);
    REQUIRE(female.contains("a") == false);
    REQUIRE(unknown.at("a") > 0); // an unknown sex applies no factor
    REQUIRE(unknown.at("b") > 0);
}

TEST_CASE("age limits and age bands", "[items][age]")
{
    auto gen = make_small_generator();
    (void)gen.extend_from_string(table::affinity, "target\tdimension\tvalue\tfactor\n"
                                 "b\tage_band\tchild\t0\n",
                                 "age.tsv");
    const auto child = count_hobbies(gen, {.country = "XB", .age = 8, .hobbies = 2, .media = 0});
    REQUIRE(child.contains("c") == false); // min_age 18
    REQUIRE(child.contains("b") == false); // factor 0 for the child band
    REQUIRE(child.at("a") > 0);
    const auto adult = count_hobbies(gen, {.country = "XB", .age = 30, .hobbies = 2, .media = 0});
    REQUIRE(adult.at("c") > 0);
    REQUIRE(adult.at("b") > 0);
    const auto unknown = count_hobbies(gen, {.country = "XB", .hobbies = 2, .media = 0});
    REQUIRE(unknown.at("c") > 0); // an unknown age applies no limit

    // Older than every band still uses the last band's limits only if it
    // matches one; 200 is outside 18-120 and gets no band factor.
    REQUIRE_NOTHROW(gen.get_interest({.country = "XB", .age = 200}, 1));
}

TEST_CASE("a person younger than every band draws no hobbies or media", "[items][age]")
{
    const auto gen = make_small_generator();
    const auto v = gen.get_interest({.country = "XB", .age = 3}, 1);
    REQUIRE(v.country_code == "XB");
    REQUIRE(v.religion == "Delta"); // religion is still drawn
    REQUIRE(v.hobbies.empty());
    REQUIRE(v.films.empty());
}

TEST_CASE("class factors multiply with item factors", "[items][stats]")
{
    auto gen = make_small_generator();
    // e and f share the class cx: class:cx halves both, e alone is doubled.
    (void)gen.extend_from_string(table::affinity, "target\tdimension\tvalue\tfactor\n"
                                 "class:cx\tsex\tmale\t0.5\n"
                                 "e\tsex\tmale\t2\n",
                                 "class.tsv");
    auto r = dasmig::interest_request{.country = "XB", .person_sex = dasmig::sex::male, .hobbies = 1, .media = 0};
    const auto counts = count_hobbies(gen, r);
    // weights: a 3, b 1, c 1, e 1 (2 * 0.5), f 0.5; total 6.5
    constexpr int draws = 20000;
    REQUIRE(counts.at("e") == Catch::Approx(draws * 1.0 / 6.5).margin(draws * 0.02));
    REQUIRE(counts.at("f") == Catch::Approx(draws * 0.5 / 6.5).margin(draws * 0.02));
    REQUIRE(gen.unresolved().empty());
}

TEST_CASE("a country row replaces the region row", "[items][geo]")
{
    auto gen = make_small_generator();
    (void)gen.extend_from_string(table::regions, "country_code\tregion\nXA\tR1\nXB\tR1\n", "regions.tsv");
    (void)gen.extend_from_string(table::affinity, "target\tdimension\tvalue\tfactor\n"
                                 "b\tregion\tR1\t0\n"
                                 "b\tcountry\tXB\t5\n",
                                 "geo.tsv");
    const auto xa = count_hobbies(gen, {.country = "XA", .hobbies = 2, .media = 0});
    REQUIRE(xa.contains("b") == false); // region factor 0
    const auto xb = count_hobbies(gen, {.country = "XB", .hobbies = 2, .media = 0});
    REQUIRE(xb.at("b") > 0); // the country row wins
}

TEST_CASE("religion factors use the drawn religion's group", "[items][religion]")
{
    auto gen = make_small_generator();
    (void)gen.extend_from_string(table::affinity, "target\tdimension\tvalue\tfactor\n"
                                 "a\treligion\tmuslim\t0\n",
                                 "religion.tsv");
    for (std::uint64_t s = 0; s < 300; ++s)
    {
        const auto v = gen.get_interest({.country = "XA", .hobbies = 1, .media = 0}, s);
        if (v.religion_group == "muslim") REQUIRE(v.hobbies.front().key != "a");
    }
}

TEST_CASE("misspelt affinity targets are listed, not fatal", "[items][errors]")
{
    auto gen = make_small_generator();
    (void)gen.extend_from_string(table::affinity, "target\tdimension\tvalue\tfactor\n"
                                 "nope\tsex\tmale\t0\n",
                                 "typo.tsv");
    REQUIRE(gen.unresolved().size() == 1);
    REQUIRE(gen.unresolved().front().find("unknown target 'nope'") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Hobbies and media: the shipped data
// ---------------------------------------------------------------------------

TEST_CASE("the shipped data shows the expected differences", "[items][data][stats]")
{
    const auto gen = make_loaded_generator();
    constexpr int draws = 6000;
    const auto share = [&](const dasmig::interest_request& r, const char* key) {
        return static_cast<double>(count_hobbies(gen, r, draws)[key]) / draws;
    };
    const auto male = request("US", 30, dasmig::sex::male);
    const auto female = request("US", 30, dasmig::sex::female);
    REQUIRE(share(male, "football") > 1.5 * share(female, "football"));
    REQUIRE(share(female, "knitting") > 2.0 * share(male, "knitting"));
    REQUIRE(share(female, "baking") > 1.5 * share(male, "baking"));

    const auto teen = request("US", 15, dasmig::sex::male);
    const auto senior = request("US", 70, dasmig::sex::male);
    REQUIRE(share(teen, "video_games") > 2.0 * share(senior, "video_games"));
    REQUIRE(share(senior, "gardening") > share(teen, "gardening"));
}

TEST_CASE("children get age-appropriate hobbies", "[items][data]")
{
    const auto gen = make_loaded_generator();
    std::set<std::string> seen;
    for (std::uint64_t s = 0; s < 1000; ++s)
    {
        const auto v = gen.get_interest(request("GB", 8, dasmig::sex::female), s);
        REQUIRE(v.hobbies.size() == 3);
        for (const auto& h : v.hobbies) seen.insert(h.key);
    }
    for (const auto* adult_only : {"clubbing", "pubs_and_bars", "fitness", "social_media", "diy", "car_mechanics"})
    {
        REQUIRE_FALSE(seen.contains(adult_only));
    }
    REQUIRE(seen.contains("playing_outside"));
}

TEST_CASE("genres follow age", "[items][data][stats]")
{
    const auto gen = make_loaded_generator();
    int teen_hiphop = 0, senior_hiphop = 0, teen_opera = 0, senior_opera = 0;
    for (std::uint64_t s = 0; s < 4000; ++s)
    {
        for (const auto& m : gen.get_interest(request("US", 16, dasmig::sex::male), s).music)
        {
            teen_hiphop += m.key == "music_hiphop";
            teen_opera += m.key == "music_opera";
        }
        for (const auto& m : gen.get_interest(request("US", 72, dasmig::sex::male), s).music)
        {
            senior_hiphop += m.key == "music_hiphop";
            senior_opera += m.key == "music_opera";
        }
    }
    REQUIRE(teen_hiphop > 3 * senior_hiphop);
    REQUIRE(senior_opera >= teen_opera);
}

// ---------------------------------------------------------------------------
// Conversion
// ---------------------------------------------------------------------------

TEST_CASE("string conversion and streaming give the religion", "[conversion]")
{
    const auto gen = make_loaded_generator();
    const auto v = gen.get_interest({}, std::uint64_t{5});
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
    REQUIRE(gen.data_hash() == 0xb02e44f9d7b744e9ULL);

    REQUIRE(gen.get_interest(request("US", 30, dasmig::sex::male), 1).to_json() == R"({"country":"US","religion":"Unaffiliated","religion_group":"unaffiliated","hobbies":[{"key":"hosting_dinners","label":"Hosting dinners"},{"key":"football","label":"Football"},{"key":"chess","label":"Chess"}],"films":[{"key":"film_horror","label":"Horror"},{"key":"film_documentary","label":"Documentary"}],"games":[{"key":"game_sports","label":"Sports"},{"key":"game_puzzle","label":"Puzzle"}],"books":[{"key":"book_fantasy","label":"Fantasy"},{"key":"book_crime","label":"Crime and mystery"}],"music":[{"key":"music_rock","label":"Rock"},{"key":"music_metal","label":"Metal"}],"signature":{"seed":"0000000000000001","data":"b02e44f9d7b744e9"}})");
    REQUIRE(gen.get_interest(request("JP", 16, dasmig::sex::female), 2).to_json() == R"({"country":"JP","religion":"Buddhist","religion_group":"buddhist","hobbies":[{"key":"languages","label":"Learning languages"},{"key":"gardening","label":"Gardening"},{"key":"comics","label":"Comics and manga"}],"films":[{"key":"film_scifi","label":"Science fiction"},{"key":"film_comedy","label":"Comedy"}],"games":[{"key":"game_action_adventure","label":"Action-adventure"},{"key":"game_sandbox","label":"Sandbox and building"}],"books":[{"key":"book_romance","label":"Romance"},{"key":"book_crime","label":"Crime and mystery"}],"music":[{"key":"music_rnb","label":"R&B and soul"},{"key":"music_pop","label":"Pop"}],"signature":{"seed":"0000000000000002","data":"b02e44f9d7b744e9"}})");
    REQUIRE(gen.get_interest(request("BR", 70, dasmig::sex::female), 3).to_json() == R"({"country":"BR","religion":"Christian","religion_group":"christian","hobbies":[{"key":"board_games","label":"Board games"},{"key":"writing","label":"Writing"},{"key":"collecting","label":"Collecting"}],"films":[{"key":"film_romance","label":"Romance"},{"key":"film_drama","label":"Drama"}],"games":[{"key":"game_sports","label":"Sports"},{"key":"game_simulation","label":"Simulation"}],"books":[{"key":"book_history","label":"History"},{"key":"book_romance","label":"Romance"}],"music":[{"key":"music_country","label":"Country"},{"key":"music_classical","label":"Classical"}],"signature":{"seed":"0000000000000003","data":"b02e44f9d7b744e9"}})");
    REQUIRE(gen.get_interest({}, std::uint64_t{4}).to_json() == R"({"country":"MZ","religion":"Other","religion_group":"other","hobbies":[{"key":"fishing","label":"Fishing"},{"key":"gardening","label":"Gardening"},{"key":"social_media","label":"Social media"}],"films":[{"key":"film_animation","label":"Animation"},{"key":"film_horror","label":"Horror"}],"games":[{"key":"game_shooter","label":"Shooter"},{"key":"game_strategy","label":"Strategy"}],"books":[{"key":"book_scifi","label":"Science fiction"},{"key":"book_thriller","label":"Thriller"}],"music":[{"key":"music_pop","label":"Pop"},{"key":"music_country","label":"Country"}],"signature":{"seed":"0000000000000004","data":"b02e44f9d7b744e9"}})");

    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::uint64_t s = 0; s < 10000; ++s)
    {
        for (const char c : gen.get_interest({}, s).to_json())
        {
            h ^= static_cast<unsigned char>(c);
            h *= 0x100000001b3ULL;
        }
    }
    REQUIRE(h == 0xbf4d37e1cb1d7b59ULL);
}

// ---------------------------------------------------------------------------
// JSON and signatures
// ---------------------------------------------------------------------------

TEST_CASE("to_json and signature", "[json]")
{
    const auto gen = make_loaded_generator();
    const auto v = gen.get_interest(request("DE", 40, dasmig::sex::female), 99);
    REQUIRE(v.signature().seed == 99);
    REQUIRE(v.seed() == 99);
    REQUIRE(v.signature().data == gen.data_hash());
    const auto json = v.to_json();
    REQUIRE(json.starts_with(R"({"country":"DE","religion":")"));
    REQUIRE(json.find(R"("hobbies":[{"key":")") != std::string::npos);
    for (const auto* name : {R"("films":[)", R"("games":[)", R"("books":[)", R"("music":[)"})
    {
        REQUIRE(json.find(name) != std::string::npos);
    }
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

TEST_CASE("extend overrides, adds and removes religion rows", "[extend]")
{
    auto gen = make_loaded_generator();
    const auto base_countries = gen.country_count();
    const auto base_hash = gen.data_hash();
    const auto before = gen.get_interest({.country = "US"}, std::uint64_t{7});

    // A layer that makes the US all "Pastafarian" and adds a new country.
    std::string layer = "country_code\treligion\tgroup\tweight\n";
    for (const auto& s : gen.shares("US"))
    {
        layer += "US\t" + s.religion + "\t" + s.group + "\t0\n";
    }
    layer += "US\tPastafarian\tother\t1\nXX\tFlying Spaghetti\tother\t5\n";
    const auto& info = gen.extend_from_string(table::religion, layer, "mod.tsv");
    REQUIRE(info.added == 2);
    REQUIRE(info.replaced == gen.shares("US").size() - 1);
    REQUIRE(gen.country_count() == base_countries + 1);
    REQUIRE(gen.data_hash() != base_hash);
    REQUIRE(gen.layers(table::religion).size() == 2);
    REQUIRE(gen.layers(table::interests).size() == 1);
    const auto after = gen.get_interest({.country = "US"}, std::uint64_t{7});
    REQUIRE(after.religion == "Pastafarian");
    REQUIRE(after.signature().data == gen.data_hash());
    REQUIRE(gen.get_interest({.country = "XX"}, std::uint64_t{7}).religion == "Flying Spaghetti");

    gen.extend_from_string(table::religion, "country_code\treligion\tdisabled\nXX\tFlying Spaghetti\t1\n", "remove.tsv");
    REQUIRE(gen.country_count() == base_countries);
    REQUIRE_THROWS_AS(gen.get_interest({.country = "XX"}, std::uint64_t{7}), std::invalid_argument);

    // Reloading the base drops every layer.
    REQUIRE(gen.load(dasmig::dataset::lite));
    REQUIRE(gen.data_hash() == base_hash);
    REQUIRE(gen.get_interest({.country = "US"}, std::uint64_t{7}).to_json() == before.to_json());
}

TEST_CASE("extend adds and removes hobbies", "[extend]")
{
    auto gen = make_loaded_generator();
    const auto count = gen.item_count();
    gen.extend_from_string(table::interests, "kind\tkey\tlabel\tweight\nhobby\tzorbing\tZorbing\t1000\n", "mod.tsv");
    REQUIRE(gen.item_count() == count + 1);
    bool seen = false;
    for (std::uint64_t s = 0; s < 20 && !seen; ++s)
    {
        for (const auto& h : gen.get_interest({.country = "US"}, s).hobbies) seen = seen || h.key == "zorbing";
    }
    REQUIRE(seen);
    gen.extend_from_string(table::interests, "kind\tkey\tdisabled\nhobby\tzorbing\t1\n", "remove.tsv");
    REQUIRE(gen.item_count() == count);
}

TEST_CASE("layer order matters: later layers win", "[extend]")
{
    auto gen = make_small_generator();
    gen.extend_from_string(table::religion, "country_code\treligion\tgroup\tweight\nXB\tDelta\tother\t0\nXB\tFoxtrot\tother\t1\n", "one.tsv");
    REQUIRE(gen.get_interest({.country = "XB"}, std::uint64_t{1}).religion == "Foxtrot");
    gen.extend_from_string(table::religion, "country_code\treligion\tgroup\tweight\nXB\tDelta\tother\t1\nXB\tFoxtrot\tother\t0\n", "two.tsv");
    REQUIRE(gen.get_interest({.country = "XB"}, std::uint64_t{1}).religion == "Delta");
}

TEST_CASE("a bad layer is rejected and leaves the data unchanged", "[extend][errors]")
{
    auto gen = make_loaded_generator();
    const auto hash = gen.data_hash();
    const auto count = gen.country_count();
    const auto header = std::string{"country_code\treligion\tgroup\tweight\n"};

    REQUIRE_THROWS_WITH(gen.extend_from_string(table::religion, header + "XX\tZulu\tother\theavy\n", "bad.tsv"),
                        "bad.tsv:2: column 'weight': expected a number, got 'heavy'");
    REQUIRE_THROWS_AS(gen.extend_from_string(table::religion, header + "XX\tZulu\tother\t-1\n", "negative.tsv"),
                      dasmig::core::tsv_error);
    REQUIRE_THROWS_AS(gen.extend_from_string(table::religion, header + "xx\tZulu\tother\t1\n", "lower.tsv"),
                      dasmig::core::tsv_error); // not two capital letters
    REQUIRE_THROWS_AS(gen.extend_from_string(table::religion, header + "XXX\tZulu\tother\t1\n", "long.tsv"),
                      dasmig::core::tsv_error);
    REQUIRE_THROWS_AS(gen.extend_from_string(table::religion, "country_code\treligion\tgroup\nXX\tZulu\tother\n", "partial.tsv"),
                      dasmig::core::tsv_error); // required column missing
    REQUIRE_THROWS_AS(gen.extend_from_string(table::religion, header + "US\tChristian\tchristian\t1\n", "clash.tsv",
                                             dasmig::core::layer_mode::add),
                      dasmig::core::tsv_error); // US Christian already loaded
    REQUIRE_THROWS_AS(gen.extend_from_string(table::interests, "kind\tkey\tlabel\tweight\nsong\tx\tX\t1\n", "kind.tsv"),
                      dasmig::core::tsv_error); // not a kind
    REQUIRE_THROWS_AS(gen.extend_from_string(table::interests, "kind\tkey\tlabel\tweight\tmin_age\tmax_age\nhobby\tx\tX\t1\t30\t20\n", "ages.tsv"),
                      dasmig::core::tsv_error); // max below min
    REQUIRE_THROWS_AS(gen.extend_from_string(table::affinity, "target\tdimension\tvalue\tfactor\nfootball\tmood\thappy\t2\n", "dim.tsv"),
                      dasmig::core::tsv_error); // not a dimension
    REQUIRE_THROWS_AS(gen.extend_from_string(table::affinity, "target\tdimension\tvalue\tfactor\nfootball\tsex\tother\t2\n", "sex.tsv"),
                      dasmig::core::tsv_error);
    REQUIRE_THROWS_AS(gen.extend_from_string(table::affinity, "target\tdimension\tvalue\tfactor\nfootball\tsex\tmale\t-2\n", "neg.tsv"),
                      dasmig::core::tsv_error);
    REQUIRE_THROWS_AS(gen.extend_from_string(table::age_bands, "band\tmin_age\tmax_age\nx\t10\t5\n", "band.tsv"),
                      dasmig::core::tsv_error);

    REQUIRE(gen.data_hash() == hash);
    REQUIRE(gen.country_count() == count);
    for (const auto t : {table::religion, table::interests, table::affinity, table::age_bands, table::regions})
    {
        REQUIRE(gen.layers(t).size() == 1);
    }
}

TEST_CASE("a directory layer is applied to all tables or none", "[extend][errors]")
{
    const auto dir = std::filesystem::temp_directory_path() / "intg_layer_dir";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const auto write = [&](const char* name, const char* text) { std::ofstream{dir / name, std::ios::binary} << text; };
    write("religion.tsv", "country_code\treligion\tgroup\tweight\nXX\tSun\tother\t1\n");
    write("affinity.tsv", "target\tdimension\tvalue\tfactor\nfootball\tmood\thappy\t2\n"); // bad

    auto gen = make_loaded_generator();
    const auto hash = gen.data_hash();
    REQUIRE_THROWS_AS(gen.extend(dir), dasmig::core::tsv_error);
    REQUIRE(gen.data_hash() == hash); // the valid religion file was not applied either
    REQUIRE(gen.shares("XX").empty());

    write("affinity.tsv", "target\tdimension\tvalue\tfactor\nfootball\tsex\tmale\t2\n"); // fixed
    gen.extend(dir);
    REQUIRE(gen.shares("XX").size() == 1);
    REQUIRE(gen.layers(table::religion).size() == 2);
    REQUIRE(gen.layers(table::affinity).size() == 2);
    REQUIRE(gen.layers(table::interests).size() == 1);

    REQUIRE_THROWS_AS(gen.extend(dir / "other.tsv"), std::invalid_argument); // not a table name
    std::filesystem::remove_all(dir);
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
    REQUIRE(gen.layers(table::religion).front().source == "embedded:lite/religion.tsv");
    REQUIRE(gen.data_hash() == 0xb02e44f9d7b744e9ULL); // same as the file
    REQUIRE(gen.get_interest(request("US", 30, dasmig::sex::male), 1).to_json() ==
            R"({"country":"US","religion":"Unaffiliated","religion_group":"unaffiliated","hobbies":[{"key":"hosting_dinners","label":"Hosting dinners"},{"key":"football","label":"Football"},{"key":"chess","label":"Chess"}],"films":[{"key":"film_horror","label":"Horror"},{"key":"film_documentary","label":"Documentary"}],"games":[{"key":"game_sports","label":"Sports"},{"key":"game_puzzle","label":"Puzzle"}],"books":[{"key":"book_fantasy","label":"Fantasy"},{"key":"book_crime","label":"Crime and mystery"}],"music":[{"key":"music_rock","label":"Rock"},{"key":"music_metal","label":"Metal"}],"signature":{"seed":"0000000000000001","data":"b02e44f9d7b744e9"}})");
}
#endif
