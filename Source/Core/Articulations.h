#pragma once

#include "JuceIncludes.h"

namespace ivi
{

/** Articulations generated automatically from the single source sample. */
enum class Articulation : int
{
    Sustain = 0,  // long note, looped sustain
    Legato,       // monophonic, notes glide into each other without re-attacking
    Staccato,     // short, detached
    Spiccato,     // very short, bouncy and bright
    Marcato,      // accented attack falling back to a held note
    Pizzicato,    // plucked: frozen spectrum, frequency-dependent decay
    Tremolo,      // rapid re-articulated amplitude/brightness modulation
    Swell,        // crescendo through the dynamic layers
    Fortepiano,   // loud attack dropping immediately to soft
    Count
};

constexpr int kNumArticulations = (int) Articulation::Count;

juce::String articulationName (Articulation a);
juce::String articulationShortName (Articulation a);
juce::StringArray articulationNames();

/** One-shot articulations play to completion and ignore note-off. */
bool isOneShot (Articulation a) noexcept;

/** Long articulations can follow the mod wheel for dynamics. */
bool isLong (Articulation a) noexcept;

} // namespace ivi
