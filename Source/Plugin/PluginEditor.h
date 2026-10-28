#pragma once

#include "PluginProcessor.h"
#include "Theme.h"
#include "Views.h"

/** Everything in the window, laid out for a given width in points. The
    editor scales it for Text Size and scrolls it when the window is smaller. */
class IVIContent : public juce::Component
{
public:
    explicit IVIContent (IVIProcessor&);

    /** Positions everything for this width; returns the height it needs. */
    int layout (int width);
    void resized() override { layout (getWidth()); }
    void paint (juce::Graphics&) override;

    void refreshFromProcessor();
    void tick();

    // ---- a titled panel holding a row of labelled controls
    struct Group : public juce::Component
    {
        explicit Group (juce::String t) : title (std::move (t)) {}
        void paint (juce::Graphics&) override;
        void resized() override;
        void add (juce::Component& c, const juce::String& name, int width);
        int naturalWidth() const;
        /** The height this panel needs at a width: its controls wrap onto more rows when narrow. */
        int heightFor (int width) const;

        static constexpr int labelHeight = 34;   // two lines of 13-point text
        static constexpr int knobHeight = 70;    // knob and its value field
        static constexpr int rowHeight = labelHeight + 4 + knobHeight;

        juce::String title;
        struct Cell { juce::Component* comp; std::unique_ptr<juce::Label> label; int width; };
        std::vector<Cell> cells;
    };

    // reachable by the editor and the tests
    juce::TextButton loadButton, demoButton, exportButton, undoButton, dismissButton, newTakesButton;
    juce::Label textSizeLabel;
    juce::ComboBox textSize;
    juce::OwnedArray<juce::TextButton> articulationButtons;
    ivi::ui::WaveformView waveform;
    ivi::ui::SpectrumView spectrum;
    ivi::ui::ZoneMapView zoneMap;
    ivi::ui::Keyboard keyboard;
    Group mapping { "Mapping" }, timbre { "Timbre" }, dynamics { "Dynamics" }, shape { "Shape" }, output { "Output" };
    std::function<void()> onExport, onLoad, onTextSize;

    static constexpr int minimumWidth = 560;
    static constexpr int preferredWidth = 1320;

private:
    juce::Slider& knob (Group&, const char* id, const juce::String& name, const juce::String& tip);
    juce::ComboBox& combo (Group&, const char* id, const juce::String& name, const juce::String& tip, const juce::StringArray& items = {});
    juce::ToggleButton& toggle (Group&, const char* id, const juce::String& name, const juce::String& tip);
    void updateArticulationButtons();

    IVIProcessor& processor;
    IVIProcessor::Message shownMessage;
    juce::Rectangle<int> messageArea;
    int shownArticulation = -1, shownKeyswitchBase = -2;
    bool wasExporting = false;

    juce::OwnedArray<juce::Component> owned;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
};

class IVIEditor : public juce::AudioProcessorEditor,
                  public juce::FileDragAndDropTarget,
                  private juce::ChangeListener,
                  private juce::Timer
{
public:
    explicit IVIEditor (IVIProcessor&);
    ~IVIEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    /** Text Size: 100, 125, 150 or 200 (%), remembered between sessions. */
    void setTextSize (int percent);
    int getTextSize() const noexcept { return textSizePercent; }
    static const juce::Array<int>& textSizes();

    IVIContent& getContent() noexcept { return content; }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void layoutContent();
    void chooseSample();
    void showExportMenu();
    juce::Rectangle<int> preferredSize() const;

    IVIProcessor& processor;
    ivi::theme::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };
    juce::Viewport viewport;
    juce::Component holder;
    IVIContent content;
    std::unique_ptr<juce::PropertiesFile> prefs;
    std::unique_ptr<juce::FileChooser> chooser;
    int textSizePercent = 100;
    float lastMessageHeight = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IVIEditor)
};
