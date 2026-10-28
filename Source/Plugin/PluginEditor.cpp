#include "PluginEditor.h"
#include "Core/Articulations.h"

using namespace ivi;
using namespace ivi::theme;
using params::str;

namespace
{
    const juce::String audioWildcard = "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3";

    /** A cell's width: at least 68 points, and wide enough for its label on two lines. */
    int labelFit (const juce::String& name)
    {
        const float one = textWidth (name);
        if (one + 10.0f <= 68.0f)
            return 68;
        float best = one;
        auto words = juce::StringArray::fromTokens (name, " ", {});
        for (int i = 1; i < words.size(); ++i)
        {
            juce::StringArray a, b;
            for (int j = 0; j < words.size(); ++j)
                (j < i ? a : b).add (words[j]);
            best = juce::jmin (best, juce::jmax (textWidth (a.joinIntoString (" ")), textWidth (b.joinIntoString (" "))));
        }
        return juce::jmax (68, (int) std::ceil (best) + 10);
    }

    juce::String articulationHint (Articulation a)
    {
        switch (a)
        {
            case Articulation::Sustain:    return "A long note that holds while the key is down.";
            case Articulation::Legato:     return "Play overlapping notes and they glide into each other without a new attack.";
            case Articulation::Staccato:   return "Short and detached. Plays to its end whatever the key does.";
            case Articulation::Spiccato:   return "Shorter and brighter than staccato, off the string.";
            case Articulation::Marcato:    return "An accented, bright attack that settles into a softer held note.";
            case Articulation::Pizzicato:  return "Plucked: it rings and dies away, the upper partials first.";
            case Articulation::Tremolo:    return "A fast, slightly uneven tremolo. Tremolo rate sets its speed.";
            case Articulation::Swell:      return "A crescendo from soft, growing brighter through the layers.";
            case Articulation::Fortepiano: return "A loud attack that drops straight to soft.";
            case Articulation::Count:      break;
        }
        return {};
    }

    /** Lays components left to right, wrapping at the edge. */
    struct Flow
    {
        int x0, width, x, y, rowH = 0;
        Flow (int left, int w, int top) : x0 (left), width (w), x (left), y (top) {}
        void add (juce::Component& c, int w, int h)
        {
            if (x > x0 && x + w > x0 + width)
                newRow();
            c.setBounds (x, y, w, h);
            x += w + gap;
            rowH = juce::jmax (rowH, h);
        }
        void newRow() { x = x0; y += rowH + gap; rowH = 0; }
        int bottom() const { return y + rowH; }
    };
}

// ============================================================================ Group

void IVIContent::Group::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (rule);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (label);
    g.setFont (font());
    g.drawText (title, juce::Rectangle<int> (12, 12, getWidth() - 24, 18), juce::Justification::centredLeft);
}

void IVIContent::Group::add (juce::Component& c, const juce::String& name, int width)
{
    auto l = std::make_unique<juce::Label> (juce::String(), name);
    l->setJustificationType (juce::Justification::centredBottom);
    l->setFont (font());
    l->setMinimumHorizontalScale (1.0f);
    l->setColour (juce::Label::textColourId, label);
    l->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (*l);
    addAndMakeVisible (c);
    cells.push_back ({ &c, std::move (l), width });
}

int IVIContent::Group::naturalWidth() const
{
    int w = 24;
    for (auto& c : cells)
        w += c.width;
    return w + gap * juce::jmax (0, (int) cells.size() - 1);
}

int IVIContent::Group::heightFor (int width) const
{
    int rows = 1, x = 12;
    for (auto& cell : cells)
    {
        if (x > 12 && x + cell.width > width - 12)
        {
            ++rows;
            x = 12;
        }
        x += cell.width + gap;
    }
    return 12 + 18 + 8 + rows * rowHeight + (rows - 1) * gap + 10;
}

void IVIContent::Group::resized()
{
    int x = 12, top = 12 + 18 + 8;
    for (auto& cell : cells)
    {
        if (x > 12 && x + cell.width > getWidth() - 12)
        {
            x = 12;
            top += rowHeight + gap;
        }
        cell.label->setBounds (x, top, cell.width, labelHeight);
        const int y = top + labelHeight + 4;
        if (dynamic_cast<juce::Slider*> (cell.comp) != nullptr)
            cell.comp->setBounds (x, y, cell.width, knobHeight);
        else
            cell.comp->setBounds (x, y + 10, cell.width, controlHeight);
        x += cell.width + gap;
    }
}

