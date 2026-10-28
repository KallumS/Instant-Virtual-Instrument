// Smoke test of the real plugin processor + editor (state round trip, MIDI, rebuilds).
#include "Plugin/PluginProcessor.h"
#include "Plugin/PluginEditor.h"
#include "Plugin/Theme.h"
#include <cstdio>
#include <atomic>
#include <chrono>
#include <thread>

namespace
{
    int failures = 0;
    void check (bool ok, const char* what)
    {
        std::printf ("  %s %s\n", ok ? "ok  " : "FAIL", what);
        if (! ok) ++failures;
    }

    void checkf (bool ok, const juce::String& what)
    {
        if (! ok) { std::printf ("  FAIL %s\n", what.toRawUTF8()); ++failures; }
    }

    // ---------------------------------------------------------------- Apple's guidelines, as the family checks them
    void checkColours()
    {
        using namespace ivi::theme;
        bool blue = true;
        for (auto c : greys())
            blue &= c.getRed() < c.getGreen() && c.getGreen() < c.getBlue();
        check (blue, "every grey is blue-shifted (R < G < B)");

        bool words = true;
        for (auto t : { text, textDim, label, warnText })
            for (auto bg : { ground, roll })
                words &= contrast (t, bg) >= 7.0;
        for (auto t : { text, label })   // what menus and tooltips are written in
            words &= contrast (t, popup) >= 7.0;
        check (words, "every text colour is 7:1 where it is used: the ground, the views, menus");

        bool inkOk = true;
        for (auto bg : { control, controlHover, accent, shade (accent, 0.18f), shade (accent, -0.18f) })
            inkOk &= contrast (ink, bg) >= 7.0;
        check (inkOk, "the ink is 7:1 on every button at rest and under the pointer");
        // the house's held grey is 6:1: above Apple's 4.5:1 minimum, shown only while pressed
        check (contrast (ink, controlHeld) >= 4.5, "the ink is 4.5:1 on a button being pressed");
        check (contrast (edge, ground) >= 3.0 && contrast (edge, sunken) >= 3.0, "edges of fields and tracks are 3:1");
        check (contrast (thumb, sunken) >= 3.0, "the scrollbar thumb is 3:1 on its track");
        check (focus != accent && contrast (focus, ground) >= 7.0, "the focus ring is near-white, not the yellow");
        check (contrast (warn, ground) < 4.5 && contrast (warnText, ground) >= 7.0, "the house red marks; warnings are written in the lighter red");
    }

    bool isSmallWord (const juce::String& w)
    {
        return juce::StringArray { "a", "an", "the", "of", "to", "for", "in", "on", "at", "and", "or", "by", "as" }.contains (w);
    }

    /** Walks every visible component of the window. */
    void forEachVisible (juce::Component& c, const std::function<void (juce::Component&)>& fn)
    {
        for (auto* child : c.getChildren())
            if (child->isVisible())
            {
                fn (*child);
                forEachVisible (*child, fn);
            }
    }

    // Apple's numbers, held here rather than read from the theme, so a change to the theme can't move them
    constexpr int appleControlHeight = 28, appleGap = 12;

