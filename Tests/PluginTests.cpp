// Smoke test of the real plugin processor + editor (state round trip, MIDI, rebuilds).
#include "Plugin/PluginProcessor.h"
#include <cstdio>

namespace
{
    int failures = 0;
    void check (bool ok, const char* what)
    {
        std::printf ("  %s %s\n", ok ? "ok  " : "FAIL", what);
        if (! ok) ++failures;
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

        // editor opens, updates and closes cleanly
        onMessageThread ([&]
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditorIfNeeded());
            check (ed != nullptr && ed->getWidth() > 800, "editor created");
            auto img = ed->createComponentSnapshot (ed->getLocalBounds());
            check (img.isValid(), "editor paints");
            const auto shot = juce::SystemStats::getEnvironmentVariable ("IVI_SNAPSHOT", {});
            if (shot.isNotEmpty())
            {
                juce::File f (shot);
                f.deleteFile();
                if (auto out = f.createOutputStream())
                    juce::PNGImageFormat().writeImageToStream (img, *out);
            }
            img = juce::Image();
            proc->editorBeingDeleted (ed.get());
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

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::thread worker ([]
    {
        runTests();
        std::printf ("%s\n", failures == 0 ? "ALL PASSED" : "FAILURES");
        juce::MessageManager::callAsync ([] { juce::MessageManager::getInstance()->stopDispatchLoop(); });
    });
    juce::MessageManager::getInstance()->runDispatchLoop();
    worker.join();
    return failures == 0 ? 0 : 1;
}
