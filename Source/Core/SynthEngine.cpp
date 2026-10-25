#include "SynthEngine.h"
#include <algorithm>
#include <cstring>

namespace ivi
{

SynthEngine::SynthEngine()
{
    voices.resize (kMaxVoices);
    legatoStack.reserve (128);
    std::memset (rrLast, 0xff, sizeof (rrLast));
}

SynthEngine::~SynthEngine()
{
    delete current;
    delete previous;
    delete pending.exchange (nullptr);
    delete trash.exchange (nullptr);
}

void SynthEngine::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    for (auto& v : voices)
        v.prepare (sr);
    mixBuffer.setSize (2, std::max (16, maxBlockSize));
    modWheel = modWheelTarget;
}

void SynthEngine::setInstrument (std::unique_ptr<Instrument> instrument)
{
    delete pending.exchange (instrument.release());
}

void SynthEngine::collectGarbage()
{
    delete trash.exchange (nullptr);
}

void SynthEngine::setArticulation (int index) noexcept
{
    articulation = juce::jlimit (0, kNumArticulations - 1, index);
    articulationAtomic.store (articulation);
}

void SynthEngine::swapInstrumentIfPending() noexcept
{
    if (previous != nullptr)
    {
        bool inUse = false;
        for (auto& v : voices)
            if (v.isActive() && v.getInstrument() == previous)
                inUse = true;

        if (inUse)
            return;

        Instrument* expected = nullptr;
        if (! trash.compare_exchange_strong (expected, previous))
            return;   // message thread hasn't collected the last one yet
        previous = nullptr;
    }

    if (auto* next = pending.exchange (nullptr))
    {
        previous = current;
        current = next;

        for (auto& v : voices)
            if (v.isActive() && v.getInstrument() != current)
                v.kill();

        std::memset (rrCounter, 0, sizeof (rrCounter));
        std::memset (rrLast, 0xff, sizeof (rrLast));
        legatoStack.clear();
    }
}

int SynthEngine::nextRoundRobin (int note, int layer) noexcept
{
    const int count = current->settings.numRoundRobins;
    if (count <= 1)
        return 0;

    int rr;
    if (settings.rrMode == 1)
    {
        rr = (int) (rrRng.uniform() * (float) (count - 1));
        if (rr >= rrLast[note][layer])
            ++rr;   // never repeat the previous take
        rr = std::min (rr, count - 1);
    }
    else
    {
        rr = rrCounter[note][layer] % count;
        rrCounter[note][layer] = (uint8_t) ((rr + 1) % count);
    }
    rrLast[note][layer] = (uint8_t) rr;
    return rr;
}

ResynthVoice* SynthEngine::allocateVoice() noexcept
{
    const int limit = juce::jlimit (1, kMaxVoices / 2, settings.polyphony);

    int playing = 0;
    for (auto& v : voices)
        if (v.isActive() && ! v.isKilling())
            ++playing;

    while (playing >= limit)
    {
        // steal: oldest released voice first, otherwise the oldest voice
        ResynthVoice* victim = nullptr;
        for (auto& v : voices)
        {
            if (! v.isActive() || v.isKilling())
                continue;
            if (victim == nullptr
                || (v.isReleased() && ! victim->isReleased())
                || (v.isReleased() == victim->isReleased() && v.getAge() < victim->getAge()))
                victim = &v;
        }
        if (victim == nullptr)
            break;
        victim->kill();
        --playing;
    }

    for (auto& v : voices)
        if (! v.isActive())
            return &v;

    // every slot is busy fading out: reuse the oldest
    ResynthVoice* oldest = &voices[0];
    for (auto& v : voices)
        if (v.getAge() < oldest->getAge())
            oldest = &v;
    return oldest;
}

void SynthEngine::noteOn (int note, float velocity) noexcept
{
    if (settings.keyswitchesEnabled && note >= settings.keyswitchBase && note < settings.keyswitchBase + kNumArticulations)
    {
        setArticulation (note - settings.keyswitchBase);
        lastKeyswitch.store (note);
        return;
    }

    if (current == nullptr || ! current->containsKey (note))
        return;

    keyDown[note] = true;
    sustained[note] = false;
    const auto art = (Articulation) articulation;

    if (art == Articulation::Legato)
    {
        legatoStack.erase (std::remove (legatoStack.begin(), legatoStack.end(), note), legatoStack.end());
        legatoStack.push_back (note);

        for (auto& v : voices)
        {
            if (v.isActive() && ! v.isReleased() && ! v.isKilling()
                && v.getArticulation() == Articulation::Legato && v.getInstrument() == current)
            {
                v.legatoTo (note, velocity);
                return;
            }
        }
    }

    const bool useWheel = settings.dynamicsSource == 1 && isLong (art);
    const float x = useWheel ? modWheel : velocity;
    const int layer = current->layerForDynamic (x);

    ResynthVoice::StartParams p;
    p.instrument = current;
    p.note = note;
    p.velocity = velocity;
    p.roundRobin = nextRoundRobin (note, layer);
    p.articulation = art;
    p.age = ++ageCounter;

    if (auto* v = allocateVoice())
        v->start (p, settings, modWheel);
}

void SynthEngine::releaseNote (int note) noexcept
{
    legatoStack.erase (std::remove (legatoStack.begin(), legatoStack.end(), note), legatoStack.end());

    for (auto& v : voices)
    {
        if (! v.isActive() || v.isReleased() || v.getNote() != note)
            continue;

        if (v.getArticulation() == Articulation::Legato && ! legatoStack.empty())
            v.legatoTo (legatoStack.back(), 0.8f);
        else
            v.release();
    }
}

