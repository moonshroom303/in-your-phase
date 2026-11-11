#include "LimeLookAndFeel.h"

namespace lp
{

juce::Font Theme::mono (float height, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font Theme::sans (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

void Theme::drawPanel (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title, juce::Colour titleColour)
{
    juce::ColourGradient body (panelHi.withAlpha (0.9f), area.getX(), area.getY(),
                               panel, area.getX(), area.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (area, 6.0f);

    g.setColour (frame.withAlpha (0.55f));
    g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);

    if (title.isNotEmpty())
    {
        auto header = area.removeFromTop (24.0f).reduced (1.0f);
        g.setColour (bgDeep.withAlpha (0.55f));
        g.fillRoundedRectangle (header, 5.0f);
        g.setColour (frame.withAlpha (0.35f));
        g.drawHorizontalLine ((int) header.getBottom(), header.getX() + 4.0f, header.getRight() - 4.0f);

        g.setColour (titleColour);
        g.setFont (sans (12.5f, true));
        g.drawText (title.toUpperCase(), header.reduced (10.0f, 0.0f), juce::Justification::centredLeft);
    }
}

void Theme::drawSilverKnob (juce::Graphics& g, juce::Rectangle<float> circle, float angle,
                            juce::Colour contour, bool enabled)
{
    using namespace juce;

    const auto c = circle.getCentre();
    const float r = circle.getWidth() * 0.5f;

    // Schatten
    for (int i = 4; i > 0; --i)
    {
        g.setColour (Colours::black.withAlpha (0.10f));
        g.fillEllipse (circle.expanded ((float) i * r * 0.025f).translated (0.0f, r * 0.05f));
    }

    // Fase: oben hell, unten dunkel
    ColourGradient bevel (Colour (0xfff6f8f9), c.x, c.y - r, Colour (0xff2f363a), c.x, c.y + r, false);
    g.setGradientFill (bevel);
    g.fillEllipse (circle);

    // Gesicht: radialer Silberverlauf mit Lichtquelle oben links
    const float fr = r * 0.88f;
    auto face = Rectangle<float> (fr * 2.0f, fr * 2.0f).withCentre (c);
    ColourGradient silver (Colour (0xffffffff), c.x - fr * 0.35f, c.y - fr * 0.45f,
                           Colour (0xff626b70), c.x + fr * 0.55f, c.y + fr * 0.75f, true);
    silver.addColour (0.30, Colour (0xffe1e5e8));
    silver.addColour (0.62, Colour (0xffa9b1b6));
    g.setGradientFill (silver);
    g.fillEllipse (face);

    // Gebürstetes Metall: feine konzentrische Ringe
    const int rings = juce::jlimit (8, 60, (int) (fr / 2.0f));
    for (int i = 1; i < rings; ++i)
    {
        const float rr = fr * (float) i / (float) rings;
        g.setColour ((i % 2 == 0 ? Colours::white : Colours::black).withAlpha (0.045f));
        g.drawEllipse (Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c), 0.8f);
    }

    // Anisotroper Glanz: zwei helle Keile wie bei gedrehtem Aluminium
    for (float base : { -0.78f, 2.36f })
    {
        Path wedge;
        wedge.addPieSegment (face, base - 0.22f, base + 0.22f, 0.0f);
        g.setColour (Colours::white.withAlpha (0.16f));
        g.fillPath (wedge);
    }

    // Glanzlicht oben links
    ColourGradient spec (Colours::white.withAlpha (0.65f), c.x - fr * 0.30f, c.y - fr * 0.42f,
                         Colours::white.withAlpha (0.0f), c.x - fr * 0.30f, c.y + fr * 0.05f, true);
    g.setGradientFill (spec);
    g.fillEllipse (Rectangle<float> (fr * 1.1f, fr * 0.7f).withCentre ({ c.x - fr * 0.22f, c.y - fr * 0.38f }));

    // Lime-Kontur mit leichtem Leuchten
    const auto ring = contour.withMultipliedAlpha (enabled ? 1.0f : 0.35f);
    g.setColour (ring.withAlpha (ring.getFloatAlpha() * 0.25f));
    g.drawEllipse (face.expanded (1.5f), 4.0f);
    g.setColour (ring);
    g.drawEllipse (face, juce::jmax (1.2f, r * 0.018f));

    // Zeiger: dunkle Kerbe mit Lime-Linie
    const float inner = fr * 0.30f, outer = fr * 0.90f;
    const Point<float> a (c.x + inner * std::sin (angle), c.y - inner * std::cos (angle));
    const Point<float> b (c.x + outer * std::sin (angle), c.y - outer * std::cos (angle));

    g.setColour (Colour (0xff141b1f).withAlpha (0.85f));
    g.drawLine ({ a, b }, juce::jmax (2.5f, r * 0.055f));
    g.setColour (ring.withAlpha (ring.getFloatAlpha() * 0.35f));
    g.drawLine ({ a, b }, juce::jmax (3.5f, r * 0.07f));
    g.setColour (ring);
    g.drawLine ({ a, b }, juce::jmax (1.3f, r * 0.025f));
    g.fillEllipse (Rectangle<float> (r * 0.07f + 3.0f, r * 0.07f + 3.0f).withCentre (b));

    // Mittelkappe
    const float cr = fr * 0.16f;
    ColourGradient cap (Colour (0xffd9dee1), c.x - cr * 0.4f, c.y - cr * 0.6f,
                        Colour (0xff6c757a), c.x + cr * 0.4f, c.y + cr * 0.7f, false);
    g.setGradientFill (cap);
    g.fillEllipse (Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.25f));
    g.drawEllipse (Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (c), 0.8f);

    if (! enabled)
    {
        g.setColour (Theme::bgDeep.withAlpha (0.45f));
        g.fillEllipse (circle);
    }
}

//==============================================================================

LimeLookAndFeel::LimeLookAndFeel()
{
    using namespace juce;

    setColour (ResizableWindow::backgroundColourId, Theme::bg);
    setColour (Label::textColourId, Theme::text);

    setColour (Slider::textBoxTextColourId, Theme::cyan);
    setColour (Slider::textBoxBackgroundColourId, Theme::plot);
    setColour (Slider::textBoxOutlineColourId, Theme::frame.withAlpha (0.35f));
    setColour (Slider::textBoxHighlightColourId, Theme::limeDeep.withAlpha (0.5f));
    setColour (Slider::rotarySliderFillColourId, Theme::lime);
    setColour (Slider::rotarySliderOutlineColourId, Theme::bgDeep);

    setColour (TextButton::buttonColourId, Theme::panelHi);
    setColour (TextButton::buttonOnColourId, Theme::lime);
    setColour (TextButton::textColourOffId, Theme::text);
    setColour (TextButton::textColourOnId, Theme::bgDeep);

    setColour (ComboBox::backgroundColourId, Theme::plot);
    setColour (ComboBox::textColourId, Theme::cyan);
    setColour (ComboBox::outlineColourId, Theme::frame.withAlpha (0.5f));
    setColour (ComboBox::arrowColourId, Theme::lime);

    setColour (PopupMenu::backgroundColourId, Theme::panel);
    setColour (PopupMenu::textColourId, Theme::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Theme::limeDeep.withAlpha (0.35f));
    setColour (PopupMenu::highlightedTextColourId, Theme::lime);

    setColour (TextEditor::backgroundColourId, Theme::plot);
    setColour (TextEditor::textColourId, Theme::cyan);
    setColour (TextEditor::highlightColourId, Theme::limeDeep.withAlpha (0.5f));
    setColour (TextEditor::outlineColourId, Theme::frame.withAlpha (0.4f));
    setColour (TextEditor::focusedOutlineColourId, Theme::lime);
    setColour (CaretComponent::caretColourId, Theme::lime);
}

void LimeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;

