#pragma once

#include <cstdint>

namespace daa {

// SplitMix64 pseudo-random generator.
//
// Used instead of <random> distributions because the standard does not fix
// their output: std::uniform_int_distribution gives different numbers on GCC,
// Clang and MSVC, which would make "the same seed" produce different datasets
// on different teammates' machines. Everything here is plain integer
// arithmetic, so a seed maps to one dataset everywhere.
class Rng {
public:
    explicit Rng(uint64_t seed) : state_(seed) {}

    uint64_t nextU64() {
        uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    // Uniform double in [0, 1) with 53 bits of precision.
    double nextDouble() {
        return static_cast<double>(nextU64() >> 11) * (1.0 / 9007199254740992.0);
    }

    // Uniform integer in [lo, hi], requires lo <= hi.
    // Modulo bias is below 2^-32 for any range used in this project.
    int32_t nextInt(int32_t lo, int32_t hi) {
        const uint64_t range = static_cast<uint64_t>(static_cast<int64_t>(hi) - lo) + 1;
        return static_cast<int32_t>(static_cast<int64_t>(lo) + static_cast<int64_t>(nextU64() % range));
    }

private:
    uint64_t state_;
};

}  // namespace daa
