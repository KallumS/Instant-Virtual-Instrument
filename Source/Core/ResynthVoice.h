#pragma once

#include "Instrument.h"
#include "Articulations.h"
#include "EngineSettings.h"
#include "Random.h"
#include <array>

namespace ivi
{

/** One playing note. Resynthesises the spectral model in real time at the
    requested pitch, dynamic and round robin using an oscillator bank for the
    partials and a filter bank for the noise residual. */
class ResynthVoice
{
public:
    static constexpr int kMaxPartials = 256;
    static constexpr int kMaxBands = 32;
    static constexpr int kSubBlock = 64;

    struct StartParams
    {
        const Instrument* instrument = nullptr;
        int note = 60;
        float velocity = 0.8f;          // 0..1
        int roundRobin = 0;
        Articulation articulation = Articulation::Sustain;
        float forcedDynamic = -1.0f;    // >= 0: fixed dynamic position, discrete layer (export)
        uint64_t age = 0;
    };

    void prepare (double sampleRate);
    void start (const StartParams& p, const EngineSettings& s, float modWheel);
    void release() noexcept;
    void kill() noexcept;
    void legatoTo (int newNote, float velocity) noexcept;

    /** Adds the voice's output into left/right. */
    void render (float* left, float* right, int numSamples, const EngineSettings& s,
                 float modWheel, float bendSemis) noexcept;

    bool isActive() const noexcept       { return active; }
    bool isReleased() const noexcept     { return released; }
    bool isKilling() const noexcept      { return killing; }
    int getNote() const noexcept         { return note; }
    int getLayer() const noexcept        { return displayLayer; }
    int getRoundRobin() const noexcept   { return roundRobin; }
    uint64_t getAge() const noexcept     { return age; }
    Articulation getArticulation() const noexcept { return articulation; }
    const Instrument* getInstrument() const noexcept { return instrument; }
    double getFramePos() const noexcept  { return framePos; }

private:
    /** Updates the oscillator/noise targets for the end of the next sub-block. Returns true when the voice has finished. */
    bool computeTargets (const EngineSettings& s, float modWheel, float bendSemis, float dt) noexcept;
    void synthesise (float* out, int n) noexcept;
    void updateNoiseFilter (int b, float fc, float q) noexcept;

    double sr = 48000.0;
    const Instrument* instrument = nullptr;
    const SpectralModel* model = nullptr;

    bool active = false, released = false, killing = false, firstBlock = true;
    int note = 60, roundRobin = 0, displayLayer = 0;
    uint64_t age = 0;
    Articulation articulation = Articulation::Sustain;
    float velocity = 0.8f, velX = 0.8f, velXTarget = 0.8f, forcedDynamic = -1.0f;

    // time / position
    float t = 0.0f, tRelease = 0.0f;
    double framePos = 0.0;
    int direction = 1;
    bool loopEnabled = true, inLoop = false, frozen = false;
    double freezeFrame = 0.0;
    float attackRate = 1.0f;

    // envelopes
    float releaseEnv = 1.0f, killEnv = 1.0f;
    float pizzTau = 0.5f;

    // legato glide
    float noteF = 60.0f, glideFrom = 60.0f, glideTo = 60.0f, glidePos = 1.0f;

    // tremolo
    float tremPhase = 0.0f, tremRateMul = 1.0f, tremDepth = 0.75f;
    Rng perfRng { 1 };

    // smoothed zone variation
    float zDetune = 0, zGain = 0, zTilt = 0, zNoise = 0;
    std::array<float, kJitterLen> zJitter {};
    ZoneVariation zoneA;   // the voice's primary zone (scoop, life rates, ...)

    // oscillator bank (structure of arrays, padded to multiples of 8)
    alignas (32) float re[kMaxPartials] {};
    alignas (32) float im[kMaxPartials] {};
    alignas (32) float amp[kMaxPartials] {};
    alignas (32) float ampTarget[kMaxPartials] {};
    alignas (32) float dAmp[kMaxPartials] {};
    alignas (32) float rotC[kMaxPartials] {};
    alignas (32) float rotS[kMaxPartials] {};
    int numPartialsPrev = 0, numPartialsNow = 0;

    // per-block scratch (source frame interpolation)
    float srcRatio[kMaxPartials] {}, srcLogF[kMaxPartials] {}, srcEnv[kMaxPartials] {}, srcFine[kMaxPartials] {};

    // noise bank
    struct Band { float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1, fc = 0, gain = 0, target = 0; };
    std::array<Band, kMaxBands> bands {};
    int numBands = 0;
    FastNoise noise;

    float panL = 1.0f, panR = 1.0f;
    float scratch[kSubBlock] {};
};

} // namespace ivi
