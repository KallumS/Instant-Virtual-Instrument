#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/DemoSource.h"

using namespace ivi;

const juce::StringArray& IVIProcessor::structuralParams()
{
    static const juce::StringArray ids { params::layers, params::roundRobins, params::lowKey, params::highKey,
                                         params::variation, params::seed, params::rootNote };
    return ids;
}

IVIProcessor::IVIProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "IVI", params::createLayout())
{
    auto p = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    raw.formant = p (params::formant);
    raw.brightness = p (params::brightness);
    raw.sourceDynamic = p (params::sourceDynamic);
    raw.dynRange = p (params::dynRange);
    raw.noise = p (params::noise);
    raw.attack = p (params::attack);
    raw.release = p (params::release);
    raw.glide = p (params::glide);
    raw.pitchDrift = p (params::pitchDrift);
    raw.tremoloRate = p (params::tremoloRate);
    raw.width = p (params::width);
    raw.gain = p (params::gain);
    raw.bendRange = p (params::bendRange);
    raw.velocityBlend = p (params::velocityBlend);
    raw.dynSource = p (params::dynSource);
    raw.rrMode = p (params::rrMode);
    raw.sustainMode = p (params::sustainMode);
    raw.polyphony = p (params::polyphony);
    raw.partials = p (params::partials);
    raw.ksBase = p (params::ksBase);
    raw.keyswitches = p (params::keyswitches);
    raw.articulation = p (params::articulation);

    for (auto& id : structuralParams())
        apvts.addParameterListener (id, this);

    startTimerHz (5);
}

IVIProcessor::~IVIProcessor()
{
    alive->store (false);
    exportCancel = true;
    for (auto& id : structuralParams())
        apvts.removeParameterListener (id, this);
    workers.removeAllJobs (true, 10000);
    cancelPendingUpdate();
}

bool IVIProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void IVIProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, std::max (samplesPerBlock, 512));
}

EngineSettings IVIProcessor::currentEngineSettings() const
{
    EngineSettings s;
    s.formant = raw.formant->load();
    s.brightnessSpan = raw.brightness->load();
    s.sourceDynamic = raw.sourceDynamic->load();
    s.dynRangeDb = raw.dynRange->load();
    s.noiseDb = raw.noise->load();
    s.attackMs = raw.attack->load();
    s.releaseMs = raw.release->load();
    s.glideMs = raw.glide->load();
    s.pitchDrift = raw.pitchDrift->load();
    s.tremoloRate = raw.tremoloRate->load();
    s.width = raw.width->load();
    s.masterGainDb = raw.gain->load();
    s.bendRange = raw.bendRange->load();
    s.velocityBlend = raw.velocityBlend->load() > 0.5f;
    s.dynamicsSource = (int) raw.dynSource->load();
    s.rrMode = (int) raw.rrMode->load();
    s.sustainMode = (int) raw.sustainMode->load();
    s.polyphony = (int) raw.polyphony->load();
    s.maxPartials = (int) raw.partials->load();
    s.keyswitchBase = (int) raw.ksBase->load();
    s.keyswitchesEnabled = raw.keyswitches->load() > 0.5f;
    return s;
}

BuildSettings IVIProcessor::currentBuildSettings() const
{
    BuildSettings b;
    b.numLayers = (int) apvts.getRawParameterValue (params::layers)->load();
    b.numRoundRobins = (int) apvts.getRawParameterValue (params::roundRobins)->load();
    b.lowKey = (int) apvts.getRawParameterValue (params::lowKey)->load();
    b.highKey = (int) apvts.getRawParameterValue (params::highKey)->load();
    if (b.highKey < b.lowKey)
        std::swap (b.lowKey, b.highKey);
    b.variation = apvts.getRawParameterValue (params::variation)->load();
    b.seed = (uint32_t) apvts.getRawParameterValue (params::seed)->load();
    return b;
}

void IVIProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    engine.setSettings (currentEngineSettings());

    const int artParam = (int) raw.articulation->load();
    if (artParam != lastArticulationParam)
    {
        lastArticulationParam = artParam;
        engine.setArticulation (artParam);
    }
    const int requested = pendingArticulation.exchange (-1);
    if (requested >= 0)
        engine.setArticulation (requested);

    engine.process (buffer, midi);
}

