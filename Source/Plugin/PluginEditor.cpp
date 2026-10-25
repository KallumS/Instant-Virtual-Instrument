#include "PluginEditor.h"
#include "Core/Articulations.h"

using namespace ivi;
namespace colours = ivi::ui::colours;

namespace
{
    const juce::String audioWildcard = "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3";
    constexpr int kWidth = 1180, kHeight = 812;
}

// ---------------------------------------------------------------------------- Group

void IVIEditor::Group::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (20).reduced (10, 0), juce::Justification::centredLeft);
}

void IVIEditor::Group::add (juce::Component& c, const juce::String& label, int width)
{
    auto l = std::make_unique<juce::Label> (juce::String(), label);
    l->setJustificationType (juce::Justification::centred);
    l->setFont (juce::FontOptions (11.5f));
    l->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (*l);
    addAndMakeVisible (c);
    cells.push_back ({ &c, std::move (l), width });
}

void IVIEditor::Group::resized()
{
    auto r = getLocalBounds().reduced (6, 0).withTrimmedTop (20).withTrimmedBottom (6);
    for (auto& cell : cells)
    {
        auto col = r.removeFromLeft (cell.width);
        cell.label->setBounds (col.removeFromTop (16));
        if (dynamic_cast<juce::Slider*> (cell.comp) != nullptr)
            cell.comp->setBounds (col);
        else
            cell.comp->setBounds (col.withSizeKeepingCentre (col.getWidth() - 6, 24).translated (0, -8));
    }
}

// ---------------------------------------------------------------------------- helpers

juce::Slider& IVIEditor::knob (Group& g, const char* id, const juce::String& label, const juce::String& tip)
{
    auto* s = ownedControls.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow));
    auto* slider = static_cast<juce::Slider*> (s);
    slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 15);
    slider->setTooltip (tip);
    g.add (*slider, label);
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, *slider));
    return *slider;
}

juce::ComboBox& IVIEditor::combo (Group& g, const char* id, const juce::String& label, const juce::String& tip, int width)
{
    auto* c = static_cast<juce::ComboBox*> (ownedControls.add (new juce::ComboBox()));
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (id)))
        c->addItemList (choice->choices, 1);
    c->setTooltip (tip);
    g.add (*c, label, width);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, id, *c));
    return *c;
}

juce::ToggleButton& IVIEditor::toggle (Group& g, const char* id, const juce::String& label, const juce::String& tip)
{
    auto* b = static_cast<juce::ToggleButton*> (ownedControls.add (new juce::ToggleButton ("On")));
    b->setTooltip (tip);
    g.add (*b, label, 66);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, id, *b));
    return *b;
}

// ---------------------------------------------------------------------------- editor

