// tools/perft_main.cpp
// Small CLI harness that prints perft node counts for both rule variants.
// Never linked into the game; used only to generate reference tables.

#include "core/Board.hpp"
#include "core/Perft.hpp"
#include "core/RuleSet.hpp"

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace draughts::core;

namespace {
bool parseDepth(std::string_view s, int& out) {
    const auto* b = s.data();
    const auto* e = s.data() + s.size();
    const auto r = std::from_chars(b, e, out);
    return r.ec == std::errc{} && r.ptr == e && out >= 1 && out <= 12;
}

void printRuleSet(RuleSet r) {
    std::cout << (r == RuleSet::InternationalMaxCapture ? "on" : "off");
}
} // namespace

int main(int argc, char** argv) {
    int  depth = 6;
    bool divide = false;
    RuleSet rules = RuleSet::InternationalMaxCapture;

    for (int i = 1; i < argc; ++i) {
        const std::string_view a{argv[i]};
        if (a == "--depth" && i + 1 < argc) {
            if (!parseDepth(argv[++i], depth)) {
                std::cerr << "bad --depth\n"; return 2;
            }
        } else if (a == "--rules" && i + 1 < argc) {
            const std::string_view v{argv[++i]};
            if      (v == "on")  rules = RuleSet::InternationalMaxCapture;
            else if (v == "off") rules = RuleSet::InternationalFreeCapture;
            else { std::cerr << "bad --rules (use on|off)\n"; return 2; }
        } else if (a == "--divide") {
            divide = true;
        } else if (a == "--help" || a == "-h") {
            std::cout <<
              "usage: perft [--depth N] [--rules on|off] [--divide]\n";
            return 0;
        } else {
            std::cerr << "unknown arg: " << a << "\n"; return 2;
        }
    }

    Board board; board.resetStandard();

    std::cout << "rules=";
    printRuleSet(rules);
    std::cout << " depth=" << depth << "\n";

    if (divide) {
        const auto rows = perftDivide(board, Color::Red, rules, depth);
        std::uint64_t total = 0;
        for (const auto& r : rows) {
            std::cout << r.move << "  " << r.nodes << "\n";
            total += r.nodes;
        }
        std::cout << "total  " << total << "\n";
        return 0;
    }

    for (int d = 1; d <= depth; ++d) {
        const auto r = perft(board, Color::Red, rules, d);
        std::cout << "depth " << d
                  << "  nodes="      << r.nodes
                  << "  captures="   << r.captures
                  << "  promotions=" << r.promotions
                  << "  ms="         << r.milliseconds
                  << "\n";
    }
    return 0;
}
