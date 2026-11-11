#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lp
{

/** Farben: Navy-Petrol und Lime-Rahmen aus der Vorlage, Cyan für Werte, Silber für die Knobs. */
namespace Theme
{
    const juce::Colour bgDeep    { 0xff050c11 };
    const juce::Colour bg        { 0xff09151c };
    const juce::Colour panel     { 0xff0c1c25 };
    const juce::Colour panelHi   { 0xff112733 };
    const juce::Colour plot      { 0xff040a0e };
    const juce::Colour grid      { 0xff14313c };
    const juce::Colour frame     { 0xff3fdc2c };   // feine grüne Rahmenlinien
    const juce::Colour lime      { 0xffa6ff2e };
    const juce::Colour limeDeep  { 0xff62d417 };
    const juce::Colour cyan      { 0xff28dcf4 };
    const juce::Colour text      { 0xffdcefe8 };
    const juce::Colour dim       { 0xff6d8a91 };
    const juce::Colour danger    { 0xffff5468 };

    juce::Font mono (float height, bool bold = false);
    juce::Font sans (float height, bool bold = false);

    /** Panel mit feinem Lime-Rahmen und Kopfzeile wie in der Vorlage. */
    void drawPanel (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title,
                    juce::Colour titleColour = lime);

    /**
        Silbern glänzender Knopf mit Lime-Kontur. angle in Radiant, 0 = 12 Uhr, im Uhrzeigersinn.
    */
    void drawSilverKnob (juce::Graphics& g, juce::Rectangle<float> circle, float angle,
                         juce::Colour contour = lime, bool enabled = true);
}

class LimeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LimeLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
};

} // namespace lp
