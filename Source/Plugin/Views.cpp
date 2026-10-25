#include "Views.h"
#include "LookAndFeel.h"
#include "PluginProcessor.h"
#include "Core/Articulations.h"

namespace ivi::ui
{

namespace
{
    void drawPanel (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (colours::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }

    void drawTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
    {
        g.setColour (colours::dim);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (text.toUpperCase(), area.removeFromTop (18).reduced (10, 0), juce::Justification::centredLeft);
    }
}

// ============================================================================ waveform

void WaveformView::refresh()
{
    auto m = processor.getModel();
    if (m != shown)
    {
        shown = m;
        peaks.clear();
    }
    repaint();
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawPanel (g, bounds);
    drawTitle (g, getLocalBounds(), "Source sample");

    auto area = getLocalBounds().reduced (10, 0).withTrimmedTop (22).withTrimmedBottom (8).toFloat();

    if (shown == nullptr)
    {
        g.setColour (colours::dim);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText (processor.isAnalysing() ? "Analysing..." : "Drop an audio file here, or press Demo",
                    area, juce::Justification::centred);
    }
    else
    {
        const auto& m = *shown;
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
        auto xFor = [&] (int frame) { return area.getX() + (float) (m.frameToSeconds (frame) / dur) * area.getWidth(); };

        // regions
        const float aX0 = xFor (m.onsetFrame), aX1 = xFor (m.attackEndFrame);
        const float lX0 = xFor (m.loopStartFrame), lX1 = xFor (m.loopEndFrame);
        g.setColour (colours::accent2.withAlpha (0.13f));
        g.fillRect (juce::Rectangle<float> (aX0, area.getY(), juce::jmax (1.0f, aX1 - aX0), area.getHeight()));
        g.setColour (colours::loop.withAlpha (m.decaying ? 0.08f : 0.16f));
        g.fillRect (juce::Rectangle<float> (lX0, area.getY(), juce::jmax (1.0f, lX1 - lX0), area.getHeight()));

        // waveform (normalised to the panel)
        float peak = 1.0e-6f;
        for (auto& p : peaks)
            peak = juce::jmax (peak, -p.first, p.second);
        const float mid = area.getCentreY(), half = area.getHeight() * 0.46f / peak;
        g.setColour (colours::accent.withAlpha (0.9f));
        for (int px = 0; px < w; ++px)
        {
            const auto& p = peaks[(size_t) px];
            g.drawVerticalLine ((int) area.getX() + px, mid - p.second * half, mid - p.first * half + 1.0f);
        }

        g.setFont (juce::FontOptions (11.0f));
        g.setColour (colours::accent2);
        g.drawVerticalLine ((int) aX1, area.getY(), area.getBottom());
        g.drawText ("attack", juce::Rectangle<float> (aX0 + 3.0f, area.getY(), 60.0f, 14.0f), juce::Justification::left);
        g.setColour (colours::loop.brighter (0.3f));
        g.drawVerticalLine ((int) lX0, area.getY(), area.getBottom());
        g.drawVerticalLine ((int) lX1, area.getY(), area.getBottom());
        g.drawText (m.decaying ? "loop (used when Sustain Mode = Loop)" : "sustain loop",
                    juce::Rectangle<float> (lX0 + 4.0f, area.getY(), 240.0f, 14.0f), juce::Justification::left);

        g.setColour (colours::dim);
        g.drawText (m.name + "   " + juce::String (dur, 2) + " s   " + juce::String (m.sampleRate / 1000.0, 1) + " kHz",
                    area.removeFromBottom (14.0f), juce::Justification::right);
    }

    if (processor.isAnalysing())
    {
        auto bar = area.withSizeKeepingCentre (juce::jmin (300.0f, area.getWidth() - 20.0f), 6.0f).translated (0.0f, 22.0f);
        g.setColour (colours::panelEdge);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (colours::accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * processor.getAnalysisProgress()), 3.0f);
    }
}

// ============================================================================ spectrum

void SpectrumView::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    drawTitle (g, getLocalBounds(), "Spectral model");