void SynthEngine::noteOff (int note) noexcept
{
    if (settings.keyswitchesEnabled && note >= settings.keyswitchBase && note < settings.keyswitchBase + kNumArticulations)
        return;

    keyDown[note] = false;
    if (sustainPedal)
    {
        sustained[note] = true;
        // legato lines still follow the fingers while the pedal is down
        legatoStack.erase (std::remove (legatoStack.begin(), legatoStack.end(), note), legatoStack.end());
        return;
    }
    releaseNote (note);
}

void SynthEngine::allNotesOff (bool immediate) noexcept
{
    for (auto& v : voices)
        if (v.isActive())
        {
            if (immediate) v.kill();
            else           v.release();
        }
    legatoStack.clear();
    std::fill (std::begin (keyDown), std::end (keyDown), false);
    std::fill (std::begin (sustained), std::end (sustained), false);
}

void SynthEngine::handleMidi (const juce::MidiMessage& msg) noexcept
{
    if (msg.isNoteOn())
        noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
    else if (msg.isNoteOff())
        noteOff (msg.getNoteNumber());
    else if (msg.isPitchWheel())
        bendRaw = (float) (msg.getPitchWheelValue() - 8192) / 8192.0f;
    else if (msg.isController())
    {
        const int cc = msg.getControllerNumber();
        const float val = (float) msg.getControllerValue() / 127.0f;
        if (cc == 1)
            modWheelTarget = val;
        else if (cc == 11)
            expressionTarget = val;
        else if (cc == 64)
        {
            const bool down = msg.getControllerValue() >= 64;
            if (sustainPedal && ! down)
            {
                sustainPedal = false;
                for (int n = 0; n < 128; ++n)
                    if (sustained[n])
                    {
                        sustained[n] = false;
                        if (! keyDown[n])
                            releaseNote (n);
                    }
            }
            sustainPedal = down;
        }
        else if (cc == 120)
            allNotesOff (true);
        else if (cc == 123)
            allNotesOff (false);
    }
    else if (msg.isAllNotesOff())
        allNotesOff (false);
    else if (msg.isAllSoundOff())
        allNotesOff (true);
}

void SynthEngine::renderVoices (float* left, float* right, int numSamples) noexcept
{
    // control-rate smoothing of the performance controllers
    const float a = 1.0f - std::exp (-(float) numSamples / (float) (0.03 * sampleRate));
    modWheel += (modWheelTarget - modWheel) * a;
    bendSemis = bendRaw * settings.bendRange;

    for (auto& v : voices)
        if (v.isActive())
            v.render (left, right, numSamples, settings, modWheel, bendSemis);
}

void SynthEngine::publishActivity() noexcept
{
    uint16_t masks[128] {};
    uint8_t rrs[128] {};
    int count = 0;
    for (auto& v : voices)
    {
        if (! v.isActive())
            continue;
        ++count;
        const int n = juce::jlimit (0, 127, v.getNote());
        masks[n] |= (uint16_t) (1u << juce::jlimit (0, 15, v.getLayer()));
        rrs[n] = (uint8_t) v.getRoundRobin();
    }
    for (int n = 0; n < 128; ++n)
    {
        keyLayerMask[(size_t) n].store (masks[n], std::memory_order_relaxed);
        if (masks[n] != 0)
            keyRoundRobin[(size_t) n].store (rrs[n], std::memory_order_relaxed);
    }
    activeVoicesAtomic.store (count, std::memory_order_relaxed);
}

void SynthEngine::process (juce::AudioBuffer<float>& output, const juce::MidiBuffer& midi) noexcept
{
    swapInstrumentIfPending();

    const int total = output.getNumSamples();
    output.clear();
    if (mixBuffer.getNumSamples() == 0)
        return;   // not prepared yet

    auto midiIt = midi.cbegin();
    const auto midiEnd = midi.cend();

    int pos = 0;
    while (pos < total)
    {
        // handle every event at or before this position
        while (midiIt != midiEnd && (*midiIt).samplePosition <= pos)
        {
            handleMidi ((*midiIt).getMessage());
            ++midiIt;
        }

        int end = std::min (total, pos + mixBuffer.getNumSamples());
        if (midiIt != midiEnd)
            end = std::min (end, std::max (pos + 1, (*midiIt).samplePosition));
        const int n = end - pos;

        auto* l = mixBuffer.getWritePointer (0);
        auto* r = mixBuffer.getWritePointer (1);
        std::fill (l, l + n, 0.0f);
        std::fill (r, r + n, 0.0f);
        renderVoices (l, r, n);

        // master gain and CC11 expression, smoothed per sample
        const float targetGain = juce::Decibels::decibelsToGain (settings.masterGainDb) * expressionTarget;
        const float step = (targetGain - masterGain) / (float) n;
        float g = masterGain;
        if (output.getNumChannels() >= 2)
        {
            auto* oL = output.getWritePointer (0);
            auto* oR = output.getWritePointer (1);
            for (int i = 0; i < n; ++i)
            {
                g += step;
                oL[pos + i] = l[i] * g;
                oR[pos + i] = r[i] * g;
            }
        }
        else if (output.getNumChannels() == 1)
        {
            auto* o = output.getWritePointer (0);
            for (int i = 0; i < n; ++i)
            {
                g += step;
                o[pos + i] = 0.5f * (l[i] + r[i]) * g;
            }
        }
        masterGain = targetGain;
        pos = end;
    }

    while (midiIt != midiEnd)
    {
        handleMidi ((*midiIt).getMessage());
        ++midiIt;
    }

    publishActivity();
}

} // namespace ivi
