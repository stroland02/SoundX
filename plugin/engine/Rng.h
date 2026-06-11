#pragma once
// JUCE-free xorshift32 — deterministic, allocation-free randomness for RT code.
#include <cstdint>

namespace soundx::engine {

class Rng {
public:
    explicit Rng(std::uint32_t seed = 0x9e3779b9u) : state_(seed != 0 ? seed : 1u) {}

    std::uint32_t next() noexcept {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    // uniform in [0, 1)
    float next01() noexcept {
        return float(next() >> 8) * (1.0f / 16777216.0f);
    }

private:
    std::uint32_t state_;
};

} // namespace soundx::engine
