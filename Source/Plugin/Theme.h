/*
    Theme - the house colour scheme (Good Idea's docs/COLOUR.md, shared by
    every app in the family) and the look-and-feel that applies it, after
    Noterator's Theme.

    Four colours carry it: a dark cool-grey ground, a light grey for the
    controls raised off it, one yellow for whatever is switched on, and the
    dark ink that letters every button, chosen or not. Every grey is
    blue-shifted, R < G < B - a neutral grey reads flat beside the yellow.

    Apple's guidelines (docs/decisions/0003): every line of text is at least
    7:1 on what it sits on; the house red marks things but is too dark to be
    read, so a warning is written in warnText. Text is the system sans-serif
    at 13 points, controls are 28 points tall and 12 apart, a chosen button
    carries an ink bar as well as the yellow, and a control reached from the
    keyboard wears a near-white ring (never the yellow, which means "on").
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ivi::theme
{

// ---- the four that carry it
inline const juce::Colour accent       { 0xffFFF200 };   // what is on: a chosen button, a ticked box, a sounding zone
inline const juce::Colour control      { 0xffA9AFBA };   // buttons, knobs, menus' faces
inline const juce::Colour ground       { 0xff23272E };   // the window
inline const juce::Colour ink          { 0xff14171C };   // the text on every button

// ---- the ramp
inline const juce::Colour roll         { 0xff111419 };   // the ground of the waveform, spectrum and zone map
inline const juce::Colour sunken       { 0xff1A1D23 };   // value fields, the knobs' tracks, scrollbar track
inline const juce::Colour rollLine     { 0xff1E2228 };   // grid lines in the views
inline const juce::Colour frameHover   { 0xff22262D };
inline const juce::Colour popup        { 0xff2A2F37 };   // menus and tooltips, a step lighter than the window
inline const juce::Colour rule         { 0xff3A404A };   // separators, panel edges, a menu's edge and highlight
inline const juce::Colour thumb        { 0xff6D7581 };   // scrollbar thumb, resize corner (3.6:1 on the track)
inline const juce::Colour edge         { 0xff8A919C };   // the edge of fields, tick boxes and knob tracks (4.7:1); never words
inline const juce::Colour controlHeld  { 0xff8F96A2 };
inline const juce::Colour textDim      { 0xffB0B6C0 };   // secondary text: hints, labels' values, the message (7.4:1)
inline const juce::Colour label        { 0xffBFC5CE };   // the names beside controls and over panels (8.6:1)
inline const juce::Colour controlHover { 0xffC0C6CF };
inline const juce::Colour text         { 0xffDDE1E7 };   // body text (11.4:1)
inline const juce::Colour focus        { 0xffF2F4F7 };   // the keyboard's ring, the playhead

// ---- off the ramp
inline const juce::Colour warn         { 0xffD2483F };   // marks only: 3.4:1 is too faint for words
inline const juce::Colour warnText     { 0xffFF9E96 };   // a warning, written (7.6:1)

/** Every colour the window uses, for the tests. */
juce::Array<juce::Colour> greys();

// ---- sizes (points; the editor scales the whole window for Text Size)
constexpr float textPt = 13.0f;
constexpr int controlHeight = 28;
constexpr int gap = 12;
constexpr int tooltipLine = 64;   // characters

juce::Font font();
float textWidth (const juce::String& s);

/** A button's width: its label plus padding, never narrower (Good Idea's pick()). */
int buttonWidth (const juce::String& label);

/** Breaks text into lines of at most `max` characters (Good Idea's lines()). */
juce::String lines (const juce::String& s, int max = tooltipLine);

/** WCAG contrast ratio between two colours (1 to 21). */
double contrast (juce::Colour a, juce::Colour b);

/** Hover and held for the accent: 18% toward white or black, alpha kept. */
juce::Colour shade (juce::Colour c, float amount);

/** Draws the keyboard's ring if c has focus; returns what is left inside it. */
juce::Rectangle<float> focusRing (juce::Graphics&, juce::Component& c, juce::Rectangle<float> r, float corner = 4.0f);

/** A click leaves the keyboard where it was, as on the Mac; Tab reaches controls instead. */
void clicksKeepFocus (juce::Component&);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool over, bool dragging) override;

    juce::Rectangle<int> getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
};

} // namespace ivi::theme
