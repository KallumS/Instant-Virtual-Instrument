#include "Parameters.h"
#include "Core/Articulations.h"

namespace ivi::params
{

juce::String noteName (int note)
{
    return juce::MidiMessage::getMidiNoteName (note, true, true, 4);
}

juce::String dynamicMarking (float x)
{
    static const char* names[] = { "ppp", "pp", "p", "mp", "mf", "f", "ff", "fff" };
    return names[juce::jlimit (0, 7, (int) (x * 8.0f))];
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pid = [] (const char* id) { return ParameterID { id, 1 }; };
    auto noteAttr = AudioParameterIntAttributes()
                        .withStringFromValueFunction ([] (int v, int) { return noteName (v); })
                        .withValueFromStringFunction ([] (const String& s)
                        {
                            for (int n = 0; n < 128; ++n)
                                if (noteName (n).equalsIgnoreCase (s.trim())) return n;
                            return s.getIntValue();
                        });
    auto percent = AudioParameterFloatAttributes()
                       .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
                       .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue() / 100.0f; });
    auto unit = [] (const char* suffix, int decimals)
    {
        return AudioParameterFloatAttributes()
            .withStringFromValueFunction ([suffix, decimals] (float v, int) { return String (v, decimals) + " " + suffix; })
            .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); });
    };
    auto notAutomatable = AudioParameterIntAttributes().withAutomatable (false);

    layout.add (std::make_unique<AudioParameterChoice> (pid (articulation), "Articulation", articulationNames(), 0));

    layout.add (std::make_unique<AudioParameterInt> (pid (layers), "Dynamic Layers", 1, 8, 5, notAutomatable));
    layout.add (std::make_unique<AudioParameterInt> (pid (roundRobins), "Round Robins", 1, 8, 4, notAutomatable));
    layout.add (std::make_unique<AudioParameterInt> (pid (lowKey), "Lowest Key", 0, 127, 36, noteAttr.withAutomatable (false)));
    layout.add (std::make_unique<AudioParameterInt> (pid (highKey), "Highest Key", 0, 127, 108, noteAttr.withAutomatable (false)));
    layout.add (std::make_unique<AudioParameterFloat> (pid (variation), "Variation", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                                       percent.withAutomatable (false)));
    layout.add (std::make_unique<AudioParameterInt> (pid (seed), "Seed", 1, 9999, 1, notAutomatable));
    layout.add (std::make_unique<AudioParameterInt> (pid (rootNote), "Root Note", -1, 127, -1,
                                                     AudioParameterIntAttributes().withAutomatable (false)
                                                         .withStringFromValueFunction ([] (int v, int) { return v < 0 ? String ("Auto") : noteName (v); })));

    layout.add (std::make_unique<AudioParameterFloat> (pid (formant), "Formant Preserve", NormalisableRange<float> (0.0f, 1.0f), 0.75f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (pid (brightness), "Dynamic Brightness", NormalisableRange<float> (0.0f, 18.0f, 0.1f), 8.0f, unit ("dB/oct", 1)));
    layout.add (std::make_unique<AudioParameterFloat> (pid (sourceDynamic), "Source Dynamic", NormalisableRange<float> (0.0f, 1.0f), 0.6f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return dynamicMarking (v); })));
    layout.add (std::make_unique<AudioParameterFloat> (pid (noise), "Noise", NormalisableRange<float> (-30.0f, 12.0f, 0.1f), 0.0f, unit ("dB", 1)));
    layout.add (std::make_unique<AudioParameterInt> (pid (partials), "Max Partials", 16, 256, 128));

    layout.add (std::make_unique<AudioParameterFloat> (pid (dynRange), "Dynamic Range", NormalisableRange<float> (0.0f, 48.0f, 0.1f), 24.0f, unit ("dB", 1)));
    layout.add (std::make_unique<AudioParameterBool> (pid (velocityBlend), "Layer Blend", true));
    layout.add (std::make_unique<AudioParameterChoice> (pid (dynSource), "Dynamics Source", StringArray { "Velocity", "Mod Wheel" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (pid (rrMode), "Round Robin Mode", StringArray { "Cycle", "Random" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (pid (attack), "Attack", NormalisableRange<float> (0.0f, 2000.0f, 0.0f, 0.35f), 0.0f, unit ("ms", 0)));
    layout.add (std::make_unique<AudioParameterFloat> (pid (release), "Release", NormalisableRange<float> (10.0f, 5000.0f, 0.0f, 0.35f), 350.0f, unit ("ms", 0)));
    layout.add (std::make_unique<AudioParameterFloat> (pid (glide), "Legato Glide", NormalisableRange<float> (10.0f, 500.0f, 0.0f, 0.5f), 90.0f, unit ("ms", 0)));
    layout.add (std::make_unique<AudioParameterFloat> (pid (tremoloRate), "Tremolo Rate", NormalisableRange<float> (3.0f, 16.0f, 0.01f), 9.0f, unit ("Hz", 1)));
    layout.add (std::make_unique<AudioParameterFloat> (pid (pitchDrift), "Pitch Drift", NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent));
    layout.add (std::make_unique<AudioParameterChoice> (pid (sustainMode), "Sustain Mode", StringArray { "Auto", "Loop", "Natural" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (pid (gain), "Output Gain", NormalisableRange<float> (-36.0f, 12.0f, 0.1f), 0.0f, unit ("dB", 1)));
    layout.add (std::make_unique<AudioParameterFloat> (pid (width), "Stereo Width", NormalisableRange<float> (0.0f, 1.0f), 0.3f, percent));
    layout.add (std::make_unique<AudioParameterInt> (pid (polyphony), "Polyphony", 1, 32, 24));
    layout.add (std::make_unique<AudioParameterInt> (pid (bendRange), "Bend Range", 0, 12, 2));
    layout.add (std::make_unique<AudioParameterInt> (pid (ksBase), "Keyswitch Base", 0, 127 - 9, 24, noteAttr.withAutomatable (false)));
    layout.add (std::make_unique<AudioParameterBool> (pid (keyswitches), "Keyswitches", true));

    return layout;
}

} // namespace ivi::params
