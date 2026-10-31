#include "Views.h"
#include "Theme.h"
#include "PluginProcessor.h"
#include "Core/Articulations.h"

namespace ivi::ui
{

using namespace ivi::theme;
using params::str;

namespace
{
    void drawRoll (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (roll);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (rule);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    }

    juce::String noteName (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 4); }
}

float TextRow::place (juce::Rectangle<float> area)
{
    area = area.reduced (10.0f, 0.0f);
    const float lw = textWidth (left), rw = textWidth (right);
    rows = (right.isEmpty() || lw + (float) gap + rw <= area.getWidth()) ? 1 : 2;
    leftBox = { area.getX(), area.getY() + 3.0f, juce::jmin (lw + 1.0f, area.getWidth()), lineHeight - 6.0f };
    const float ry = area.getY() + (float) (rows - 1) * lineHeight + 3.0f;
    rightBox = right.isEmpty() ? juce::Rectangle<float>()
                               : juce::Rectangle<float> (rows == 1 ? area.getRight() - rw - 1.0f : area.getX(), ry,
                                                         juce::jmin (rw + 1.0f, area.getWidth()), lineHeight - 6.0f);
    return lineHeight * (float) rows;
}

void TextRow::draw (juce::Graphics& g, juce::Colour leftColour, juce::Colour rightColour) const
{
    g.setFont (font());
    g.setColour (leftColour);
    g.drawText (left, leftBox, juce::Justification::centredLeft, true);
    if (right.isNotEmpty())
    {
        g.setColour (rightColour);
        g.drawText (right, rightBox, juce::Justification::centredLeft, true);
    }
}

juce::Colour layerColour (int layer, int numLayers)
{
    // from the menus' grey to the held controls' grey: every step stays blue (R < G < B)
    const float t = numLayers <= 1 ? 0.6f : (float) layer / (float) (numLayers - 1);
    return popup.interpolatedWith (controlHeld, t);
}

// ============================================================================ waveform

void WaveformView::refresh()
{
    auto m = processor.getModel();
    if (m != shown)
    {
        shown = m;
        peaks.clear();
        resized();
    }
    repaint();
}

void WaveformView::tick()
{
    const float p = processor.getEngine().getPlayheadFrame();
    if (std::abs (p - lastPlayhead) > 0.5f || (p < 0.0f) != (lastPlayhead < 0.0f) || processor.isAnalysing())
        repaint();
}

void WaveformView::resized()
{
    header.left = "Sample";
    header.right = {};
    if (shown != nullptr)
        header.right << shown->name << str ("  ·  ") << juce::String (shown->durationSeconds(), 2) << str (" s  ·  ")
                     << juce::String (shown->sampleRate / 1000.0, 1) << " kHz";
    header.place (getLocalBounds().toFloat());
}

juce::Array<juce::Rectangle<float>> WaveformView::textBoxes() const
{
    return { header.leftBox, header.rightBox };
}

