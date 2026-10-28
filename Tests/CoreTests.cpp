// Headless tests for the analysis + resynthesis engine.
#include "Core/SpectralAnalyzer.h"
#include "Core/PitchTracker.h"
#include "Core/Instrument.h"
#include "Core/SynthEngine.h"
#include "Core/MultisampleExporter.h"
#include "Core/DemoSource.h"
#include <chrono>
#include <cstdio>
#include <functional>
#include <set>

using namespace ivi;

namespace
{
    int failures = 0, checks = 0;

    void check (bool ok, const juce::String& what)
    {
        ++checks;
        if (! ok)
        {
            ++failures;
            std::printf ("    FAIL: %s\n", what.toRawUTF8());
        }
    }

    void section (const char* name) { std::printf ("\n[%s]\n", name); }
    void info (const juce::String& s) { std::printf ("    %s\n", s.toRawUTF8()); }

    constexpr double kSr = 48000.0;

    float rms (const juce::AudioBuffer<float>& b, double fromSec, double toSec, double sr = kSr)
    {
        const int a = juce::jlimit (0, b.getNumSamples(), (int) (fromSec * sr));
        const int e = juce::jlimit (a, b.getNumSamples(), (int) (toSec * sr));
        return e > a ? b.getRMSLevel (0, a, e - a) : 0.0f;
    }

    float db (float x) { return juce::Decibels::gainToDecibels (x, -200.0f); }

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (c, i)))
                    return false;
        return true;
    }

    /** Average magnitude spectrum (power) over a region, FFT size 8192. */
    std::vector<float> averageSpectrum (const juce::AudioBuffer<float>& b, double fromSec, double toSec, double sr = kSr)
    {
        constexpr int order = 13, N = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> acc (N / 2 + 1, 0.0f), buf (2 * N);
        int frames = 0;
        for (int start = (int) (fromSec * sr); start + N <= (int) (toSec * sr) && start + N <= b.getNumSamples(); start += N / 4)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            for (int i = 0; i < N; ++i)
                buf[(size_t) i] = b.getSample (0, start + i) * (0.5f - 0.5f * std::cos (6.2831853f * i / (N - 1)));
            fft.performFrequencyOnlyForwardTransform (buf.data(), true);
            for (int k = 0; k <= N / 2; ++k)
                acc[(size_t) k] += buf[(size_t) k] * buf[(size_t) k];
            ++frames;
        }
        for (auto& v : acc)
            v /= (float) std::max (1, frames);
        return acc;
    }

    float centroid (const std::vector<float>& pw, double sr = kSr)
    {
        const double binHz = sr / (2.0 * (double) (pw.size() - 1));
        double num = 0, den = 0;
        for (size_t k = 1; k < pw.size(); ++k)
        {
            num += k * binHz * pw[k];
            den += pw[k];
        }
        return den > 0 ? (float) (num / den) : 0.0f;
    }

    /** Third-octave band levels (dB) between 100 Hz and 8 kHz. */
    std::vector<float> thirdOctaves (const std::vector<float>& pw, double sr = kSr)
    {
        const double binHz = sr / (2.0 * (double) (pw.size() - 1));
        std::vector<float> out;
        for (double fc = 125.0; fc < 8000.0; fc *= std::pow (2.0, 1.0 / 3.0))
        {
            const double lo = fc / std::pow (2.0, 1.0 / 6.0), hi = fc * std::pow (2.0, 1.0 / 6.0);
            double e = 0;
            for (int k = (int) (lo / binHz); k <= (int) (hi / binHz) && k < (int) pw.size(); ++k)
                e += pw[(size_t) k];
            out.push_back (10.0f * std::log10 ((float) e + 1.0e-20f));
        }
        return out;
    }

    float centsError (float hz, float expectedHz) { return 1200.0f * std::log2 (hz / expectedHz); }

    std::shared_ptr<SpectralModel> analyseDemo (const DemoOptions& o, float forcedRoot = -1.0f)
    {
        auto audio = makeDemoSample (o);
        AnalysisOptions ao;
        ao.forcedRootMidi = forcedRoot;
        return SpectralAnalyzer::analyse (audio, o.sampleRate, ao);
    }

    std::unique_ptr<Instrument> build (std::shared_ptr<SpectralModel> m, int layers = 5, int rrs = 4,
                                       float variation = 0.5f, int lo = 21, int hi = 108)
    {
        BuildSettings bs;
        bs.lowKey = lo;
        bs.highKey = hi;
        bs.numLayers = layers;
        bs.numRoundRobins = rrs;
        bs.variation = variation;
        bs.seed = 42;
        return buildInstrument (m, bs);
    }

    juce::AudioBuffer<float> render (const Instrument& inst, int key, int layer, int rr, Articulation art,
                                     float hold = 1.5f, EngineSettings s = {})
    {
        return MultisampleExporter::renderZone (inst, s, key, layer, rr, art, kSr, hold);
    }
}