    auto m = processor.getModel();
    auto area = getLocalBounds().reduced (10, 0).withTrimmedTop (22).withTrimmedBottom (18).toFloat();
    if (m == nullptr)
        return;

    const int frame = juce::jlimit (0, m->numFrames - 1, (m->loopStartFrame + m->loopEndFrame) / 2);
    const float fLo = 30.0f, fHi = 20000.0f;
    auto xFor = [&] (float f) { return area.getX() + area.getWidth() * std::log (f / fLo) / std::log (fHi / fLo); };

    float maxDb = -200.0f;
    for (int h = 0; h < m->numPartials; ++h)
        maxDb = juce::jmax (maxDb, m->partialDb (frame, h));
    const float range = 90.0f;
    auto yFor = [&] (float dB) { return area.getBottom() - area.getHeight() * juce::jlimit (0.0f, 1.0f, (dB - (maxDb - range)) / range); };

    // grid
    g.setColour (colours::panelEdge);
    g.setFont (juce::FontOptions (10.0f));
    for (float f : { 100.0f, 1000.0f, 10000.0f })
    {
        g.drawVerticalLine ((int) xFor (f), area.getY(), area.getBottom());
        g.setColour (colours::dim.withAlpha (0.7f));
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (xFor (f) + 2.0f, area.getBottom() + 2.0f, 30.0f, 12.0f), juce::Justification::left);
        g.setColour (colours::panelEdge);
    }

    // noise residual
    juce::Path noise;
    noise.startNewSubPath (xFor (m->bandCentre[0]), area.getBottom());
    for (int b = 0; b < m->numBands; ++b)
    {
        const float a = m->noiseAmp[(size_t) frame * (size_t) m->numBands + (size_t) b];
        noise.lineTo (xFor (m->bandCentre[(size_t) b]), yFor (juce::Decibels::gainToDecibels (a, -200.0f)));
    }
    noise.lineTo (xFor (m->bandCentre[(size_t) m->numBands - 1]), area.getBottom());
    noise.closeSubPath();
    g.setColour (colours::accent2.withAlpha (0.18f));
    g.fillPath (noise);

    // partials
    const double f0 = m->f0Ref * m->f0Ratio[(size_t) frame];
    juce::Path body;
    bool started = false;
    for (int h = 0; h < m->numPartials; ++h)
    {
        const auto idx = (size_t) frame * (size_t) m->numPartials + (size_t) h;
        const float f = (float) (m->ratio[idx] * f0);
        if (f < fLo || f > fHi)
            continue;
        const float x = xFor (f);
        g.setColour (colours::accent);
        g.drawLine (x, area.getBottom(), x, yFor (m->partialDb (frame, h)), 1.6f);

        const float env = m->slopeA[(size_t) frame] + m->slopeB[(size_t) frame] * m->logRatio[idx] + m->envDb[idx];
        if (! started) { body.startNewSubPath (x, yFor (env)); started = true; }
        else           body.lineTo (x, yFor (env));
    }
    g.setColour (colours::text.withAlpha (0.6f));
    g.strokePath (body, juce::PathStrokeType (1.2f));

    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String (m->numPartials) + " partials   envelope   noise", area.removeFromTop (14.0f), juce::Justification::right);
}

// ============================================================================ zone map

juce::Rectangle<float> ZoneMapView::gridArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f, 0.0f).withTrimmedTop (36.0f).withTrimmedBottom (18.0f);
}

ZoneMapView::Hit ZoneMapView::hitTest (juce::Point<float> p) const
{
    const auto area = gridArea();
    if (! area.contains (p))
        return {};
    Hit h;
    h.key = juce::jlimit (0, 127, (int) ((p.x - area.getX()) / area.getWidth() * 128.0f));
    const auto bs = processor.currentBuildSettings();
    const float row = (p.y - area.getY()) / area.getHeight();
    h.layer = juce::jlimit (0, bs.numLayers - 1, bs.numLayers - 1 - (int) (row * (float) bs.numLayers));
    return h;
}

