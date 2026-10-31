#pragma once

#include "JuceIncludes.h"
#include <vector>

namespace ivi
{

struct PitchEstimate
{
    float f0 = 0.0f;            // Hz, 0 if unvoiced
    float aperiodicity = 1.0f;  // YIN CMND minimum (0 = perfectly periodic)
    float rms = 0.0f;
};

/** FFT-accelerated YIN pitch tracker. */
struct PitchTracker
{
    static std::vector<PitchEstimate> track (const float* x, int numSamples, double sampleRate,
                                             int hop, float fMin, float fMax);

    /** Single-shot estimate over a whole buffer region (used by the tests). */
    static PitchEstimate estimate (const float* x, int numSamples, double sampleRate,
                                   float fMin = 27.0f, float fMax = 4500.0f);
};

} // namespace ivi