    void checkWindow (IVIEditor& ed, const juce::String& when)
    {
        auto& content = ed.getContent();
        int problems = 0;
        auto fail = [&] (const juce::String& what) { if (problems++ < 20) checkf (false, when + ": " + what); };

        forEachVisible (content, [&] (juce::Component& c)
        {
            const auto name = c.getTitle().isNotEmpty() ? c.getTitle() : c.getName();
            auto* parent = c.getParentComponent();
            if (parent != nullptr && ! parent->getLocalBounds().contains (c.getBounds()))
                fail ("runs off its panel: " + juce::String (typeid (c).name()) + " " + name + " " + c.getBounds().toString());

            const bool control = dynamic_cast<juce::Button*> (&c) != nullptr || dynamic_cast<juce::ComboBox*> (&c) != nullptr
                              || dynamic_cast<juce::Slider*> (&c) != nullptr;
            if (control)
            {
                if (! c.getWantsKeyboardFocus())          fail ("not reachable from the keyboard: " + name);
                if (c.getMouseClickGrabsKeyboardFocus())  fail ("a click takes the keyboard: " + name);
                auto* tipper = dynamic_cast<juce::SettableTooltipClient*> (&c);
                const auto tip = tipper != nullptr ? tipper->getTooltip() : juce::String();
                if (tip.isEmpty())                        fail ("no tooltip: " + name);
                else if (! tip.trimEnd().endsWithChar ('.')) fail ("tooltip is not a sentence: " + tip);
                for (auto& line : juce::StringArray::fromLines (ivi::theme::lines (tip)))
                    if (line.length() > ivi::theme::tooltipLine) fail ("tooltip line over 64 characters: " + line);
            }
            if (dynamic_cast<juce::Button*> (&c) != nullptr || dynamic_cast<juce::ComboBox*> (&c) != nullptr)
                if (c.getHeight() < appleControlHeight) fail ("shorter than 28 points: " + name);

            if (auto* b = dynamic_cast<juce::TextButton*> (&c))
            {
                const auto t = b->getButtonText();
                if (b->getWidth() < ivi::theme::textWidth (t) + 16.0f) fail ("narrower than its label: " + t);
                if (t.contains ("...")) fail ("three full stops instead of an ellipsis: " + t);
                auto words = juce::StringArray::fromTokens (t, " ()", {});
                for (int i = 0; i < words.size(); ++i)
                    if (words[i].isNotEmpty() && (i == 0 || ! isSmallWord (words[i])) && juce::CharacterFunctions::isLowerCase (words[i][0]))
                        fail ("not in title style: " + t);
            }
            if (auto* l = dynamic_cast<juce::Label*> (&c))
            {
                if (l->getFont().getHeight() < ivi::theme::textPt - 0.01f) fail ("text under 13 points: " + l->getText());
                const auto col = l->findColour (juce::Label::textColourId);
                const auto bg = l->findColour (juce::Label::backgroundColourId);
                // a combo box's own text sits on the control's grey
                const auto under = bg.getAlpha() > 0 ? bg
                                 : (dynamic_cast<juce::ComboBox*> (l->getParentComponent()) != nullptr ? ivi::theme::control : ivi::theme::ground);
                if (l->getText().isNotEmpty() && ivi::theme::contrast (col, under) < 7.0) fail ("label under 7:1: " + l->getText());
            }
        });

        // text the views draw themselves: inside the view, and never over other text
        forEachVisible (content, [&] (juce::Component& c)
        {
            if (auto* d = dynamic_cast<ivi::ui::DrawsText*> (&c))
            {
                auto boxes = d->textBoxes();
                boxes.removeIf ([] (const juce::Rectangle<float>& r) { return r.isEmpty(); });
                for (int i = 0; i < boxes.size(); ++i)
                {
                    if (! c.getLocalBounds().toFloat().contains (boxes[i])) fail ("text runs off a view: " + c.getTitle() + " " + boxes[i].toString());
                    for (int j = i + 1; j < boxes.size(); ++j)
                        if (boxes[i].intersects (boxes[j])) fail ("text drawn over text in a view: " + boxes[i].toString() + " / " + boxes[j].toString());
                }
            }
        });

        // controls in a row sit at least 12 points apart
        std::vector<juce::Component*> buttons;
        forEachVisible (content, [&] (juce::Component& c)
        {
            if (dynamic_cast<juce::Button*> (&c) != nullptr || dynamic_cast<juce::ComboBox*> (&c) != nullptr)
                buttons.push_back (&c);
        });
        for (size_t i = 0; i < buttons.size(); ++i)
            for (size_t j = i + 1; j < buttons.size(); ++j)
            {
                if (buttons[i]->getParentComponent() != buttons[j]->getParentComponent()) continue;
                auto a = buttons[i]->getBounds(), b = buttons[j]->getBounds();
                if (a.getY() >= b.getBottom() || b.getY() >= a.getBottom()) continue;   // different rows
                const int apart = a.getX() < b.getX() ? b.getX() - a.getRight() : a.getX() - b.getRight();
                if (apart < appleGap) fail ("closer than 12 points: " + buttons[i]->getTitle() + buttons[i]->getName() + " / " + buttons[j]->getName());
            }

        check (problems == 0, (when + ": every control fits, is 28 points, 12 apart, reachable by keyboard, with a sentence for a tooltip").toRawUTF8());
    }

    template <typename Fn>
    void onMessageThread (Fn&& fn)
    {
        juce::WaitableEvent done;
        juce::MessageManager::callAsync ([&] { fn(); done.signal(); });
        done.wait();
    }

    bool waitFor (std::function<bool()> cond, int ms = 20000)
    {
        for (int t = 0; t < ms; t += 20)
        {
            bool ok = false;
            onMessageThread ([&] { ok = cond(); });
            if (ok) return true;
            juce::Thread::sleep (20);
        }
        return false;
    }