void WaveformView::paint (juce::Graphics& g)
{
    drawRoll (g, getLocalBounds().toFloat());
    header.draw (g, label, textDim);
    auto area = getLocalBounds().toFloat().withTrimmedTop (TextRow::lineHeight * (float) header.rows).reduced (10.0f, 0.0f).withTrimmedBottom (8.0f);

    g.setFont (font());
    if (shown == nullptr)
    {
        g.setColour (textDim);
        g.drawText (processor.isAnalysing() ? str ("Analysing…") : str ("Drop a sample here, or choose Load Sample…"),
                    area, juce::Justification::centred);
    }
    else
    {
        const auto& m = *shown;
        auto labels = area.removeFromTop (18.0f);
        const int w = juce::jmax (1, (int) area.getWidth());
        const auto* x = m.source.getReadPointer (0);
        const int n = m.source.getNumSamples();

        if ((int) peaks.size() != w)
        {
            peaks.assign ((size_t) w, { 0.0f, 0.0f });
            for (int px = 0; px < w; ++px)
            {
                const int a = (int) ((juce::int64) px * n / w), b = juce::jmax (a + 1, (int) ((juce::int64) (px + 1) * n / w));
                float lo = 0.0f, hi = 0.0f;
                for (int i = a; i < b && i < n; ++i)
                {
                    lo = juce::jmin (lo, x[i]);
                    hi = juce::jmax (hi, x[i]);
                }
                peaks[(size_t) px] = { lo, hi };
            }
        }

        const double dur = m.durationSeconds();
        auto xFor = [&] (double frame) { return area.getX() + (float) (m.frameToSeconds (frame) / dur) * area.getWidth(); };

        const float aX0 = xFor (m.onsetFrame), aX1 = xFor (m.attackEndFrame);
        const float lX0 = xFor (m.loopStartFrame), lX1 = xFor (m.loopEndFrame);
        g.setColour (frameHover);
        g.fillRect (juce::Rectangle<float> (aX0, area.getY(), juce::jmax (1.0f, aX1 - aX0), area.getHeight()));
        g.setColour (m.decaying ? frameHover : rule);
        g.fillRect (juce::Rectangle<float> (lX0, area.getY(), juce::jmax (1.0f, lX1 - lX0), area.getHeight()));

        float peak = 1.0e-6f;
        for (auto& p : peaks)
            peak = juce::jmax (peak, -p.first, p.second);
        const float mid = area.getCentreY(), half = area.getHeight() * 0.47f / peak;
        g.setColour (control);
        for (int px = 0; px < w; ++px)
        {
            const auto& p = peaks[(size_t) px];
            g.drawVerticalLine ((int) area.getX() + px, mid - p.second * half, mid - p.first * half + 1.0f);
        }

        g.setColour (thumb);
        for (float lx : { aX1, lX0, lX1 })
            g.drawVerticalLine ((int) lx, area.getY(), area.getBottom());

        g.setColour (textDim);
        g.drawText ("Attack", juce::Rectangle<float> (aX0, labels.getY(), juce::jmax (60.0f, aX1 - aX0 + 40.0f), 18.0f), juce::Justification::centredLeft);
        g.drawText (m.decaying ? str ("Loop — used when Sustain is Loop") : str ("Sustain loop"),
                    juce::Rectangle<float> (lX0 + 4.0f, labels.getY(), juce::jmax (240.0f, lX1 - lX0), 18.0f), juce::Justification::centredLeft);

        lastPlayhead = processor.getEngine().getPlayheadFrame();
        if (lastPlayhead >= 0.0f)
        {
            g.setColour (focus);
            g.fillRect (juce::Rectangle<float> (xFor (lastPlayhead) - 1.0f, area.getY(), 2.0f, area.getHeight()));
        }
    }

    if (processor.isAnalysing())
    {
        auto bar = getLocalBounds().toFloat().withSizeKeepingCentre (juce::jmin (300.0f, (float) getWidth() - 40.0f), 8.0f).translated (0.0f, 24.0f);
        g.setColour (sunken);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (edge);
        g.drawRoundedRectangle (bar, 3.0f, 1.0f);
        g.setColour (control);
        g.fillRoundedRectangle (bar.reduced (1.0f).withWidth ((bar.getWidth() - 2.0f) * processor.getAnalysisProgress()), 2.0f);
    }
}

// ============================================================================ spectrum

void SpectrumView::resized()
{
    auto m = processor.getModel();
    header.left = "Spectral model";
    header.right = m != nullptr ? juce::String (m->numPartials) + " partials" : juce::String();
    header.place (getLocalBounds().toFloat());
}

juce::Array<juce::Rectangle<float>> SpectrumView::textBoxes() const
{
    return { header.leftBox, header.rightBox };
}