static void testAnalysis()
{
    section ("analysis: pitch, partials, regions");
    DemoOptions o;
    auto m = analyseDemo (o);
    check (m != nullptr, "model created");
    if (! m) return;

    info ("root " + juce::String (m->rootMidi, 3) + " f0 " + juce::String (m->f0Ref, 2) + " Hz, partials "
          + juce::String (m->numPartials) + ", frames " + juce::String (m->numFrames) + ", fft " + juce::String (m->fftSize)
          + ", attackEnd " + juce::String (m->attackEndFrame) + ", loop " + juce::String (m->loopStartFrame) + "-"
          + juce::String (m->loopEndFrame) + (m->decaying ? ", decaying" : ", sustained"));

    check (m->pitched, "pitch detected");
    check (std::abs (m->rootMidi - 57.0f) < 0.1f, "root note is A3 (57)");
    check (m->numPartials > 60, "enough partials tracked");
    check (! m->decaying, "bowed source classified as sustained");
    check (m->loopEndFrame - m->loopStartFrame > (int) (0.5 / m->hopSeconds), "loop region at least 0.5 s");
    check (m->attackEndFrame * m->hopSeconds > 0.03 && m->attackEndFrame * m->hopSeconds < 0.3, "attack end near 120 ms");

    // harmonic amplitudes against the known synthesis recipe (steady, no vibrato, no noise)
    DemoOptions clean;
    clean.vibrato = false;
    clean.noise = 0.0f;
    auto mc = analyseDemo (clean);
    auto body = [] (double f)
    {
        auto peak = [f] (double c, double w, double g) { const double d = std::log2 (f / c) / w; return g * std::exp (-0.5 * d * d); };
        return 0.25 + peak (480.0, 0.3, 1.0) + peak (1400.0, 0.35, 0.7) + peak (3100.0, 0.4, 0.45);
    };
    const int frame = mc->numFrames / 2;
    float worst = 0.0f;
    for (int h = 2; h <= 30; ++h)
    {
        const double expected = 20.0 * std::log10 ((body (h * 220.0) / std::pow (h, 0.85)) / body (220.0));
        const double measured = mc->partialDb (frame, h - 1) - mc->partialDb (frame, 0);
        worst = std::max (worst, (float) std::abs (measured - expected));
    }
    info ("max harmonic level error (h2..h30): " + juce::String (worst, 3) + " dB");
    check (worst < 0.5f, "harmonic levels within 0.5 dB");

    float maxRatioErr = 0.0f;
    for (int h = 1; h <= 30; ++h)
        maxRatioErr = std::max (maxRatioErr, std::abs (mc->ratio[(size_t) frame * mc->numPartials + h - 1] - (float) h));
    check (maxRatioErr < 0.01f, "harmonic frequencies tracked (max ratio error " + juce::String (maxRatioErr, 4) + ")");

    float noiseSum = 0.0f;
    for (int b = 0; b < mc->numBands; ++b)
        noiseSum += mc->noiseAmp[(size_t) frame * mc->numBands + b];
    float noisySum = 0.0f;
    for (int b = 0; b < m->numBands; ++b)
        noisySum += m->noiseAmp[(size_t) (m->numFrames / 2) * m->numBands + b];
    info ("residual noise clean " + juce::String (noiseSum, 5) + " vs noisy " + juce::String (noisySum, 5));
    check (noisySum > 3.0f * noiseSum, "bow noise lands in the residual");
}