void ZoneMapView::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());

    const auto bs = processor.currentBuildSettings();
    const auto es = processor.currentEngineSettings();
    const auto& engine = processor.getEngine();
    const bool haveModel = processor.getModel() != nullptr;
    const int currentArt = engine.getArticulation();

    {
        juce::String title;
        title << "Generated library   " << (bs.highKey - bs.lowKey + 1) << " keys x " << bs.numLayers << " dynamic layers x "
              << bs.numRoundRobins << " round robins x " << kNumArticulations << " articulations = "
              << (bs.highKey - bs.lowKey + 1) * bs.numLayers * bs.numRoundRobins * kNumArticulations << " zones";
        drawTitle (g, getLocalBounds(), title);
    }

    const auto area = gridArea();
    const float kw = area.getWidth() / 128.0f;
    const int L = bs.numLayers, R = bs.numRoundRobins;
    const float rowH = area.getHeight() / (float) L;

    // octave labels
    g.setFont (juce::FontOptions (10.0f));
    for (int k = 0; k < 128; k += 12)
    {
        g.setColour (colours::dim.withAlpha (0.8f));
        g.drawText (juce::MidiMessage::getMidiNoteName (k, true, true, 4), juce::Rectangle<float> (area.getX() + k * kw, area.getY() - 15.0f, 30.0f, 12.0f),
                    juce::Justification::left);
    }

    for (int k = 0; k < 128; ++k)
    {
        const auto col = juce::Rectangle<float> (area.getX() + k * kw, area.getY(), kw, area.getHeight());
        const bool black = juce::MidiMessage::isMidiNoteBlack (k);
        g.setColour (black ? colours::background.darker (0.3f) : colours::background);
        g.fillRect (col);

        const bool ks = es.keyswitchesEnabled && k >= es.keyswitchBase && k < es.keyswitchBase + kNumArticulations;
        if (ks)
        {
            const bool cur = (k - es.keyswitchBase) == currentArt;
            g.setColour (colours::accent2.withAlpha (cur ? 0.95f : 0.3f));
            g.fillRect (col.reduced (0.5f, 0.0f));
            continue;
        }

        if (k < bs.lowKey || k > bs.highKey)
            continue;

        const uint16_t mask = haveModel ? engine.getKeyLayerMask (k) : 0;
        const int rr = engine.getKeyRoundRobin (k);
        for (int l = 0; l < L; ++l)
        {
            const auto cell = juce::Rectangle<float> (col.getX(), area.getY() + (float) (L - 1 - l) * rowH, kw, rowH).reduced (0.5f);
            const auto c = colours::forDynamic (((float) l + 0.5f) / (float) L);
            const bool on = (mask >> l) & 1u;
            g.setColour (c.withAlpha (haveModel ? (on ? 0.55f : 0.22f) : 0.08f));
            g.fillRect (cell);

            if (R > 1 && rowH / (float) R >= 3.0f)
            {
                const float sh = cell.getHeight() / (float) R;
                g.setColour (colours::background.withAlpha (0.35f));
                for (int r = 1; r < R; ++r)
                    g.fillRect (cell.getX(), cell.getY() + sh * (float) r - 0.5f, cell.getWidth(), 1.0f);
                if (on)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.9f));
                    g.fillRect (cell.getX(), cell.getY() + sh * (float) rr, cell.getWidth(), sh);
                }
            }
            else if (on)
            {
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.fillRect (cell);
            }
        }
    }

    // layer names
    g.setFont (juce::FontOptions (10.0f));
    for (int l = 0; l < L; ++l)
    {
        g.setColour (colours::text.withAlpha (0.75f));
        g.drawText (Instrument::layerName (l, L), juce::Rectangle<float> (area.getX() + bs.lowKey * kw + 3.0f, area.getY() + (float) (L - 1 - l) * rowH, 40.0f, rowH),
                    juce::Justification::centredLeft);
    }

    // legend
    auto legend = getLocalBounds().toFloat().reduced (10.0f, 0.0f).removeFromBottom (17.0f);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    juce::String ksText;
    if (es.keyswitchesEnabled)
        ksText << "Keyswitches " << juce::MidiMessage::getMidiNoteName (es.keyswitchBase, true, true, 4) << "-"
               << juce::MidiMessage::getMidiNoteName (es.keyswitchBase + kNumArticulations - 1, true, true, 4)
               << "  (now: " << articulationName ((Articulation) currentArt) << ")";
    g.drawText ("rows: dynamic layers   stripes: round robins   click a cell to audition", legend, juce::Justification::left);
    g.setColour (colours::accent2);
    g.drawText (ksText, legend, juce::Justification::right);
}