    float render (IVIProcessor& p, juce::MidiBuffer midi, int blocks)
    {
        juce::AudioBuffer<float> buf (2, 512);
        float peak = 0.0f;
        for (int i = 0; i < blocks; ++i)
        {
            p.processBlock (buf, midi);
            midi.clear();
            peak = std::max (peak, buf.getMagnitude (0, 512));
            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < 512; ++s)
                    if (! std::isfinite (buf.getSample (c, s))) return -1.0f;
        }
        return peak;
    }

    void runTests()
    {
        std::printf ("Plugin smoke test\n");
        std::unique_ptr<IVIProcessor> proc;
        onMessageThread ([&] { proc = std::make_unique<IVIProcessor>(); });
        proc->setPlayConfigDetails (0, 2, 48000.0, 512);
        proc->prepareToPlay (48000.0, 512);

        check (render (*proc, {}, 4) == 0.0f, "silent before a sample is loaded");

        onMessageThread ([&] { proc->loadDemo(); });
        check (waitFor ([&] { return proc->getModel() != nullptr && ! proc->isAnalysing(); }), "demo sample analysed");
        render (*proc, {}, 2);   // picks up the instrument

        juce::MidiBuffer on;
        on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        on.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 60), 10);
        const float peak = render (*proc, on, 40);
        check (peak > 0.01f && peak < 1.0f, "plays a chord (finite, no clipping)");

        checkColours();

        // the window, against Apple's guidelines
        onMessageThread ([&]
        {
            std::unique_ptr<juce::AudioProcessorEditor> base (proc->createEditorIfNeeded());
            auto* ed = dynamic_cast<IVIEditor*> (base.get());
            check (ed != nullptr && ed->getWidth() >= IVIContent::minimumWidth, "editor created");
            if (ed == nullptr) return;
            ed->setTextSize (100);
            ed->setSize (IVIContent::preferredWidth, 900);
            proc->dismissMessage();
            checkWindow (*ed, "100%");

            // the message sits at the top, with Undo and Dismiss
            proc->makeNewTakes();
            auto& c = ed->getContent();
            c.refreshFromProcessor();   // what the change message does
            check (c.undoButton.isVisible() && c.dismissButton.isVisible() && c.undoButton.getY() < c.waveform.getY(),
                   "a change that replaces the takes leaves Undo in the message, at the top");
            checkWindow (*ed, "100% with a message");
            const int seedAfter = (int) proc->apvts.getRawParameterValue (ivi::params::seed)->load();
            proc->undo();
            check ((int) proc->apvts.getRawParameterValue (ivi::params::seed)->load() != seedAfter, "Undo brings the old takes back");
            c.refreshFromProcessor();
            proc->loadSampleFile (juce::File ("/no/such/file.wav"));
            c.refreshFromProcessor();
            check (proc->getMessage().warning && proc->getMessage().text.contains ("Choose"), "a file that can't be read says what to do");
            check (! c.undoButton.isVisible() && c.dismissButton.isVisible(), "a warning stays until Dismiss");
            c.dismissButton.onClick();
            c.refreshFromProcessor();
            check (! c.dismissButton.isVisible(), "Dismiss clears the message");

            // a chosen articulation carries an ink bar, not the yellow alone
            proc->requestArticulation (2);
            {
                juce::AudioBuffer<float> b (2, 64); juce::MidiBuffer mb; proc->processBlock (b, mb);
            }
            c.tick();
            auto* chosen = c.articulationButtons[2];
            auto shot = chosen->createComponentSnapshot (chosen->getLocalBounds(), true, 1.0f);
            int inkBelow = 0;
            const int barY = chosen->getHeight() / 2 + 8;
            for (int yy = barY; yy < juce::jmin (chosen->getHeight() - 3, barY + 4); ++yy)
                for (int xx = 8; xx < chosen->getWidth() - 8; ++xx)
                    if (shot.getPixelAt (xx, yy).getBrightness() < 0.2f) ++inkBelow;
            check (chosen->getToggleState() && inkBelow > 20, "the chosen articulation carries an ink bar under its label");

            // Text Size: the whole window grows, nothing runs off, and it is remembered
            ed->keyPressed (juce::KeyPress ('=', juce::ModifierKeys::commandModifier, '='));
            check (ed->getTextSize() == 125, "Cmd+= makes the text larger");
            ed->setTextSize (200);
            ed->setSize (1600, 1000);
            check (c.getTransform().getScaleFactor() > 1.9f, "200% scales the window");
            checkWindow (*ed, "200%");
            ed->setSize (IVIContent::minimumWidth * 2, 800);
            checkWindow (*ed, "200% at the smallest width");
            ed->keyPressed (juce::KeyPress ('0', juce::ModifierKeys::commandModifier, '0'));
            check (ed->getTextSize() == 100, "Cmd+0 goes back to 100%");
            ed->setSize (IVIContent::minimumWidth, 600);
            checkWindow (*ed, "100% at the smallest width");
            ed->setSize (IVIContent::preferredWidth, 900);

            auto img = ed->createComponentSnapshot (ed->getLocalBounds());
            check (img.isValid(), "editor paints");
            const auto shotPath = juce::SystemStats::getEnvironmentVariable ("IVI_SNAPSHOT", {});
            if (shotPath.isNotEmpty())
            {
                juce::File f (shotPath);
                f.deleteFile();
                if (auto out = f.createOutputStream())
                    juce::PNGImageFormat().writeImageToStream (img, *out);
                ed->setTextSize (200);
                ed->setSize (IVIContent::minimumWidth * 2, 1400);
                auto big = ed->createComponentSnapshot (ed->getLocalBounds());
                juce::File f2 (shotPath.upToLastOccurrenceOf (".", false, false) + "-200.png");
                f2.deleteFile();
                if (auto out = f2.createOutputStream())
                    juce::PNGImageFormat().writeImageToStream (big, *out);
                ed->setTextSize (100);
            }
            proc->editorBeingDeleted (ed);
        });

        // structural change triggers a rebuild while playing
        onMessageThread ([&]
        {
            auto* p = proc->apvts.getParameter (ivi::params::layers);
            p->setValueNotifyingHost (p->convertTo0to1 (3.0f));
        });
        juce::Thread::sleep (200);
        check (render (*proc, on, 20) > 0.0f, "plays after a layer-count rebuild");

        // state round trip into a new instance
        juce::MemoryBlock state;
        proc->getStateInformation (state);
        check (state.getSize() > 10000, "state contains the embedded sample");

        std::unique_ptr<IVIProcessor> second;
        onMessageThread ([&] { second = std::make_unique<IVIProcessor>(); });
        second->setPlayConfigDetails (0, 2, 48000.0, 512);
        second->prepareToPlay (48000.0, 512);
        onMessageThread ([&] { second->setStateInformation (state.getData(), (int) state.getSize()); });
        check (waitFor ([&] { return second->getModel() != nullptr && ! second->isAnalysing(); }), "restored instance re-analyses the sample");
        check (second->currentBuildSettings().numLayers == 3, "restored parameters");
        render (*second, {}, 2);
        check (render (*second, on, 20) > 0.01f, "restored instance plays");

        onMessageThread ([&] { proc.reset(); second.reset(); });
    }
}