static void testOtherSources()
{
    section ("analysis: other sources");
    {
        DemoOptions o;
        o.plucked = true;
        o.inharmonicity = 0.0003f;
        o.f0 = 130.81;
        o.seconds = 2.0;
        auto m = analyseDemo (o);
        check (m && m->decaying, "plucked source classified as decaying");
        check (m && std::abs (m->rootMidi - 48.0f) < 0.25f, "plucked root C3 (got " + juce::String (m ? m->rootMidi : 0.0f, 2) + ")");
        if (m)
        {
            const int f = m->attackEndFrame + 4;
            const float r20 = m->ratio[(size_t) f * m->numPartials + 19];
            const float expected = 20.0f * std::sqrt (1.0f + 0.0003f * 400.0f);
            info ("partial 20 ratio " + juce::String (r20, 3) + " (stretched " + juce::String (expected, 3) + ")");
            check (std::abs (r20 - expected) < 0.15f, "inharmonic partial stretch tracked");
        }
    }
    for (double f0 : { 41.2, 1046.5, 2093.0 })
    {
        DemoOptions o;
        o.f0 = f0;
        o.seconds = 1.2;
        auto m = analyseDemo (o);
        check (m != nullptr && std::abs (m->f0Ref / f0 - 1.0) < 0.01, "pitch of " + juce::String (f0) + " Hz source");
        if (m)
        {
            auto inst = build (m, 3, 2, 0.5f, 24, 108);
            auto a = render (*inst, 60, 1, 0, Articulation::Sustain, 0.5f);
            check (allFinite (a) && a.getMagnitude (0, 0, a.getNumSamples()) < 2.0f, "renders cleanly from " + juce::String (f0) + " Hz");
        }
    }
    {
        juce::AudioBuffer<float> noise (1, 48000);
        Rng r (3);
        for (int i = 0; i < noise.getNumSamples(); ++i)
            noise.setSample (0, i, r.bipolar() * 0.3f);
        auto m = SpectralAnalyzer::analyse (noise, kSr);
        check (m != nullptr && ! m->pitched, "unpitched source handled (falls back to C4)");
        if (m)
        {
            auto inst = build (m, 2, 2);
            auto a = render (*inst, 72, 1, 0, Articulation::Sustain, 0.4f);
            check (allFinite (a), "unpitched render finite");
        }
    }
    {
        DemoOptions o;
        o.seconds = 0.06;
        auto m = analyseDemo (o, 57.0f);
        check (m != nullptr, "very short (60 ms) source analysed");
        if (m)
        {
            auto inst = build (m, 2, 2);
            EngineSettings loop;
            loop.sustainMode = 1;
            auto a = render (*inst, 64, 1, 0, Articulation::Sustain, 1.0f, loop);
            check (allFinite (a) && rms (a, 0.6, 0.9) > 1.0e-4f, "short source still sustains when looped");
        }
    }
    {
        juce::AudioBuffer<float> silent (1, 4800);
        silent.clear();
        check (SpectralAnalyzer::analyse (silent, kSr) == nullptr, "silent file rejected");
    }
}

static void testPitchAcrossKeys (const Instrument& inst)
{
    section ("mapping: every semitone in tune");
    float worst = 0.0f;
    for (int key = 33; key <= 93; key += 5)
    {
        auto a = render (inst, key, 2, 0, Articulation::Sustain, 1.2f);
        const auto est = PitchTracker::estimate (a.getReadPointer (0, (int) (0.3 * kSr)), (int) (0.8 * kSr), kSr);
        const float err = centsError (est.f0, (float) midiToHz (key));
        worst = std::max (worst, std::abs (err));
        if (std::abs (err) > 10.0f)
            info ("key " + juce::String (key) + " off by " + juce::String (err, 1) + " cents");
    }
    info ("worst pitch error " + juce::String (worst, 2) + " cents (includes vibrato + humanised detune)");
    check (worst < 10.0f, "all keys within 10 cents");
}