// ============================================================================ content

juce::Slider& IVIContent::knob (Group& g, const char* id, const juce::String& name, const juce::String& tip)
{
    auto* s = static_cast<juce::Slider*> (owned.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow)));
    const int w = labelFit (name);
    s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, w, 22);
    s->setWantsKeyboardFocus (true);   // JUCE's sliders turn it off; the arrow keys then move the value
    s->setTooltip (tip);
    s->setTitle (name);
    g.add (*s, name, w);
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, *s));
    return *s;
}

juce::ComboBox& IVIContent::combo (Group& g, const char* id, const juce::String& name, const juce::String& tip, const juce::StringArray& itemsIn)
{
    auto* c = static_cast<juce::ComboBox*> (owned.add (new juce::ComboBox()));
    auto items = itemsIn;
    if (items.isEmpty())
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (id)))
            items = choice->choices;
    c->addItemList (items, 1);
    float widest = 0.0f;
    for (auto& i : items)
        widest = juce::jmax (widest, textWidth (i));
    const int w = juce::jmax (labelFit (name), (int) std::ceil (widest) + 46);
    c->setTooltip (tip);
    c->setTitle (name);
    g.add (*c, name, w);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, id, *c));
    return *c;
}

juce::ToggleButton& IVIContent::toggle (Group& g, const char* id, const juce::String& name, const juce::String& tip)
{
    auto* b = static_cast<juce::ToggleButton*> (owned.add (new juce::ToggleButton ("On")));
    b->setTooltip (tip);
    b->setTitle (name);
    g.add (*b, name, juce::jmax (labelFit (name), (int) std::ceil (textWidth ("On")) + 44));
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, id, *b));
    return *b;
}