IVIEditor::IVIEditor (IVIProcessor& p)
    : AudioProcessorEditor (p), processor (p), waveform (p), spectrum (p), zoneMap (p), keyboard (p, p.keyboardState)
{
    setLookAndFeel (&lnf);

    for (auto* b : { &loadButton, &demoButton, &exportButton })
        addAndMakeVisible (*b);
    loadButton.onClick = [this] { chooseSample(); };
    demoButton.onClick = [this] { processor.loadDemo(); };
    exportButton.onClick = [this]
    {
        if (processor.isExporting()) processor.cancelExport();
        else                         showExportMenu();
    };
    loadButton.setTooltip ("Load one audio sample (wav, aiff, flac, ogg, mp3). You can also drag a file onto the window.");
    demoButton.setTooltip ("Build an instrument from a built-in synthetic string tone");
    exportButton.setTooltip ("Bake the generated library to WAV files plus an SFZ map for any SFZ sampler");

    statusLabel.setJustificationType (juce::Justification::centredRight);
    statusLabel.setColour (juce::Label::textColourId, colours::dim);
    statusLabel.setFont (juce::FontOptions (12.5f));
    addAndMakeVisible (statusLabel);

    addAndMakeVisible (waveform);
    addAndMakeVisible (spectrum);
    addAndMakeVisible (zoneMap);
    addAndMakeVisible (keyboard);

    for (int a = 0; a < kNumArticulations; ++a)
    {
        auto* b = articulationButtons.add (new juce::TextButton (articulationName ((Articulation) a)));
        b->setClickingTogglesState (false);
        b->onClick = [this, a] { processor.requestArticulation (a); };
        addAndMakeVisible (b);
    }

    // ---- mapping
    {
        auto* root = static_cast<juce::ComboBox*> (ownedControls.add (new juce::ComboBox()));
        root->addItem ("Auto", 1);
        for (int n = 0; n < 128; ++n)
            root->addItem (params::noteName (n), n + 2);
        root->setTooltip ("Root note of the sample. Auto uses pitch detection; set it if detection picks the wrong octave.");
        mapping.add (*root, "Root", 86);
        comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, params::rootNote, *root));
    }
    knob (mapping, params::lowKey, "Low Key", "Lowest key the sample is mapped to");
    knob (mapping, params::highKey, "High Key", "Highest key the sample is mapped to");
    knob (mapping, params::layers, "Layers", "Number of dynamic layers (pp ... ff) generated for every key");
    knob (mapping, params::roundRobins, "Round Robins", "Alternate takes generated for every key and layer");
    knob (mapping, params::variation, "Variation", "How different the takes are: tuning, timbre, attack, timing and noise");
    knob (mapping, params::seed, "Seed", "Seed for the generated variations");
    newSeedButton.setTooltip ("Generate a fresh set of round robins and layer variations");
    newSeedButton.onClick = [this]
    {
        if (auto* s = processor.apvts.getParameter (params::seed))
            s->setValueNotifyingHost (s->convertTo0to1 ((float) juce::Random::getSystemRandom().nextInt ({ 1, 9999 })));
    };
    mapping.add (newSeedButton, "", 84);
    addAndMakeVisible (mapping);

    // ---- timbre
    knob (timbre, params::formant, "Formant", "0% = timbre moves with pitch (chipmunk). 100% = body resonances stay fixed (natural).");
    knob (timbre, params::brightness, "Dyn Bright", "How much brighter loud layers are than soft ones");
    knob (timbre, params::sourceDynamic, "Source Dyn", "The dynamic your sample was played at. Layers are generated around it.");
    knob (timbre, params::noise, "Noise", "Level of the bow / breath / hammer noise component");
    knob (timbre, params::partials, "Partials", "Maximum number of partials per voice (quality vs CPU)");
    addAndMakeVisible (timbre);

    // ---- dynamics
    knob (dynamics, params::dynRange, "Range", "Loudness difference between the softest and loudest layer");
    toggle (dynamics, params::velocityBlend, "Blend", "Smoothly interpolate between neighbouring layers instead of switching");
    combo (dynamics, params::dynSource, "Dynamics", "Velocity, or the mod wheel (CC1) for long articulations", 96);
    combo (dynamics, params::rrMode, "RR Mode", "Cycle through round robins, or pick randomly without repeats", 84);
    addAndMakeVisible (dynamics);

    // ---- shape
    knob (shape, params::attack, "Attack", "Extra fade-in");
    knob (shape, params::release, "Release", "Release time after note-off");
    knob (shape, params::glide, "Glide", "Legato transition time");
    knob (shape, params::tremoloRate, "Trem Rate", "Tremolo speed");
    knob (shape, params::pitchDrift, "Pitch Drift", "How much of the sample's own pitch movement (vibrato) is kept");
    combo (shape, params::sustainMode, "Sustain", "Auto loops sustained sounds and lets plucked/struck sounds decay", 84);
    addAndMakeVisible (shape);

    // ---- output
    knob (output, params::gain, "Gain", "Output level");
    knob (output, params::width, "Width", "Stereo spread across the keyboard");
    knob (output, params::polyphony, "Polyphony", "Maximum simultaneous notes");
    knob (output, params::bendRange, "Bend", "Pitch bend range (semitones)");
    knob (output, params::ksBase, "KS Base", "Lowest keyswitch note. The 9 articulations follow chromatically.");
    toggle (output, params::keyswitches, "Keyswitch", "Enable keyswitches");
    addAndMakeVisible (output);

    processor.addChangeListener (this);
    setSize (kWidth, kHeight);
    waveform.refresh();
    changeListenerCallback (nullptr);
    startTimerHz (30);
}

IVIEditor::~IVIEditor()
{
    processor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void IVIEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    auto header = getLocalBounds().removeFromTop (50).reduced (14, 0);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("INSTANT VIRTUAL INSTRUMENT", header.removeFromLeft (330), juce::Justification::centredLeft);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (12.5f));
    g.drawText ("one sample in, a whole instrument out", header.removeFromLeft (240), juce::Justification::centredLeft);
}