void ZoneMapView::mouseDown (const juce::MouseEvent& e)
{
    const auto h = hitTest (e.position);
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
    auditioning = h.key;
    const float vel = juce::jlimit (0.01f, 1.0f, ((float) h.layer + 0.5f) / (float) bs.numLayers);
    processor.keyboardState.noteOn (1, h.key, vel);
}

void ZoneMapView::mouseUp (const juce::MouseEvent&)
{
    if (auditioning >= 0)
        processor.keyboardState.noteOff (1, auditioning, 0.0f);
    auditioning = -1;
}

void ZoneMapView::mouseMove (const juce::MouseEvent& e)
{
    const auto h = hitTest (e.position);
    juce::String tip;
    if (h.key >= 0)
    {
        const auto es = processor.currentEngineSettings();
        const auto bs = processor.currentBuildSettings();
        const auto name = juce::MidiMessage::getMidiNoteName (h.key, true, true, 4);
        if (es.keyswitchesEnabled && h.key >= es.keyswitchBase && h.key < es.keyswitchBase + kNumArticulations)
            tip = "Keyswitch " + name + ": " + articulationName ((Articulation) (h.key - es.keyswitchBase));
        else if (h.key >= bs.lowKey && h.key <= bs.highKey)
            tip = name + "  |  layer " + Instrument::layerName (h.layer, bs.numLayers) + " (" + juce::String (h.layer + 1) + "/"
                + juce::String (bs.numLayers) + ")  |  " + juce::String (bs.numRoundRobins) + " round robins";
        else
            tip = name + " (outside the mapped range)";
    }
    setTooltip (tip);
}

// ============================================================================ keyboard

Keyboard::Keyboard (IVIProcessor& p, juce::MidiKeyboardState& s)
    : juce::MidiKeyboardComponent (s, juce::MidiKeyboardComponent::horizontalKeyboard), processor (p)
{
    setAvailableRange (21, 108);
    setOctaveForMiddleC (4);
    setColour (keyDownOverlayColourId, colours::accent.withAlpha (0.7f));
    setColour (mouseOverKeyOverlayColourId, colours::accent.withAlpha (0.25f));
}

bool Keyboard::isKeyswitch (int note) const
{
    const auto es = processor.currentEngineSettings();
    return es.keyswitchesEnabled && note >= es.keyswitchBase && note < es.keyswitchBase + kNumArticulations;
}

bool Keyboard::isMapped (int note) const
{
    const auto bs = processor.currentBuildSettings();
    return note >= bs.lowKey && note <= bs.highKey;
}

void Keyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                              juce::Colour lineColour, juce::Colour textColour)
{
    juce::MidiKeyboardComponent::drawWhiteNote (note, g, area, isDown, isOver, lineColour, textColour);
    if (isKeyswitch (note))
    {
        g.setColour (colours::accent2.withAlpha (note - processor.currentEngineSettings().keyswitchBase == processor.getEngine().getArticulation() ? 0.75f : 0.35f));
        g.fillRect (area.reduced (1.0f, 0.0f));
    }
    else if (! isMapped (note))
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (area);
    }
}

void Keyboard::drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                              juce::Colour noteFillColour)
{
    juce::MidiKeyboardComponent::drawBlackNote (note, g, area, isDown, isOver, noteFillColour);
    if (isKeyswitch (note))
    {
        g.setColour (colours::accent2.withAlpha (0.55f));
        g.fillRect (area.reduced (1.0f));
    }
}

} // namespace ivi::ui