IVIContent::IVIContent (IVIProcessor& p)
    : loadButton (str ("Load Sample…")), demoButton ("Load Demo Sound"), exportButton (str ("Export SFZ…")),
      undoButton ("Undo"), dismissButton ("Dismiss"), newTakesButton ("New Takes"),
      waveform (p), spectrum (p), zoneMap (p), keyboard (p, p.keyboardState), processor (p)
{
    setFocusContainerType (juce::Component::FocusContainerType::keyboardFocusContainer);

    for (auto* b : { &loadButton, &demoButton, &exportButton, &undoButton, &dismissButton })
        addChildComponent (*b);
    for (auto* b : { &loadButton, &demoButton, &exportButton })
        b->setVisible (true);
    loadButton.setTooltip (str ("Choose one audio file of a single note: WAV, AIFF, FLAC, Ogg or MP3. You can also drop a file on the window."));
    demoButton.setTooltip ("Build an instrument from a built-in string-like tone, to try things out.");
    exportButton.setTooltip ("Write every key, layer and round robin to WAV files, with an SFZ file that maps them for any SFZ player.");
    undoButton.setTooltip ("Bring back what the last change replaced.");
    dismissButton.setTooltip ("Clear this message.");
    loadButton.onClick = [this] { if (onLoad) onLoad(); };
    demoButton.onClick = [this] { processor.loadDemo(); };
    exportButton.onClick = [this] { if (onExport) onExport(); };
    undoButton.onClick = [this] { processor.undo(); };
    dismissButton.onClick = [this] { processor.dismissMessage(); };

    textSizeLabel.setText ("Text size", juce::dontSendNotification);
    textSizeLabel.setFont (font());
    textSizeLabel.setColour (juce::Label::textColourId, label);
    textSizeLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (textSizeLabel);
    for (auto s : IVIEditor::textSizes())
        textSize.addItem (juce::String (s) + "%", s);
    textSize.setTooltip (str ("Make the whole window larger. Cmd+= and Cmd+– change it, and Cmd+0 goes back to 100% (Ctrl on Windows)."));
    textSize.setTitle ("Text size");
    textSize.onChange = [this] { if (onTextSize) onTextSize(); };
    addAndMakeVisible (textSize);

    addAndMakeVisible (waveform);
    addAndMakeVisible (spectrum);
    addAndMakeVisible (zoneMap);
    addAndMakeVisible (keyboard);

    for (int a = 0; a < kNumArticulations; ++a)
    {
        auto* b = articulationButtons.add (new juce::TextButton (articulationName ((Articulation) a)));
        b->onClick = [this, a] { processor.requestArticulation (a); };
        addAndMakeVisible (b);
    }

    // ---- mapping
    {
        juce::StringArray notes { "Auto" };
        for (int n = 0; n < 128; ++n)
            notes.add (params::noteName (n));
        combo (mapping, params::rootNote, "Root note",
               "The note your sample plays. Auto finds it; set it if Auto picks the wrong octave.", notes);
    }
    knob (mapping, params::lowKey, "Lowest key", "The lowest key the instrument plays.");
    knob (mapping, params::highKey, "Highest key", "The highest key the instrument plays.");
    knob (mapping, params::layers, "Dynamic layers", "How many dynamic layers each key has, from pp to ff. Louder layers are brighter and speak faster.");
    knob (mapping, params::roundRobins, "Round robins", "How many takes each key and layer has. Repeated notes cycle through them.");
    knob (mapping, params::variation, "Variation", "How different the takes are: tuning, tone, attack, timing and noise.");
    knob (mapping, params::seed, "Seed", "The number the variations are made from. The same number always gives the same takes.");
    newTakesButton.setTooltip (str ("Make a fresh set of round robins and layer variations. Undo brings the old ones back."));
    newTakesButton.onClick = [this] { processor.makeNewTakes(); };
    mapping.add (newTakesButton, "", buttonWidth (newTakesButton.getButtonText()));
    addAndMakeVisible (mapping);

    // ---- timbre
    knob (timbre, params::formant, "Formant", str ("At 0% the tone moves with the pitch, like a sped-up tape. At 100% the instrument’s body stays put, as a real one does."));
    knob (timbre, params::brightness, "Brightness", "How much brighter the loud layers are than the soft ones.");
    knob (timbre, params::sourceDynamic, "Source dynamic", "The dynamic your sample was played at. The layers are made around it.");
    knob (timbre, params::noise, "Noise", "The level of the bow, breath or hammer noise in the sound.");
    knob (timbre, params::partials, "Partials", "How many partials each note has. Fewer use less CPU; more sound fuller in low notes.");
    addAndMakeVisible (timbre);

    // ---- dynamics
    knob (dynamics, params::dynRange, "Dynamic range", "How much louder the loudest layer is than the softest.");
    toggle (dynamics, params::velocityBlend, "Layer blend", "When on, the sound moves smoothly between neighbouring layers instead of switching.");
    combo (dynamics, params::dynSource, "Dynamics from", "Velocity, or the mod wheel (CC1) for long articulations, as orchestral libraries do.");
    combo (dynamics, params::rrMode, "Round robin order", "Cycle through the takes in turn, or pick them at random without repeating one.");
    addAndMakeVisible (dynamics);

    // ---- shape
    knob (shape, params::attack, "Attack", "An extra fade-in at the start of each note.");
    knob (shape, params::release, "Release", "How long a note takes to die away after you let go.");
    knob (shape, params::glide, "Legato glide", "How long Legato takes to slide from one note to the next.");
    knob (shape, params::tremoloRate, "Tremolo rate", "How fast Tremolo is, in strokes a second.");
    knob (shape, params::pitchDrift, "Pitch drift", str ("How much of the sample’s own pitch movement, such as vibrato, is kept."));
    combo (shape, params::sustainMode, "Sustain", "Auto loops sustained sounds and lets plucked or struck ones die away. Loop and Natural choose for you.");
    addAndMakeVisible (shape);

    // ---- output
    knob (output, params::gain, "Gain", "The output level.");
    knob (output, params::width, "Stereo width", "Spreads the keyboard across the stereo field: low notes left, high notes right.");
    knob (output, params::polyphony, "Polyphony", "How many notes can sound at once.");
    knob (output, params::bendRange, "Bend range", "How far the pitch wheel bends, in semitones.");
    toggle (output, params::keyswitches, "Keyswitches", "When on, the nine keys from Lowest keyswitch choose the articulation.");
    {
        juce::StringArray notes;
        for (int n = 0; n <= 127 - 9; ++n)
            notes.add (params::noteName (n));
        combo (output, params::ksBase, "Lowest keyswitch", "The key that chooses Sustain. The other eight articulations follow it up the keyboard.", notes);
    }
    addAndMakeVisible (output);

    clicksKeepFocus (*this);
    updateArticulationButtons();
    refreshFromProcessor();
}

