#pragma once

#include "JuceIncludes.h"

namespace ivi
{

struct DemoOptions
{
    double sampleRate = 48000.0;
    double f0 = 220.0;
    double seconds = 2.5;
    bool vibrato = true;
    float noise = 0.015f;         // bow noise level
    bool plucked = false;         // decaying instead of sustained
    float inharmonicity = 0.0f;   // piano-like partial stretch (B coefficient)
    uint32_t seed = 7;
};

/** Synthesises a bowed-string-like test tone with body resonances, vibrato and
    bow noise. Used for the "Demo" button and the test suite. */
juce::AudioBuffer<float> makeDemoSample (const DemoOptions& options = {});

} // namespace ivi