    auto bounds = Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const float size = jmin (bounds.getWidth(), bounds.getHeight());
    auto circle = Rectangle<float> (size, size).withCentre (bounds.getCentre());
    const auto c = circle.getCentre();
    const float r = size * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Skala: dunkle Spur und Lime-Wert
    Path track;
    track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (Theme::bgDeep);
    g.strokePath (track, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    const auto arcColour = slider.findColour (Slider::rotarySliderFillColourId);
    Path value;
    value.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, startAngle, angle, true);
    g.setColour (arcColour.withAlpha (0.3f));
    g.strokePath (value, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (arcColour);
    g.strokePath (value, PathStrokeType (2.5f, PathStrokeType::curved, PathStrokeType::rounded));

    Theme::drawSilverKnob (g, circle.reduced (r * 0.17f), angle, Theme::lime, slider.isEnabled());
}

void LimeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool over, bool down)
{
    auto area = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    if (on)
    {
        juce::ColourGradient fill (Theme::lime, area.getX(), area.getY(), Theme::limeDeep, area.getX(), area.getBottom(), false);
        g.setGradientFill (fill);
    }
    else
    {
        g.setColour (Theme::panelHi.brighter (over ? 0.15f : 0.0f).darker (down ? 0.2f : 0.0f));
    }

    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (on ? Theme::lime.brighter (0.3f) : Theme::frame.withAlpha (over ? 0.8f : 0.45f));
    g.drawRoundedRectangle (area, 4.0f, 1.0f);
}

void LimeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (button.getToggleState() ? Theme::bgDeep : Theme::text.withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

juce::Font LimeLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return Theme::sans (juce::jmin (13.0f, (float) buttonHeight * 0.55f), true);
}

void LimeLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                    int, int, int, int, juce::ComboBox& box)
{
    auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (area, 3.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId).withMultipliedAlpha (box.isMouseOver() ? 1.6f : 1.0f));
    g.drawRoundedRectangle (area, 3.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) width - 14.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

juce::Font LimeLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return Theme::mono (13.0f);
}

void LimeLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font LimeLookAndFeel::getPopupMenuFont()
{
    return Theme::sans (14.0f);
}

juce::Label* LimeLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (Theme::mono (12.5f));
    label->setJustificationType (juce::Justification::centred);
    return label;
}

} // namespace lp