void IVIContent::paint (juce::Graphics& g)
{
    g.fillAll (ground);
    if (shownMessage.text.isNotEmpty() && ! messageArea.isEmpty())
    {
        auto r = messageArea.toFloat();
        if (shownMessage.warning)
        {
            // the red marks; the words are in the lighter red, which can be read
            g.setColour (warn);
            g.fillRoundedRectangle (r.removeFromLeft (4.0f), 2.0f);
            r.removeFromLeft (8.0f);
        }
        juce::AttributedString s;
        s.append (shownMessage.text, font(), shownMessage.warning ? warnText : textDim);
        s.setWordWrap (juce::AttributedString::byWord);
        s.setJustification (juce::Justification::centredLeft);
        s.draw (g, r);
    }
}

int IVIContent::layout (int W)
{
    const int m = gap, inner = W - 2 * m;
    int y = m;

    // toolbar: the actions, then Text size at the right
    {
        Flow f (m, inner, y);
        for (auto* b : { &loadButton, &demoButton, &exportButton })
            f.add (*b, buttonWidth (b->getButtonText()), controlHeight);
        const int lw = (int) std::ceil (textWidth (textSizeLabel.getText())) + 6, cw = (int) std::ceil (textWidth ("200%")) + 46;
        if (f.x + lw + cw > m + inner)
            f.newRow();
        const int right = m + inner;
        textSize.setBounds (right - cw, f.y, cw, controlHeight);
        textSizeLabel.setBounds (right - cw - lw - 6, f.y, lw, controlHeight);
        f.rowH = juce::jmax (f.rowH, controlHeight);
        y = f.bottom() + m;
    }

    // the message, at the top until dismissed
    undoButton.setVisible (shownMessage.text.isNotEmpty() && shownMessage.canUndo);
    dismissButton.setVisible (shownMessage.text.isNotEmpty());
    messageArea = {};
    if (shownMessage.text.isNotEmpty())
    {
        int right = m + inner;
        const int dw = buttonWidth (dismissButton.getButtonText()), uw = buttonWidth (undoButton.getButtonText());
        dismissButton.setBounds (right - dw, y, dw, controlHeight);
        right -= dw + gap;
        if (shownMessage.canUndo)
        {
            undoButton.setBounds (right - uw, y, uw, controlHeight);
            right -= uw + gap;
        }
        const int tw = juce::jmax (100, right - m - (shownMessage.warning ? 12 : 0));
        juce::AttributedString s;
        s.append (shownMessage.text, font());
        s.setWordWrap (juce::AttributedString::byWord);
        juce::TextLayout tl;
        tl.createLayout (s, (float) tw);
        const int h = juce::jmax (controlHeight, (int) std::ceil (tl.getHeight()) + 4);
        messageArea = { m, y, right - m, h };
        y += h + m;
    }

    // the sample and its model
    constexpr int viewH = 150;
    if (inner >= 900)
    {
        const int ww = (int) ((inner - gap) * 0.58f);
        waveform.setBounds (m, y, ww, viewH);
        spectrum.setBounds (m + ww + gap, y, inner - ww - gap, viewH);
        y += viewH + m;
    }
    else
    {
        waveform.setBounds (m, y, inner, viewH);
        y += viewH + m;
        spectrum.setBounds (m, y, inner, viewH);
        y += viewH + m;
    }

    // the library
    {
        const int zh = zoneMap.heightFor (inner);
        zoneMap.setBounds (m, y, inner, zh);
        y += zh + m;
    }

    // articulations: one row across the window when they fit, else wrapped
    {
        int bw = 0;
        for (auto* b : articulationButtons)
            bw = juce::jmax (bw, buttonWidth (b->getButtonText()));
        const int n = articulationButtons.size();
        if (n * bw + (n - 1) * gap <= inner)
            bw = (inner - (n - 1) * gap) / n;
        Flow f (m, inner, y);
        for (auto* b : articulationButtons)
            f.add (*b, bw, controlHeight);
        y = f.bottom() + m;
    }

    // the settings, wrapping at the edge
    {
        Flow f (m, inner, y);
        for (auto* g : { &mapping, &timbre, &dynamics, &shape, &output })
        {
            const int gw = juce::jmin (inner, g->naturalWidth());
            f.add (*g, gw, g->heightFor (gw));
        }
        y = f.bottom() + m;
    }

    keyboard.setBounds (m, y, inner, 80);
    keyboard.setKeyWidth ((float) inner / 52.0f);
    y += 80 + m;
    return y;
}

