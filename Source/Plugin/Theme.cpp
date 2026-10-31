#include "Theme.h"

namespace ivi::theme
{

juce::Array<juce::Colour> greys()
{
    return { control, ground, ink, roll, sunken, rollLine, frameHover, popup, rule, thumb, edge,
             controlHeld, textDim, label, controlHover, text, focus };
}

juce::Font font()
{
    return juce::Font (juce::FontOptions (textPt));
}

float textWidth (const juce::String& s)
{
    return juce::GlyphArrangement::getStringWidth (font(), s);
}

int buttonWidth (const juce::String& s)
{
    return juce::jmax (controlHeight, (int) std::ceil (textWidth (s)) + 28);
}

juce::String lines (const juce::String& s, int max)
{
    juce::StringArray out;
    for (auto& para : juce::StringArray::fromLines (s))
    {
        juce::String line;
        for (auto& word : juce::StringArray::fromTokens (para, " ", {}))
        {
            if (line.isNotEmpty() && line.length() + 1 + word.length() > max)
            {
                out.add (line);
                line = {};
            }
            line << (line.isEmpty() ? "" : " ") << word;
        }
        out.add (line);
    }
    return out.joinIntoString ("\n");
}

double contrast (juce::Colour a, juce::Colour b)
{
    auto lum = [] (juce::Colour c)
    {
        auto ch = [] (juce::uint8 v)
        {
            const double s = v / 255.0;
            return s <= 0.03928 ? s / 12.92 : std::pow ((s + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * ch (c.getRed()) + 0.7152 * ch (c.getGreen()) + 0.0722 * ch (c.getBlue());
    };
    const double la = lum (a), lb = lum (b);
    return (juce::jmax (la, lb) + 0.05) / (juce::jmin (la, lb) + 0.05);
}

juce::Colour shade (juce::Colour c, float amount)
{
    // keep the alpha: a colour handed back transparent is a bug this function has had before
    if (amount >= 0) return c.interpolatedWith (juce::Colours::white.withAlpha (c.getFloatAlpha()), amount);
    return c.interpolatedWith (juce::Colours::black.withAlpha (c.getFloatAlpha()), -amount);
}

juce::Rectangle<float> focusRing (juce::Graphics& g, juce::Component& c, juce::Rectangle<float> r, float corner)
{
    if (! c.hasKeyboardFocus (false))
        return r;
    g.setColour (focus);
    g.drawRoundedRectangle (r.reduced (1.0f), corner + 2.0f, 2.0f);
    return r.reduced (4.0f);
}

void clicksKeepFocus (juce::Component& c)
{
    if (dynamic_cast<juce::Button*> (&c) != nullptr || dynamic_cast<juce::ComboBox*> (&c) != nullptr
        || dynamic_cast<juce::Slider*> (&c) != nullptr)
        c.setMouseClickGrabsKeyboardFocus (false);
    for (auto* child : c.getChildren())
        clicksKeepFocus (*child);
}

// ============================================================================ LookAndFeel

LookAndFeel::LookAndFeel()
{
    setDefaultSansSerifTypefaceName (juce::Font::getDefaultSansSerifFontName());

    setColour (juce::ResizableWindow::backgroundColourId, ground);
    setColour (juce::TextButton::buttonColourId, control);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TextButton::textColourOnId, ink);
    setColour (juce::ComboBox::backgroundColourId, control);
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::ComboBox::arrowColourId, ink);
    setColour (juce::ComboBox::outlineColourId, ink);
    setColour (juce::PopupMenu::backgroundColourId, popup);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, rule);
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::Label::textColourId, label);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::backgroundColourId, sunken);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, edge);
    setColour (juce::TextEditor::focusedOutlineColourId, focus);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, text);
    setColour (juce::CaretComponent::caretColourId, text);
    setColour (juce::ScrollBar::thumbColourId, thumb);
    setColour (juce::ScrollBar::trackColourId, sunken);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, sunken);
    setColour (juce::Slider::textBoxOutlineColourId, edge);
    setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.35f));
    setColour (juce::Slider::rotarySliderFillColourId, control);
    setColour (juce::Slider::rotarySliderOutlineColourId, sunken);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, ink);
    setColour (juce::ToggleButton::tickDisabledColourId, edge);
    setColour (juce::TooltipWindow::backgroundColourId, popup);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, rule);
    setColour (juce::AlertWindow::backgroundColourId, popup);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, rule);
    setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, accent);
    setColour (juce::DirectoryContentsDisplayComponent::textColourId, text);
    setColour (juce::DirectoryContentsDisplayComponent::highlightedTextColourId, ink);
    setColour (juce::FileChooserDialogBox::titleTextColourId, text);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = focusRing (g, b, b.getLocalBounds().toFloat()).reduced (0.5f);
    const bool on = b.getToggleState();
    juce::Colour fill = on ? accent : control;
    if (! b.isEnabled())   fill = fill.withAlpha (0.4f);
    else if (down)         fill = on ? shade (accent, -0.18f) : controlHeld;
    else if (highlighted)  fill = on ? shade (accent, 0.18f) : controlHover;
    g.setColour (fill);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (ink);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return font();
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    // Ink on every button, chosen or not: the controls are lighter than the
    // ground, so the window's own light text would vanish on them.
    const auto f = font();
    g.setFont (f);
    g.setColour (b.isEnabled() ? ink : ink.withAlpha (0.55f));
    auto area = b.getLocalBounds().reduced (4, 2);
    g.drawFittedText (b.getButtonText(), area, juce::Justification::centred, 1, 1.0f);

    // a chosen button is not told by colour alone: an ink bar under its label
    if (b.getToggleState())
    {
        const float w = juce::jmin ((float) area.getWidth(), textWidth (b.getButtonText()));
        const float y = (float) area.getCentreY() + f.getHeight() * 0.5f + 2.0f;
        g.fillRect (juce::Rectangle<float> ((float) area.getCentreX() - w * 0.5f, y, w, 2.0f));
    }
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return font();
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& l)
{
    l.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    l.setFont (font());
    l.setColour (juce::Label::textColourId, ink);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox& box)
{
    auto r = focusRing (g, box, juce::Rectangle<float> (0, 0, (float) width, (float) height)).reduced (0.5f);
    g.setColour (! box.isEnabled() ? control.withAlpha (0.4f)
                                    : (down ? controlHeld : (box.isMouseOver (true) ? controlHover : control)));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (ink);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    juce::Path arrow;
    const float ax = r.getRight() - 12.0f, ay = r.getCentreY();
    arrow.addTriangle (ax - 4.5f, ay - 2.0f, ax + 4.5f, ay - 2.0f, ax, ay + 3.0f);
    g.fillPath (arrow);
}

