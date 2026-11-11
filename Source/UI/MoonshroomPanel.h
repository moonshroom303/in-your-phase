#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lp
{

/** Grüne Link-Taste im Hendrix-Stil */
class MoonshroomLinkButton : public juce::Button
{
public:
    MoonshroomLinkButton() : juce::Button ({}) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

/** Logo-Kachel, führt zur Website */
class MoonshroomLogoButton : public juce::Button
{
public:
    MoonshroomLogoButton();
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Image logo;
};

/** Moonshroom-Studio-Karte wie in Hendrix: Logo, vier Links, Credit-Zeile */
class MoonshroomPanel : public juce::Component
{
public:
    struct Link { const char* label; const char* url; };

    static constexpr int numLinks = 4;
    static const Link links[numLinks];

    /** Öffnet eine Adresse; für Tests austauschbar */
    using Opener = std::function<void (const juce::String& url)>;

    explicit MoonshroomPanel (juce::String productName);

    void paint (juce::Graphics&) override;
    void resized() override;

    void setOpener (Opener o) { opener = std::move (o); }

    juce::Button& getLinkButton (int index) { return buttons[(size_t) index]; }
    juce::Button& getLogoButton() { return logo; }

    /** Bevorzugte Größe der Karte */
    static constexpr int preferredWidth = 570, preferredHeight = 310;

private:
    void open (const char* url);

    juce::String product;
    Opener opener;
    MoonshroomLogoButton logo;
    std::array<MoonshroomLinkButton, numLinks> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoonshroomPanel)
};

/** Halbtransparente Ebene über dem Editor, Karte in der Mitte.
    Klick daneben oder Escape schließt. */
class MoonshroomOverlay : public juce::Component
{
public:
    explicit MoonshroomOverlay (juce::String productName);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void show();
    void hide();

    MoonshroomPanel panel;
    std::function<void()> onClose;
};

} // namespace lp