void IVIEditor::resized()
{
    auto r = getLocalBounds().reduced (12, 0);

    auto header = r.removeFromTop (50);
    header.removeFromLeft (580);
    loadButton.setBounds (header.removeFromLeft (118).withSizeKeepingCentre (112, 28));
    demoButton.setBounds (header.removeFromLeft (70).withSizeKeepingCentre (64, 28));
    exportButton.setBounds (header.removeFromLeft (118).withSizeKeepingCentre (112, 28));
    statusLabel.setBounds (getLocalBounds().removeFromTop (78).removeFromBottom (24).reduced (14, 0));
    r.removeFromTop (26);

    auto top = r.removeFromTop (172);
    waveform.setBounds (top.removeFromLeft (690));
    top.removeFromLeft (8);
    spectrum.setBounds (top);
    r.removeFromTop (8);

    zoneMap.setBounds (r.removeFromTop (132));
    r.removeFromTop (8);

    auto artRow = r.removeFromTop (30);
    const int bw = artRow.getWidth() / kNumArticulations;
    for (auto* b : articulationButtons)
        b->setBounds (artRow.removeFromLeft (bw).reduced (2, 0));
    r.removeFromTop (8);

    auto row1 = r.removeFromTop (106);
    mapping.setBounds (row1.removeFromLeft (620));
    row1.removeFromLeft (8);
    timbre.setBounds (row1);
    r.removeFromTop (8);

    auto row2 = r.removeFromTop (106);
    dynamics.setBounds (row2.removeFromLeft (324));
    row2.removeFromLeft (8);
    shape.setBounds (row2.removeFromLeft (420));
    row2.removeFromLeft (8);
    output.setBounds (row2);
    r.removeFromTop (8);

    keyboard.setBounds (r.removeFromTop (r.getHeight() - 8));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 52.0f);
}

void IVIEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    statusLabel.setText (processor.getStatus(), juce::dontSendNotification);
    waveform.refresh();
    spectrum.repaint();
    zoneMap.repaint();
    exportButton.setEnabled (processor.getModel() != nullptr);
}

void IVIEditor::timerCallback()
{
    const bool busy = processor.isAnalysing() || processor.isExporting();
    if (processor.isAnalysing())
        waveform.repaint();
    if (processor.isExporting())
        statusLabel.setText ("Exporting... " + juce::String (juce::roundToInt (processor.getExportProgress() * 100.0f)) + " %  (click Cancel to stop)",
                             juce::dontSendNotification);
    if (busy != wasBusy)
    {
        exportButton.setButtonText (processor.isExporting() ? "Cancel" : "Export SFZ...");
        wasBusy = busy;
    }
    updateArticulationButtons();
    zoneMap.repaint();
    keyboard.repaint();
}

void IVIEditor::updateArticulationButtons()
{
    const int a = processor.getEngine().getArticulation();
    if (a == shownArticulation)
        return;
    shownArticulation = a;
    const auto es = processor.currentEngineSettings();
    for (int i = 0; i < articulationButtons.size(); ++i)
    {
        auto* b = articulationButtons[i];
        b->setToggleState (i == a, juce::dontSendNotification);
        b->setTooltip (articulationName ((Articulation) i) + (es.keyswitchesEnabled
                           ? "  (keyswitch " + params::noteName (es.keyswitchBase + i) + ")" : juce::String()));
    }
}

void IVIEditor::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a sample", juce::File(), audioWildcard);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile())
                                  loadFile (f);
                          });
}

void IVIEditor::loadFile (const juce::File& f)
{
    juce::String error;
    if (! processor.loadSampleFile (f, error))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not load sample", error);
}

void IVIEditor::showExportMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Export current articulation (" + articulationName ((Articulation) processor.getEngine().getArticulation()) + ")...");
    menu.addItem (2, "Export all articulations (keyswitched SFZ)...");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (exportButton), [this] (int choice)
    {
        if (choice == 0)
            return;
        const bool all = choice == 2;
        chooser = std::make_unique<juce::FileChooser> ("Choose a folder for the exported library",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this, all] (const juce::FileChooser& fc)
                              {
                                  const auto dir = fc.getResult();
                                  if (dir.isDirectory())
                                      processor.startExport (dir, all);
                              });
    });
}

bool IVIEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension (audioWildcard))
            return true;
    return false;
}

void IVIEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension (audioWildcard))
        {
            loadFile (juce::File (f));
            return;
        }
}
