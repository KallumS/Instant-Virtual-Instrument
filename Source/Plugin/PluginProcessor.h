#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Core/SynthEngine.h"
#include "Core/SpectralAnalyzer.h"
#include "Core/MultisampleExporter.h"
#include "Parameters.h"
#include <atomic>
#include <mutex>

class IVIProcessor : public juce::AudioProcessor,
                     public juce::ChangeBroadcaster,
                     private juce::AudioProcessorValueTreeState::Listener,
                     private juce::AsyncUpdater,
                     private juce::Timer
{
public:
    IVIProcessor();
    ~IVIProcessor() override;

    // ---- AudioProcessor ------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- editor API (message thread) -----------------------------------------
    bool loadSampleFile (const juce::File& file, juce::String& error);
    void loadDemo();
    void requestArticulation (int index);

    std::shared_ptr<const ivi::SpectralModel> getModel() const;
    juce::String getSampleName() const;
    bool isAnalysing() const noexcept { return analysing.load(); }
    float getAnalysisProgress() const noexcept { return analysisProgress.load(); }
    juce::String getStatus() const;

    void startExport (const juce::File& folder, bool allArticulations);
    void cancelExport() { exportCancel = true; }
    bool isExporting() const noexcept { return exporting.load(); }
    float getExportProgress() const noexcept { return exportProgress.load(); }

    ivi::BuildSettings currentBuildSettings() const;
    ivi::EngineSettings currentEngineSettings() const;
    const ivi::SynthEngine& getEngine() const noexcept { return engine; }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    static const juce::StringArray& structuralParams();

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;

    void setSourceAudio (juce::AudioBuffer<float> mono, double sampleRate, const juce::String& name);
    void startAnalysis();
    void rebuildInstrument();
    void setStatus (const juce::String& s);

    ivi::SynthEngine engine;

    // source + model (message thread writes; state save may read from another thread)
    mutable std::mutex modelMutex;
    juce::AudioBuffer<float> sourceAudio;
    double sourceRate = 44100.0;
    juce::String sampleName;
    std::shared_ptr<const ivi::SpectralModel> model;

    juce::ThreadPool workers { 1 };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::atomic<int> analysisGeneration { 0 };
    std::atomic<bool> analysing { false };
    std::atomic<float> analysisProgress { 0.0f };
    std::atomic<bool> needsReanalysis { false }, needsRebuild { false };

    std::atomic<bool> exporting { false }, exportCancel { false };
    std::atomic<float> exportProgress { 0.0f };

    mutable std::mutex statusMutex;
    juce::String status { "Drop a sample (or press Demo) to build an instrument" };

    std::atomic<int> pendingArticulation { -1 };
    int lastArticulationParam = -1;

    // cached raw parameter pointers for the audio thread
    struct Raw
    {
        std::atomic<float>* formant {}, *brightness {}, *sourceDynamic {}, *dynRange {}, *noise {}, *attack {}, *release {},
                          *glide {}, *pitchDrift {}, *tremoloRate {}, *width {}, *gain {}, *bendRange {}, *velocityBlend {},
                          *dynSource {}, *rrMode {}, *sustainMode {}, *polyphony {}, *partials {}, *ksBase {}, *keyswitches {},
                          *articulation {};
    } raw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IVIProcessor)
};