void IVIContent::refreshFromProcessor()
{
    const auto msg = processor.getMessage();
    const bool relayout = msg.text != shownMessage.text || msg.canUndo != shownMessage.canUndo || msg.warning != shownMessage.warning;
    shownMessage = msg;
    if (relayout && getWidth() > 0)
        layout (getWidth());
    waveform.refresh();
    spectrum.repaint();
    zoneMap.repaint();
    exportButton.setEnabled (processor.getModel() != nullptr || processor.isExporting());
    repaint();
}

void IVIContent::updateArticulationButtons()
{
    const int a = processor.getEngine().getArticulation();
    const auto es = processor.currentEngineSettings();
    const int ks = es.keyswitchesEnabled ? es.keyswitchBase : -1;
    if (a == shownArticulation && ks == shownKeyswitchBase)
        return;
    shownArticulation = a;
    shownKeyswitchBase = ks;
    for (int i = 0; i < articulationButtons.size(); ++i)
    {
        auto* b = articulationButtons[i];
        b->setToggleState (i == a, juce::dontSendNotification);
        juce::String tip = articulationHint ((Articulation) i);
        if (es.keyswitchesEnabled)
            tip << " Keyswitch " << params::noteName (es.keyswitchBase + i) << ".";
        b->setTooltip (tip);
    }
}

void IVIContent::tick()
{
    updateArticulationButtons();
    waveform.tick();
    zoneMap.repaint();
    keyboard.repaint();

    if (processor.isExporting())
    {
        shownMessage = { str ("Exporting… ") + juce::String (juce::roundToInt (processor.getExportProgress() * 100.0f))
                             + str ("%. Stop Export ends it; the samples written so far are kept."), false, false };
        repaint (messageArea);
    }
    if (processor.isExporting() != wasExporting)
    {
        wasExporting = processor.isExporting();
        exportButton.setButtonText (wasExporting ? "Stop Export" : str ("Export SFZ…"));
        layout (getWidth());
        if (! wasExporting)
            refreshFromProcessor();
    }
}

// ============================================================================ editor

const juce::Array<int>& IVIEditor::textSizes()
{
    static const juce::Array<int> sizes { 100, 125, 150, 200 };
    return sizes;
}

IVIEditor::IVIEditor (IVIProcessor& p)
    : AudioProcessorEditor (p), processor (p), content (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    juce::PropertiesFile::Options o;
    o.applicationName = "Instant Virtual Instrument";
    o.filenameSuffix = ".settings";
    o.folderName = "Instant Virtual Instrument";
    o.osxLibrarySubFolder = "Application Support";
    prefs = std::make_unique<juce::PropertiesFile> (o);
    textSizePercent = prefs->getIntValue ("textSize", 100);
    if (! textSizes().contains (textSizePercent))
        textSizePercent = 100;

    viewport.setScrollBarsShown (true, false, true, false);
    viewport.setViewedComponent (&holder, false);
    holder.addAndMakeVisible (content);
    addAndMakeVisible (viewport);

    content.onLoad = [this] { chooseSample(); };
    content.onExport = [this]
    {
        if (processor.isExporting()) processor.cancelExport();
        else                         showExportMenu();
    };
    content.onTextSize = [this] { setTextSize (content.textSize.getSelectedId()); };
    content.textSize.setSelectedId (textSizePercent, juce::dontSendNotification);

    processor.addChangeListener (this);
    setResizable (true, true);
    const auto pref = preferredSize();
    setResizeLimits (IVIContent::minimumWidth, 400, 8000, 6000);
    setSize (pref.getWidth(), pref.getHeight());
    startTimerHz (30);
}

