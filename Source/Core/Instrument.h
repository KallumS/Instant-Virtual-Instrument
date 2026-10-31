#pragma once

#include "SpectralModel.h"
#include <array>
#include <memory>
#include <vector>

namespace ivi
{

constexpr int kMaxLayers = 8;
constexpr int kMaxRoundRobins = 8;
constexpr int kJitterLen = 64;

/** One "recording" in the generated library: a specific key, dynamic layer and
    round robin. Each zone is a deterministic set of performance variations
    applied on top of the spectral model, so no two zones sound identical. */
struct ZoneVariation
{
    float detuneCents = 0.0f;
    float gainDb = 0.0f;
    float tiltDb = 0.0f;           // brightness offset (dB / octave)
    float attackScale = 1.0f;      // >1 = slower attack
    float speed = 1.0f;            // time-stretch of the sustain evolution
    float noiseDb = 0.0f;          // bow / breath noise offset
    float scoopCents = 0.0f;       // initial pitch deviation
    float scoopSeconds = 0.03f;
    float startOffsetSec = 0.0f;
    float lifeRate1 = 0.3f, lifeRate2 = 0.2f, lifePhase = 0.0f;  // slow "human" drift
    uint32_t phaseSeed = 1, noiseSeed = 1;
    std::array<float, kJitterLen> jitterDb {};                   // per-partial timbre variation
};

struct BuildSettings
{
    int lowKey = 36;
    int highKey = 108;
    int numLayers = 5;
    int numRoundRobins = 4;
    uint32_t seed = 1;
    float variation = 0.5f;   // 0 = identical takes, 1 = very loose performance

    bool operator== (const BuildSettings& o) const noexcept
    {
        return lowKey == o.lowKey && highKey == o.highKey && numLayers == o.numLayers
            && numRoundRobins == o.numRoundRobins && seed == o.seed && variation == o.variation;
    }
};

/** The generated virtual instrument: a spectral model mapped across every semitone
    of the key range, with dynamic layers and round robins for each key. Immutable
    once built; the audio thread only reads it. */
struct Instrument
{
    std::shared_ptr<const SpectralModel> model;
    BuildSettings settings;
    std::vector<ZoneVariation> zones;

    int numKeys() const noexcept { return settings.highKey - settings.lowKey + 1; }
    bool containsKey (int key) const noexcept { return key >= settings.lowKey && key <= settings.highKey; }

    const ZoneVariation& zone (int key, int layer, int rr) const noexcept
    {
        key = juce::jlimit (settings.lowKey, settings.highKey, key);
        layer = juce::jlimit (0, settings.numLayers - 1, layer);
        rr = juce::jlimit (0, settings.numRoundRobins - 1, rr);
        const auto idx = ((size_t) (key - settings.lowKey) * (size_t) settings.numLayers + (size_t) layer)
                           * (size_t) settings.numRoundRobins + (size_t) rr;
        return zones[idx];
    }

    /** Dynamic position (0 = ppp, 1 = fff) of a layer's centre. */
    float layerCentre (int layer) const noexcept
    {
        return ((float) layer + 0.5f) / (float) settings.numLayers;
    }

    int layerForDynamic (float x) const noexcept
    {
        return juce::jlimit (0, settings.numLayers - 1, (int) (x * (float) settings.numLayers));
    }

    static juce::String layerName (int layer, int numLayers);
};

std::unique_ptr<Instrument> buildInstrument (std::shared_ptr<const SpectralModel> model, BuildSettings settings);

} // namespace ivi
