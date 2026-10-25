#include "Instrument.h"
#include "Random.h"

namespace ivi
{

juce::String Instrument::layerName (int layer, int numLayers)
{
    static const char* names[] = { "ppp", "pp", "p", "mp", "mf", "f", "ff", "fff" };
    if (numLayers <= 1)
        return "mf";
    // spread the layer over the 8 traditional markings
    const int idx = juce::jlimit (0, 7, (int) std::round ((float) layer * 7.0f / (float) (numLayers - 1)));
    if (numLayers <= 5)
    {
        static const char* five[][5] = {
            { "", "", "", "", "" },
            { "mf", "", "", "", "" },
            { "p", "f", "", "", "" },
            { "p", "mf", "f", "", "" },
            { "pp", "mp", "mf", "ff", "" },
            { "pp", "p", "mf", "f", "ff" },
        };
        return five[numLayers][juce::jlimit (0, numLayers - 1, layer)];
    }
    return names[idx];
}

namespace
{
    /** Smooth random curve over log2(partial index): sum of a few cosines. */
    void addSmoothCurve (std::array<float, kJitterLen>& out, Rng& rng, float amountDb)
    {
        if (amountDb <= 0.0f)
            return;

        float freq[3], phase[3], amp[3];
        float norm = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            freq[i] = rng.range (0.3f, 1.4f);
            phase[i] = rng.range (0.0f, juce::MathConstants<float>::twoPi);
            amp[i] = rng.range (0.4f, 1.0f);
            norm += amp[i];
        }
        for (int k = 0; k < kJitterLen; ++k)
        {
            const float u = std::log2 ((float) k + 1.0f);
            float v = 0.0f;
            for (int i = 0; i < 3; ++i)
                v += amp[i] * std::cos (juce::MathConstants<float>::twoPi * freq[i] * u + phase[i]);
            // fundamental and low partials vary less than the upper spectrum
            const float weight = juce::jmin (1.0f, 0.35f + 0.65f * u / 3.0f);
            out[(size_t) k] += amountDb * weight * v / norm;
        }
    }
}

std::unique_ptr<Instrument> buildInstrument (std::shared_ptr<const SpectralModel> model, BuildSettings s)
{
    if (model == nullptr)
        return nullptr;

    s.lowKey = juce::jlimit (0, 127, s.lowKey);
    s.highKey = juce::jlimit (s.lowKey, 127, s.highKey);
    s.numLayers = juce::jlimit (1, kMaxLayers, s.numLayers);
    s.numRoundRobins = juce::jlimit (1, kMaxRoundRobins, s.numRoundRobins);
    s.variation = juce::jlimit (0.0f, 1.0f, s.variation);

    auto inst = std::make_unique<Instrument>();
    inst->model = std::move (model);
    inst->settings = s;
    inst->zones.resize ((size_t) inst->numKeys() * (size_t) s.numLayers * (size_t) s.numRoundRobins);

    const float h = s.variation;
    size_t idx = 0;

    for (int key = s.lowKey; key <= s.highKey; ++key)
    {
        // character shared by every take of this key (string / bore / resonance differences)
        Rng keyRng (mixSeed (s.seed, (uint64_t) key + 1000));
        const float keyGain = keyRng.bipolar() * 0.6f * h;
        const float keyTilt = keyRng.bipolar() * 0.25f * h;
        std::array<float, kJitterLen> keyCurve {};
        addSmoothCurve (keyCurve, keyRng, 1.0f * h);

        for (int layer = 0; layer < s.numLayers; ++layer)
        {
            Rng layerRng (mixSeed (mixSeed (s.seed, (uint64_t) key), (uint64_t) layer + 77));
            std::array<float, kJitterLen> layerCurve = keyCurve;
            addSmoothCurve (layerCurve, layerRng, 0.8f * h);

            for (int rr = 0; rr < s.numRoundRobins; ++rr)
            {
                Rng r (mixSeed (mixSeed (mixSeed (s.seed, (uint64_t) key), (uint64_t) layer), (uint64_t) rr + 31));
                auto& z = inst->zones[idx++];

                z.detuneCents = r.gaussian() * 2.5f * h;
                z.gainDb = keyGain + r.bipolar() * 0.8f * h;
                z.tiltDb = keyTilt + r.bipolar() * 0.25f * h;
                z.attackScale = std::pow (2.0f, r.bipolar() * 0.3f * h);
                z.speed = std::pow (2.0f, r.bipolar() * 0.06f * h);
                z.noiseDb = r.bipolar() * 1.5f * h;
                z.scoopCents = r.gaussian() * 7.0f * h;
                z.scoopSeconds = r.range (0.02f, 0.07f);
                z.startOffsetSec = r.uniform() * 0.006f * h;
                z.lifeRate1 = r.range (0.12f, 0.45f);
                z.lifeRate2 = r.range (0.2f, 0.7f);
                z.lifePhase = r.range (0.0f, juce::MathConstants<float>::twoPi);
                z.phaseSeed = (uint32_t) (r.next() | 1u);
                z.noiseSeed = (uint32_t) (r.next() | 1u);

                z.jitterDb = layerCurve;
                addSmoothCurve (z.jitterDb, r, 1.0f * h);
                for (int k = 1; k < kJitterLen; ++k)
                {
                    const float upper = std::log2 ((float) k + 1.0f) / 6.0f;
                    z.jitterDb[(size_t) k] += r.bipolar() * 0.5f * h * upper;
                }
            }
        }
    }

    return inst;
}

} // namespace ivi
