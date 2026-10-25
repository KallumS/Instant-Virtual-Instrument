#include "PitchTracker.h"
#include <cmath>
#include <algorithm>

namespace ivi
{

namespace
{
    struct Yin
    {
        Yin (double sr, float fMin, float fMax)
        {
            maxLag = (int) std::ceil (sr / fMin);
            minLag = std::max (2, (int) std::floor (sr / fMax));
            window = std::max (maxLag, (int) std::ceil (0.025 * sr));
            span = window + maxLag + 2;

            int order = 1;
            while ((1 << order) < span)
                ++order;

            fft = std::make_unique<juce::dsp::FFT> (order);
            size = 1 << order;
            a.assign ((size_t) size * 2, 0.0f);
            b.assign ((size_t) size * 2, 0.0f);
            d.assign ((size_t) maxLag + 2, 0.0f);
        }

        PitchEstimate run (const float* x, int n, int start, double sr)
        {
            std::fill (a.begin(), a.end(), 0.0f);
            std::fill (b.begin(), b.end(), 0.0f);

            double e0 = 0.0;
            for (int j = 0; j < span; ++j)
            {
                const int idx = start + j;
                const float v = (idx >= 0 && idx < n) ? x[idx] : 0.0f;
                b[(size_t) j] = v;
                if (j < window)
                {
                    a[(size_t) j] = v;
                    e0 += (double) v * v;
                }
            }

            PitchEstimate est;
            est.rms = (float) std::sqrt (e0 / window);
            if (est.rms < 1.0e-5f)
                return est;

            std::vector<float> bCopy (b.begin(), b.begin() + span);

            fft->performRealOnlyForwardTransform (a.data());
            fft->performRealOnlyForwardTransform (b.data());

            // conj(A) * B
            for (int k = 0; k <= size / 2; ++k)
            {
                const float ar = a[(size_t) 2 * k], ai = a[(size_t) 2 * k + 1];
                const float br = b[(size_t) 2 * k], bi = b[(size_t) 2 * k + 1];
                b[(size_t) 2 * k]     = ar * br + ai * bi;
                b[(size_t) 2 * k + 1] = ar * bi - ai * br;
            }
            fft->performRealOnlyInverseTransform (b.data());

            double eTau = 0.0;
            for (int j = 0; j < window; ++j)
                eTau += (double) bCopy[(size_t) j] * bCopy[(size_t) j];

            d[0] = 1.0f;
            double running = 0.0;
            for (int tau = 1; tau <= maxLag; ++tau)
            {
                eTau += (double) bCopy[(size_t) (tau + window - 1)] * bCopy[(size_t) (tau + window - 1)]
                      - (double) bCopy[(size_t) (tau - 1)] * bCopy[(size_t) (tau - 1)];
                const double diff = std::max (0.0, e0 + eTau - 2.0 * (double) b[(size_t) tau]);
                running += diff;
                d[(size_t) tau] = running > 0.0 ? (float) (diff * tau / running) : 1.0f;
            }

            // absolute threshold, then walk to the local minimum
            constexpr float threshold = 0.12f;
            int best = -1;
            for (int tau = minLag; tau < maxLag; ++tau)
            {
                if (d[(size_t) tau] < threshold)
                {
                    while (tau + 1 < maxLag && d[(size_t) tau + 1] < d[(size_t) tau])
                        ++tau;
                    best = tau;
                    break;
                }
            }

            if (best < 0)
            {
                best = minLag;
                for (int tau = minLag; tau < maxLag; ++tau)
                    if (d[(size_t) tau] < d[(size_t) best])
                        best = tau;
            }

            float tauF = (float) best;
            if (best > minLag && best < maxLag - 1)
            {
                const float s0 = d[(size_t) best - 1], s1 = d[(size_t) best], s2 = d[(size_t) best + 1];
                const float denom = s0 - 2.0f * s1 + s2;
                if (std::abs (denom) > 1.0e-9f)
                    tauF += juce::jlimit (-0.5f, 0.5f, 0.5f * (s0 - s2) / denom);
            }

            est.aperiodicity = d[(size_t) best];
            est.f0 = (float) (sr / tauF);
            return est;
        }

        int maxLag = 0, minLag = 0, window = 0, span = 0, size = 0;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> a, b, d;
    };
}

std::vector<PitchEstimate> PitchTracker::track (const float* x, int numSamples, double sampleRate,
                                                int hop, float fMin, float fMax)
{
    Yin yin (sampleRate, fMin, fMax);
    const int numFrames = std::max (1, numSamples / std::max (1, hop) + 1);
    std::vector<PitchEstimate> out ((size_t) numFrames);

    for (int i = 0; i < numFrames; ++i)
        out[(size_t) i] = yin.run (x, numSamples, i * hop - yin.span / 2, sampleRate);

    return out;
}

PitchEstimate PitchTracker::estimate (const float* x, int numSamples, double sampleRate, float fMin, float fMax)
{
    const int hop = std::max (1, (int) (0.02 * sampleRate));
    auto frames = track (x, numSamples, sampleRate, hop, fMin, fMax);

    std::vector<float> logs;
    float maxRms = 0.0f;
    for (auto& f : frames)
        maxRms = std::max (maxRms, f.rms);

    for (auto& f : frames)
        if (f.f0 > 0.0f && f.aperiodicity < 0.3f && f.rms > maxRms * 0.1f)
            logs.push_back (std::log (f.f0));

    PitchEstimate e;
    e.rms = maxRms;
    if (logs.empty())
        return e;

    std::nth_element (logs.begin(), logs.begin() + (long) logs.size() / 2, logs.end());
    e.f0 = std::exp (logs[logs.size() / 2]);
    e.aperiodicity = 0.0f;
    return e;
}

} // namespace ivi
