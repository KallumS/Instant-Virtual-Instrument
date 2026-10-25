#include "SpectralAnalyzer.h"
#include "PitchTracker.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace ivi
{

namespace
{
    constexpr float kFloorDb = -140.0f;

    float toDb (float lin) noexcept { return lin > 1.0e-7f ? 20.0f * std::log10 (lin) : kFloorDb; }

    juce::AudioBuffer<float> prepareSource (const juce::AudioBuffer<float>& in, double sr, double maxSeconds)
    {
        const int n = in.getNumSamples();
        std::vector<float> mono ((size_t) n, 0.0f);
        const int chans = std::max (1, in.getNumChannels());
        for (int c = 0; c < in.getNumChannels(); ++c)
        {
            auto* src = in.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                mono[(size_t) i] += src[i] / (float) chans;
        }

        // DC / rumble removal (one-pole high-pass at ~15 Hz)
        const float R = 1.0f - (float) (2.0 * juce::MathConstants<double>::pi * 15.0 / sr);
        float x1 = 0.0f, y1 = 0.0f;
        for (auto& v : mono)
        {
            const float y = v - x1 + R * y1;
            x1 = v;
            y1 = y;
            v = y;
        }

        float peak = 0.0f;
        for (auto v : mono)
            peak = std::max (peak, std::abs (v));

        juce::AudioBuffer<float> out;
        if (peak <= 1.0e-6f)
            return out;

        const float thresh = peak * 0.001f;   // -60 dB
        int first = 0, last = n - 1;
        while (first < n && std::abs (mono[(size_t) first]) < thresh) ++first;
        while (last > first && std::abs (mono[(size_t) last]) < thresh) --last;

        first = std::max (0, first - (int) (0.003 * sr));
        last  = std::min (n - 1, last + (int) (0.01 * sr));
        int len = std::min (last - first + 1, (int) (maxSeconds * sr));
        len = std::max (len, 16);

        out.setSize (1, len);
        auto* dst = out.getWritePointer (0);
        const float gain = 0.12f / peak;   // about -18 dBFS: headroom for loud layers and chords
        for (int i = 0; i < len; ++i)
        {
            const int idx = first + i;
            dst[i] = idx < n ? mono[(size_t) idx] * gain : 0.0f;
        }

        const int fadeIn = std::min (len / 4, (int) (0.001 * sr));
        const int fadeOut = std::min (len / 4, (int) (0.01 * sr));
        for (int i = 0; i < fadeIn; ++i)  dst[i] *= (float) i / (float) fadeIn;
        for (int i = 0; i < fadeOut; ++i) dst[len - 1 - i] *= (float) i / (float) fadeOut;
        return out;
    }

    void medianFilter (std::vector<float>& v, int radius)
    {
        if (v.size() < 3)
            return;
        std::vector<float> out (v.size()), win;
        for (size_t i = 0; i < v.size(); ++i)
        {
            win.clear();
            for (int d = -radius; d <= radius; ++d)
            {
                const long j = juce::jlimit (0L, (long) v.size() - 1, (long) i + d);
                win.push_back (v[(size_t) j]);
            }
            std::nth_element (win.begin(), win.begin() + (long) win.size() / 2, win.end());
            out[i] = win[win.size() / 2];
        }
        v.swap (out);
    }

    std::vector<float> movingAverage (const std::vector<float>& v, int radius)
    {
        std::vector<float> out (v.size(), 0.0f);
        if (v.empty())
            return out;
        std::vector<double> cum (v.size() + 1, 0.0);
        for (size_t i = 0; i < v.size(); ++i)
            cum[i + 1] = cum[i] + v[i];
        for (size_t i = 0; i < v.size(); ++i)
        {
            const long lo = std::max (0L, (long) i - radius);
            const long hi = std::min ((long) v.size() - 1, (long) i + radius);
            out[i] = (float) ((cum[(size_t) hi + 1] - cum[(size_t) lo]) / (double) (hi - lo + 1));
        }
        return out;
    }

    /** Weighted RMS of x around centre with the given window. */
    float windowedRms (const float* x, int n, int centre, const std::vector<float>& w)
    {
        const int half = (int) w.size() / 2;
        double acc = 0.0, wsum = 0.0;
        for (int j = 0; j < (int) w.size(); ++j)
        {
            const int idx = centre - half + j;
            const float v = (idx >= 0 && idx < n) ? x[idx] : 0.0f;
            acc += (double) w[(size_t) j] * v * v;
            wsum += w[(size_t) j];
        }
        return wsum > 0.0 ? (float) std::sqrt (acc / wsum) : 0.0f;
    }
}

std::shared_ptr<SpectralModel> SpectralAnalyzer::analyse (const juce::AudioBuffer<float>& audio, double sr,
                                                          const AnalysisOptions& opt, const Progress& progress)
{
    auto report = [&] (float p) { return ! progress || progress (p); };

    auto model = std::make_shared<SpectralModel>();
    model->sampleRate = sr;
    model->source = prepareSource (audio, sr, opt.maxSeconds);

    const int n = model->source.getNumSamples();
    if (n < 64)
        return nullptr;

    const float* x = model->source.getReadPointer (0);

    // ------------------------------------------------------------------ pitch
    const int pitchHop = std::max (1, (int) std::round (0.01 * sr));
    auto pitch = PitchTracker::track (x, n, sr, pitchHop, 27.0f, 4500.0f);
    if (! report (0.15f))
        return nullptr;

    float maxRms = 0.0f;
    for (auto& p : pitch)
        maxRms = std::max (maxRms, p.rms);

    std::vector<float> voicedLogs;
    int loudCount = 0;
    for (auto& p : pitch)
    {
        if (p.rms > maxRms * 0.1f)
        {
            ++loudCount;
            if (p.f0 > 0.0f && p.aperiodicity < 0.3f)
                voicedLogs.push_back (std::log (p.f0));
        }
    }

    const bool detected = (int) voicedLogs.size() >= std::max (3, loudCount / 5);
    if (opt.forcedRootMidi >= 0.0f)
    {
        model->f0Ref = midiToHz (opt.forcedRootMidi);
        model->pitched = true;
        model->rootForced = true;
    }
    else if (detected)
    {
        std::nth_element (voicedLogs.begin(), voicedLogs.begin() + (long) voicedLogs.size() / 2, voicedLogs.end());
        model->f0Ref = std::exp (voicedLogs[voicedLogs.size() / 2]);
        model->pitched = true;
    }
    else
    {
        model->f0Ref = midiToHz (60.0);
        model->pitched = false;
    }
    model->rootMidi = (float) hzToMidi (model->f0Ref);

    // per-frame pitch ratio (vibrato, scoops), octave-folded and cleaned
    std::vector<float> pitchRatio (pitch.size(), 1.0f);
    {
        std::vector<char> valid (pitch.size(), 0);
        if (detected)
        {
            for (size_t i = 0; i < pitch.size(); ++i)
            {
                const auto& p = pitch[i];
                if (p.f0 <= 0.0f || p.aperiodicity > 0.35f || p.rms < maxRms * 0.03f)
                    continue;
                double r = p.f0 / model->f0Ref;
                while (r > 1.4142) r *= 0.5;
                while (r < 0.7071) r *= 2.0;
                if (std::abs (1200.0 * std::log2 (r)) > 250.0)
                    continue;
                pitchRatio[i] = (float) r;
                valid[i] = 1;
            }
        }

        // fill invalid frames by interpolating neighbours
        int lastValid = -1;
        for (int i = 0; i < (int) pitchRatio.size(); ++i)
        {
            if (! valid[(size_t) i])
                continue;
            if (lastValid < 0)
                for (int j = 0; j < i; ++j) pitchRatio[(size_t) j] = pitchRatio[(size_t) i];
            else
                for (int j = lastValid + 1; j < i; ++j)
                {
                    const float t = (float) (j - lastValid) / (float) (i - lastValid);
                    pitchRatio[(size_t) j] = pitchRatio[(size_t) lastValid] * (1.0f - t) + pitchRatio[(size_t) i] * t;
                }
            lastValid = i;
        }
        if (lastValid >= 0)
            for (int j = lastValid + 1; j < (int) pitchRatio.size(); ++j)
                pitchRatio[(size_t) j] = pitchRatio[(size_t) lastValid];
        else
            std::fill (pitchRatio.begin(), pitchRatio.end(), 1.0f);

        medianFilter (pitchRatio, 2);
        pitchRatio = movingAverage (pitchRatio, 1);
    }

    // ------------------------------------------------------------------ STFT setup
    int N = 512;
    while (N < 6.0 * sr / model->f0Ref && N < 16384)
        N <<= 1;
    const int M = N * 2;                       // 2x zero padding
    int order = 0;
    while ((1 << order) < M) ++order;
    juce::dsp::FFT fft (order);

    const int hop = std::max (1, std::min ((int) std::round (0.005 * sr), N / 2));
    model->hopSeconds = hop / sr;
    model->fftSize = N;

    const int numFrames = n / hop + 2;
    const int P = juce::jlimit (1, opt.maxPartials, (int) std::floor (0.47 * sr / model->f0Ref));
    const int B = opt.numBands;
    model->numFrames = numFrames;
    model->numPartials = P;
    model->numBands = B;

    std::vector<float> window ((size_t) N);
    double sumW = 0.0, sumW2 = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const double ph = 2.0 * juce::MathConstants<double>::pi * i / (N - 1);
        window[(size_t) i] = (float) (0.42 - 0.5 * std::cos (ph) + 0.08 * std::cos (2.0 * ph));  // Blackman
        sumW += window[(size_t) i];
        sumW2 += (double) window[(size_t) i] * window[(size_t) i];
    }
    const float ampScale = (float) (2.0 / sumW);
    const double binHz = sr / M;
    const int lobeBins = 3 * (M / N) + 1;        // Blackman main-lobe half width (+1 for safety)

    // noise bands (log spaced)
    const double bandTop = std::min (20000.0, 0.48 * sr), bandBottom = 40.0;
    model->bandLo.resize ((size_t) B);
    model->bandHi.resize ((size_t) B);
    model->bandCentre.resize ((size_t) B);
    for (int b = 0; b < B; ++b)
    {
        const double lo = bandBottom * std::pow (bandTop / bandBottom, (double) b / B);
        const double hi = bandBottom * std::pow (bandTop / bandBottom, (double) (b + 1) / B);
        model->bandLo[(size_t) b] = (float) lo;
        model->bandHi[(size_t) b] = (float) hi;
        model->bandCentre[(size_t) b] = (float) std::sqrt (lo * hi);
    }

    model->f0Ratio.assign ((size_t) numFrames, 1.0f);
    model->ratio.assign ((size_t) numFrames * P, 1.0f);
    model->logRatio.assign ((size_t) numFrames * P, 0.0f);
    model->envDb.assign ((size_t) numFrames * P, kFloorDb);
    model->fineDb.assign ((size_t) numFrames * P, 0.0f);
    model->noiseAmp.assign ((size_t) numFrames * B, 0.0f);
    model->level.assign ((size_t) numFrames, 0.0f);
    model->slopeA.assign ((size_t) numFrames, 0.0f);
    model->slopeB.assign ((size_t) numFrames, -6.0f);
    model->loopGain.assign ((size_t) numFrames, 1.0f);

    std::vector<float> buf ((size_t) M * 2), mag ((size_t) M / 2 + 1), magDb ((size_t) M / 2 + 1);
    std::vector<char> mask ((size_t) M / 2 + 1);
    std::vector<float> partialDb ((size_t) P), logF ((size_t) P);

    // ------------------------------------------------------------------ frame loop
    for (int fi = 0; fi < numFrames; ++fi)
    {
        if ((fi & 63) == 0 && ! report (0.15f + 0.7f * (float) fi / (float) numFrames))
            return nullptr;

        const int centre = fi * hop;

        // pitch ratio at this frame
        const float pPos = (float) centre / (float) pitchHop;
        const int p0 = juce::jlimit (0, (int) pitchRatio.size() - 1, (int) pPos);
        const int p1 = std::min (p0 + 1, (int) pitchRatio.size() - 1);
        const float pa = juce::jlimit (0.0f, 1.0f, pPos - (float) p0);
        const float fr = pitchRatio[(size_t) p0] * (1.0f - pa) + pitchRatio[(size_t) p1] * pa;
        model->f0Ratio[(size_t) fi] = fr;
        const double f0t = model->f0Ref * fr;

        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int j = 0; j < N; ++j)
        {
            const int idx = centre - N / 2 + j;
            if (idx >= 0 && idx < n)
                buf[(size_t) j] = x[idx] * window[(size_t) j];
        }
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        for (int k = 0; k <= M / 2; ++k)
        {
            mag[(size_t) k] = buf[(size_t) k];
            magDb[(size_t) k] = toDb (buf[(size_t) k]);
        }
        std::fill (mask.begin(), mask.end(), 0);

        float frameMaxDb = kFloorDb;
        for (int k = 1; k < M / 2; ++k)
            frameMaxDb = std::max (frameMaxDb, magDb[(size_t) k]);

        // harmonic-guided peak tracking (tolerates inharmonic stretch)
        float prevR = 0.0f, spacing = 1.0f, r1 = 1.0f;
        bool prevStrong = false, r1Strong = false;
        for (int h = 0; h < P; ++h)
        {
            const float expected = h == 0 ? 1.0f : prevR + spacing;
            const double fExp = expected * f0t;
            float rMeasured = expected;
            float dB = kFloorDb;
            bool strong = false;

            if (fExp < 0.475 * sr)
            {
                const double hw = 0.28 * f0t;
                const int lo = juce::jlimit (1, M / 2 - 1, (int) std::floor ((fExp - hw) / binHz));
                const int hi = juce::jlimit (1, M / 2 - 1, (int) std::ceil ((fExp + hw) / binHz));
                int kp = lo;
                for (int k = lo; k <= hi; ++k)
                    if (mag[(size_t) k] > mag[(size_t) kp])
                        kp = k;

                if (kp > lo && kp < hi && mag[(size_t) kp] > 0.0f)
                {
                    const float al = magDb[(size_t) kp - 1], be = magDb[(size_t) kp], ga = magDb[(size_t) kp + 1];
                    const float denom = al - 2.0f * be + ga;
                    const float pOff = std::abs (denom) > 1.0e-9f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (al - ga) / denom) : 0.0f;
                    const double fPeak = (kp + pOff) * binHz;
                    dB = be - 0.25f * (al - ga) * pOff;
                    rMeasured = (float) (fPeak / f0t);
                    strong = dB > frameMaxDb - 45.0f;

                    for (int k = std::max (0, kp - lobeBins); k <= std::min (M / 2, kp + lobeBins); ++k)
                        mask[(size_t) k] = 1;
                }
                else
                {
                    const int kc = juce::jlimit (0, M / 2, (int) std::round (fExp / binHz));
                    dB = magDb[(size_t) kc];
                }
                dB += toDb (ampScale);
            }

            if (h > 0)
                rMeasured = std::max (rMeasured, prevR + 0.5f);
            else if (strong && std::abs (rMeasured - 1.0f) < 0.06f)
            {
                r1 = rMeasured;
                r1Strong = true;
            }

            if (h > 0 && strong && prevStrong)
                spacing = 0.7f * spacing + 0.3f * juce::jlimit (0.9f, 1.6f, rMeasured - prevR);

            const auto idx = (size_t) fi * P + (size_t) h;
            model->ratio[idx] = rMeasured;
            model->logRatio[idx] = std::log2 (rMeasured);
            partialDb[(size_t) h] = std::max (kFloorDb, dB);
            logF[(size_t) h] = (float) std::log2 (std::max (1.0, rMeasured * f0t));
            prevR = rMeasured;
            prevStrong = strong;
        }

        // refine the frame's f0 from the measured first partial (removes YIN bias on stretched spectra)
        if (r1Strong)
        {
            model->f0Ratio[(size_t) fi] = fr * r1;
            for (int h = 0; h < P; ++h)
            {
                const auto idx = (size_t) fi * P + (size_t) h;
                model->ratio[idx] /= r1;
                model->logRatio[idx] = std::log2 (model->ratio[idx]);
            }
        }

        // smooth spectral envelope (power mean over +-0.2 octave), fine structure = remainder
        for (int h = 0; h < P; ++h)
        {
            double acc = 0.0;
            int cnt = 0;
            for (int j = h; j >= 0 && logF[(size_t) h] - logF[(size_t) j] <= 0.2f; --j)
            {
                acc += std::pow (10.0, partialDb[(size_t) j] / 10.0);
                ++cnt;
            }
            for (int j = h + 1; j < P && logF[(size_t) j] - logF[(size_t) h] <= 0.2f; ++j)
            {
                acc += std::pow (10.0, partialDb[(size_t) j] / 10.0);
                ++cnt;
            }
            const float env = (float) (10.0 * std::log10 (std::max (1.0e-14, acc / std::max (1, cnt))));
            const auto idx = (size_t) fi * P + (size_t) h;
            model->envDb[idx] = env;
            model->fineDb[idx] = partialDb[(size_t) h] - env;
        }

        // source slope: least-squares line of the envelope over log2(harmonic number);
        // what remains in envDb is the body resonance
        {
            float maxEnv = kFloorDb;
            for (int h = 0; h < P; ++h)
                maxEnv = std::max (maxEnv, model->envDb[(size_t) fi * P + (size_t) h]);
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            int cnt = 0;
            for (int h = 0; h < P; ++h)
            {
                const auto idx = (size_t) fi * P + (size_t) h;
                const double f = model->ratio[idx] * model->f0Ref * model->f0Ratio[(size_t) fi];
                if (f > std::min (10000.0, 0.45 * sr) || model->envDb[idx] < maxEnv - 50.0f)
                    continue;
                const double xv = model->logRatio[idx], yv = model->envDb[idx];
                sx += xv; sy += yv; sxx += xv * xv; sxy += xv * yv;
                ++cnt;
            }
            float A = cnt > 0 ? (float) (sy / cnt) : maxEnv, Bs = -6.0f;
            const double den = cnt * sxx - sx * sx;
            if (cnt >= 3 && den > 1.0e-9)
            {
                Bs = juce::jlimit (-30.0f, 10.0f, (float) ((cnt * sxy - sx * sy) / den));
                A = (float) ((sy - Bs * sx) / cnt);
            }
            model->slopeA[(size_t) fi] = A;
            model->slopeB[(size_t) fi] = Bs;
            for (int h = 0; h < P; ++h)
            {
                const auto idx = (size_t) fi * P + (size_t) h;
                model->envDb[idx] -= A + Bs * model->logRatio[idx];
            }
        }

        // stochastic residual: band energies of the bins not claimed by partials.
        // Bands fully covered by partials are interpolated from their neighbours.
        {
            std::vector<double> density ((size_t) B, -1.0);   // white-noise variance per bin
            for (int b = 0; b < B; ++b)
            {
                const int lo = std::max (1, (int) std::floor (model->bandLo[(size_t) b] / binHz));
                const int hi = std::min (M / 2, std::max (lo, (int) std::ceil (model->bandHi[(size_t) b] / binHz)));
                double acc = 0.0;
                int cnt = 0;
                for (int k = lo; k <= hi; ++k)
                    if (! mask[(size_t) k])
                    {
                        acc += (double) mag[(size_t) k] * mag[(size_t) k];
                        ++cnt;
                    }
                if (cnt > 0)
                    density[(size_t) b] = (acc / cnt) / sumW2;
            }

            for (int b = 0; b < B; ++b)
            {
                double d = density[(size_t) b];
                if (d < 0.0)
                {
                    int l = b - 1, r = b + 1;
                    while (l >= 0 && density[(size_t) l] < 0.0) --l;
                    while (r < B && density[(size_t) r] < 0.0) ++r;
                    if (l >= 0 && r < B)
                    {
                        const double t = (double) (b - l) / (double) (r - l);
                        d = std::exp (std::log (density[(size_t) l] + 1e-30) * (1.0 - t) + std::log (density[(size_t) r] + 1e-30) * t);
                    }
                    else if (l >= 0) d = density[(size_t) l];
                    else if (r < B)  d = density[(size_t) r];
                    else             d = 0.0;
                }
                const double v = d * (model->bandHi[(size_t) b] - model->bandLo[(size_t) b]) / (0.5 * sr);
                model->noiseAmp[(size_t) fi * B + (size_t) b] = (float) std::sqrt (std::max (0.0, v));
            }
        }
    }

    if (! report (0.88f))
        return nullptr;

    // re-reference the pitch to the median of the refined per-frame f0 (keeps the instrument in tune)
    if (model->pitched && detected)
    {
        float maxL = kFloorDb;
        for (int fi = 0; fi < numFrames; ++fi)
            maxL = std::max (maxL, model->partialDb (fi, 0));
        std::vector<double> logs;
        for (int fi = 0; fi < numFrames; ++fi)
            if (model->partialDb (fi, 0) > maxL - 30.0f)
                logs.push_back (std::log (model->f0Ref * model->f0Ratio[(size_t) fi]));
        if (! logs.empty())
        {
            std::nth_element (logs.begin(), logs.begin() + (long) logs.size() / 2, logs.end());
            double newRef = std::exp (logs[logs.size() / 2]);
            bool accept = true;
            if (model->rootForced)
            {
                // keep the user's note, but still correct the sample's tuning by up to a semitone
                const double forced = midiToHz (opt.forcedRootMidi);
                while (newRef > forced * 1.4142) newRef *= 0.5;
                while (newRef < forced * 0.7071) newRef *= 2.0;
                accept = std::abs (12.0 * std::log2 (newRef / forced)) < 1.0;
            }
            if (accept)
            {
                const float scale = (float) (model->f0Ref / newRef);
                for (auto& r : model->f0Ratio)
                    r *= scale;
                model->f0Ref = newRef;
                model->rootMidi = (float) hzToMidi (newRef);
            }
        }
    }

    // ------------------------------------------------------------------ attack sharpening
    // The long analysis window smears onsets. Re-impose the short-time envelope of the
    // original over the attack so transients stay crisp.
    {
        std::vector<float> shortWin ((size_t) std::max (8, (int) (0.008 * sr)));
        for (size_t i = 0; i < shortWin.size(); ++i)
            shortWin[i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) (shortWin.size() - 1));

        std::vector<float> envShort ((size_t) numFrames), envLong ((size_t) numFrames);
        for (int fi = 0; fi < numFrames; ++fi)
        {
            envShort[(size_t) fi] = windowedRms (x, n, fi * hop, shortWin);
            envLong[(size_t) fi]  = windowedRms (x, n, fi * hop, window);
        }

        const float peakShort = *std::max_element (envShort.begin(), envShort.end());
        int peakFrame = 0;
        const int searchEnd = std::min (numFrames, (int) (1.5 / model->hopSeconds));
        for (int fi = 0; fi < searchEnd; ++fi)
            if (envShort[(size_t) fi] >= 0.9f * peakShort) { peakFrame = fi; break; }

        const int corrEnd = peakFrame + (int) (0.06 / model->hopSeconds);
        const int fadeLen = std::max (1, (int) (0.04 / model->hopSeconds));

        for (int fi = 0; fi < std::min (numFrames, corrEnd + fadeLen); ++fi)
        {
            const float w = fi <= corrEnd ? 1.0f : 1.0f - (float) (fi - corrEnd) / (float) fadeLen;
            const float g = juce::jlimit (0.0f, 3.0f, envShort[(size_t) fi] / std::max (1.0e-7f, envLong[(size_t) fi]));
            const float gDb = toDb (std::max (g, 1.0e-6f)) * w;
            const float gLin = std::pow (10.0f, gDb / 20.0f);
            model->slopeA[(size_t) fi] += gDb;
            for (int b = 0; b < B; ++b)
                model->noiseAmp[(size_t) fi * B + (size_t) b] *= gLin;
        }
    }

    // ------------------------------------------------------------------ levels & regions
    for (int fi = 0; fi < numFrames; ++fi)
    {
        double e = 0.0;
        for (int h = 0; h < P; ++h)
        {
            const double a = std::pow (10.0, model->partialDb (fi, h) / 20.0);
            e += 0.5 * a * a;
        }
        for (int b = 0; b < B; ++b)
        {
            const double v = model->noiseAmp[(size_t) fi * B + (size_t) b];
            e += v * v;
        }
        model->level[(size_t) fi] = (float) std::sqrt (e);
    }

    const auto framesFor = [&] (double sec) { return std::max (1, (int) std::round (sec / model->hopSeconds)); };
    auto lvl = movingAverage (model->level, framesFor (0.015));

    const auto peakIt = std::max_element (lvl.begin(), lvl.end());
    const float peak = std::max (*peakIt, 1.0e-9f);

    int onset = 0;
    while (onset < numFrames - 1 && lvl[(size_t) onset] < peak * 0.01f) ++onset;
    int attackEnd = onset;
    while (attackEnd < numFrames - 1 && lvl[(size_t) attackEnd] < peak * 0.85f) ++attackEnd;
    attackEnd = std::max (attackEnd, onset + 1);
    int end = numFrames - 1;
    while (end > attackEnd && lvl[(size_t) end] < peak * 0.003f) --end;

    // drop the silent tail
    const int keepFrames = std::min (numFrames, end + framesFor (0.03) + 2);
    if (keepFrames < numFrames)
    {
        model->numFrames = keepFrames;
        model->f0Ratio.resize ((size_t) keepFrames);
        model->ratio.resize ((size_t) keepFrames * P);
        model->logRatio.resize ((size_t) keepFrames * P);
        model->envDb.resize ((size_t) keepFrames * P);
        model->fineDb.resize ((size_t) keepFrames * P);
        model->noiseAmp.resize ((size_t) keepFrames * B);
        model->level.resize ((size_t) keepFrames);
        model->slopeA.resize ((size_t) keepFrames);
        model->slopeB.resize ((size_t) keepFrames);
        model->loopGain.resize ((size_t) keepFrames);
        lvl.resize ((size_t) keepFrames);
    }
    const int nf = model->numFrames;
    end = std::min (end, nf - 1);
    attackEnd = std::min (attackEnd, nf - 2);

    const double sustainSec = (end - attackEnd) * model->hopSeconds;
    bool decaying = sustainSec < 0.25;
    if (! decaying)
    {
        const int a = std::min (end, attackEnd + framesFor (0.15));
        const int b = std::min (end, a + (int) (0.6 * (end - a)));
        decaying = lvl[(size_t) b] < lvl[(size_t) a] * 0.2f;
    }

    int loopStart, loopEnd;
    if (! decaying)
    {
        std::vector<float> sus (lvl.begin() + attackEnd, lvl.begin() + end + 1);
        std::nth_element (sus.begin(), sus.begin() + (long) sus.size() / 2, sus.end());
        const float median = sus[sus.size() / 2];
        int releaseStart = end;
        while (releaseStart > attackEnd && lvl[(size_t) releaseStart] < 0.5f * median) --releaseStart;

        loopStart = attackEnd + std::max (framesFor (0.06), (int) (0.08 * (end - attackEnd)));
        loopEnd = releaseStart - framesFor (0.05);
        if (loopEnd - loopStart < framesFor (0.15))
        {
            loopStart = attackEnd;
            loopEnd = std::max (loopStart + 1, end - 1);
        }
    }
    else
    {
        loopStart = attackEnd + framesFor (0.03);
        loopEnd = loopStart + std::max (1, std::min (framesFor (0.5), (int) (0.4 * (end - loopStart))));
    }
    loopStart = juce::jlimit (0, nf - 2, loopStart);
    loopEnd = juce::jlimit (loopStart + 1, nf - 1, loopEnd);

    auto slow = movingAverage (model->level, framesFor (0.12));
    const float ref = std::max (1.0e-9f, slow[(size_t) loopStart]);
    for (int fi = loopStart; fi <= loopEnd; ++fi)
        model->loopGain[(size_t) fi] = juce::jlimit (0.25f, 8.0f, ref / std::max (1.0e-9f, slow[(size_t) fi]));

    model->onsetFrame = onset;
    model->attackEndFrame = attackEnd;
    model->loopStartFrame = loopStart;
    model->loopEndFrame = loopEnd;
    model->decaying = decaying;

    report (1.0f);
    return model;
}

} // namespace ivi