/** Runs the tests the way JUCE runs any app (on the Mac that sets up NSApplication,
    which a hand-made message loop did not, and the test hung there for six hours on
    CI). The tests run on a worker thread and hand window work to the message thread. */
class PluginTestApp final : public juce::JUCEApplicationBase
{
public:
    const juce::String getApplicationName() override    { return "IVIPluginTests"; }
    const juce::String getApplicationVersion() override { return "1"; }
    bool moreThanOneInstanceAllowed() override          { return true; }
    void anotherInstanceStarted (const juce::String&) override {}
    void systemRequestedQuit() override                 { quit(); }
    void suspended() override {}
    void resumed() override {}
    void unhandledException (const std::exception*, const juce::String&, int) override {}

    void initialise (const juce::String&) override
    {
        worker = std::thread ([this]
        {
            runTests();
            std::printf ("%s\n", failures == 0 ? "ALL PASSED" : "FAILURES");
            done = true;
            juce::MessageManager::callAsync ([this]
            {
                setApplicationReturnValue (failures == 0 ? 0 : 1);
                quit();
            });
        });

        // a watchdog: a hang says so and fails in minutes, not hours
        watchdog = std::thread ([this]
        {
            for (int s = 0; s < 300 && ! done; ++s)
                std::this_thread::sleep_for (std::chrono::seconds (1));
            if (! done)
            {
                std::printf ("FAIL the plugin test hung (5 minutes); the last line above is the last check that finished\n");
                std::_Exit (3);
            }
        });
    }

    void shutdown() override
    {
        if (worker.joinable())   worker.join();
        if (watchdog.joinable()) watchdog.join();
    }

private:
    std::thread worker, watchdog;
    std::atomic<bool> done { false };
};

static juce::JUCEApplicationBase* createTestApp() { return new PluginTestApp(); }

int main (int argc, char* argv[])
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);   // show progress at once, even under CI
    juce::JUCEApplicationBase::createInstance = &createTestApp;
   #if JUCE_WINDOWS && ! defined (_CONSOLE)
    juce::ignoreUnused (argc, argv);
    return juce::JUCEApplicationBase::main();
   #else
    return juce::JUCEApplicationBase::main (argc, (const char**) argv);
   #endif
}
