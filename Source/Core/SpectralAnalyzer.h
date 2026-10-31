#pragma once

#include "SpectralModel.h"
#include <functional>
#include <memory>

namespace ivi
{

struct AnalysisOptions
{
    float forcedRootMidi = -1.0f;   // < 0 = detect automatically
    int maxPartials = 160;
    int numBands = 24;
    double maxSeconds = 30.0;
};

/** Turns one audio sample into a SpectralModel. Runs off the audio thread. */
struct SpectralAnalyzer
{
    /** progress receives 0..1 and returns false to cancel (the result is then nullptr). */
    using Progress = std::function<bool (float)>;

    static std::shared_ptr<SpectralModel> analyse (const juce::AudioBuffer<float>& audio,
                                                   double sampleRate,
                                                   const AnalysisOptions& options = {},
                                                   const Progress& progress = {});
};

} // namespace ivi