IVIEditor::~IVIEditor()
{
    processor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

juce::Rectangle<int> IVIEditor::preferredSize() const
{
    const float s = (float) textSizePercent / 100.0f;
    IVIContent& c = const_cast<IVIContent&> (content);
    const int h = c.layout (IVIContent::preferredWidth);
    int w = juce::roundToInt ((float) IVIContent::preferredWidth * s), hh = juce::roundToInt ((float) h * s);
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        w = juce::jmin (w, d->userArea.getWidth() - 40);
        hh = juce::jmin (hh, d->userArea.getHeight() - 80);
    }
    return { juce::jmax (IVIContent::minimumWidth, w), juce::jmax (400, hh) };
}

void IVIEditor::setTextSize (int percent)
{
    if (! textSizes().contains (percent) || percent == textSizePercent)
        return;
    textSizePercent = percent;
    prefs->setValue ("textSize", percent);
    prefs->saveIfNeeded();
    content.textSize.setSelectedId (percent, juce::dontSendNotification);
    const auto pref = preferredSize();
    setResizeLimits (juce::roundToInt ((float) IVIContent::minimumWidth * (float) percent / 100.0f), 400, 8000, 6000);
    setSize (pref.getWidth(), pref.getHeight());
    layoutContent();
}

void IVIEditor::paint (juce::Graphics& g)
{
    g.fillAll (ground);
}

void IVIEditor::resized()
{
    viewport.setBounds (getLocalBounds());
    layoutContent();
}

void IVIEditor::layoutContent()
{
    const float s = (float) textSizePercent / 100.0f;
    auto fit = [&] (int available)
    {
        const int w = juce::jmax (IVIContent::minimumWidth, (int) ((float) available / s));
        return std::make_pair (w, content.layout (w));
    };
    auto [w, h] = fit (viewport.getWidth());
    if ((float) h * s > (float) viewport.getHeight())
        std::tie (w, h) = fit (viewport.getWidth() - viewport.getScrollBarThickness());
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, w, h);
    holder.setSize (juce::roundToInt ((float) w * s), juce::roundToInt ((float) h * s));
}

bool IVIEditor::keyPressed (const juce::KeyPress& k)
{
    const auto& sizes = textSizes();
    const int i = sizes.indexOf (textSizePercent);
    if (k.getModifiers().isCommandDown())
    {
        const auto c = k.getTextCharacter();
        if (c == '=' || c == '+' || k.getKeyCode() == '=')  { setTextSize (sizes[juce::jmin (sizes.size() - 1, i + 1)]); return true; }
        if (c == '-' || k.getKeyCode() == '-')               { setTextSize (sizes[juce::jmax (0, i - 1)]); return true; }
        if (c == '0' || k.getKeyCode() == '0')               { setTextSize (100); return true; }
    }
    if (k.isKeyCode (juce::KeyPress::escapeKey))
    {
        // Esc lets go of the keyboard first, as on the Mac
        if (auto* f = juce::Component::getCurrentlyFocusedComponent(); f != nullptr && f != this)
        {
            grabKeyboardFocus();
            return true;
        }
    }
    return false;
}

void IVIEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const auto before = content.getHeight();
    content.refreshFromProcessor();
    if (content.layout (content.getWidth()) != before)
        layoutContent();
}

void IVIEditor::timerCallback()
{
    content.tick();
}

void IVIEditor::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a Sample", juce::File(), audioWildcard);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile())
                                  processor.loadSampleFile (f);
                          });
}

void IVIEditor::showExportMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, str ("Export This Articulation (") + articulationName ((Articulation) processor.getEngine().getArticulation()) + str (")…"));
    menu.addItem (2, str ("Export All Articulations…"));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (content.exportButton), [this] (int choice)
    {
        if (choice == 0)
            return;
        const bool all = choice == 2;
        chooser = std::make_unique<juce::FileChooser> ("Choose a Folder for the Library",
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
            processor.loadSampleFile (juce::File (f));
            return;
        }
    processor.loadSampleFile (juce::File (files[0]));   // says what it can't read, and what to choose instead
}
