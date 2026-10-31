#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "Core/SpectralModel.h"

class IVIProcessor;

namespace ivi::ui
{

/** A line of two texts, a name at the left and a detail at the right, that
    becomes two lines when the two don't fit side by side. */
struct TextRow
{
    juce::String left, right;
    juce::Rectangle<float> leftBox, rightBox;
    int rows = 1;

    /** Lays the row out across `area` (top edge, full width); returns its height. */
    float place (juce::Rectangle<float> area);
    void draw (juce::Graphics&, juce::Colour leftColour, juce::Colour rightColour) const;
    static constexpr float lineHeight = 24.0f;
};

/** Every piece of text a view draws, where it draws it (for the tests). */
struct DrawsText
{
    virtual ~DrawsText() = default;
    virtual juce::Array<juce::Rectangle<float>> textBoxes() const = 0;
};

/** The sample: its waveform, the attack and the sustain loop the analysis
    found, and a playhead where the newest note is. */
class WaveformView : public juce::Component, public DrawsText
{
public:
    explicit WaveformView (IVIProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Array<juce::Rectangle<float>> textBoxes() const override;
    void refresh();
    /** Repaints if the playhead moved; called by the editor's timer. */
    void tick();

private:
    IVIProcessor& processor;
    std::shared_ptr<const SpectralModel> shown;
    std::vector<std::pair<float, float>> peaks;
    float lastPlayhead = -2.0f;
    TextRow header;
};

/** The spectral model: the partials, the body curve and the noise. */
class SpectrumView : public juce::Component, public DrawsText
{
public:
    explicit SpectrumView (IVIProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Array<juce::Rectangle<float>> textBoxes() const override;

private:
    IVIProcessor& processor;
    TextRow header;
};

/** The generated library: every key by dynamic layer by round robin, and the
    keyswitches. A zone takes the yellow while it sounds. Click a cell to hear
    it, or Tab to it and use the arrow keys and Space. */
class ZoneMapView : public juce::Component, public juce::SettableTooltipClient, public DrawsText, private juce::Timer
{
public:
    explicit ZoneMapView (IVIProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Array<juce::Rectangle<float>> textBoxes() const override;
    /** The height the map needs at this width: its text wraps onto more lines when narrow. */
    int heightFor (int width) const;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

    static constexpr int labelWidth = 44;

private:
    struct Hit { int key = -1; int layer = -1; };
    Hit hitAt (juce::Point<float>) const;
    juce::Rectangle<float> gridArea() const;
    juce::String describe (Hit) const;
    void press (Hit);
    void releaseAudition();
    void timerCallback() override;

    void updateText();

    IVIProcessor& processor;
    int auditioning = -1;
    Hit cursor { 60, 2 };
    TextRow header, legend;
};

/** The keyboard, in the house colours: keys sound in the yellow, keyswitches
    carry a notch, keys outside the mapped range are dimmed. */
class Keyboard : public juce::MidiKeyboardComponent
{
public:
    Keyboard (IVIProcessor& p, juce::MidiKeyboardState& s);
    void paint (juce::Graphics&) override;
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    bool isKeyswitch (int note) const;
    bool isCurrentKeyswitch (int note) const;
    bool isMapped (int note) const;
    IVIProcessor& processor;
};

/** The colour of a dynamic layer in the zone map: lighter for louder. */
juce::Colour layerColour (int layer, int numLayers);

} // namespace ivi::ui
