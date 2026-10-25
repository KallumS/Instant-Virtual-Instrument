#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ivi::ui
{
    namespace colours
    {
        const juce::Colour background { 0xff111417 };
        const juce::Colour panel      { 0xff1a1e23 };
        const juce::Colour panelEdge  { 0xff2a3038 };
        const juce::Colour text       { 0xffdfe5ec };
        const juce::Colour dim        { 0xff8a95a3 };
        const juce::Colour accent     { 0xff3cc8b8 };   // teal
        const juce::Colour accent2    { 0xfff2a54a };   // amber (keyswitches, markers)
        const juce::Colour loop       { 0xff5d8cff };

        /** Colour for a dynamic position: cool (ppp) to hot (fff). */
        juce::Colour forDynamic (float x);
    }

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                                   bool highlighted, bool down) override;
        void drawGroupComponentOutline (juce::Graphics&, int w, int h, const juce::String& text,
                                        const juce::Justification&, juce::GroupComponent&) override;
        juce::Font getLabelFont (juce::Label&) override;
    };
}
