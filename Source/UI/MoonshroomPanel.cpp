#include "MoonshroomPanel.h"
#include "LimeLookAndFeel.h"
#include "BinaryData.h"

namespace lp
{

namespace
{
    const juce::Colour cardBg      { 0xff08151c };
    const juce::Colour cardBorder  { 0xff355044 };
    const juce::Colour tileBg      { 0xff030a0d };
    const juce::Colour tileBorder  { 0xff274039 };
    const juce::Colour linkBorder  { 0xff4c6e59 };
    const juce::Colour linkTop     { 0xff183324 };
    const juce::Colour linkBottom  { 0xff041a18 };

    float textWidth (const juce::Font& f, const juce::String& s)
    {
        return juce::GlyphArrangement::getStringWidth (f, s);
    }
}

const MoonshroomPanel::Link MoonshroomPanel::links[MoonshroomPanel::numLinks] =
{
    { "Website",      "https://moonshroom.moonshroom303.chatgpt.site/" },
    { "SoundCloud",   "https://www.soundcloud.com/moonshroom303" },
    { "YouTube",      "https://www.youtube.com/@moonshroom303" },
    { "ShroomsLoops", "https://shroomsloops-studio-231026.moonshroom303.chatgpt.site/#" },
};

//==============================================================================
void MoonshroomLinkButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);

    if (highlighted)
    {
        g.setColour (Theme::lime.withAlpha (0.18f));
        g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
    }

    juce::ColourGradient grad (linkTop, 0.0f, r.getY(), linkBottom, 0.0f, r.getBottom(), false);
    if (down)
        grad.multiplyOpacity (0.75f);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 4.0f);

    // feiner grüner Schimmer innen
    g.setColour (Theme::lime.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (2.0f), 3.0f, 2.0f);

    g.setColour (highlighted ? Theme::lime : linkBorder);
    g.drawRoundedRectangle (r, 4.0f, 1.2f);

    const float size = juce::jlimit (11.0f, 15.0f, (float) getHeight() * 0.36f);
    g.setFont (Theme::sans (size).withExtraKerningFactor (0.08f));
    g.setColour (highlighted ? Theme::lime : Theme::text);
    g.drawFittedText (getButtonText().toUpperCase(), getLocalBounds().reduced (6, 0),
                      juce::Justification::centred, 1, 0.8f);
}

//==============================================================================
MoonshroomLogoButton::MoonshroomLogoButton() : juce::Button ("Moonshroom Studio")
{
    logo = juce::ImageCache::getFromMemory (BinaryData::moonshroomstudio_png,
                                            BinaryData::moonshroomstudio_pngSize);
}

void MoonshroomLogoButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);

    g.setColour (tileBg);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (highlighted ? Theme::lime : tileBorder);
    g.drawRoundedRectangle (r, 5.0f, highlighted ? 2.0f : 1.0f);

    if (logo.isValid())
    {
        auto art = r.reduced (10.0f);
        g.drawImage (logo, art, juce::RectanglePlacement::centred);
    }
}

//==============================================================================
MoonshroomPanel::MoonshroomPanel (juce::String productName)
    : product (std::move (productName)),
      opener ([] (const juce::String& url) { juce::Process::openDocument (url, {}); })
{
    logo.setTooltip (links[0].url);
    logo.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    logo.onClick = [this] { open (links[0].url); };
    addAndMakeVisible (logo);

    for (int i = 0; i < numLinks; ++i)
    {
        auto& b = buttons[(size_t) i];
        b.setButtonText (links[i].label);
        b.setTooltip (links[i].url);
        b.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        b.onClick = [this, i] { open (links[i].url); };
        addAndMakeVisible (b);
    }

    setSize (preferredWidth, preferredHeight);
}

void MoonshroomPanel::open (const char* url)
{
    if (opener)
        opener (juce::String (url));
}

void MoonshroomPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (cardBg);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (cardBorder);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    // „entwickelt von moonshroom studio · In Your Phase“
    const auto plain = Theme::sans (14.0f);
    const auto bold = Theme::sans (14.0f, true);
    const juce::String a ("entwickelt von "), b ("moonshroom studio"), c (juce::String::fromUTF8 (" · ") + product);

    const float total = textWidth (plain, a) + textWidth (bold, b) + textWidth (plain, c);
    auto line = getLocalBounds().removeFromBottom (44).toFloat();
    float x = line.getCentreX() - total * 0.5f;

    auto draw = [&] (const juce::Font& f, juce::Colour col, const juce::String& s)
    {
        const float w = textWidth (f, s);
        g.setFont (f);
        g.setColour (col);
        g.drawText (s, juce::Rectangle<float> (x, line.getY(), w + 2.0f, line.getHeight() - 8.0f),
                    juce::Justification::centredLeft, false);
        x += w;
    };

    draw (plain, Theme::dim, a);
    draw (bold, Theme::lime, b);
    draw (plain, Theme::dim, c);
}

void MoonshroomPanel::resized()
{
    auto area = getLocalBounds().reduced (18);
    area.removeFromBottom (36); // Credit-Zeile

    const int tile = juce::jmin (area.getHeight(), area.getWidth() / 2 - 8);
    logo.setBounds (area.removeFromLeft (tile).withSizeKeepingCentre (tile, tile));
    area.removeFromLeft (16);

    const int gap = 10;
    const int h = (area.getHeight() - gap * (numLinks - 1)) / numLinks;
    for (auto& b : buttons)
    {
        b.setBounds (area.removeFromTop (h));
        area.removeFromTop (gap);
    }
}

//==============================================================================
MoonshroomOverlay::MoonshroomOverlay (juce::String productName)
    : panel (std::move (productName))
{
    addAndMakeVisible (panel);
    setWantsKeyboardFocus (true);
    setVisible (false);
}

void MoonshroomOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.62f));
}

void MoonshroomOverlay::resized()
{
    panel.setBounds (getLocalBounds().withSizeKeepingCentre (MoonshroomPanel::preferredWidth,
                                                             MoonshroomPanel::preferredHeight));
}

void MoonshroomOverlay::mouseUp (const juce::MouseEvent& e)
{
    if (! panel.getBounds().contains (e.getPosition()))
        hide();
}

bool MoonshroomOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        hide();
        return true;
    }
    return false;
}

void MoonshroomOverlay::show()
{
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
}

void MoonshroomOverlay::hide()
{
    setVisible (false);
    if (onClose)
        onClose();
}

} // namespace lp
