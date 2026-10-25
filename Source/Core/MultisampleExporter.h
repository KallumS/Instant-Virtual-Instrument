#pragma once

#include "Instrument.h"
#include "Articulations.h"
#include "EngineSettings.h"
#include <functional>
#include <vector>

namespace ivi
{

struct ExportOptions
{
    juce::File parentFolder;                  // a sub-folder named after the instrument is created here
    juce::String instrumentName = "Instrument";
    std::vector<Articulation> articulations { Articulation::Sustain };
    double sampleRate = 48000.0;
    int bitDepth = 24;
    float holdSeconds = 2.5f;                 // note length for long articulations before release
};

struct ExportResult
{
    bool ok = false;
    juce::String message;
    int filesWritten = 0;
    int clippedFiles = 0;
    juce::File sfzFile;
};

/** Bakes every zone (key x layer x round robin x articulation) to WAV files plus
    an SFZ mapping, so the generated library also works in any SFZ sampler. */
struct MultisampleExporter
{
    using Progress = std::function<bool (float)>;   // return false to cancel

    static ExportResult exportSfz (const Instrument& instrument, const EngineSettings& settings,
                                   const ExportOptions& options, const Progress& progress = {});

    /** Renders one zone offline (also used by the tests). */
    static juce::AudioBuffer<float> renderZone (const Instrument& instrument, const EngineSettings& settings,
                                                int key, int layer, int rr, Articulation art,
                                                double sampleRate, float holdSeconds);
};

} // namespace ivi