static void testFidelity (std::shared_ptr<SpectralModel> m, const juce::AudioBuffer<float>& source)
{
    section ("resynthesis fidelity at the root key");
    auto inst = build (m, 1, 1, 0.0f, 50, 64);
    EngineSettings s;
    s.pitchDrift = 1.0f;
    auto a = render (*inst, 57, 0, 0, Articulation::Sustain, 1.8f, s);
    const auto src = thirdOctaves (averageSpectrum (source, 0.5, 1.9));
    const auto out = thirdOctaves (averageSpectrum (a, 0.5, 1.9));
    float sum = 0.0f, worst = 0.0f;
    juce::String bandsLine;
    for (size_t i = 0; i < src.size(); ++i)
        bandsLine << juce::String (out[i] - src[i], 1) << " ";
    info ("band diffs (dB, 125 Hz..8 kHz): " + bandsLine);
    for (size_t i = 0; i < src.size(); ++i)
    {
        const float d = std::abs (src[i] - out[i]);
        sum += d;
        worst = std::max (worst, d);
    }
    if (juce::SystemStats::getEnvironmentVariable ("IVI_DEBUG", {}).isNotEmpty())
    {
        EngineSettings q = s;
        q.noiseDb = -100.0f;
        auto noNoise = thirdOctaves (averageSpectrum (render (*inst, 57, 0, 0, Articulation::Sustain, 1.8f, q), 0.5, 1.9));
        juce::String l2;
        for (size_t i = 0; i < src.size(); ++i) l2 << juce::String (noNoise[i] - src[i], 1) << " ";
        info ("partials only:  " + l2);
        juce::String l3;
        const int f = m->numFrames / 2;
        for (int b = 0; b < m->numBands; ++b) l3 << juce::String ((int) m->bandCentre[(size_t) b]) << ":" << juce::String (db (m->noiseAmp[(size_t) f * m->numBands + b]), 1) << " ";
        info ("noise bands: " + l3);
    }
    info ("1/3-octave spectral distance mean " + juce::String (sum / (float) src.size(), 2) + " dB, worst " + juce::String (worst, 2) + " dB");
    check (sum / (float) src.size() < 1.5f, "mean spectral distance < 1.5 dB");
    check (worst < 4.0f, "worst band < 4 dB");
    info ("level: source " + juce::String (db (rms (source, 0.5, 1.9)), 2) + " dB, resynth " + juce::String (db (rms (a, 0.5, 1.9)), 2) + " dB");
    check (std::abs (db (rms (source, 0.5, 1.9)) - db (rms (a, 0.5, 1.9))) < 1.5f, "level preserved at the source dynamic");
}

static void testLayers (const Instrument& inst)
{
    section ("dynamic layers: louder and brighter");
    float prevRms = 0.0f, prevCentroid = 0.0f;
    bool louder = true, brighter = true;
    juce::String line;
    for (int l = 0; l < inst.settings.numLayers; ++l)
    {
        auto a = render (inst, 60, l, 0, Articulation::Sustain, 1.0f);
        const float r = rms (a, 0.3, 0.9);
        const float c = centroid (averageSpectrum (a, 0.3, 0.95));
        line << Instrument::layerName (l, inst.settings.numLayers) << ": " << juce::String (db (r), 1) << " dB / "
             << juce::String (c, 0) << " Hz   ";
        if (l > 0)
        {
            louder &= r > prevRms * 1.25f;
            brighter &= c > prevCentroid * 1.03f;
        }
        prevRms = r;
        prevCentroid = c;
        check (allFinite (a), "layer " + juce::String (l) + " finite");
    }
    info (line);
    check (louder, "each layer louder than the last");
    check (brighter, "each layer brighter than the last");
}

