#include "../dasmig/interestgen.hpp"
#include <cstdint>
#include <iostream>

namespace
{
void print(const dasmig::interest& v)
{
    std::cout << "  " << v.country_code << ", " << v.religion << '\n';
    const auto line = [](const char* title, const std::vector<dasmig::interest_item>& items) {
        std::cout << "    " << title << ":";
        for (const auto& i : items) std::cout << ' ' << i.label << ';';
        std::cout << '\n';
    };
    line("hobbies", v.hobbies);
    line("films", v.films);
    line("games", v.games);
    line("books", v.books);
    line("music", v.music);
}
} // namespace

int main()
{
    // The singleton loads the lite tier (compiled in, or from resources/).
    auto& gen = dasmig::intg::instance();
    if (!gen.has_data())
    {
        std::cerr << "Could not load the interest data.\n";
        return 1;
    }
    std::cout << "Loaded " << gen.country_count() << " countries and " << gen.item_count()
              << " hobbies and genres.\n\n";

    std::cout << "A 16-year-old girl in Japan:\n";
    print(gen.get_interest({.country = "JP", .age = 16, .person_sex = dasmig::sex::female}));

    std::cout << "\nA 70-year-old man in Brazil:\n";
    print(gen.get_interest({.country = "BR", .age = 70, .person_sex = dasmig::sex::male}));

    std::cout << "\nA random person (country, age and sex unknown):\n";
    print(gen.get_interest({}));

    std::cout << "\nSeed 42 in India, twice:\n";
    const dasmig::interest_request request{.country = "IN"};
    const auto a = gen.get_interest(request, std::uint64_t{42});
    const auto b = gen.get_interest(request, std::uint64_t{42});
    std::cout << "  " << a.to_json() << '\n'
              << "  " << b.to_json() << '\n';

    std::cout << "\nWith a layer that adds a hobby everyone in Iceland has:\n";
    gen.extend_from_string(dasmig::intg::table::interests,
                           "kind\tkey\tlabel\tweight\nhobby\tgeothermal_bathing\tGeothermal bathing\t1000\n",
                           "example.tsv");
    for (int i = 0; i < 3; ++i)
    {
        print(gen.get_interest({.country = "IS"}));
    }
    return 0;
}
