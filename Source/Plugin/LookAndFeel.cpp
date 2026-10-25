#include "LookAndFeel.h"

namespace ivi::ui
{

juce::Colour colours::forDynamic (float x)
{
    x = juce::jlimit (0.0f, 1.0f, x);
    const juce::Colour soft { 0xff4a7bd8 }, mid { 0xff3cc8b8 }, loud { 0xffef5a4a };
    return x < 0.5f ? soft.interpolatedWith (mid, x * 2.0f) : mid.interpolatedWith (loud, (x - 0.5f) * 2.0f);
}

LookAndFeel::LookAndFeel()
{
    setColourScheme ({ colours::background, colours::panel, colours::panel, colours::panelEdge, colours::text,
                       colours::accent, juce::Colours::black, colours::accent, colours::text });

    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, colours::panelEdge);
    setColour (juce::Label::textColourId, colours::dim);
    setColour (juce::TextButton::buttonColourId, colours::panel.brighter (0.08f));
    setColour (juce::TextButton::buttonOnColourId, colours::accent.withAlpha (0.85f));
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ComboBox::backgroundColourId, colours::panel.brighter (0.08f));
    setColour (juce::ComboBox::outlineColourId, colours::panelEdge);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::dim);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.3f));
    setColour (juce::ToggleButton::tickColourId, colours::accent);
    setColour (juce::ToggleButton::textColourId, colours::text);
    setColour (juce::GroupComponent::outlineColourId, colours::panelEdge);
    setColour (juce::GroupComponent::textColourId, colours::dim);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panel.brighter (0.15f));
    setColour (juce::TooltipWindow::textColourId, colours::text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float track = juce::jmax (2.5f, radius * 0.16f);

    juce::Path bg;
    bg.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::panelEdge);
    g.strokePath (bg, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, angle, true);
    g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId).withAlpha (s.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (value, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = radius - track * 2.2f;
    g.setColour (colours::panel.brighter (0.12f));
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -knobR, 3.0f, knobR * 0.55f, 1.5f);
    g.setColour (colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& background,
                                        bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : background;
    if (down) c = c.brighter (0.2f);
    else if (highlighted) c = c.brighter (0.08f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::panelEdge.brighter (0.1f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void LookAndFeel::drawGroupComponentOutline (juce::Graphics& g, int w, int h, const juce::String& text,
                                             const juce::Justification&, juce::GroupComponent&)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText (text.toUpperCase(), juce::Rectangle<int> (10, 4, w - 20, 14), juce::Justification::centredLeft);
}

juce::Font LookAndFeel::getLabelFont (juce::Label& l)
{
    return juce::Font (juce::FontOptions (juce::jmin (13.0f, (float) l.getHeight() * 0.85f)));
}

} // namespace ivi::ui