static void testRoundRobins (const Instrument& inst)
{
    section ("round robins: same note, different takes");
    std::vector<juce::AudioBuffer<float>> takes;
    for (int rr = 0; rr < inst.settings.numRoundRobins; ++rr)
        takes.push_back (render (inst, 62, 2, rr, Articulation::Staccato));

    float maxCorr = 0.0f, maxLevelDiff = 0.0f;
    for (size_t i = 0; i < takes.size(); ++i)
        for (size_t j = i + 1; j < takes.size(); ++j)
        {
            const int n = std::min (takes[i].getNumSamples(), takes[j].getNumSamples());
            double xy = 0, xx = 0, yy = 0;
            for (int k = 0; k < n; ++k)
            {
                const double a = takes[i].getSample (0, k), b = takes[j].getSample (0, k);
                xy += a * b; xx += a * a; yy += b * b;
            }
            maxCorr = std::max (maxCorr, (float) (xy / std::sqrt (xx * yy + 1e-30)));
            maxLevelDiff = std::max (maxLevelDiff, std::abs (db (rms (takes[i], 0, 0.2)) - db (rms (takes[j], 0, 0.2))));
        }
    info ("max waveform correlation " + juce::String (maxCorr, 3) + ", max level difference " + juce::String (maxLevelDiff, 2) + " dB");
    check (maxCorr < 0.95f, "takes are audibly different");
    check (maxLevelDiff < 3.0f, "takes are consistent in level");

    // the same zone renders identically (deterministic library)
    auto again = render (inst, 62, 2, 1, Articulation::Staccato);
    float diff = 0.0f;
    for (int k = 0; k < std::min (again.getNumSamples(), takes[1].getNumSamples()); ++k)
        diff = std::max (diff, std::abs (again.getSample (0, k) - takes[1].getSample (0, k)));
    check (diff < 1.0e-6f, "zones are deterministic");
}

static void testArticulations (std::shared_ptr<SpectralModel> m)
{
    section ("articulations");
    auto inst = build (m, 5, 2);
    auto lengthOf = [] (const juce::AudioBuffer<float>& b) { return (double) b.getNumSamples() / kSr; };

    for (int a = 0; a < kNumArticulations; ++a)
    {
        auto buf = render (*inst, 60, 3, 0, (Articulation) a, 2.0f);
        check (allFinite (buf), articulationName ((Articulation) a) + " finite");
        check (buf.getMagnitude (0, 0, buf.getNumSamples()) < 1.0f, articulationName ((Articulation) a) + " below 0 dBFS");
        info (articulationName ((Articulation) a).paddedRight (' ', 11) + " length " + juce::String (lengthOf (buf), 2) + " s, peak "
              + juce::String (db (buf.getMagnitude (0, 0, buf.getNumSamples())), 1) + " dB");
    }

    auto sus = render (*inst, 60, 3, 0, Articulation::Sustain, 4.0f);
    check (lengthOf (sus) > 4.0, "sustain holds past the 2.5 s source (looped)");
    check (db (rms (sus, 3.5, 3.9)) > db (rms (sus, 0.5, 1.0)) - 3.0f, "sustain level steady inside the loop");
    check (lengthOf (sus) < 4.0 + 1.5, "sustain releases after note-off");

    auto stac = render (*inst, 60, 3, 0, Articulation::Staccato, 2.0f);
    check (lengthOf (stac) < 0.6, "staccato is short");
    auto spic = render (*inst, 60, 3, 0, Articulation::Spiccato, 2.0f);
    check (lengthOf (spic) < lengthOf (stac), "spiccato shorter than staccato");
    check (centroid (averageSpectrum (spic, 0, 0.2)) > centroid (averageSpectrum (stac, 0, 0.2)), "spiccato brighter than staccato");

    auto marc = render (*inst, 60, 3, 0, Articulation::Marcato, 2.0f);
    check (db (rms (marc, 0.05, 0.15)) > db (rms (marc, 1.0, 1.5)) + 4.0f, "marcato accent");

    auto pizz = render (*inst, 60, 3, 0, Articulation::Pizzicato, 2.0f);
    check (db (rms (pizz, 0.0, 0.1)) > db (rms (pizz, 1.0, 1.2)) + 15.0f, "pizzicato decays");
    check (centroid (averageSpectrum (pizz, 0.0, 0.2)) > centroid (averageSpectrum (pizz, 0.6, 0.8)), "pizzicato darkens as it rings");

    auto trem = render (*inst, 60, 3, 0, Articulation::Tremolo, 2.0f);
    {
        float lo = 1e9f, hi = 0.0f;
        for (double t0 = 0.5; t0 < 1.5; t0 += 0.004)
        {
            const float r = rms (trem, t0, t0 + 0.004);
            lo = std::min (lo, r);
            hi = std::max (hi, r);
        }
        info ("tremolo modulation depth " + juce::String (db (hi) - db (lo), 1) + " dB");
        check (db (hi) - db (lo) > 6.0f, "tremolo modulates");
    }

    auto swell = render (*inst, 60, 4, 0, Articulation::Swell, 2.5f);
    check (db (rms (swell, 1.9, 2.2)) > db (rms (swell, 0.1, 0.3)) + 10.0f, "swell crescendos");
    check (centroid (averageSpectrum (swell, 1.9, 2.1)) > centroid (averageSpectrum (swell, 0.2, 0.4)), "swell brightens through the layers");

    auto fp = render (*inst, 60, 3, 0, Articulation::Fortepiano, 2.0f);
    check (db (rms (fp, 0.0, 0.1)) > db (rms (fp, 0.8, 1.2)) + 8.0f, "fortepiano drops after the attack");
}

