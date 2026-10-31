#include "Articulations.h"

namespace ivi
{

juce::String articulationName (Articulation a)
{
    switch (a)
    {
        case Articulation::Sustain:    return "Sustain";
        case Articulation::Legato:     return "Legato";
        case Articulation::Staccato:   return "Staccato";
        case Articulation::Spiccato:   return "Spiccato";
        case Articulation::Marcato:    return "Marcato";
        case Articulation::Pizzicato:  return "Pizzicato";
        case Articulation::Tremolo:    return "Tremolo";
        case Articulation::Swell:      return "Swell";
        case Articulation::Fortepiano: return "Fortepiano";
        case Articulation::Count:      break;
    }
    return {};
}

juce::String articulationShortName (Articulation a)
{
    switch (a)
    {
        case Articulation::Sustain:    return "Sus";
        case Articulation::Legato:     return "Leg";
        case Articulation::Staccato:   return "Stac";
        case Articulation::Spiccato:   return "Spic";
        case Articulation::Marcato:    return "Marc";
        case Articulation::Pizzicato:  return "Pizz";
        case Articulation::Tremolo:    return "Trem";
        case Articulation::Swell:      return "Swell";
        case Articulation::Fortepiano: return "fp";
        case Articulation::Count:      break;
    }
    return {};
}

juce::StringArray articulationNames()
{
    juce::StringArray s;
    for (int i = 0; i < kNumArticulations; ++i)
        s.add (articulationName ((Articulation) i));
    return s;
}

bool isOneShot (Articulation a) noexcept
{
    return a == Articulation::Staccato || a == Articulation::Spiccato;
}

bool isLong (Articulation a) noexcept
{
    return a == Articulation::Sustain || a == Articulation::Legato || a == Articulation::Marcato
        || a == Articulation::Tremolo;
}

} // namespace ivi
