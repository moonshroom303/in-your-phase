#pragma once

#include "LimeLookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace lp
{

/**
    Großer Phasen-Knopf: frei drehbar über 360°, ohne Anschlag (über ±180° hinweg
    geht es nahtlos weiter). Kreisförmig ziehen; Shift = fein (1/10);
    Mausrad = 1° (mit Shift 0,1°); Doppelklick = 0°.
*/
class PhaseDial : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    explicit PhaseDial (juce::RangedAudioParameter& parameter);

    float getValue() const noexcept { return value; }
    void setValueFromUser (float degrees);

    std::function<void (float)> onValueChange;
    void setArcColour (juce::Colour c) { arcColour = c; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    float angleAt (juce::Point<float> p) const;

    juce::ParameterAttachment attachment;
    float value = 0.0f;
    float lastMouseAngle = 0.0f;
    juce::Colour arcColour = Theme::lime;
};

/** Ein Kanal (L, R oder beide gekoppelt): Phasen-Knopf, Wertfeld, Schnellwahl, Verzögerung, Pegel, Polarität. */
class ChannelPanel : public juce::Component
{
public:
    ChannelPanel (juce::AudioProcessorValueTreeState& state, bool rightChannel);

    void setTitle (const juce::String& newTitle, juce::Colour colour);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void updateValueLabel (float degrees);

    juce::String title;
    juce::Colour titleColour = Theme::lime;

    PhaseDial dial;
    juce::Label valueLabel;
    juce::TextButton presets[4];

    juce::Slider delaySlider, gainSlider;
    juce::Label delayCaption, gainCaption;
    juce::TextButton invertButton;

    juce::AudioProcessorValueTreeState::SliderAttachment delayAttachment, gainAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment invertAttachment;
};

} // namespace lp