static void testEngine (std::shared_ptr<SpectralModel> m)
{
    section ("engine: MIDI, keyswitches, legato, round-robin cycling, pedal");
    SynthEngine engine;
    engine.prepare (kSr, 512);
    engine.setInstrument (build (m, 5, 4));
    EngineSettings s;
    engine.setSettings (s);

    juce::AudioBuffer<float> out (2, 512);
    auto run = [&] (std::function<void (juce::MidiBuffer&)> fill, int blocks = 1)
    {
        for (int i = 0; i < blocks; ++i)
        {
            juce::MidiBuffer mb;
            if (i == 0 && fill) fill (mb);
            engine.process (out, mb);
        }
    };

    run ({});
    run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0); }, 4);
    check (engine.getActiveVoiceCount() == 1, "note on starts one voice");
    check (out.getMagnitude (0, 0, 512) > 1.0e-4f, "audio produced");
    run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::noteOff (1, 60), 0); }, 200);
    check (engine.getActiveVoiceCount() == 0, "note off releases the voice");

    // round robin cycling on repeated notes
    std::set<int> seen;
    for (int i = 0; i < 4; ++i)
    {
        run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 70), 0); }, 2);
        seen.insert (engine.getKeyRoundRobin (64));
        run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::noteOff (1, 64), 0); }, 1);
    }
    check (seen.size() == 4, "4 repeated notes use 4 different round robins");

    // velocity selects the layer
    run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::allNotesOff (1), 0); }, 200);
    run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 10), 0); }, 2);
    const auto softMask = engine.getKeyLayerMask (67);
    run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 125), 0); }, 2);
    const auto loudMask = engine.getKeyLayerMask (69);
    check (softMask == 1u && loudMask == (1u << 4), "velocity 10 -> layer 1, velocity 125 -> layer 5");

    // keyswitch
    run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::allNotesOff (1), 0); }, 200);
    run ([&] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::noteOn (1, s.keyswitchBase + (int) Articulation::Legato, (juce::uint8) 100), 0); });
    check (engine.getArticulation() == (int) Articulation::Legato, "keyswitch selects legato");
    check (engine.getActiveVoiceCount() == 0, "keyswitch makes no sound");

    // legato: overlapping notes glide in one voice
    run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0); }, 10);
    run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0); }, 10);
    check (engine.getActiveVoiceCount() == 1, "legato keeps a single voice");
    check (engine.getKeyLayerMask (64) != 0 && engine.getKeyLayerMask (60) == 0, "legato voice moved to the new note");
    run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::noteOff (1, 64), 0); }, 10);
    check (engine.getKeyLayerMask (60) != 0, "releasing the top note returns to the held note");
    run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::noteOff (1, 60), 0); }, 200);
    check (engine.getActiveVoiceCount() == 0, "legato line ends");

    // sustain pedal
    engine.setArticulation (0);
    run ([] (juce::MidiBuffer& mb) {
        mb.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
        mb.addEvent (juce::MidiMessage::noteOn (1, 72, (juce::uint8) 90), 1);
        mb.addEvent (juce::MidiMessage::noteOff (1, 72), 100);
    }, 100);
    check (engine.getActiveVoiceCount() == 1, "pedal holds the note");
    run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0); }, 200);
    check (engine.getActiveVoiceCount() == 0, "pedal up releases");

    // polyphony limit
    s.polyphony = 8;
    engine.setSettings (s);
    run ([] (juce::MidiBuffer& mb) { for (int n = 40; n < 60; ++n) mb.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 90), n - 40); }, 50);
    check (engine.getActiveVoiceCount() <= 8, "polyphony limit respected (" + juce::String (engine.getActiveVoiceCount()) + ")");
    check (allFinite (out), "engine output finite");

    // instrument swap while playing
    engine.setInstrument (build (m, 3, 2));
    run ({}, 20);
    engine.collectGarbage();
    engine.setInstrument (build (m, 2, 2));
    run ({}, 20);
    engine.collectGarbage();
    run ([] (juce::MidiBuffer& mb) { mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 90), 0); }, 4);
    check (engine.getKeyLayerMask (60) != 0, "new instrument takes over after swaps");
}

