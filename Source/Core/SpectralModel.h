#pragma once

#include "JuceIncludes.h"
#include <vector>

namespace ivi
{

/** The result of analysing a single sample: a sinusoidal + noise ("SMS") model.

    The harmonic part is stored as a time series of partial tracks. Each partial's
    level is split into three parts:
      - a source slope (straight line over log harmonic number; moves with pitch),
      - a body resonance (formants, tied to absolute frequency),
      - a fine structure (tied to the harmonic index, e.g. a clarinet's weak even harmonics).
    Keeping them apart lets the engine move the pitch while choosing how much of the
    body resonance to preserve.

    The stochastic part (bow, breath, hammer noise) is stored as band energies.
*/
struct SpectralModel
{
    // ---- source --------------------------------------------------------------
    juce::AudioBuffer<float> source;       // trimmed, normalised mono source
    double sampleRate = 44100.0;           // analysis/source sample rate
    juce::String name;

    // ---- pitch ---------------------------------------------------------------
    bool  pitched = true;                  // false if no stable pitch was found
    bool  rootForced = false;
    double f0Ref = 220.0;                  // reference fundamental (Hz)
    float rootMidi = 57.0f;                // f0Ref as a (fractional) MIDI note

    // ---- frames --------------------------------------------------------------
    double hopSeconds = 0.005;
    int fftSize = 0;
    int numFrames = 0;
    int numPartials = 0;                   // P
    int numBands = 0;                      // B

    std::vector<float> f0Ratio;            // [frame]       f0(t) / f0Ref
    std::vector<float> ratio;              // [frame*P + h] partial frequency / f0(t)
    std::vector<float> logRatio;           // [frame*P + h] log2 (ratio), precomputed for the audio thread
    std::vector<float> envDb;              // [frame*P + h] body resonance at that partial (dB), slope removed
    std::vector<float> fineDb;             // [frame*P + h] partial level minus the smooth envelope (dB)
    std::vector<float> slopeA, slopeB;     // [frame] source slope: dB = A + B * log2 (ratio), moves with pitch
    std::vector<float> noiseAmp;           // [frame*B + b] band RMS (linear)
    std::vector<float> level;              // [frame]       overall RMS (linear)
    std::vector<float> loopGain;           // [frame]       level flattening inside the loop region

    std::vector<float> bandLo, bandHi, bandCentre;  // Hz

    // ---- regions (frame indices) --------------------------------------------
    int onsetFrame = 0;
    int attackEndFrame = 0;
    int loopStartFrame = 0;
    int loopEndFrame = 0;
    bool decaying = false;                 // plucked / struck sources

    float partialDb (int frame, int h) const noexcept
    {
        const auto i = (size_t) frame * (size_t) numPartials + (size_t) h;
        return slopeA[(size_t) frame] + slopeB[(size_t) frame] * logRatio[i] + envDb[i] + fineDb[i];
    }

    double frameToSeconds (double frame) const noexcept { return frame * hopSeconds; }
    double durationSeconds() const noexcept { return (double) source.getNumSamples() / sampleRate; }
};

inline double midiToHz (double note) noexcept { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }
inline double hzToMidi (double hz) noexcept   { return 69.0 + 12.0 * std::log2 (hz / 440.0); }

} // namespace ivi