juce::Font LookAndFeel::getPopupMenuFont()
{
    return font();
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (popup);
    g.setColour (rule);
    g.drawRect (0, 0, width, height);
}

void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                     bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& itemText,
                                     const juce::String& shortcutKeyText, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (rule);
        g.fillRect (area.reduced (6, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    if (isHighlighted && isActive)
    {
        g.setColour (rule);
        g.fillRect (area);
    }
    auto r = area.reduced (8, 0);
    if (isTicked)
    {
        // the tick is a shape, in the yellow: this item is on
        auto box = r.removeFromLeft (18).toFloat().withSizeKeepingCentre (12.0f, 12.0f);
        juce::Path tick;
        tick.startNewSubPath (box.getX() + 1.0f, box.getCentreY());
        tick.lineTo (box.getX() + 4.5f, box.getBottom() - 1.5f);
        tick.lineTo (box.getRight() - 1.0f, box.getY() + 1.5f);
        g.setColour (accent);
        g.strokePath (tick, juce::PathStrokeType (2.0f));
    }
    else
    {
        r.removeFromLeft (18);
    }
    g.setFont (font());
    g.setColour (isActive ? text : edge);
    g.drawText (itemText, r, juce::Justification::centredLeft);
    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (label);   // 7.2:1 on the menu
        g.drawText (shortcutKeyText, r, juce::Justification::centredRight);
    }
    if (hasSubMenu)
    {
        juce::Path arrow;
        const float x = (float) r.getRight() - 4.0f, y = (float) r.getCentreY();
        arrow.addTriangle (x - 4.0f, y - 4.0f, x - 4.0f, y + 4.0f, x, y);
        g.setColour (text);
        g.fillPath (arrow);
    }
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const float h = (float) b.getHeight();
    if (b.hasKeyboardFocus (false))
    {
        // the ring goes round the box and its words, as the Mac's does
        g.setColour (focus);
        g.drawRoundedRectangle (juce::Rectangle<float> (1.0f, 1.0f, juce::jmin ((float) b.getWidth() - 2.0f, textWidth (b.getButtonText()) + 34.0f), h - 2.0f), 5.0f, 2.0f);
    }
    auto box = juce::Rectangle<float> (7.0f, (h - 16.0f) * 0.5f, 16.0f, 16.0f);
    g.setColour (b.getToggleState() ? accent : (highlighted ? controlHover : control));
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (ink);
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
    if (b.getToggleState())
    {
        juce::Path tick;
        tick.startNewSubPath (box.getX() + 4.0f, box.getCentreY());
        tick.lineTo (box.getX() + 7.0f, box.getBottom() - 4.0f);
        tick.lineTo (box.getRight() - 3.5f, box.getY() + 4.0f);
        g.strokePath (tick, juce::PathStrokeType (2.0f));
    }
    g.setColour (b.isEnabled() ? text : edge);
    g.setFont (font());
    g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (31), juce::Justification::centredLeft);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    if (s.hasKeyboardFocus (false))
    {
        g.setColour (focus);
        g.drawRoundedRectangle (bounds.reduced (1.0f), 6.0f, 2.0f);
    }
    bounds = bounds.reduced (5.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const float trackW = 5.0f;
    const float trackR = radius - trackW * 0.5f - 1.0f;

    // the track: sunken, with a 4.7:1 edge so it can be seen (Apple asks 3:1 for things you grab)
    juce::Path track;
    track.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, start, end, true);
    g.setColour (edge);
    g.strokePath (track, juce::PathStrokeType (trackW + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (sunken);
    g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // the value: the controls' grey, the yellow while it is being dragged (as Good Idea's slider grab)
    const bool dragging = s.isMouseButtonDown();
    juce::Path value;
    value.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, start, angle, true);
    g.setColour (dragging ? accent : control);
    g.strokePath (value, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // the knob: raised like a button, ink pointer
    const float kr = trackR - trackW - 3.0f;
    const bool over = s.isMouseOverOrDragging() && s.isEnabled();
    g.setColour (! s.isEnabled() ? control.withAlpha (0.4f) : (over ? controlHover : control));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (ink);
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -kr + 2.0f, 3.0f, kr * 0.6f, 1.5f);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c));
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font());
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, text);
    l->setColour (juce::Label::backgroundColourId, sunken);
    l->setColour (juce::Label::outlineColourId, edge);
    return l;
}