void SpectrumView::paint (juce::Graphics& g)
{
    drawRoll (g, getLocalBounds().toFloat());
    auto m = processor.getModel();
    if (header.right.isEmpty() != (m == nullptr))
        resized();
    header.draw (g, label, textDim);
    auto area = getLocalBounds().toFloat().withTrimmedTop (TextRow::lineHeight * (float) header.rows).reduced (10.0f, 0.0f).withTrimmedBottom (22.0f);
    if (m == nullptr)
        return;

    const int frame = juce::jlimit (0, m->numFrames - 1, (m->loopStartFrame + m->loopEndFrame) / 2);
    const float fLo = 30.0f, fHi = 20000.0f;
    auto xFor = [&] (float f) { return area.getX() + area.getWidth() * std::log (f / fLo) / std::log (fHi / fLo); };

    float maxDb = -200.0f;
    for (int h = 0; h < m->numPartials; ++h)
        maxDb = juce::jmax (maxDb, m->partialDb (frame, h));
    constexpr float range = 90.0f;
    auto yFor = [&] (float dB) { return area.getBottom() - area.getHeight() * juce::jlimit (0.0f, 1.0f, (dB - (maxDb - range)) / range); };

    g.setFont (font());
    for (float f : { 100.0f, 1000.0f, 10000.0f })
    {
        g.setColour (rollLine);
        g.drawVerticalLine ((int) xFor (f), area.getY(), area.getBottom());
        g.setColour (textDim);
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + " kHz" : juce::String ((int) f) + " Hz",
                    juce::Rectangle<float> (xFor (f) + 3.0f, area.getBottom() + 3.0f, 60.0f, 17.0f), juce::Justification::centredLeft);
    }

    juce::Path noise;
    noise.startNewSubPath (xFor (m->bandCentre[0]), area.getBottom());
    for (int b = 0; b < m->numBands; ++b)
        noise.lineTo (xFor (m->bandCentre[(size_t) b]),
                      yFor (juce::Decibels::gainToDecibels (m->noiseAmp[(size_t) frame * (size_t) m->numBands + (size_t) b], -200.0f)));
    noise.lineTo (xFor (m->bandCentre[(size_t) m->numBands - 1]), area.getBottom());
    noise.closeSubPath();
    g.setColour (rule);
    g.fillPath (noise);

    const double f0 = m->f0Ref * m->f0Ratio[(size_t) frame];
    juce::Path body;
    bool started = false;
    g.setColour (control);
    for (int h = 0; h < m->numPartials; ++h)
    {
        const auto idx = (size_t) frame * (size_t) m->numPartials + (size_t) h;
        const float f = (float) (m->ratio[idx] * f0);
        if (f < fLo || f > fHi)
            continue;
        const float x = xFor (f);
        g.drawLine (x, area.getBottom(), x, yFor (m->partialDb (frame, h)), 1.5f);
        const float env = m->slopeA[(size_t) frame] + m->slopeB[(size_t) frame] * m->logRatio[idx] + m->envDb[idx];
        if (! started) { body.startNewSubPath (x, yFor (env)); started = true; }
        else           body.lineTo (x, yFor (env));
    }
    g.setColour (text);
    g.strokePath (body, juce::PathStrokeType (1.2f));
}

// ============================================================================ zone map

ZoneMapView::ZoneMapView (IVIProcessor& p) : processor (p)
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (false);
    setTitle ("Zone map");
}

juce::Rectangle<float> ZoneMapView::gridArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft ((float) labelWidth).withTrimmedRight (10.0f)
               .withTrimmedTop (TextRow::lineHeight * (float) header.rows + 18.0f)
               .withTrimmedBottom (TextRow::lineHeight * (float) legend.rows + 2.0f);
}

void ZoneMapView::updateText()
{
    const auto bs = processor.currentBuildSettings();
    const auto es = processor.currentEngineSettings();
    const int keys = bs.highKey - bs.lowKey + 1;
    const int total = keys * bs.numLayers * bs.numRoundRobins * kNumArticulations;
    header.left = "Library";
    header.right = {};
    header.right << keys << str (" keys × ") << bs.numLayers << str (" layers × ") << bs.numRoundRobins << str (" round robins × ")
                 << kNumArticulations << " articulations = "
                 << (total >= 1000 ? juce::String (total / 1000) + "," + juce::String (total % 1000).paddedLeft ('0', 3) : juce::String (total))
                 << " zones";
    legend.left = "Rows are dynamic layers; stripes are round robins. Click a cell to hear it.";
    legend.right = {};
    if (es.keyswitchesEnabled)
        legend.right << "Keyswitches " << noteName (es.keyswitchBase) << str ("–") << noteName (es.keyswitchBase + kNumArticulations - 1)
                     << str (" · now ") << articulationName ((Articulation) processor.getEngine().getArticulation());
}

