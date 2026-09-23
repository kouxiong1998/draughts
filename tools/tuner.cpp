// tools/tuner.cpp
// Reads a selfplay binary (from selfplay_gen), runs Texel-style batch
// gradient descent on the PST weights, prints train/val loss per epoch,
// saves the best weights to an .ini file.
//
// Internally weights are stored as doubles. Integer rounding happens only
// on save. This avoids the classic "gradient too small for integer step"
// bug where weights never move.

#include "core/Board.hpp"
#include "core/BoardConstants.hpp"
#include "ai/PST.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace draughts;
using namespace draughts::ai;

namespace {

// ?? Binary record layout (must match tools/selfplay_gen.cpp) ???????????????
struct Record {
    std::uint64_t red;
    std::uint64_t yellow;
    std::uint64_t kings;
    std::uint8_t  sideToMove;
    std::uint8_t  result;
    std::uint8_t  pad0[6];
};
static_assert(sizeof(Record) == 32, "Record layout mismatch");

struct FileHeader {
    char          magic[4];
    std::uint32_t version;
    std::uint32_t recordSize;
    std::uint64_t recordCount;
    std::uint8_t  ruleVariant;
    std::uint8_t  pad0[7];
};
static_assert(sizeof(FileHeader) == 32, "Header layout mismatch");

// ?? Doubles weights (tuner-internal) ??????????????????????????????????????
struct DoubleWeights {
    std::array<double, core::kNumPlayableSquares> men{};
    std::array<double, core::kNumPlayableSquares> kings{};

    static DoubleWeights fromPst(const pst::Weights& w) {
        DoubleWeights d{};
        for (std::size_t i = 0; i < core::kNumPlayableSquares; ++i) {
            d.men  [i] = static_cast<double>(w.men  [i]);
            d.kings[i] = static_cast<double>(w.kings[i]);
        }
        return d;
    }

    [[nodiscard]] pst::Weights toPst() const {
        pst::Weights w{};
        for (std::size_t i = 0; i < core::kNumPlayableSquares; ++i) {
            w.men  [i] = static_cast<int>(std::lround(men  [i]));
            w.kings[i] = static_cast<int>(std::lround(kings[i]));
        }
        return w;
    }
};

// ?? One training sample ???????????????????????????????????????????????????
struct Sample {
    std::array<std::int8_t, core::kNumPlayableSquares> coeffMen{};
    std::array<std::int8_t, core::kNumPlayableSquares> coeffKings{};
    std::int16_t material;
    std::int8_t  sign;    // +1 if Red to move, -1 if Yellow
    float        target;  // 1.0 win, 0.5 draw, 0.0 loss

