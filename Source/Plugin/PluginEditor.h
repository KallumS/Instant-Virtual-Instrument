#pragma once

#include "PluginProcessor.h"
#include "LookAndFeel.h"
#include "Views.h"

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

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    /** A titled panel holding a row of labelled controls. */
    struct Group : public juce::Component
    {
        explicit Group (juce::String t) : title (std::move (t)) {}
        void paint (juce::Graphics&) override;
        void resized() override;
        void add (juce::Component& c, const juce::String& label, int width = 64);

        juce::String title;
        struct Cell { juce::Component* comp; std::unique_ptr<juce::Label> label; int width; };
        std::vector<Cell> cells;
    };

    juce::Slider& knob (Group& g, const char* paramId, const juce::String& label, const juce::String& tip);
    juce::ComboBox& combo (Group& g, const char* paramId, const juce::String& label, const juce::String& tip, int width = 92);
    juce::ToggleButton& toggle (Group& g, const char* paramId, const juce::String& label, const juce::String& tip);

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void chooseSample();
    void showExportMenu();
    void loadFile (const juce::File&);
    void updateArticulationButtons();

    IVIProcessor& processor;
    ivi::ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    juce::TextButton loadButton { "Load Sample..." }, demoButton { "Demo" }, exportButton { "Export SFZ..." };
    juce::Label statusLabel;

    ivi::ui::WaveformView waveform;
    ivi::ui::SpectrumView spectrum;
    ivi::ui::ZoneMapView zoneMap;
    juce::OwnedArray<juce::TextButton> articulationButtons;
    ivi::ui::Keyboard keyboard;

    Group mapping { "Mapping" }, timbre { "Timbre" }, dynamics { "Dynamics" }, shape { "Shape" }, output { "Output & Control" };
    juce::TextButton newSeedButton { "New takes" };

    juce::OwnedArray<juce::Component> ownedControls;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    std::unique_ptr<juce::FileChooser> chooser;
    int shownArticulation = -1;
    bool wasBusy = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IVIEditor)
};