void IVIProcessor::requestArticulation (int index)
{
    pendingArticulation = index;
    if (auto* p = apvts.getParameter (params::articulation))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
}

// ---------------------------------------------------------------------------- sample loading

void IVIProcessor::loadSampleFile (const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
    {
        say (params::str ("Couldn’t read “") + file.getFileName()
                 + params::str ("”. Choose a WAV, AIFF, FLAC, Ogg or MP3 file."), true);
        return;
    }

    const auto maxSamples = (juce::int64) (30.0 * reader->sampleRate);
    const int len = (int) std::min (reader->lengthInSamples, maxSamples);
    if (len < 64)
    {
        say (params::str ("“") + file.getFileName() + params::str ("” is too short to use. Choose a sample of at least a tenth of a second."), true);
        return;
    }

    juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
    reader->read (&buf, 0, len, 0, true, true);

    juce::AudioBuffer<float> mono (1, len);
    mono.clear();
    for (int c = 0; c < buf.getNumChannels(); ++c)
        mono.addFrom (0, 0, buf, c, 0, len, 1.0f / (float) buf.getNumChannels());

    setSourceAudio (std::move (mono), reader->sampleRate, file.getFileNameWithoutExtension(), true);
}

void IVIProcessor::loadDemo()
{
    DemoOptions o;
    o.sampleRate = 48000.0;
    o.seconds = 3.0;
    setSourceAudio (makeDemoSample (o), o.sampleRate, "Demo Strings", true);
}

void IVIProcessor::setSourceAudio (juce::AudioBuffer<float> mono, double sampleRate, const juce::String& name, bool keepForUndo)
{
    {
        std::lock_guard<std::mutex> lock (modelMutex);
        offerUndoAfterAnalysis = false;
        if (keepForUndo && sourceAudio.getNumSamples() > 0)
        {
            undoAudio = std::move (sourceAudio);
            undoRate = sourceRate;
            undoName = sampleName;
            undoKind = UndoKind::sample;
            offerUndoAfterAnalysis = true;
        }
        sourceAudio = std::move (mono);
        sourceRate = sampleRate;
        sampleName = name;
    }
    startAnalysis();
}

void IVIProcessor::makeNewTakes()
{
    auto* seed = apvts.getParameter (params::seed);
    const int old = (int) apvts.getRawParameterValue (params::seed)->load();
    int next = old;
    while (next == old)
        next = juce::Random::getSystemRandom().nextInt ({ 1, 10000 });
    seed->setValueNotifyingHost (seed->convertTo0to1 ((float) next));
    undoKind = UndoKind::seed;
    undoSeed = old;
    say (params::str ("New round robins and layer variations, from seed ") + juce::String (next) + ".", false, true);
}

void IVIProcessor::undo()
{
    if (undoKind == UndoKind::seed)
    {
        auto* seed = apvts.getParameter (params::seed);
        seed->setValueNotifyingHost (seed->convertTo0to1 ((float) undoSeed));
        undoKind = UndoKind::none;
        say (params::str ("The takes from seed ") + juce::String (undoSeed) + params::str (" are back."));
    }
    else if (undoKind == UndoKind::sample)
    {
        undoKind = UndoKind::none;
        juce::AudioBuffer<float> audio;
        {
            std::lock_guard<std::mutex> lock (modelMutex);
            audio = std::move (undoAudio);
        }
        setSourceAudio (std::move (audio), undoRate, undoName, false);
    }
}

void IVIProcessor::dismissMessage()
{
    {
        std::lock_guard<std::mutex> lock (statusMutex);
        message = {};
    }
    undoKind = UndoKind::none;
    sendChangeMessage();
}

IVIProcessor::Message IVIProcessor::getMessage() const
{
    std::lock_guard<std::mutex> lock (statusMutex);
    return message;
}

void IVIProcessor::say (const juce::String& text, bool warning, bool canUndo)
{
    {
        std::lock_guard<std::mutex> lock (statusMutex);
        message = { text, warning, canUndo };
    }
    sendChangeMessage();
}

