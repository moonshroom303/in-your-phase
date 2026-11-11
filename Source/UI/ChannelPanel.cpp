#include "ChannelPanel.h"
#include "../PluginProcessor.h"

#include <cmath>

namespace lp
{

namespace
{
float wrap180 (float deg)
{
    deg = std::fmod (deg + 180.0f, 360.0f);
    if (deg < 0.0f)
        deg += 360.0f;
    return deg - 180.0f;
}

juce::String formatDegrees (float v)
{
    if (std::abs (v) >= 179.995f)
        return juce::String::fromUTF8 ("±180.00°");
    return (v >= 0.0f ? "+" : "") + juce::String (v, 2) + juce::String::fromUTF8 ("°");
}
} // namespace

//==============================================================================
// PhaseDial

PhaseDial::PhaseDial (juce::RangedAudioParameter& parameter)
    : attachment (parameter, [this] (float v)
                  {
                      value = v;
                      repaint();
                      if (onValueChange)
                          onValueChange (v);
                  })
{
    setRepaintsOnMouseActivity (true);
    attachment.sendInitialUpdate();
}

void PhaseDial::setValueFromUser (float degrees)
{
    attachment.setValueAsCompleteGesture (wrap180 (degrees));
}

float PhaseDial::angleAt (juce::Point<float> p) const
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return juce::radiansToDegrees (std::atan2 (p.x - c.x, c.y - p.y)); // 0° oben, im Uhrzeigersinn
}

void PhaseDial::mouseDown (const juce::MouseEvent& e)
{
    lastMouseAngle = angleAt (e.position);
    attachment.beginGesture();
}

void PhaseDial::mouseDrag (const juce::MouseEvent& e)
{
    const float now = angleAt (e.position);
    float delta = wrap180 (now - lastMouseAngle);
    lastMouseAngle = now;

    // Sehr nah an der Mitte springt der Winkel, dort nicht reagieren
    const auto c = getLocalBounds().toFloat().getCentre();
    if (e.position.getDistanceFrom (c) < getWidth() * 0.06f)
        return;

    if (e.mods.isShiftDown())
        delta *= 0.1f;

    attachment.setValueAsPartOfGesture (wrap180 (value + delta));
}

void PhaseDial::mouseUp (const juce::MouseEvent&)
{
    attachment.endGesture();
}

void PhaseDial::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (0.0f);
}

void PhaseDial::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const float step = e.mods.isShiftDown() ? 0.1f : 1.0f;
    const float dir = (wheel.deltaY + wheel.deltaX) >= 0.0f ? 1.0f : -1.0f;
    attachment.setValueAsCompleteGesture (wrap180 (value + dir * step));
}

void PhaseDial::paint (juce::Graphics& g)
{
    using namespace juce;

    auto bounds = getLocalBounds().toFloat();
    const float size = jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float R = size * 0.5f - 2.0f;

    // Skalenring
    const auto ringArea = Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c);
    ColourGradient ringFill (Theme::bgDeep, c.x, c.y - R, Theme::panel, c.x, c.y + R, false);
    g.setGradientFill (ringFill);
    g.fillEllipse (ringArea);
    g.setColour (Theme::frame.withAlpha (0.55f));
    g.drawEllipse (ringArea.reduced (0.5f), 1.0f);

    // Teilstriche: alle 15°, Hauptstriche alle 45°
    for (int deg = -180; deg < 180; deg += 5)
    {
        const bool major = deg % 45 == 0;
        const bool mid = deg % 15 == 0;
        const float a = degreesToRadians ((float) deg);
        const float r1 = R * (major ? 0.80f : (mid ? 0.84f : 0.865f));
        const float r2 = R * 0.89f;
        g.setColour (major ? Theme::lime.withAlpha (0.85f) : Theme::dim.withAlpha (mid ? 0.8f : 0.45f));
        g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a),
                    c.x + r2 * std::sin (a), c.y - r2 * std::cos (a), major ? 1.6f : 1.0f);
    }

    // Beschriftung
    g.setFont (Theme::mono (jmax (9.0f, R * 0.075f)));
    for (int deg : { 0, 45, 90, 135, 180, -135, -90, -45 })
    {
        const float a = degreesToRadians ((float) deg);
        const float rl = R * 0.945f;
        auto label = Rectangle<float> (R * 0.24f, R * 0.09f).withCentre ({ c.x + rl * std::sin (a), c.y - rl * std::cos (a) });
        g.setColour (deg == 0 ? Theme::lime : Theme::cyan.withAlpha (0.75f));
        g.drawText (deg == 180 ? String::fromUTF8 ("±180") : String (deg), label, Justification::centred);
    }

    // Wertebogen von 0° bis zum aktuellen Winkel
    const float valueRad = degreesToRadians (value);
    const float arcR = R * 0.745f;
    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, 0.0f, MathConstants<float>::twoPi, true);
    g.setColour (Theme::bgDeep.brighter (0.05f));
    g.strokePath (track, PathStrokeType (R * 0.05f));

    if (std::abs (value) > 0.01f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (0.0f, valueRad), jmax (0.0f, valueRad), true);
        g.setColour (arcColour.withAlpha (0.25f));
        g.strokePath (arc, PathStrokeType (R * 0.085f, PathStrokeType::curved, PathStrokeType::butt));
        g.setColour (arcColour);
        g.strokePath (arc, PathStrokeType (R * 0.04f, PathStrokeType::curved, PathStrokeType::butt));
    }

    // Silberner Knopf
    const float knobR = R * 0.66f;
    Theme::drawSilverKnob (g, Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c), valueRad,
                           Theme::lime, isEnabled());

    if (isMouseOverOrDragging())
    {
        g.setColour (Theme::lime.withAlpha (0.18f));
        g.drawEllipse (ringArea.reduced (2.0f), 2.0f);
    }
}