int ZoneMapView::heightFor (int width) const
{
    TextRow h = header, l = legend;
    const auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) width, 100.0f);
    const int layers = processor.currentBuildSettings().numLayers;
    return (int) (h.place (area) + 18.0f + (float) juce::jmax (90, layers * 18) + l.place (area) + 2.0f);
}

void ZoneMapView::resized()
{
    updateText();
    header.place (getLocalBounds().toFloat());
    legend.place (getLocalBounds().toFloat().withTop ((float) getHeight() - TextRow::lineHeight * 2.0f));
    // the legend sits at the bottom: place it again now that its row count is known
    legend.place (getLocalBounds().toFloat().withTop ((float) getHeight() - TextRow::lineHeight * (float) legend.rows));
}

juce::Array<juce::Rectangle<float>> ZoneMapView::textBoxes() const
{
    return { header.leftBox, header.rightBox, legend.leftBox, legend.rightBox };
}

ZoneMapView::Hit ZoneMapView::hitAt (juce::Point<float> p) const
{
    const auto area = gridArea();
    if (! area.contains (p))
        return {};
    const auto bs = processor.currentBuildSettings();
    Hit h;
    h.key = juce::jlimit (0, 127, (int) ((p.x - area.getX()) / area.getWidth() * 128.0f));
    h.layer = juce::jlimit (0, bs.numLayers - 1, bs.numLayers - 1 - (int) ((p.y - area.getY()) / area.getHeight() * (float) bs.numLayers));
    return h;
}

juce::String ZoneMapView::describe (Hit h) const
{
    if (h.key < 0)
        return {};
    const auto es = processor.currentEngineSettings();
    const auto bs = processor.currentBuildSettings();
    const auto name = noteName (h.key);
    if (es.keyswitchesEnabled && h.key >= es.keyswitchBase && h.key < es.keyswitchBase + kNumArticulations)
        return name + str (" — keyswitch for ") + articulationName ((Articulation) (h.key - es.keyswitchBase)) + ". Click to choose it.";
    if (h.key >= bs.lowKey && h.key <= bs.highKey)
        return name + str (" — layer ") + Instrument::layerName (h.layer, bs.numLayers) + " (" + juce::String (h.layer + 1) + " of "
             + juce::String (bs.numLayers) + "), " + juce::String (bs.numRoundRobins) + " round robins. Click to hear it.";
    return name + str (" is outside the mapped keys. Lowest key and Highest key set them.");
}