void IVIProcessor::startAnalysis()
{
    juce::AudioBuffer<float> audio;
    double rate;
    juce::String name;
    {
        std::lock_guard<std::mutex> lock (modelMutex);
        if (sourceAudio.getNumSamples() == 0)
            return;
        audio.makeCopyOf (sourceAudio);
        rate = sourceRate;
        name = sampleName;
    }

    AnalysisOptions opt;
    opt.forcedRootMidi = (float) apvts.getRawParameterValue (params::rootNote)->load();

    const int generation = ++analysisGeneration;
    analysing = true;
    analysisProgress = 0.0f;
    say (params::str ("Analysing “") + name + params::str ("”…"));

    auto aliveFlag = alive;
    workers.addJob ([this, audio = std::move (audio), rate, name, opt, generation, aliveFlag]
    {
        auto result = SpectralAnalyzer::analyse (audio, rate, opt, [this, generation, aliveFlag] (float p)
        {
            if (! aliveFlag->load() || analysisGeneration.load() != generation)
                return false;
            analysisProgress = p;
            return true;
        });

        if (result != nullptr)
            result->name = name;

        juce::MessageManager::callAsync ([this, result, generation, name, aliveFlag]
        {
            if (! aliveFlag->load() || analysisGeneration.load() != generation)
                return;
            analysing = false;
            if (result == nullptr)
            {
                say (params::str ("“") + name + params::str ("” is silent or too short to analyse. Choose a longer sample."), true);
                return;
            }
            {
                std::lock_guard<std::mutex> lock (modelMutex);
                model = result;
            }
            rebuildInstrument();
            const auto& m = *result;
            const auto bs = currentBuildSettings();
            juce::String s;
            s << params::str ("“") << name << params::str ("” is ready — ");
            if (m.pitched)
                s << "root " << params::noteName ((int) std::round (m.rootMidi)) << " (" << juce::String (m.f0Ref, 1) << " Hz), ";
            else
                s << params::str ("no clear pitch, so it’s played as C4, ");
            s << (m.decaying ? "a plucked or struck sound" : "a sustained sound") << ", " << m.numPartials << " partials. "
              << (bs.highKey - bs.lowKey + 1) << params::str (" keys × ") << bs.numLayers << params::str (" layers × ")
              << bs.numRoundRobins << " round robins.";
            if (! m.pitched)
                s << params::str (" If it should have a pitch, set Root note.");
            bool canUndo;
            {
                std::lock_guard<std::mutex> lock (modelMutex);
                canUndo = offerUndoAfterAnalysis && undoKind == UndoKind::sample;
                offerUndoAfterAnalysis = false;
            }
            say (s, false, canUndo);
            sendChangeMessage();
        });
    });
}

void IVIProcessor::rebuildInstrument()
{
    std::shared_ptr<const SpectralModel> m;
    {
        std::lock_guard<std::mutex> lock (modelMutex);
        m = model;
    }
    if (m == nullptr)
        return;
    engine.setInstrument (buildInstrument (m, currentBuildSettings()));
    sendChangeMessage();
}

std::shared_ptr<const SpectralModel> IVIProcessor::getModel() const
{
    std::lock_guard<std::mutex> lock (modelMutex);
    return model;
}

juce::String IVIProcessor::getSampleName() const
{
    std::lock_guard<std::mutex> lock (modelMutex);
    return sampleName;
}

void IVIProcessor::parameterChanged (const juce::String& id, float)
{
    if (id == params::rootNote)
        needsReanalysis = true;
    else
        needsRebuild = true;
    triggerAsyncUpdate();
}

void IVIProcessor::handleAsyncUpdate()
{
    if (needsReanalysis.exchange (false))
    {
        needsRebuild = false;
        startAnalysis();
    }
    else if (needsRebuild.exchange (false))
    {
        rebuildInstrument();
    }
}

void IVIProcessor::timerCallback()
{
    engine.collectGarbage();
}

// ---------------------------------------------------------------------------- export