static void testPerformance (std::shared_ptr<SpectralModel> m)
{
    section ("performance");
    SynthEngine engine;
    engine.prepare (kSr, 256);
    engine.setInstrument (build (m, 5, 4));
    EngineSettings s;
    s.polyphony = 32;
    engine.setSettings (s);
    juce::AudioBuffer<float> out (2, 256);

    juce::MidiBuffer first;
    for (int i = 0; i < 32; ++i)
        first.addEvent (juce::MidiMessage::noteOn (1, 36 + i * 2, (juce::uint8) 100), i);
    engine.process (out, first);

    const int blocks = (int) (5.0 * kSr / 256);
    const auto t0 = std::chrono::steady_clock::now();
    juce::MidiBuffer empty;
    for (int i = 0; i < blocks; ++i)
        engine.process (out, empty);
    const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    info ("32 voices x 5 s rendered in " + juce::String (sec, 3) + " s  (" + juce::String (sec / 5.0 * 100.0, 1)
          + "% of one core, " + juce::String (engine.getActiveVoiceCount()) + " voices)");
    check (sec < 5.0, "faster than real time with 32 voices");
}

static void testExport (std::shared_ptr<SpectralModel> m)
{
    section ("export: WAV + SFZ");
    auto inst = build (m, 2, 2, 0.5f, 60, 62);
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ivi-export-test");
    dir.deleteRecursively();
    ExportOptions o;
    o.parentFolder = dir;
    o.instrumentName = "Test Cello";
    o.articulations = { Articulation::Sustain, Articulation::Staccato };
    o.holdSeconds = 0.5f;
    auto r = MultisampleExporter::exportSfz (*inst, EngineSettings {}, o);
    info (r.message);
    check (r.ok, "export succeeded");
    check (r.filesWritten == 3 * 2 * 2 * 2, "3 keys x 2 layers x 2 RR x 2 articulations");
    const auto sfz = r.sfzFile.loadFileAsString();
    check (sfz.contains ("seq_length=2") && sfz.contains ("sw_last=") && sfz.contains ("lovel="), "sfz has RR, keyswitch and velocity mapping");
    juce::StringArray lines;
    lines.addLines (sfz);
    int regions = 0;
    for (auto& l : lines)
        if (l.startsWith ("<region>")) ++regions;
    check (regions == r.filesWritten, "one region per sample");
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    auto files = dir.findChildFiles (juce::File::findFiles, true, "*.wav");
    std::unique_ptr<juce::AudioFormatReader> reader (files.isEmpty() ? nullptr : fm.createReaderFor (files[0]));
    check (reader != nullptr && reader->numChannels == 1 && reader->lengthInSamples > 1000, "WAV readable");
    dir.deleteRecursively();
}