void ZoneMapView::paint (juce::Graphics& g)
{
    drawRoll (g, getLocalBounds().toFloat());

    const auto bs = processor.currentBuildSettings();
    const auto es = processor.currentEngineSettings();
    const auto& engine = processor.getEngine();
    const bool haveModel = processor.getModel() != nullptr;
    const int currentArt = engine.getArticulation();
    {
        const auto before = legend.right;
        updateText();
        if (legend.right != before)
            resized();
    }
    header.draw (g, label, textDim);

    const auto area = gridArea();
    const float kw = area.getWidth() / 128.0f;
    const int L = bs.numLayers, R = bs.numRoundRobins;
    const float rowH = area.getHeight() / (float) L;

    g.setFont (font());
    for (int k = 0; k < 128; k += 12)
    {
        g.setColour (textDim);
        g.drawText (noteName (k), juce::Rectangle<float> (area.getX() + (float) k * kw, area.getY() - 18.0f, 40.0f, 17.0f),
                    juce::Justification::centredLeft);
    }

    for (int k = 0; k < 128; ++k)
    {
        const auto col = juce::Rectangle<float> (area.getX() + (float) k * kw, area.getY(), kw, area.getHeight());
        g.setColour (juce::MidiMessage::isMidiNoteBlack (k) ? sunken : roll);
        g.fillRect (col);

        if (es.keyswitchesEnabled && k >= es.keyswitchBase && k < es.keyswitchBase + kNumArticulations)
        {
            // keyswitches are buttons: the controls' grey, the chosen one yellow with an ink bar
            const bool cur = (k - es.keyswitchBase) == currentArt;
            const auto cell = col.reduced (0.5f, 0.0f);
            g.setColour (cur ? accent : control);
            g.fillRect (cell);
            g.setColour (ink);
            g.fillRect (cell.withTrimmedTop (cell.getHeight() - (cur ? 6.0f : 1.0f)));
            continue;
        }
        if (k < bs.lowKey || k > bs.highKey)
            continue;

        const uint16_t mask = haveModel ? engine.getKeyLayerMask (k) : 0;
        const int rr = engine.getKeyRoundRobin (k);
        for (int l = 0; l < L; ++l)
        {
            const auto cell = juce::Rectangle<float> (col.getX(), area.getY() + (float) (L - 1 - l) * rowH, kw, rowH).reduced (0.5f);
            const bool on = (mask >> l) & 1u;
            g.setColour (on ? accent : (haveModel ? layerColour (l, L) : frameHover));
            g.fillRect (cell);

            const float sh = cell.getHeight() / (float) R;
            if (R > 1 && sh >= 3.0f)
            {
                g.setColour (roll);
                for (int r = 1; r < R; ++r)
                    g.fillRect (cell.getX(), cell.getY() + sh * (float) r - 0.5f, cell.getWidth(), 1.0f);
            }
            if (on)
            {
                // which take is sounding: an ink stripe, not colour alone
                g.setColour (ink);
                g.fillRect (cell.getX(), cell.getY() + sh * (float) juce::jlimit (0, R - 1, rr) + sh * 0.3f, cell.getWidth(), juce::jmax (1.0f, sh * 0.4f));
            }
        }
    }

    // layer names, left of the rows
    for (int l = 0; l < L; ++l)
    {
        g.setColour (text);
        g.drawText (Instrument::layerName (l, L), juce::Rectangle<float> (6.0f, area.getY() + (float) (L - 1 - l) * rowH, (float) labelWidth - 10.0f, rowH),
                    juce::Justification::centredRight);
    }

    // the keyboard's cursor
    if (hasKeyboardFocus (false))
    {
        focusRing (g, *this, getLocalBounds().toFloat(), 4.0f);
        const bool ks = es.keyswitchesEnabled && cursor.key >= es.keyswitchBase && cursor.key < es.keyswitchBase + kNumArticulations;
        auto c = ks ? juce::Rectangle<float> (area.getX() + (float) cursor.key * kw, area.getY(), kw, area.getHeight())
                    : juce::Rectangle<float> (area.getX() + (float) cursor.key * kw, area.getY() + (float) (L - 1 - juce::jlimit (0, L - 1, cursor.layer)) * rowH, kw, rowH);
        g.setColour (focus);
        g.drawRect (c.expanded (2.0f), 2.0f);
    }

    legend.draw (g, textDim, textDim);
}

void ZoneMapView::press (Hit h)
{
    if (h.key < 0)
        return;
    const auto es = processor.currentEngineSettings();
    if (es.keyswitchesEnabled && h.key >= es.keyswitchBase && h.key < es.keyswitchBase + kNumArticulations)
    {
        processor.requestArticulation (h.key - es.keyswitchBase);
        return;
    }
    const auto bs = processor.currentBuildSettings();
    if (h.key < bs.lowKey || h.key > bs.highKey)
        return;
    releaseAudition();
    auditioning = h.key;
    processor.keyboardState.noteOn (1, h.key, juce::jlimit (0.01f, 1.0f, ((float) h.layer + 0.5f) / (float) bs.numLayers));
}

void ZoneMapView::releaseAudition()
{
    stopTimer();
    if (auditioning >= 0)
        processor.keyboardState.noteOff (1, auditioning, 0.0f);
    auditioning = -1;
}

void ZoneMapView::timerCallback()
{
    releaseAudition();
}

void ZoneMapView::mouseDown (const juce::MouseEvent& e)
{
    const auto h = hitAt (e.position);
    if (h.key >= 0)
        cursor = h;
    press (h);
}

void ZoneMapView::mouseUp (const juce::MouseEvent&)
{
    releaseAudition();
}

void ZoneMapView::mouseMove (const juce::MouseEvent& e)
{
    setTooltip (describe (hitAt (e.position)));
}

