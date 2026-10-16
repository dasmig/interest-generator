// Benchmarks for interest-generator; see bench.hpp for the options.

#include "../dasmig/interestgen.hpp"
#include "bench.hpp"

int main(int argc, char** argv)
{
    dasmig::bench::runner run{"interest-generator", argc, argv};

    run.load("load", [&] {
        dasmig::intg gen;
        return gen.load(run.tier());
    });

    dasmig::intg gen;
    if (!gen.load(run.tier())) return run.missing_tier();

    run.calls("get_interest", [&](std::uint64_t seed) { return gen.get_interest({}, seed); });
    run.calls("get_interest BR female 30", [&](std::uint64_t seed) {
        return gen.get_interest({.country = "BR", .age = std::uint8_t{30}, .person_sex = dasmig::sex::female}, seed);
    });
    run.calls("get_interest to_json", [&](std::uint64_t seed) {
        return gen.get_interest({}, seed).to_json();
    });

    run.batches("get_interest_batch", [&](std::size_t count, std::size_t threads, std::uint64_t seed) {
        return gen.get_interest_batch({}, {.count = count, .threads = threads}, seed);
    });

    return run.finish();
}