juce::Font LookAndFeel::getLabelFont (juce::Label&)
{
    return font();
}

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                                 int thumbStart, int thumbSize, bool over, bool down)
{
    g.setColour (sunken);
    g.fillRect (x, y, width, height);
    auto t = vertical ? juce::Rectangle<int> (x + 2, thumbStart, width - 4, thumbSize)
                      : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, height - 4);
    g.setColour (down ? control : (over ? controlHover : thumb));
    g.fillRoundedRectangle (t.toFloat(), 3.0f);
}

void LookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool over, bool dragging)
{
    g.setColour (dragging || over ? control : thumb);
    for (float i = 0.3f; i < 1.0f; i += 0.3f)
        g.drawLine ((float) w * i, (float) h, (float) w, (float) h * i, 1.5f);
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const auto wrapped = lines (tip);
    const auto rows = juce::StringArray::fromLines (wrapped);
    float w = 0.0f;
    for (auto& r : rows)
        w = juce::jmax (w, textWidth (r));
    const int tw = (int) std::ceil (w) + 16;
    const int th = (int) std::ceil (font().getHeight() * 1.25f * (float) rows.size()) + 10;
    const int tx = pos.x > parent.getCentreX() ? pos.x - (tw + 12) : pos.x + 24;
    const int ty = pos.y > parent.getCentreY() ? pos.y - (th + 6) : pos.y + 6;
    return juce::Rectangle<int> (tx, ty, tw, th).constrainedWithin (parent);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& tip, int width, int height)
{
    g.fillAll (popup);
    g.setColour (rule);
    g.drawRect (0, 0, width, height);
    g.setColour (text);
    g.setFont (font());
    const auto rows = juce::StringArray::fromLines (lines (tip));
    const float lh = font().getHeight() * 1.25f;
    for (int i = 0; i < rows.size(); ++i)
        g.drawText (rows[i], juce::Rectangle<float> (8.0f, 5.0f + lh * (float) i, (float) width - 16.0f, lh),
                    juce::Justification::centredLeft);
}

} // namespace ivi::theme
