#include "../dasmig/interestgen.hpp"
#include <cstdint>
#include <iostream>

int main()
{
    // The singleton loads the lite tier (compiled in, or from resources/).
    auto& gen = dasmig::intg::instance();
    if (!gen.has_data())
    {
        std::cerr << "Could not load the interest data.\n";
        return 1;
    }
    std::cout << "Loaded religion shares for " << gen.country_count() << " countries.\n\n";

    std::cout << "Random countries:\n";
    for (int i = 0; i < 5; ++i)
    {
        const auto v = gen.get_interest();
        std::cout << "  " << v.country_code << ": " << v << '\n';
    }

    std::cout << "\nBrazil, ten people:\n ";
    for (int i = 0; i < 10; ++i)
    {
        std::cout << ' ' << gen.get_interest("BR");
    }
    std::cout << "\n\nSeed 42 in India, twice:\n";
    const auto a = gen.get_interest("IN", std::uint64_t{42});
    const auto b = gen.get_interest("IN", std::uint64_t{42});
    std::cout << "  " << a.to_json() << '\n'
              << "  " << b.to_json() << '\n';

    std::cout << "\nWith a layer that makes Iceland 100% Pastafarian:\n";
    gen.extend_from_string("country_code\treligion\tgroup\tweight\n"
                           "IS\tChristian\tchristian\t0\n"
                           "IS\tUnaffiliated\tunaffiliated\t0\n"
                           "IS\tOther\tother\t0\n"
                           "IS\tPastafarian\tother\t100\n",
                           "example.tsv");
    for (int i = 0; i < 3; ++i)
    {
        std::cout << "  " << gen.get_interest("IS") << '\n';
    }
    return 0;
}
