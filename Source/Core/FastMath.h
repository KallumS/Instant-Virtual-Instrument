#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>

namespace ivi::fast
{

/** 2^x, relative error < 2e-5 (plenty for gains). */
inline float exp2 (float x) noexcept
{
    x = x < -126.0f ? -126.0f : (x > 126.0f ? 126.0f : x);
    const float fl = std::floor (x);
    const float f = x - fl;
    // minimax-ish polynomial for 2^f on [0, 1)
    const float p = 1.0f + f * (0.69314718f + f * (0.24022650f + f * (0.05550411f
                         + f * (0.00961813f + f * (0.00133336f + f * 0.00015403f)))));
    const int32_t e = (int32_t) fl + 127;
    uint32_t bits = (uint32_t) e << 23;
    float scale;
    std::memcpy (&scale, &bits, sizeof (float));
    return p * scale;
}

/** dB to linear gain. */
inline float dbToGain (float db) noexcept { return exp2 (db * 0.16609640f); }

/** sin and cos of w for w in [0, pi] (Taylor series around pi/2, error < 1e-7). */
inline void sinCosHalf (float w, float& s, float& c) noexcept
{
    const float x = w - 1.57079633f;          // [-pi/2, pi/2]
    const float x2 = x * x;
    // sin(x)
    const float sx = x * (1.0f + x2 * (-1.6666667e-1f + x2 * (8.3333333e-3f + x2 * (-1.9841270e-4f
                        + x2 * (2.7557319e-6f + x2 * (-2.5052108e-8f))))));
    // cos(x)
    const float cx = 1.0f + x2 * (-0.5f + x2 * (4.1666667e-2f + x2 * (-1.3888889e-3f
                        + x2 * (2.4801587e-5f + x2 * (-2.7557319e-7f + x2 * 2.0876757e-9f)))));
    // w = x + pi/2  ->  sin w = cos x, cos w = -sin x
    s = cx;
    c = -sx;
}

} // namespace ivi::fast
