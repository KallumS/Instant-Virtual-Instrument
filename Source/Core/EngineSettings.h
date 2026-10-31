#pragma once

namespace ivi
{

/** Real-time playback settings, copied into the engine at the start of every block. */
struct EngineSettings
{
    float formant = 0.75f;          // 0 = timbre follows pitch, 1 = body resonance fixed
    float brightnessSpan = 8.0f;    // dB/octave tilt difference between ppp and fff
    float sourceDynamic = 0.6f;     // where the source recording sits on the ppp..fff axis
    float dynRangeDb = 24.0f;       // loudness difference between ppp and fff
    float noiseDb = 0.0f;           // stochastic (bow/breath) level offset
    float attackMs = 0.0f;          // extra fade-in
    float releaseMs = 350.0f;
    float glideMs = 90.0f;          // legato transition time
    float pitchDrift = 0.7f;        // how much of the source's own pitch movement is kept
    float tremoloRate = 9.0f;       // Hz
    float width = 0.3f;             // keyboard-position stereo spread
    float masterGainDb = 0.0f;
    float bendRange = 2.0f;         // semitones

    bool velocityBlend = true;      // interpolate between neighbouring layers
    int dynamicsSource = 0;         // 0 = velocity, 1 = mod wheel (long articulations)
    int rrMode = 0;                 // 0 = cycle, 1 = random (no immediate repeat)
    int sustainMode = 0;            // 0 = auto, 1 = always loop, 2 = natural decay
    int polyphony = 32;
    int maxPartials = 128;
    int keyswitchBase = 24;
    bool keyswitchesEnabled = true;
};

} // namespace ivi
