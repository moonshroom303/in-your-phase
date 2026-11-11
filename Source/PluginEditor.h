#pragma once

#include "PluginProcessor.h"
#include "UI/AnalysisViews.h"
#include "UI/ChannelPanel.h"
#include "UI/LimeLookAndFeel.h"
#include "UI/MoonshroomPanel.h"

namespace lp
{

class InYourPhaseEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit InYourPhaseEditor (InYourPhaseProcessor&);
    ~InYourPhaseEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Für Tests und Screenshots */
    MoonshroomOverlay& getStudioOverlay() { return studio; }
    juce::Button& getStudioButton() { return studioButton; }

private:
    void timerCallback() override;
    void updateMode (bool stereo);

    InYourPhaseProcessor& owner;
    LimeLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    juce::TextButton coupledButton { "GEKOPPELT" };
    juce::TextButton stereoButton { "STEREO" };
    juce::ParameterAttachment modeAttachment;

    juce::ComboBox qualityBox;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment qualityAttachment;

    ChannelPanel left, right;
    ScopeView scope;
    StereoFieldView stereoField;

    MoonshroomLinkButton studioButton;
    MoonshroomOverlay studio { "In Your Phase" };

    bool stereoMode = true;
    std::vector<AnalysisSample> drainBuffer;
    double lastSampleRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InYourPhaseEditor)
};

} // namespace lp
