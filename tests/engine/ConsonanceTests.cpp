#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "engine/analysis/Consonance.h"
#include "engine/SpectralModel.h"

using namespace soundx::engine;
using Catch::Approx;

namespace {
// two-note chord as 6-harmonic complexes with 1/k amplitude rolloff
std::vector<SpectralPartial> dyad(float f1, float f2) {
    std::vector<SpectralPartial> p;
    for (int k = 1; k <= 6; ++k) {
        p.push_back({f1 * float(k), 1.0f / float(k)});
        p.push_back({f2 * float(k), 1.0f / float(k)});
    }
    return p;
}
} // namespace

TEST_CASE("a perfect fifth is more consonant than a tritone") {
    const auto fifth = dyad(440.0f, 660.0f);
    const auto tritone = dyad(440.0f, 440.0f * std::sqrt(2.0f));
    const float cFifth = consonanceScore01(fifth.data(), fifth.size());
    const float cTritone = consonanceScore01(tritone.data(), tritone.size());
    REQUIRE(cFifth > cTritone);
}

TEST_CASE("consonance score is bounded between zero and one") {
    const auto fifth = dyad(440.0f, 660.0f);
    const float c = consonanceScore01(fifth.data(), fifth.size());
    REQUIRE(c > 0.0f);
    REQUIRE(c <= 1.0f);

    // single partial: no pairs, perfectly consonant
    SpectralPartial solo{440.0f, 1.0f};
    REQUIRE(consonanceScore01(&solo, 1) == Approx(1.0f));
}

TEST_CASE("interval naming identifies tempered intervals") {
    const auto p5 = nearestInterval(1.5f);
    REQUIRE(std::string(p5.name) == "P5");
    REQUIRE(p5.cents == Approx(2.0f).margin(1.0f)); // just ratio 3:2 is +1.96c vs tempered

    const auto octave = nearestInterval(2.0f);
    REQUIRE(std::string(octave.name) == "Octave");
    REQUIRE(octave.cents == Approx(0.0f).margin(0.5f));

    const auto unison = nearestInterval(1.0f);
    REQUIRE(std::string(unison.name) == "Unison");

    const auto tt = nearestInterval(std::sqrt(2.0f));
    REQUIRE(std::string(tt.name) == "TT");
}
