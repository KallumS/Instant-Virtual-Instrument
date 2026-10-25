#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ivi::params
{
    // performance
    inline constexpr const char* articulation  = "articulation";
    // mapping (rebuilds the zone map)
    inline constexpr const char* layers        = "layers";
    inline constexpr const char* roundRobins   = "roundRobins";
    inline constexpr const char* lowKey        = "lowKey";
    inline constexpr const char* highKey       = "highKey";
    inline constexpr const char* variation     = "variation";
    inline constexpr const char* seed          = "seed";
    inline constexpr const char* rootNote      = "rootNote";     // re-analyses the sample
    // timbre
    inline constexpr const char* formant       = "formant";
    inline constexpr const char* brightness    = "brightness";
    inline constexpr const char* sourceDynamic = "sourceDynamic";
    inline constexpr const char* noise         = "noise";
    inline constexpr const char* partials      = "partials";
    // dynamics
    inline constexpr const char* dynRange      = "dynRange";
    inline constexpr const char* velocityBlend = "velocityBlend";
    inline constexpr const char* dynSource     = "dynSource";
    inline constexpr const char* rrMode        = "rrMode";
    // envelope
    inline constexpr const char* attack        = "attack";
    inline constexpr const char* release       = "release";
    inline constexpr const char* glide         = "glide";
    inline constexpr const char* tremoloRate   = "tremoloRate";
    inline constexpr const char* pitchDrift    = "pitchDrift";
    inline constexpr const char* sustainMode   = "sustainMode";
    // output / control
    inline constexpr const char* gain          = "gain";
    inline constexpr const char* width         = "width";
    inline constexpr const char* polyphony     = "polyphony";
    inline constexpr const char* bendRange     = "bendRange";
    inline constexpr const char* ksBase        = "ksBase";
    inline constexpr const char* keyswitches   = "keyswitches";

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::String noteName (int note);
    juce::String dynamicMarking (float x);
}