    [[nodiscard]] double evalWith(const DoubleWeights& w) const noexcept {
        double s = static_cast<double>(material);
        for (int sq = 0; sq < core::kNumPlayableSquares; ++sq) {
            s += w.men  [sq] * coeffMen  [sq];
            s += w.kings[sq] * coeffKings[sq];
        }
        return s * sign;
    }
};

// ?? File loading ??????????????????????????????????????????????????????????
std::vector<Sample> loadSamples(const std::string& path,
                                core::RuleSet& rulesOut,
                                bool& ok)
{
    ok = false;
    std::vector<Sample> out;

    std::ifstream in(path, std::ios::binary);
    if (!in) return out;

    FileHeader hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (std::memcmp(hdr.magic, "DSPG", 4) != 0 || hdr.version != 1) return out;
    if (hdr.recordSize != sizeof(Record)) return out;

    rulesOut = (hdr.ruleVariant == 1)
        ? core::RuleSet::InternationalFreeCapture
        : core::RuleSet::InternationalMaxCapture;

    out.reserve(static_cast<std::size_t>(hdr.recordCount));

    for (std::uint64_t i = 0; i < hdr.recordCount; ++i) {
        Record r{};
        if (!in.read(reinterpret_cast<char*>(&r), sizeof(r))) break;

        Sample s{};
        s.sign = (r.sideToMove == 0) ? +1 : -1;

        const core::Bitboard redMen    = r.red    & ~r.kings;
        const core::Bitboard redKings  = r.red    &  r.kings;
        const core::Bitboard yellMen   = r.yellow & ~r.kings;
        const core::Bitboard yellKings = r.yellow &  r.kings;

        auto scan = [](core::Bitboard bb, auto fn) {
            while (bb) {
                const auto sq = static_cast<core::Square>(std::countr_zero(bb));
                bb &= bb - 1;
                fn(sq);
            }
        };

        scan(redMen,    [&](core::Square sq){ s.coeffMen  [sq] += 1; });
        scan(redKings,  [&](core::Square sq){ s.coeffKings[sq] += 1; });
        scan(yellMen,   [&](core::Square sq){
            const auto m = core::bc::mirrorSquare(sq);
            s.coeffMen[m] -= 1;
        });
        scan(yellKings, [&](core::Square sq){
            const auto m = core::bc::mirrorSquare(sq);
            s.coeffKings[m] -= 1;
        });

        const int redMenN    = std::popcount(redMen);
        const int redKingsN  = std::popcount(redKings);
        const int yellMenN   = std::popcount(yellMen);
        const int yellKingsN = std::popcount(yellKings);

        constexpr int kMan  = 100;
        constexpr int kKing = 300;

        s.material = static_cast<std::int16_t>(
            (redMenN * kMan + redKingsN * kKing)
          - (yellMenN * kMan + yellKingsN * kKing));

        switch (r.result) {
            case 2: s.target = 1.0f; break;
            case 1: s.target = 0.5f; break;
            case 0: s.target = 0.0f; break;
            default: s.target = 0.5f; break;
        }

        out.push_back(s);
    }

    ok = !out.empty();
    return out;
}

// ?? Loss / gradient ???????????????????????????????????????????????????????
double sigmoid(double x) {
    if (x >= 0) {
        const double e = std::exp(-x);
        return 1.0 / (1.0 + e);
    } else {
        const double e = std::exp(x);
        return e / (1.0 + e);
    }
}

double lossOf(const std::vector<Sample>& samples,
              const DoubleWeights& w,
              double K)
{
    if (samples.empty()) return 0.0;
    double total = 0.0;
    for (const auto& s : samples) {
        const double p = sigmoid(s.evalWith(w) / K);
        const double d = p - s.target;
        total += d * d;
    }
    return total / static_cast<double>(samples.size());
}

void accumulateGradient(const std::vector<Sample>& samples,
                        const DoubleWeights& w,
                        double K,
                        std::array<double, core::kNumPlayableSquares>& gMen,
                        std::array<double, core::kNumPlayableSquares>& gKings)
{
    gMen.fill(0.0);
    gKings.fill(0.0);

    for (const auto& s : samples) {
        const double z  = s.evalWith(w) / K;
        const double p  = sigmoid(z);
        const double d  = p - s.target;
        const double dp = p * (1.0 - p);

        const double shared = 2.0 * d * dp / K;
        const double sgn    = static_cast<double>(s.sign);

        for (int sq = 0; sq < core::kNumPlayableSquares; ++sq) {
            gMen  [sq] += shared * sgn * static_cast<double>(s.coeffMen  [sq]);
            gKings[sq] += shared * sgn * static_cast<double>(s.coeffKings[sq]);
        }
    }

    const double n = static_cast<double>(samples.size());
    if (n > 0) {
        for (auto& v : gMen)   v /= n;
        for (auto& v : gKings) v /= n;
    }
}

void clipWeights(DoubleWeights& w, double limit) {
    auto clipArr = [&](auto& arr) {
        for (auto& v : arr) v = std::clamp(v, -limit, limit);
    };
    clipArr(w.men);
    clipArr(w.kings);
}

// ?? Args ??????????????????????????????????????????????????????????????????
struct Options {
    std::string   input;
    std::string   output = "data/weights_tuned.ini";
    int           epochs = 200;
    double        lr     = 2000.0;   // scaled for mean-gradient descent
    double        K      = 300.0;
    double        valFraction = 0.2;
    std::uint64_t seed = 0xC0FFEEULL;
};

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto nextStr = [&]() -> std::string { return (i + 1 < argc) ? std::string(argv[++i]) : std::string{}; };
        auto nextInt = [&]() -> int { return (i + 1 < argc) ? std::stoi(argv[++i]) : 0; };
        auto nextDbl = [&]() -> double { return (i + 1 < argc) ? std::stod(argv[++i]) : 0.0; };

