#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace lp
{

juce::AudioProcessorValueTreeState::ParameterLayout InYourPhaseProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Modus",
                                                        StringArray { String::fromUTF8 ("Gekoppelt"), "Stereo" }, 1));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::quality, 1 }, String::fromUTF8 ("Qualität"),
        StringArray { String::fromUTF8 ("Präzise · ab 12 Hz"), String::fromUTF8 ("Ausgewogen · ab 25 Hz"),
                      String::fromUTF8 ("Schnell · ab 100 Hz") },
        1, AudioParameterChoiceAttributes().withAutomatable (false)));

    const auto degrees = AudioParameterFloatAttributes()
                             .withLabel (String::fromUTF8 ("°"))
                             .withStringFromValueFunction ([] (float v, int) { return String (v, 2) + String::fromUTF8 ("°"); });

    const auto ms = AudioParameterFloatAttributes()
                        .withLabel ("ms")
                        .withStringFromValueFunction ([] (float v, int) { return String (v, 3) + " ms"; });

    const auto db = AudioParameterFloatAttributes()
                        .withLabel ("dB")
                        .withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " dB"; });

    for (int side = 0; side < 2; ++side)
    {
        const String suffix = side == 0 ? " L" : " R";
        const char* phaseId  = side == 0 ? ParamIDs::phaseL  : ParamIDs::phaseR;
        const char* delayId  = side == 0 ? ParamIDs::delayL  : ParamIDs::delayR;
        const char* gainId   = side == 0 ? ParamIDs::gainL   : ParamIDs::gainR;
        const char* invertId = side == 0 ? ParamIDs::invertL : ParamIDs::invertR;

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { phaseId, 1 }, "Phase" + suffix,
                                                           NormalisableRange<float> (-180.0f, 180.0f, 0.0f), 0.0f, degrees));

        NormalisableRange<float> delayRange (0.0f, (float) PhaseRotator::maxDelayMs, 0.0f);
        delayRange.setSkewForCentre (3.0f); // fein im Bereich weniger Millisekunden
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { delayId, 1 }, String::fromUTF8 ("Verzögerung") + suffix,
                                                           delayRange, 0.0f, ms));

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { gainId, 1 }, "Pegel" + suffix,
                                                           NormalisableRange<float> (-24.0f, 12.0f, 0.0f), 0.0f, db));

        layout.add (std::make_unique<AudioParameterBool> (ParameterID { invertId, 1 }, String::fromUTF8 ("Polarität") + suffix, false));
    }

    return layout;
}

InYourPhaseProcessor::InYourPhaseProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "InYourPhase", createLayout())
{
    modeParam = apvts.getRawParameterValue (ParamIDs::mode);
    qualityParam = apvts.getRawParameterValue (ParamIDs::quality);

    phase[0]  = apvts.getRawParameterValue (ParamIDs::phaseL);
    phase[1]  = apvts.getRawParameterValue (ParamIDs::phaseR);
    delay[0]  = apvts.getRawParameterValue (ParamIDs::delayL);
    delay[1]  = apvts.getRawParameterValue (ParamIDs::delayR);
    gain[0]   = apvts.getRawParameterValue (ParamIDs::gainL);
    gain[1]   = apvts.getRawParameterValue (ParamIDs::gainR);
    invert[0] = apvts.getRawParameterValue (ParamIDs::invertL);
    invert[1] = apvts.getRawParameterValue (ParamIDs::invertR);

    apvts.addParameterListener (ParamIDs::quality, this);

    rotator.setQuality ((Quality) (int) qualityParam->load());
}

InYourPhaseProcessor::~InYourPhaseProcessor()
{
    apvts.removeParameterListener (ParamIDs::quality, this);
    cancelPendingUpdate();
}

bool InYourPhaseProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

double InYourPhaseProcessor::getTailLengthSeconds() const
{
    return (double) rotator.getLatencySamples() / juce::jmax (1.0, currentSampleRate) + PhaseRotator::maxDelayMs * 0.001;
}

void InYourPhaseProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    rotator.setQuality ((Quality) (int) qualityParam->load());
    rotator.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());

    alignedInput.setSize (2, juce::jmax (1, samplesPerBlock));
    analysisScratch.resize ((size_t) juce::jmax (1, samplesPerBlock));

    setLatencySamples (rotator.getLatencySamples());
}

ChannelParams InYourPhaseProcessor::readChannel (bool left) const noexcept
{
    // Im gekoppelten Modus folgt der rechte Kanal dem linken
    const int i = (left || ! isStereoMode()) ? 0 : 1;

    ChannelParams p;
    p.phaseDeg = phase[i]->load();
    p.delayMs = delay[i]->load();
    p.gainDb = gain[i]->load();
    p.invert = invert[i]->load() > 0.5f;
    return p;
}

void InYourPhaseProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels());

    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    rotator.setQuality ((Quality) (int) qualityParam->load());
    rotator.setParams (0, readChannel (true));
    rotator.setParams (1, readChannel (false));

    // Größere Blöcke als angekündigt in Stücken verarbeiten
    const int maxChunk = alignedInput.getNumSamples();

    for (int offset = 0; offset < numSamples; offset += maxChunk)
    {
        const int n = juce::jmin (maxChunk, numSamples - offset);
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), numChannels, offset, n);
        juce::AudioBuffer<float> aligned (alignedInput.getArrayOfWritePointers(), 2, 0, n);
        rotator.process (view, &aligned);

        // Analyse: Ausgang + ausgerichteter Eingang + musikalische Position am Ausgang
        double ppq = -1.0, bpm = 0.0, beatsPerBar = 4.0;

        if (auto* head = getPlayHead())
        {
            if (auto pos = head->getPosition())
            {
                if (auto b = pos->getBpm())
                    bpm = *b;

                if (auto sig = pos->getTimeSignature())
                    beatsPerBar = sig->numerator * 4.0 / juce::jmax (1, sig->denominator);

                if (pos->getIsPlaying())
                    if (auto p = pos->getPpqPosition())
                        ppq = *p;
            }
        }

        const double beatsPerSample = bpm > 0.0 ? bpm / 60.0 / currentSampleRate : 0.0;
        // Der Ausgang ist um die Latenz verzögert, die Position entsprechend zurückrechnen
        const double outPpq = ppq >= 0.0 ? ppq + offset * beatsPerSample - getLatencySamples() * beatsPerSample : -1.0;

        for (int i = 0; i < n; ++i)
        {
            auto& s = analysisScratch[(size_t) i];
            s.outL = view.getSample (0, i);
            s.outR = numChannels > 1 ? view.getSample (1, i) : s.outL;
            s.inL = aligned.getSample (0, i);
            s.inR = numChannels > 1 ? aligned.getSample (1, i) : s.inL;
            s.ppq = outPpq >= 0.0 ? juce::jmax (0.0, outPpq + i * beatsPerSample) : -1.0;
            s.bpm = (float) bpm;
            s.beatsPerBar = (float) beatsPerBar;
        }

        analysis.push (analysisScratch.data(), n);
    }
}

void InYourPhaseProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == ParamIDs::quality)
    {
        rotator.setQuality ((Quality) (int) qualityParam->load());
        triggerAsyncUpdate(); // Latenz auf dem Message-Thread melden
    }
}

void InYourPhaseProcessor::handleAsyncUpdate()
{
    setLatencySamples (rotator.getLatencySamples());
}

void InYourPhaseProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void InYourPhaseProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* InYourPhaseProcessor::createEditor()
{
    return new InYourPhaseEditor (*this);
}

} // namespace lp

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new lp::InYourPhaseProcessor();
}