bool ZoneMapView::keyPressed (const juce::KeyPress& k)
{
    const auto bs = processor.currentBuildSettings();
    if (k.isKeyCode (juce::KeyPress::leftKey))       cursor.key = juce::jmax (0, cursor.key - 1);
    else if (k.isKeyCode (juce::KeyPress::rightKey)) cursor.key = juce::jmin (127, cursor.key + 1);
    else if (k.isKeyCode (juce::KeyPress::upKey))    cursor.layer = juce::jmin (bs.numLayers - 1, cursor.layer + 1);
    else if (k.isKeyCode (juce::KeyPress::downKey))  cursor.layer = juce::jmax (0, cursor.layer - 1);
    else if (k.isKeyCode (juce::KeyPress::spaceKey) || k.isKeyCode (juce::KeyPress::returnKey))
    {
        cursor.layer = juce::jlimit (0, bs.numLayers - 1, cursor.layer);
        press (cursor);
        if (auditioning >= 0)
            startTimer (700);
    }
    else
        return false;
    setTooltip (describe (cursor));
    repaint();
    return true;
}

// ============================================================================ keyboard

Keyboard::Keyboard (IVIProcessor& p, juce::MidiKeyboardState& s)
    : juce::MidiKeyboardComponent (s, juce::MidiKeyboardComponent::horizontalKeyboard), processor (p)
{
    setAvailableRange (21, 108);
    setOctaveForMiddleC (4);
    setScrollButtonsVisible (false);
    setMouseClickGrabsKeyboardFocus (false);
    setTitle ("Keyboard");
    setColour (shadowColourId, juce::Colours::transparentBlack);
    setColour (keySeparatorLineColourId, ink);
    setColour (whiteNoteColourId, control);
    setColour (blackNoteColourId, ink);
}

bool Keyboard::isKeyswitch (int note) const
{
    const auto es = processor.currentEngineSettings();
    return es.keyswitchesEnabled && note >= es.keyswitchBase && note < es.keyswitchBase + kNumArticulations;
}

bool Keyboard::isCurrentKeyswitch (int note) const
{
    return isKeyswitch (note) && note - processor.currentEngineSettings().keyswitchBase == processor.getEngine().getArticulation();
}

bool Keyboard::isMapped (int note) const
{
    const auto bs = processor.currentBuildSettings();
    return note >= bs.lowKey && note <= bs.highKey;
}

void Keyboard::paint (juce::Graphics& g)
{
    juce::MidiKeyboardComponent::paint (g);
    focusRing (g, *this, getLocalBounds().toFloat(), 2.0f);
}

void Keyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                              juce::Colour, juce::Colour)
{
    const bool ks = isKeyswitch (note);
    juce::Colour fill = isMapped (note) || ks ? control : thumb;
    if (isOver) fill = controlHover;
    if (isDown || isCurrentKeyswitch (note)) fill = accent;
    g.setColour (fill);
    g.fillRect (area);
    g.setColour (ink);
    g.fillRect (area.withWidth (1.0f).withX (area.getRight() - 1.0f));
    if (ks)
    {
        // a keyswitch carries a notch at the top, so it isn't told by colour alone
        juce::Path notch;
        const float cx = area.getCentreX(), top = area.getY();
        notch.addTriangle (cx - 5.0f, top, cx + 5.0f, top, cx, top + 7.0f);
        g.fillPath (notch);
    }
    if (note % 12 == 0 && area.getWidth() >= textWidth ("C10"))
    {
        g.setFont (font());
        g.drawText (noteName (note), area.withTrimmedBottom (4.0f).removeFromBottom (18.0f), juce::Justification::centred);
    }
}

void Keyboard::drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    juce::Colour fill = isMapped (note) || isKeyswitch (note) ? ink : rule;
    if (isOver) fill = popup;
    if (isDown || isCurrentKeyswitch (note)) fill = accent;
    g.setColour (fill);
    g.fillRect (area);
    g.setColour (ink);
    g.drawRect (area, 1.0f);
    if (isKeyswitch (note))
    {
        juce::Path notch;
        const float cx = area.getCentreX(), top = area.getY() + 2.0f;
        notch.addTriangle (cx - 4.0f, top, cx + 4.0f, top, cx, top + 6.0f);
        g.setColour (isCurrentKeyswitch (note) ? ink : control);
        g.fillPath (notch);
    }
}

} // namespace ivi::ui
