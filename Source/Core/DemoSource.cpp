#include "DemoSource.h"
#include "Random.h"
#include <cmath>
#include <vector>

namespace ivi
{

juce::AudioBuffer<float> makeDemoSample (const DemoOptions& o)
{
    const int n = (int) (o.seconds * o.sampleRate);
    juce::AudioBuffer<float> buf (1, n);
    buf.clear();
    auto* x = buf.getWritePointer (0);

    const double twoPi = juce::MathConstants<double>::twoPi;
    auto body = [] (double f)
    {
        auto peak = [f] (double centre, double width, double gain)
        {
            const double d = std::log2 (f / centre) / width;
            return gain * std::exp (-0.5 * d * d);
        };
        return 0.25 + peak (480.0, 0.3, 1.0) + peak (1400.0, 0.35, 0.7) + peak (3100.0, 0.4, 0.45);
    };

    std::vector<double> phase (200, 0.0);
    Rng rng (o.seed);
    for (auto& p : phase)
        p = rng.uniform() * twoPi;

    const int maxH = std::min (200, (int) (0.45 * o.sampleRate / o.f0));
    std::vector<double> ratio ((size_t) maxH + 1), amp ((size_t) maxH + 1);
    for (int h = 1; h <= maxH; ++h)
    {
        ratio[(size_t) h] = h * std::sqrt (1.0 + o.inharmonicity * h * h);
        amp[(size_t) h] = body (h * o.f0) / std::pow ((double) h, 0.85);
    }

    const double attack = o.plucked ? 0.004 : 0.12;
    const double releaseStart = o.seconds - 0.25;
    double vibPhase = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const double t = i / o.sampleRate;
        double env;
        if (o.plucked)
            env = std::min (1.0, t / attack) * std::exp (-t / 0.7);
        else
        {
            env = std::min (1.0, t / attack);
            env = env * env * (3.0 - 2.0 * env);
            if (t > releaseStart)
                env *= std::max (0.0, 1.0 - (t - releaseStart) / 0.25);
        }

        const double vibDepth = o.vibrato ? 0.006 * std::min (1.0, std::max (0.0, (t - 0.4) / 0.5)) : 0.0;
        vibPhase += twoPi * 5.5 / o.sampleRate;
        const double f0 = o.f0 * (1.0 + vibDepth * std::sin (vibPhase));

        double s = 0.0;
        for (int h = 1; h <= maxH; ++h)
        {
            const double f = f0 * ratio[(size_t) h];
            if (f > 0.45 * o.sampleRate)
                break;
            phase[(size_t) h - 1] += twoPi * f / o.sampleRate;
            // plucked: upper partials die away faster
            const double decay = o.plucked ? std::exp (-t * 0.8 * h) : 1.0;
            s += amp[(size_t) h] * decay * std::sin (phase[(size_t) h - 1]);
        }
        x[i] = (float) (s * env * 0.08 + o.noise * env * rng.bipolar());
        if (i % 4096 == 0)
            for (auto& p : phase)
                p = std::fmod (p, twoPi);
    }
    return buf;
}

} // namespace ivi
