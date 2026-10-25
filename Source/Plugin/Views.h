#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "Core/SpectralModel.h"

class IVIProcessor;

namespace ivi::ui
{

/** Source waveform with the detected attack, loop and release regions. */
class WaveformView : public juce::Component
{
public:
    explicit WaveformView (IVIProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void refresh();

private:
    IVIProcessor& processor;
    std::shared_ptr<const SpectralModel> shown;
    std::vector<std::pair<float, float>> peaks;
};

/** Partial levels and noise floor of the analysed sample. */
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (IVIProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;

private:
    IVIProcessor& processor;
};

/** The generated library: every key x dynamic layer x round robin, plus keyswitches.
    Lights up as zones play; click a cell to audition it. */
class ZoneMapView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ZoneMapView (IVIProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    struct Hit { int key = -1; int layer = -1; };
    Hit hitTest (juce::Point<float>) const;
    juce::Rectangle<float> gridArea() const;

    IVIProcessor& processor;
    int auditioning = -1;
};

/** Keyboard that marks keyswitches and the mapped range. */
class Keyboard : public juce::MidiKeyboardComponent
{
public:
    Keyboard (IVIProcessor& p, juce::MidiKeyboardState& s);
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;

private:
    bool isKeyswitch (int note) const;
    bool isMapped (int note) const;
    IVIProcessor& processor;
};

} // namespace ivi::ui