void IVIProcessor::startExport (const juce::File& folder, bool allArticulations)
{
    auto m = getModel();
    if (m == nullptr || exporting.load())
        return;

    std::shared_ptr<Instrument> inst = buildInstrument (m, currentBuildSettings());
    const auto settings = currentEngineSettings();

    ExportOptions opt;
    opt.parentFolder = folder;
    opt.instrumentName = getSampleName().isNotEmpty() ? getSampleName() : juce::String ("Instrument");
    opt.articulations.clear();
    if (allArticulations)
        for (int a = 0; a < kNumArticulations; ++a)
            opt.articulations.push_back ((Articulation) a);
    else
        opt.articulations.push_back ((Articulation) engine.getArticulation());

    exporting = true;
    exportCancel = false;
    exportProgress = 0.0f;
    say (params::str ("Exporting…"));

    auto aliveFlag = alive;
    workers.addJob ([this, inst, settings, opt, aliveFlag]
    {
        auto result = MultisampleExporter::exportSfz (*inst, settings, opt, [this, aliveFlag] (float p)
        {
            exportProgress = p;
            return aliveFlag->load() && ! exportCancel.load();
        });

        juce::MessageManager::callAsync ([this, result, aliveFlag]
        {
            if (! aliveFlag->load())
                return;
            exporting = false;
            say (result.message, ! result.ok || result.clippedFiles > 0);
        });
    });
}

// ---------------------------------------------------------------------------- state

void IVIProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    juce::AudioBuffer<float> audio;
    double rate;
    juce::String name;
    {
        std::lock_guard<std::mutex> lock (modelMutex);
        audio.makeCopyOf (sourceAudio);
        rate = sourceRate;
        name = sampleName;
    }

    if (audio.getNumSamples() > 0)
    {
        // embed the source sample (FLAC) so the project reopens without the original file
        juce::MemoryBlock flacData;
        {
            juce::FlacAudioFormat flac;
            std::unique_ptr<juce::OutputStream> out = std::make_unique<juce::MemoryOutputStream> (flacData, false);
            auto writer = flac.createWriterFor (out, juce::AudioFormatWriterOptions{}
                                                         .withSampleRate (rate)
                                                         .withNumChannels (1)
                                                         .withBitsPerSample (24));
            if (writer != nullptr)
                writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
        }
        auto* sample = xml->createNewChildElement ("Sample");
        sample->setAttribute ("name", name);
        sample->setAttribute ("rate", rate);
        sample->setAttribute ("flac", flacData.toBase64Encoding());
    }

    copyXmlToBinary (*xml, destData);
}

void IVIProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr)
        return;

    juce::AudioBuffer<float> audio;
    double rate = 44100.0;
    juce::String name;
    if (auto* sample = xml->getChildByName ("Sample"))
    {
        juce::MemoryBlock flacData;
        if (flacData.fromBase64Encoding (sample->getStringAttribute ("flac")))
        {
            juce::FlacAudioFormat flac;
            std::unique_ptr<juce::AudioFormatReader> reader (
                flac.createReaderFor (new juce::MemoryInputStream (flacData, true), true));
            if (reader != nullptr && reader->lengthInSamples > 0)
            {
                audio.setSize (1, (int) reader->lengthInSamples);
                reader->read (&audio, 0, audio.getNumSamples(), 0, true, false);
                rate = reader->sampleRate;
            }
        }
        name = sample->getStringAttribute ("name");
        xml->removeChildElement (sample, true);
    }

    if (xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));

    if (audio.getNumSamples() > 0)
    {
        auto apply = [this, audio = std::move (audio), rate, name]() mutable
        {
            setSourceAudio (std::move (audio), rate, name, false);
        };
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
            apply();
        else
            juce::MessageManager::callAsync ([aliveFlag = alive, apply = std::move (apply)]() mutable
            {
                if (aliveFlag->load()) apply();
            });

        // listeners fired by replaceState would only rebuild/re-analyse: the analysis above covers that
        needsRebuild = false;
        needsReanalysis = false;
    }
}

juce::AudioProcessorEditor* IVIProcessor::createEditor()
{
    return new IVIEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IVIProcessor();
}