/** IVI_RENDER_DEMO=<file.wav> renders a short phrase showing layers, round robins and articulations. */
static void renderDemoPhrase (std::shared_ptr<SpectralModel> m, const juce::File& file)
{
    SynthEngine engine;
    engine.prepare (kSr, 512);
    engine.setInstrument (build (m, 5, 4));
    EngineSettings s;
    s.polyphony = 32;
    engine.setSettings (s);

    struct Ev { double t; juce::MidiMessage msg; };
    std::vector<Ev> ev;
    auto note = [&] (double t, double len, int n, int vel)
    {
        ev.push_back ({ t, juce::MidiMessage::noteOn (1, n, (juce::uint8) vel) });
        ev.push_back ({ t + len, juce::MidiMessage::noteOff (1, n) });
    };
    auto ks = [&] (double t, Articulation a) { note (t, 0.05, s.keyswitchBase + (int) a, 100); };

    double t = 0.1;
    ks (t, Articulation::Sustain);                       // same note through the 5 layers
    for (int v : { 15, 40, 70, 95, 125 }) { note (t, 0.8, 55, v); t += 1.0; }
    ks (t, Articulation::Staccato);                      // repeated notes: round robins
    for (int i = 0; i < 8; ++i) { note (t, 0.1, 62, 90); t += 0.18; }
    t += 0.3;
    ks (t, Articulation::Spiccato);
    for (int n : { 60, 62, 64, 65, 67, 69, 71, 72 }) { note (t, 0.08, n, 100); t += 0.12; }
    t += 0.4;
    ks (t, Articulation::Pizzicato);
    for (int n : { 48, 55, 60, 64, 67, 72 }) { note (t, 0.3, n, 100); t += 0.25; }
    t += 0.8;
    ks (t, Articulation::Legato);
    for (int n : { 62, 64, 66, 67, 69, 67, 66, 64 }) { note (t, 0.5, n, 90); t += 0.45; }
    t += 0.8;
    ks (t, Articulation::Marcato);
    for (int n : { 50, 57, 62 }) { note (t, 0.7, n, 110); t += 0.8; }
    ks (t, Articulation::Tremolo);
    for (int n : { 50, 57, 62, 66 }) note (t, 2.0, n, 90);
    t += 2.6;
    ks (t, Articulation::Swell);
    for (int n : { 43, 50, 59, 62 }) note (t, 2.6, n, 120);
    t += 3.2;
    ks (t, Articulation::Fortepiano);
    for (int n : { 45, 52, 61, 64 }) note (t, 2.0, n, 120);
    t += 3.0;

    std::stable_sort (ev.begin(), ev.end(), [] (auto& a, auto& b) { return a.t < b.t; });
    const int total = (int) (t * kSr);
    juce::AudioBuffer<float> out (2, total), block (2, 512);
    size_t e = 0;
    for (int pos = 0; pos < total; pos += 512)
    {
        juce::MidiBuffer mb;
        while (e < ev.size() && ev[e].t * kSr < pos + 512)
        {
            mb.addEvent (ev[e].msg, juce::jlimit (0, 511, (int) (ev[e].t * kSr) - pos));
            ++e;
        }
        engine.process (block, mb);
        const int n = std::min (512, total - pos);
        for (int c = 0; c < 2; ++c)
            out.copyFrom (c, pos, block, c, 0, n);
    }

    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    juce::WavAudioFormat wav;
    if (auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (kSr).withNumChannels (2).withBitsPerSample (24)))
        w->writeFromAudioSampleBuffer (out, 0, total);
    info ("demo phrase written to " + file.getFullPathName() + " (peak " + juce::String (db (out.getMagnitude (0, total)), 1) + " dB)");
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);   // show progress at once, even under CI
    std::printf ("Instant Virtual Instrument - core tests\n");

    testAnalysis();
    testOtherSources();

    DemoOptions o;
    auto source = makeDemoSample (o);
    auto model = SpectralAnalyzer::analyse (source, kSr);
    if (model == nullptr)
    {
        std::printf ("analysis failed\n");
        return 1;
    }
    // analysis normalises the source; use the same signal for comparisons
    const auto& normSource = model->source;

    auto inst = build (model, 5, 4);
    testPitchAcrossKeys (*inst);
    testFidelity (model, normSource);
    testLayers (*inst);
    testRoundRobins (*inst);
    testArticulations (model);
    testEngine (model);
    testPerformance (model);
    testExport (model);

    const auto demoPath = juce::SystemStats::getEnvironmentVariable ("IVI_RENDER_DEMO", {});
    if (demoPath.isNotEmpty())
    {
        juce::AudioBuffer<float> longer = makeDemoSample ({ kSr, 220.0, 3.0 });
        renderDemoPhrase (SpectralAnalyzer::analyse (longer, kSr), juce::File (demoPath));
    }

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
