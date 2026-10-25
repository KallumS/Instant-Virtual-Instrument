#pragma once

#include "ResynthVoice.h"
#include <atomic>
#include <vector>

namespace ivi
{

/** Polyphonic engine: MIDI handling, keyswitches, round-robin selection,
    legato, sustain pedal, voice allocation and a lock-free instrument swap. */
class SynthEngine
{
public:
    static constexpr int kMaxVoices = 64;

    SynthEngine();
    ~SynthEngine();

    void prepare (double sampleRate, int maxBlockSize);

    // ---- message thread -------------------------------------------------
    /** Hands a freshly built instrument to the audio thread (picked up at the next block). */
    void setInstrument (std::unique_ptr<Instrument> instrument);
    /** Frees instruments the audio thread no longer uses. Call periodically. */
    void collectGarbage();

    // ---- audio thread -----------------------------------------------------
    void setSettings (const EngineSettings& s) noexcept { settings = s; }
    void setArticulation (int index) noexcept;
    void process (juce::AudioBuffer<float>& output, const juce::MidiBuffer& midi) noexcept;
    void allNotesOff (bool immediate) noexcept;

    // ---- any thread (UI) --------------------------------------------------
    int getArticulation() const noexcept { return articulationAtomic.load(); }
    int getActiveVoiceCount() const noexcept { return activeVoicesAtomic.load(); }
    uint16_t getKeyLayerMask (int key) const noexcept { return keyLayerMask[(size_t) key].load (std::memory_order_relaxed); }
    int getKeyRoundRobin (int key) const noexcept { return keyRoundRobin[(size_t) key].load (std::memory_order_relaxed); }
    int getLastKeyswitchNote() const noexcept { return lastKeyswitch.load(); }

private:
    void handleMidi (const juce::MidiMessage& msg) noexcept;
    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void releaseNote (int note) noexcept;
    void renderVoices (float* left, float* right, int numSamples) noexcept;
    void swapInstrumentIfPending() noexcept;
    int nextRoundRobin (int note, int layer) noexcept;
    ResynthVoice* allocateVoice() noexcept;
    void publishActivity() noexcept;

    double sampleRate = 48000.0;
    EngineSettings settings;
    std::vector<ResynthVoice> voices;
    juce::AudioBuffer<float> mixBuffer;

    Instrument* current = nullptr;            // audio thread owned
    Instrument* previous = nullptr;           // fading out
    std::atomic<Instrument*> pending { nullptr };
    std::atomic<Instrument*> trash { nullptr };

    int articulation = 0;
    float modWheel = 1.0f, modWheelTarget = 1.0f;
    float expressionTarget = 1.0f;
    float bendSemis = 0.0f;
    float bendRaw = 0.0f;
    float masterGain = 1.0f;
    bool sustainPedal = false;
    bool keyDown[128] {};
    bool sustained[128] {};
    std::vector<int> legatoStack;
    uint64_t ageCounter = 0;

    uint8_t rrCounter[128][kMaxLayers] {};
    uint8_t rrLast[128][kMaxLayers] {};
    Rng rrRng { 0x5EEDull };

    std::atomic<int> articulationAtomic { 0 };
    std::atomic<int> activeVoicesAtomic { 0 };
    std::atomic<int> lastKeyswitch { -1 };
    std::array<std::atomic<uint16_t>, 128> keyLayerMask {};
    std::array<std::atomic<uint8_t>, 128> keyRoundRobin {};
};

} // namespace ivi