//==============================================================================
// ChannelPanel

ChannelPanel::ChannelPanel (juce::AudioProcessorValueTreeState& state, bool right)
    : dial (*state.getParameter (right ? ParamIDs::phaseR : ParamIDs::phaseL)),
      delayAttachment (state, right ? ParamIDs::delayR : ParamIDs::delayL, delaySlider),
      gainAttachment (state, right ? ParamIDs::gainR : ParamIDs::gainL, gainSlider),
      invertAttachment (state, right ? ParamIDs::invertR : ParamIDs::invertL, invertButton)
{
    addAndMakeVisible (dial);
    dial.setTooltip (juce::String::fromUTF8 ("Ziehen: drehen · Shift: fein · Mausrad: 1° · Doppelklick: 0°"));

    addAndMakeVisible (valueLabel);
    valueLabel.setEditable (false, true, false);
    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setFont (Theme::mono (22.0f, true));
    valueLabel.setColour (juce::Label::textColourId, Theme::cyan);
    valueLabel.setColour (juce::Label::backgroundColourId, Theme::plot);
    valueLabel.setColour (juce::Label::outlineColourId, Theme::frame.withAlpha (0.45f));
    valueLabel.setTooltip ("Doppelklick: Wert eingeben");
    valueLabel.onTextChange = [this]
    {
        auto text = valueLabel.getText().replaceCharacter (',', '.').retainCharacters ("+-.0123456789");
        if (text.isNotEmpty())
            dial.setValueFromUser (text.getFloatValue());
        updateValueLabel (dial.getValue());
    };

    dial.onValueChange = [this] (float v) { updateValueLabel (v); };
    updateValueLabel (dial.getValue());

    const float presetValues[] = { -90.0f, 0.0f, 90.0f, 180.0f };
    const char* presetNames[] = { "-90", "0", "+90", "180" };
    for (int i = 0; i < 4; ++i)
    {
        presets[i].setButtonText (juce::String (presetNames[i]) + juce::String::fromUTF8 ("°"));
        const float v = presetValues[i];
        presets[i].onClick = [this, v] { dial.setValueFromUser (v); };
        addAndMakeVisible (presets[i]);
    }

    for (auto* s : { &delaySlider, &gainSlider })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 86, 20);
        s->setVelocityBasedMode (false);
        s->setMouseDragSensitivity (300);
        addAndMakeVisible (s);
    }

    delaySlider.setDoubleClickReturnValue (true, 0.0);
    gainSlider.setDoubleClickReturnValue (true, 0.0);
    delaySlider.setTooltip (juce::String::fromUTF8 ("Feinverzögerung zur Zeitausrichtung · Doppelklick: 0"));
    gainSlider.setTooltip (juce::String::fromUTF8 ("Pegel · Doppelklick: 0 dB"));

    for (auto* l : { &delayCaption, &gainCaption })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (Theme::sans (11.5f, true));
        l->setColour (juce::Label::textColourId, Theme::dim);
        addAndMakeVisible (l);
    }
    delayCaption.setText (juce::String::fromUTF8 ("VERZÖGERUNG"), juce::dontSendNotification);
    gainCaption.setText ("PEGEL", juce::dontSendNotification);

    invertButton.setButtonText (juce::String::fromUTF8 ("Ø  POLARITÄT"));
    invertButton.setClickingTogglesState (true);
    invertButton.setTooltip (juce::String::fromUTF8 ("Polarität umkehren"));
    addAndMakeVisible (invertButton);
}

void ChannelPanel::setTitle (const juce::String& newTitle, juce::Colour colour)
{
    title = newTitle;
    titleColour = colour;
    dial.setArcColour (colour);
    repaint();
}

void ChannelPanel::updateValueLabel (float degrees)
{
    valueLabel.setText (formatDegrees (degrees), juce::dontSendNotification);
}

void ChannelPanel::paint (juce::Graphics& g)
{
    Theme::drawPanel (g, getLocalBounds().toFloat(), title, titleColour);
}

void ChannelPanel::resized()
{
    auto area = getLocalBounds().reduced (14);
    area.removeFromTop (22); // Kopfzeile

    // Unten: Verzögerung, Pegel, Polarität
    auto controls = area.removeFromBottom (128);
    auto buttonRow = controls.removeFromBottom (28);
    invertButton.setBounds (buttonRow.withSizeKeepingCentre (juce::jmin (220, buttonRow.getWidth()), 28));
    controls.removeFromBottom (6);

    auto left = controls.removeFromLeft (controls.getWidth() / 2);
    delayCaption.setBounds (left.removeFromTop (16));
    delaySlider.setBounds (left);
    gainCaption.setBounds (controls.removeFromTop (16));
    gainSlider.setBounds (controls);

    area.removeFromBottom (8);

    // Schnellwahl und Wertfeld
    auto presetRow = area.removeFromBottom (26);
    const int pw = juce::jmin (64, (presetRow.getWidth() - 18) / 4);
    auto presetArea = presetRow.withSizeKeepingCentre (pw * 4 + 18, 26);
    for (auto& p : presets)
    {
        p.setBounds (presetArea.removeFromLeft (pw));
        presetArea.removeFromLeft (6);
    }

    area.removeFromBottom (6);
    auto valueRow = area.removeFromBottom (36);
    valueLabel.setBounds (valueRow.withSizeKeepingCentre (juce::jmin (190, valueRow.getWidth()), 36));
    area.removeFromBottom (6);

    // Der Phasen-Knopf bekommt den ganzen Rest: der größte Regler der Oberfläche
    const int size = juce::jmin (area.getWidth(), area.getHeight());
    dial.setBounds (area.withSizeKeepingCentre (size, size));
}

} // namespace lp