        if      (a == "--in")      o.input       = nextStr();
        else if (a == "--out")     o.output      = nextStr();
        else if (a == "--epochs")  o.epochs      = nextInt();
        else if (a == "--lr")      o.lr          = nextDbl();
        else if (a == "--K")       o.K           = nextDbl();
        else if (a == "--val")     o.valFraction = nextDbl();
        else if (a == "--seed")    o.seed        = std::stoull(nextStr());
        else if (a == "--help" || a == "-h") {
            std::printf(
                "usage: tuner --in FILE [--out FILE] [--epochs N] [--lr X]\n"
                "             [--K X] [--val X] [--seed N]\n");
            std::exit(0);
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
            std::exit(2);
        }
    }
    return o;
}

} // namespace

int main(int argc, char** argv) {
    const Options o = parseArgs(argc, argv);
    if (o.input.empty()) {
        std::fprintf(stderr, "error: --in FILE required\n");
        return 2;
    }

    core::RuleSet rules = core::RuleSet::InternationalMaxCapture;
    bool ok = false;
    auto samples = loadSamples(o.input, rules, ok);
    if (!ok) {
        std::fprintf(stderr, "failed to load %s\n", o.input.c_str());
        return 1;
    }

    std::printf("tuner\n");
    std::printf("  input       %s\n", o.input.c_str());
    std::printf("  output      %s\n", o.output.c_str());
    std::printf("  samples     %zu\n", samples.size());
    std::printf("  rules       %s\n",
        rules == core::RuleSet::InternationalFreeCapture ? "off (free)" : "on (majority)");
    std::printf("  epochs      %d\n", o.epochs);
    std::printf("  lr          %g\n", o.lr);
    std::printf("  K           %g\n", o.K);
    std::printf("  val         %g\n", o.valFraction);
    std::printf("  seed        0x%llx\n", (unsigned long long)o.seed);
    std::printf("\n");

    std::mt19937_64 rng(o.seed);
    std::shuffle(samples.begin(), samples.end(), rng);

    const std::size_t nVal = static_cast<std::size_t>(samples.size() * o.valFraction);
    std::vector<Sample> val(samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(nVal));
    std::vector<Sample> train(samples.begin() + static_cast<std::ptrdiff_t>(nVal), samples.end());

    std::printf("  train       %zu\n", train.size());
    std::printf("  val         %zu\n", val.size());
    std::printf("\n");

    DoubleWeights w = DoubleWeights::fromPst(pst::defaultWeights(rules));
    const double baseLossTrain = lossOf(train, w, o.K);
    const double baseLossVal   = lossOf(val,   w, o.K);
    std::printf("baseline  train %.6f   val %.6f\n\n", baseLossTrain, baseLossVal);

    // Track the best validation loss and the weights at that point.
    double     bestVal   = baseLossVal;
    DoubleWeights bestW  = w;

    std::array<double, core::kNumPlayableSquares> gMen{}, gKings{};

    for (int epoch = 1; epoch <= o.epochs; ++epoch) {
        std::shuffle(train.begin(), train.end(), rng);
        accumulateGradient(train, w, o.K, gMen, gKings);

        for (int sq = 0; sq < core::kNumPlayableSquares; ++sq) {
            w.men  [sq] -= o.lr * gMen  [sq];
            w.kings[sq] -= o.lr * gKings[sq];
        }
        clipWeights(w, 60.0);

        if (epoch % 10 == 0 || epoch == 1 || epoch == o.epochs) {
            const double tr = lossOf(train, w, o.K);
            const double vl = lossOf(val,   w, o.K);
            std::printf("epoch %4d   train %.6f   val %.6f", epoch, tr, vl);
            if (vl < bestVal) {
                bestVal = vl;
                bestW   = w;
                std::printf("   *");
            }
            std::printf("\n");
            std::fflush(stdout);
        } else {
            const double vl = lossOf(val, w, o.K);
            if (vl < bestVal) { bestVal = vl; bestW = w; }
        }
    }

    const double finalTrain = lossOf(train, bestW, o.K);
    const double finalVal   = lossOf(val,   bestW, o.K);
    std::printf("\n");
    std::printf("baseline  train %.6f  val %.6f\n", baseLossTrain, baseLossVal);
    std::printf("best      train %.6f  val %.6f\n", finalTrain,   finalVal);
    std::printf("delta     train %+.6f  val %+.6f\n",
                finalTrain - baseLossTrain, finalVal - baseLossVal);
    std::printf("\nSaving best-validation weights.\n");

    pst::saveToFile(o.output, bestW.toPst());
    std::printf("wrote %s\n", o.output.c_str());
    return 0;
}
