#pragma once

#include "Analysis/AnalysisFifo.h"
#include "DSP/PhaseRotator.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace lp
{

namespace ParamIDs
{
    inline constexpr const char* mode    = "mode";     // 0 = gekoppelt, 1 = stereo
    inline constexpr const char* quality = "quality";
    inline constexpr const char* phaseL  = "phaseL";
    inline constexpr const char* phaseR  = "phaseR";
    inline constexpr const char* delayL  = "delayL";
    inline constexpr const char* delayR  = "delayR";
    inline constexpr const char* gainL   = "gainL";
    inline constexpr const char* gainR   = "gainR";
    inline constexpr const char* invertL = "invertL";
    inline constexpr const char* invertR = "invertR";
}

/**
    In Your Phase: stufenlose, frequenzunabhängige Phasendrehung um 360°, pro Kanal
    mit Feinverzögerung, Pegel und Polarität. Im gekoppelten Modus gelten die
    Einstellungen des linken Kanals für beide Kanäle.
*/
class InYourPhaseProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::AsyncUpdater
{
public:
    InYourPhaseProcessor();
    ~InYourPhaseProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "In Your Phase"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    AnalysisFifo analysis;

    double getCurrentSampleRate() const noexcept { return currentSampleRate; }
    bool isStereoMode() const noexcept { return modeParam->load() > 0.5f; }

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    ChannelParams readChannel (bool left) const noexcept;

    PhaseRotator rotator;
    juce::AudioBuffer<float> alignedInput;
    std::vector<AnalysisSample> analysisScratch;
    double currentSampleRate = 48000.0;

    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* qualityParam = nullptr;
    std::atomic<float>* phase[2] {};
    std::atomic<float>* delay[2] {};
    std::atomic<float>* gain[2] {};
    std::atomic<float>* invert[2] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InYourPhaseProcessor)
};

} // namespace lp
