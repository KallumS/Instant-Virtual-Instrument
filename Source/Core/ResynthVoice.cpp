#include "ResynthVoice.h"
#include "FastMath.h"
#include <algorithm>
#include <cmath>

namespace ivi
{

namespace
{
    constexpr float kTwoPi = 6.28318531f;
    constexpr float kPi = 3.14159265f;

    inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
    inline float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
    inline float smoothstep (float x) noexcept { x = clamp01 (x); return x * x * (3.0f - 2.0f * x); }

    float articulationAttackSpeed (Articulation a) noexcept
    {
        switch (a)
        {
            case Articulation::Staccato:   return 1.3f;
            case Articulation::Spiccato:   return 1.7f;
            case Articulation::Marcato:    return 1.4f;
            case Articulation::Fortepiano: return 1.3f;
            default:                       return 1.0f;
        }
    }
}

void ResynthVoice::prepare (double sampleRate)
{
    sr = sampleRate;
    active = false;
}

void ResynthVoice::start (const StartParams& p, const EngineSettings& s, float modWheel)
{
    jassert (p.instrument != nullptr && p.instrument->model != nullptr);
    instrument = p.instrument;
    model = instrument->model.get();
    const auto& m = *model;

    note = p.note;
    velocity = clamp01 (p.velocity);
    velX = velXTarget = velocity;
    forcedDynamic = p.forcedDynamic;
    roundRobin = p.roundRobin;
    articulation = p.articulation;
    age = p.age;

    active = true;
    released = killing = false;
    firstBlock = true;
    t = tRelease = 0.0f;
    releaseEnv = killEnv = 1.0f;
    noteF = glideFrom = glideTo = (float) note;
    glidePos = 1.0f;

    const bool useWheel = forcedDynamic < 0.0f && s.dynamicsSource == 1 && isLong (articulation);
    const float x0 = forcedDynamic >= 0.0f ? forcedDynamic : (useWheel ? modWheel : velX);
    displayLayer = instrument->layerForDynamic (x0);
    zoneA = instrument->zone (note, displayLayer, roundRobin);
    zDetune = zoneA.detuneCents;
    zGain = zoneA.gainDb;
    zTilt = zoneA.tiltDb;
    zNoise = zoneA.noiseDb;
    zJitter = zoneA.jitterDb;

    // louder notes speak faster, softer notes bloom more slowly
    const float attackDyn = forcedDynamic >= 0.0f ? forcedDynamic : velX;
    const float dynScale = fast::exp2 ((s.sourceDynamic - attackDyn) * 1.6f);
    attackRate = articulationAttackSpeed (articulation) / (dynScale * zoneA.attackScale);

    const bool wantsLoop = s.sustainMode == 1 || (s.sustainMode == 0 && ! m.decaying);
    loopEnabled = wantsLoop && ! isOneShot (articulation) && articulation != Articulation::Pizzicato;
    inLoop = false;
    direction = 1;

    framePos = std::max (0, m.onsetFrame - 2) + zoneA.startOffsetSec / m.hopSeconds;
    frozen = articulation == Articulation::Pizzicato;
    freezeFrame = std::min ((double) m.numFrames - 1.0, m.attackEndFrame + 0.03 / m.hopSeconds);
    if (frozen)
        framePos = freezeFrame;

    const float hz = (float) midiToHz (note);
    pizzTau = juce::jlimit (0.12f, 2.5f, 0.55f * std::pow (261.63f / hz, 0.35f)) * (0.8f + 0.4f * attackDyn);

    perfRng = Rng (zoneA.noiseSeed ^ 0xABCDEFu);
    tremPhase = 0.0f;
    tremRateMul = 1.0f;
    tremDepth = 0.75f;

    Rng ph (zoneA.phaseSeed);
    for (int k = 0; k < kMaxPartials; ++k)
    {
        const float a = ph.uniform() * kTwoPi;
        re[k] = std::cos (a);
        im[k] = std::sin (a);
        amp[k] = ampTarget[k] = dAmp[k] = 0.0f;
        rotC[k] = 1.0f;
        rotS[k] = 0.0f;
    }
    numPartialsPrev = numPartialsNow = 0;

    numBands = std::min (m.numBands, kMaxBands);
    for (auto& b : bands)
        b = Band {};
    noise.seed (zoneA.noiseSeed);

    const float pan = juce::jlimit (-1.0f, 1.0f, ((float) note - 60.0f) / 48.0f * s.width);
    const float ang = (pan + 1.0f) * kPi * 0.25f;
    panL = std::cos (ang) * 1.41421356f;
    panR = std::sin (ang) * 1.41421356f;
}

void ResynthVoice::release() noexcept
{
    if (! released)
    {
        released = true;
        tRelease = 0.0f;
    }
}

void ResynthVoice::kill() noexcept
{
    killing = true;
}

void ResynthVoice::legatoTo (int newNote, float vel) noexcept
{
    glideFrom = noteF;
    glideTo = (float) newNote;
    glidePos = 0.0f;
    note = newNote;
    velXTarget = clamp01 (vel);
}

void ResynthVoice::updateNoiseFilter (int b, float fc, float q) noexcept
{
    auto& bd = bands[(size_t) b];
    const float g = std::tan (kPi * std::min (fc, 0.49f * (float) sr) / (float) sr);
    bd.k = 1.0f / std::max (0.3f, q);
    bd.a1 = 1.0f / (1.0f + g * (g + bd.k));
    bd.a2 = g * bd.a1;
    bd.a3 = g * bd.a2;
    bd.fc = fc;
}

bool ResynthVoice::computeTargets (const EngineSettings& s, float modWheel, float bendSemis, float dt) noexcept
{
    const auto& m = *model;
    const auto& inst = *instrument;
    const int L = inst.settings.numLayers;

    t += dt;
    if (released)
        tRelease += dt;

    const float smoothA = firstBlock ? 1.0f : 1.0f - std::exp (-dt / 0.04f);
    velX += (velXTarget - velX) * (firstBlock ? 1.0f : 1.0f - std::exp (-dt / 0.08f));

    // ------------------------------------------------------------ articulation shaping
    const bool useWheel = forcedDynamic < 0.0f && s.dynamicsSource == 1 && isLong (articulation);
    const float xBase = forcedDynamic >= 0.0f ? forcedDynamic : (useWheel ? modWheel : velX);
    float xDyn = xBase, env = 1.0f, artDb = 0.0f, extraTilt = 0.0f, noiseMulDb = 0.0f, artCents = 0.0f;
    bool finished = false;

    switch (articulation)
    {
        case Articulation::Sustain:
        case Articulation::Legato:
            break;

        case Articulation::Staccato:
        {
            artDb = 1.5f * std::exp (-t / 0.05f);
            constexpr float hold = 0.09f;
            if (t > hold) env = std::exp (-(t - hold) / 0.04f);
            break;
        }
        case Articulation::Spiccato:
        {
            artDb = 2.0f * std::exp (-t / 0.03f);
            extraTilt = 1.5f;
            constexpr float hold = 0.035f;
            if (t > hold) env = std::exp (-(t - hold) / 0.028f);
            break;
        }
        case Articulation::Marcato:
        {
            const float e = std::exp (-t / 0.15f);
            artDb = 7.0f * e - 4.0f * (1.0f - e);
            extraTilt = 2.5f * std::exp (-t / 0.15f);
            break;
        }
        case Articulation::Pizzicato:
        {
            artDb = 3.0f;
            extraTilt = (xBase - 0.5f) * 3.0f;
            noiseMulDb = 20.0f * std::log10 (0.15f + 3.0f * std::exp (-t / 0.012f));
            artCents = 8.0f * std::exp (-t / 0.03f);
            if (t > 9.5f * pizzTau)
                finished = true;
            break;
        }
        case Articulation::Tremolo:
        {
            tremPhase += kTwoPi * s.tremoloRate * tremRateMul * dt;
            if (tremPhase >= kTwoPi)
            {
                tremPhase -= kTwoPi;
                tremRateMul = 1.0f + 0.08f * perfRng.bipolar();
                tremDepth = 0.6f + 0.25f * perfRng.uniform();
            }
            const float shape = 0.5f + 0.5f * std::cos (tremPhase);
            env = 1.0f - tremDepth * (1.0f - shape);
            extraTilt = 1.5f * (shape - 0.5f);
            noiseMulDb = 20.0f * std::log10 (0.5f + shape);
            break;
        }
        case Articulation::Swell:
        {
            xDyn = xBase * smoothstep (t / 1.8f);
            const float f = std::min (1.0f, t / 0.3f);
            env = f * f;
            break;
        }
        case Articulation::Fortepiano:
        {
            const float hi = std::max (xBase, 0.9f), lo = std::max (0.05f, xBase * 0.35f);
            xDyn = t < 0.08f ? hi : lo + (hi - lo) * std::exp (-(t - 0.08f) / 0.12f);
            break;
        }
        case Articulation::Count:
            break;
    }
    xDyn = clamp01 (xDyn);

    if (s.attackMs > 0.5f)
        env *= std::sin (std::min (1.0f, t / (s.attackMs * 0.001f)) * kPi * 0.5f);

    if (glidePos < 1.0f)
    {
        glidePos = std::min (1.0f, glidePos + dt / std::max (0.005f, s.glideMs * 0.001f));
        noteF = lerp (glideFrom, glideTo, smoothstep (glidePos));
        env *= 1.0f - 0.18f * std::sin (kPi * glidePos);
    }
    else
    {
        noteF = glideTo;
    }

    if (released && ! isOneShot (articulation))
    {
        const float relSec = articulation == Articulation::Pizzicato ? 0.08f : std::max (0.01f, s.releaseMs * 0.001f);
        releaseEnv *= std::exp (-dt * 6.9f / relSec);
    }
    if (killing)
        killEnv *= std::exp (-dt / 0.0015f);

    const float envTotal = env * releaseEnv * killEnv;
    if ((released || killing || isOneShot (articulation)) && t > 0.02f && envTotal < 1.0e-4f)
        finished = true;

    // ------------------------------------------------------------ dynamic layers
    const bool blend = s.velocityBlend && forcedDynamic < 0.0f && L > 1;
    int lA = 0, lB = 0;
    float w = 0.0f;
    if (blend)
    {
        const float u = juce::jlimit (0.0f, (float) (L - 1), xDyn * (float) L - 0.5f);
        lA = (int) u;
        lB = std::min (lA + 1, L - 1);
        w = u - (float) lA;
    }
    else
    {
        lA = lB = inst.layerForDynamic (xDyn);
    }
    displayLayer = w < 0.5f ? lA : lB;
    const float xTimbre = L <= 1 ? s.sourceDynamic : (blend ? xDyn : inst.layerCentre (lA));

    const auto& za = inst.zone (note, lA, roundRobin);
    const auto& zb = inst.zone (note, lB, roundRobin);
    zDetune += (lerp (za.detuneCents, zb.detuneCents, w) - zDetune) * smoothA;
    zGain   += (lerp (za.gainDb, zb.gainDb, w) - zGain) * smoothA;
    zTilt   += (lerp (za.tiltDb, zb.tiltDb, w) - zTilt) * smoothA;
    zNoise  += (lerp (za.noiseDb, zb.noiseDb, w) - zNoise) * smoothA;
    for (int k = 0; k < kJitterLen; ++k)
        zJitter[(size_t) k] += (lerp (za.jitterDb[(size_t) k], zb.jitterDb[(size_t) k], w) - zJitter[(size_t) k]) * smoothA;

    // ------------------------------------------------------------ position in the model
    if (frozen)
    {
        framePos = freezeFrame;
    }
    else
    {
        const double rate = (framePos < m.attackEndFrame ? attackRate : 1.0) * zoneA.speed;
        framePos += (double) dt / m.hopSeconds * rate * direction;

        if (loopEnabled)
        {
            if (! inLoop && framePos >= m.loopStartFrame)
                inLoop = true;

            if (inLoop)
            {
                const double ls = m.loopStartFrame, le = m.loopEndFrame;
                if (le - ls < 1.0)
                    framePos = ls;
                else
                    for (int guard = 0; guard < 4; ++guard)
                    {
                        if (direction > 0 && framePos > le)      { framePos = 2.0 * le - framePos; direction = -1; }
                        else if (direction < 0 && framePos < ls) { framePos = 2.0 * ls - framePos; direction = 1; }
                        else break;
                    }
                framePos = juce::jlimit (ls, le, framePos);
            }
        }
        else if (framePos >= m.numFrames - 1)
        {
            framePos = m.numFrames - 1;
            killing = true;
        }
    }

    const int nf = m.numFrames;
    const int i0 = juce::jlimit (0, nf - 1, (int) framePos);
    const int i1 = std::min (i0 + 1, nf - 1);
    const float fa = clamp01 ((float) (framePos - i0));
    const int P = std::min (m.numPartials, kMaxPartials);
    const int B = numBands;

    const float f0r = lerp (m.f0Ratio[(size_t) i0], m.f0Ratio[(size_t) i1], fa);
    const float slopeA = lerp (m.slopeA[(size_t) i0], m.slopeA[(size_t) i1], fa);
    const float slopeB = lerp (m.slopeB[(size_t) i0], m.slopeB[(size_t) i1], fa);
    const float log2SrcF0 = (float) std::log2 (m.f0Ref * f0r);
    {
        const size_t o0 = (size_t) i0 * (size_t) m.numPartials, o1 = (size_t) i1 * (size_t) m.numPartials;
        for (int h = 0; h < P; ++h)
        {
            srcRatio[h] = lerp (m.ratio[o0 + (size_t) h], m.ratio[o1 + (size_t) h], fa);
            srcLogF[h]  = lerp (m.logRatio[o0 + (size_t) h], m.logRatio[o1 + (size_t) h], fa) + log2SrcF0;
            srcEnv[h]   = lerp (m.envDb[o0 + (size_t) h], m.envDb[o1 + (size_t) h], fa);
            srcFine[h]  = lerp (m.fineDb[o0 + (size_t) h], m.fineDb[o1 + (size_t) h], fa);
        }
    }

    // ------------------------------------------------------------ pitch
    const float scoop = zoneA.scoopCents * std::exp (-t / zoneA.scoopSeconds);
    const float bite = xBase > 0.6f ? (xBase - 0.6f) * 15.0f * std::exp (-t / 0.025f) : 0.0f;
    const float cents = zDetune + scoop + bite + artCents;
    const float log2Drift = s.pitchDrift * std::log2 (std::max (1.0e-3f, f0r));
    const float log2Out = std::log2 (440.0f) + (noteF + bendSemis - 69.0f) / 12.0f + cents / 1200.0f + log2Drift;
    const float outF0 = fast::exp2 (log2Out);
    const float log2Warp = (1.0f - s.formant) * (log2Out - log2SrcF0);

    // ------------------------------------------------------------ gains / brightness
    const float var = inst.settings.variation;
    const float lifeAmt = clamp01 ((t - 0.05f) / 0.4f);
    const float lifeDb = lifeAmt * 0.5f * var * std::sin (kTwoPi * zoneA.lifeRate1 * t + zoneA.lifePhase);
    const float lifeTilt = lifeAmt * 0.3f * var * std::sin (kTwoPi * zoneA.lifeRate2 * t + 1.7f * zoneA.lifePhase);
    float loopDb = 0.0f;
    if (loopEnabled)
        loopDb = 20.0f * std::log10 (std::max (1.0e-3f, lerp (m.loopGain[(size_t) i0], m.loopGain[(size_t) i1], fa)));

    const float gainDb = s.dynRangeDb * (xDyn - s.sourceDynamic) + zGain + artDb + lifeDb + loopDb;
    const float tilt = s.brightnessSpan * (xTimbre - s.sourceDynamic) + zTilt + extraTilt + lifeTilt;
    const float noiseDb = s.noiseDb + zNoise + 2.5f * (s.sourceDynamic - xTimbre) + noiseMulDb;

    // ------------------------------------------------------------ partial targets
    const float limitHz = std::min (0.46f * (float) sr, 20000.0f);
    const float invFade = 1.0f / (0.08f * limitHz);
    const int kMax = juce::jlimit (1, kMaxPartials, s.maxPartials);
    const float lastR = srcRatio[P - 1];
    const float step = P >= 2 ? std::max (1.0f, srcRatio[P - 1] - srcRatio[P - 2]) : 1.0f;
    const float pizzRate = frozen ? 8.686f * t / pizzTau : 0.0f;
    const float wScale = kTwoPi / (float) sr;
    const float queryOffset = log2Out - log2SrcF0 - log2Warp;   // added to log2(source partial freq)

    int K = 0, j = 0;
    float energyFlat = 0.0f, energyTilted = 0.0f;
    for (int k = 0; k < kMax; ++k)
    {
        float r, lr, fine;
        if (k < P)
        {
            r = srcRatio[k];
            lr = srcLogF[k] - log2SrcF0;
            fine = srcFine[k];
        }
        else
        {
            r = lastR + (float) (k - P + 1) * step;
            lr = std::log2 (r);
            fine = 0.0f;
        }

        const float fk = r * outF0;
        if (fk >= limitHz)
            break;

        // body resonance lookup: where this output partial falls on the source's body response
        const float q = lr + log2SrcF0 + queryOffset;
        while (j + 1 < P && srcLogF[j + 1] < q)
            ++j;

        float e;
        if (q <= srcLogF[0])
            e = srcEnv[0];
        else if (j >= P - 1)
            e = srcEnv[P - 1] - 6.0f * (q - srcLogF[P - 1]);
        else
        {
            const float span = srcLogF[j + 1] - srcLogF[j];
            const float a = span > 1.0e-6f ? (q - srcLogF[j]) / span : 0.0f;
            e = srcEnv[j] + (srcEnv[j + 1] - srcEnv[j]) * clamp01 (a);
        }

        float db = slopeA + slopeB * lr + e + fine + zJitter[(size_t) std::min (k, kJitterLen - 1)] + gainDb;
        if (frozen)
            db -= pizzRate * std::sqrt (r);

        const float base = fast::dbToGain (db) * envTotal * std::min (1.0f, (limitHz - fk) * invFade);
        const float tilted = base * fast::dbToGain (tilt * std::min (lr, 4.5f));
        energyFlat += base * base;
        energyTilted += tilted * tilted;
        ampTarget[k] = tilted;
        fast::sinCosHalf (fk * wScale, rotS[k], rotC[k]);
        K = k + 1;
    }
    numPartialsNow = K;

    // brightness changes timbre only; loudness comes from the dynamic range setting
    const float tiltNorm = energyTilted > 1.0e-20f
                         ? juce::jlimit (0.05f, 20.0f, std::sqrt (energyFlat / energyTilted)) : 1.0f;
    for (int k = 0; k < K; ++k)
        ampTarget[k] *= tiltNorm;

    // ------------------------------------------------------------ noise targets
    {
        float noiseLin = fast::dbToGain (noiseDb + gainDb) * envTotal * tiltNorm;
        if (frozen)
            noiseLin *= fast::dbToGain (-8.686f * t / (0.5f * pizzTau));

        const float warp = fast::exp2 (log2Warp);
        const size_t o0 = (size_t) i0 * (size_t) m.numBands, o1 = (size_t) i1 * (size_t) m.numBands;
        for (int b = 0; b < B; ++b)
        {
            auto& bd = bands[(size_t) b];
            const float fc = m.bandCentre[(size_t) b] * warp;
            const float bw = (m.bandHi[(size_t) b] - m.bandLo[(size_t) b]) * warp;
            if (fc + 0.5f * bw >= limitHz || fc < 20.0f)
            {
                bd.target = 0.0f;
                continue;
            }
            if (bd.fc <= 0.0f || std::abs (fc - bd.fc) > 0.003f * bd.fc)
                updateNoiseFilter (b, fc, fc / bw);

            const float src = lerp (m.noiseAmp[o0 + (size_t) b], m.noiseAmp[o1 + (size_t) b], fa);
            const float enbw = (0.5f * kPi * bw) / (0.5f * (float) sr);
            // the noise follows the dynamic's brightness too (soft bowing / breath is darker)
            const float octAboveF0 = juce::jlimit (0.0f, 4.5f, std::log2 (fc / outF0));
            bd.target = src * noiseLin * fast::dbToGain (tilt * octAboveF0) / std::sqrt (enbw);
        }
    }

    firstBlock = false;
    return finished;
}

void ResynthVoice::synthesise (float* out, int n) noexcept
{
    const int kLoop = std::min (kMaxPartials, (std::max (numPartialsNow, numPartialsPrev) + 7) & ~7);
    for (int k = numPartialsNow; k < kLoop; ++k)
        ampTarget[k] = 0.0f;

    const float invN = 1.0f / (float) n;
    for (int k = 0; k < kLoop; ++k)
        dAmp[k] = (ampTarget[k] - amp[k]) * invN;

    for (int i = 0; i < n; ++i)
    {
        float lane[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        for (int kb = 0; kb < kLoop; kb += 8)
        {
            for (int l = 0; l < 8; ++l)
            {
                const int idx = kb + l;
                const float r = re[idx] * rotC[idx] - im[idx] * rotS[idx];
                const float q = re[idx] * rotS[idx] + im[idx] * rotC[idx];
                re[idx] = r;
                im[idx] = q;
                lane[l] += amp[idx] * q;
                amp[idx] += dAmp[idx];
            }
        }
        out[i] += ((lane[0] + lane[1]) + (lane[2] + lane[3])) + ((lane[4] + lane[5]) + (lane[6] + lane[7]));
    }

    for (int k = 0; k < kLoop; ++k)
    {
        amp[k] = ampTarget[k];
        const float m2 = re[k] * re[k] + im[k] * im[k];
        const float g = 1.5f - 0.5f * m2;   // keeps the rotation on the unit circle
        re[k] *= g;
        im[k] *= g;
    }
    numPartialsPrev = numPartialsNow;

    // noise residual
    float white[kSubBlock];
    for (int i = 0; i < n; ++i)
        white[i] = noise.next() * 1.7320508f;

    for (int b = 0; b < numBands; ++b)
    {
        auto& bd = bands[(size_t) b];
        if (bd.gain <= 1.0e-9f && bd.target <= 1.0e-9f)
        {
            bd.gain = bd.target;
            continue;
        }
        float g = bd.gain;
        const float dg = (bd.target - g) * invN;
        float ic1 = bd.ic1, ic2 = bd.ic2;
        const float a1 = bd.a1, a2 = bd.a2, a3 = bd.a3, gk = bd.k;
        for (int i = 0; i < n; ++i)
        {
            const float v3 = white[i] - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            out[i] += g * gk * v1;
            g += dg;
        }
        bd.ic1 = ic1;
        bd.ic2 = ic2;
        bd.gain = bd.target;
    }
}

void ResynthVoice::render (float* left, float* right, int numSamples, const EngineSettings& s,
                           float modWheel, float bendSemis) noexcept
{
    int pos = 0;
    while (pos < numSamples && active)
    {
        const int n = std::min (kSubBlock, numSamples - pos);
        const bool finished = computeTargets (s, modWheel, bendSemis, (float) n / (float) sr);

        std::fill (scratch, scratch + n, 0.0f);
        synthesise (scratch, n);
        for (int i = 0; i < n; ++i)
        {
            left[pos + i] += scratch[i] * panL;
            right[pos + i] += scratch[i] * panR;
        }
        pos += n;

        if (finished)
            active = false;
    }
}

} // namespace ivi
