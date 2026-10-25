#pragma once

#include <cstdint>
#include <cmath>

namespace ivi
{

/** Deterministic splitmix64 generator. Used when building zones so a given seed
    always produces the same instrument. */
struct Rng
{
    explicit Rng (uint64_t seed) noexcept : state (seed * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull) {}

    uint64_t next() noexcept
    {
        uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    /** [0, 1) */
    float uniform() noexcept { return (float) (next() >> 40) * (1.0f / 16777216.0f); }

    /** [-1, 1) */
    float bipolar() noexcept { return uniform() * 2.0f - 1.0f; }

    float range (float lo, float hi) noexcept { return lo + (hi - lo) * uniform(); }

    /** Approximately normal (sum of four uniforms), unit variance. */
    float gaussian() noexcept
    {
        const float s = uniform() + uniform() + uniform() + uniform() - 2.0f;
        return s * 1.7320508f;
    }

    uint64_t state;
};

inline uint64_t mixSeed (uint64_t a, uint64_t b) noexcept
{
    Rng r (a ^ (b * 0xD6E8FEB86659FD93ull));
    return r.next();
}

/** Very cheap white noise for the audio thread. Output is in [-1, 1). */
struct FastNoise
{
    uint32_t s = 0x12345678u;

    void seed (uint32_t v) noexcept { s = v != 0 ? v : 0x12345678u; }

    float next() noexcept
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};

} // namespace ivi
