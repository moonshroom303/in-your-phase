#include "PluginEditor.h"

namespace lp
{

InYourPhaseEditor::InYourPhaseEditor (InYourPhaseProcessor& p)
    : AudioProcessorEditor (p),
      owner (p),
      modeAttachment (*p.apvts.getParameter (ParamIDs::mode), [this] (float v) { updateMode (v > 0.5f); }),
      qualityAttachment (p.apvts, ParamIDs::quality, qualityBox),
      left (p.apvts, false),
      right (p.apvts, true),
      scope (p.apvts.state)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* b : { &coupledButton, &stereoButton })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (b);
    }
    coupledButton.setTooltip (juce::String::fromUTF8 ("Ein Regler für beide Kanäle"));
    stereoButton.setTooltip (juce::String::fromUTF8 ("Linker und rechter Kanal getrennt"));
    coupledButton.onClick = [this] { modeAttachment.setValueAsCompleteGesture (0.0f); };
    stereoButton.onClick = [this] { modeAttachment.setValueAsCompleteGesture (1.0f); };

    // Qualitätsliste aus dem Parameter
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (ParamIDs::quality)))
    {
        qualityBox.clear (juce::dontSendNotification);
        qualityBox.addItemList (choice->choices, 1);
        qualityBox.setSelectedItemIndex (choice->getIndex(), juce::dontSendNotification);
    }
    qualityBox.setTooltip (juce::String::fromUTF8 ("Genauigkeit im Bass gegen Latenz"));
    addAndMakeVisible (qualityBox);

    addAndMakeVisible (left);
    addAndMakeVisible (right);
    addAndMakeVisible (scope);
    addAndMakeVisible (stereoField);

    studioButton.setButtonText ("Moonshroom Studio");
    studioButton.setTooltip (juce::String::fromUTF8 ("Über moonshroom studio · Website, SoundCloud, YouTube, ShroomsLoops"));
    studioButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    studioButton.onClick = [this] { studio.show(); };
    addAndMakeVisible (studioButton);

    studio.onClose = [this] { studioButton.setState (juce::Button::buttonNormal); };
    addChildComponent (studio);

    // Kinder wurden vor setLookAndFeel angelegt: Theme an alle weiterreichen,
    // damit z. B. die Wertfelder der Regler neu im Lime-Stil erzeugt werden
    sendLookAndFeelChange();

    drainBuffer.resize (1 << 15);
    owner.analysis.setActive (true);

    modeAttachment.sendInitialUpdate();

    setResizable (true, true);
    setResizeLimits (1000, 640, 2000, 1300);
    setSize (1180, 720);

    startTimerHz (60);
}

InYourPhaseEditor::~InYourPhaseEditor()
{
    stopTimer();
    owner.analysis.setActive (false);
    setLookAndFeel (nullptr);
}

void InYourPhaseEditor::updateMode (bool stereo)
{
    stereoMode = stereo;
    coupledButton.setToggleState (! stereo, juce::dontSendNotification);
    stereoButton.setToggleState (stereo, juce::dontSendNotification);

    if (stereo)
        left.setTitle ("Kanal L", Theme::lime);
    else
        left.setTitle (juce::String::fromUTF8 ("L + R · gekoppelt"), Theme::lime);

    right.setTitle ("Kanal R", Theme::cyan);
    right.setVisible (stereo);
    resized();
}

void InYourPhaseEditor::timerCallback()
{
    const double sr = owner.getCurrentSampleRate();
    if (sr != lastSampleRate)
    {
        scope.setSampleRate (sr);
        stereoField.setSampleRate (sr);
        lastSampleRate = sr;
    }

    const int n = owner.analysis.pull (drainBuffer.data(), (int) drainBuffer.size());
    for (int i = 0; i < n; ++i)
    {
        const auto& s = drainBuffer[(size_t) i];
        scope.push (s);
        stereoField.push (s.outL, s.outR);
    }

    scope.refresh();
    stereoField.refresh();
    repaint (getLocalBounds().removeFromTop (54)); // Latenzanzeige
}

void InYourPhaseEditor::paint (juce::Graphics& g)
{
    using namespace juce;

    ColourGradient bg (Theme::bg, 0.0f, 0.0f, Theme::bgDeep, 0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Kopfzeile mit Lime-Linie wie in der Vorlage
    auto header = getLocalBounds().removeFromTop (54).toFloat();
    g.setColour (Theme::bgDeep.withAlpha (0.7f));
    g.fillRect (header);
    g.setColour (Theme::frame);
    g.fillRect (header.removeFromBottom (2.0f));

    auto logo = getLocalBounds().removeFromTop (52).reduced (18, 0).toFloat();
    g.setFont (Theme::sans (26.0f, true));
    g.setColour (Theme::text);
    g.drawText ("IN YOUR", logo.removeFromLeft (118.0f), Justification::centredLeft);
    g.setColour (Theme::lime);
    g.drawText ("PHASE", logo.removeFromLeft (96.0f), Justification::centredLeft);

    // Latenz
    const double sr = jmax (1.0, owner.getCurrentSampleRate());
    const double ms = 1000.0 * owner.getLatencySamples() / sr;
    auto latency = getLocalBounds().removeFromTop (52).removeFromRight (190).reduced (14, 12).toFloat();
    g.setColour (Theme::plot);
    g.fillRoundedRectangle (latency, 3.0f);
    g.setColour (Theme::frame.withAlpha (0.4f));
    g.drawRoundedRectangle (latency, 3.0f, 1.0f);
    g.setFont (Theme::sans (10.5f, true));
    g.setColour (Theme::dim);
    g.drawText ("LATENZ", latency.reduced (8.0f, 0.0f), Justification::centredLeft);
    g.setFont (Theme::mono (13.0f, true));
    g.setColour (Theme::cyan);
    g.drawText (String (ms, 1) + " ms", latency.reduced (8.0f, 0.0f), Justification::centredRight);
}

void InYourPhaseEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (54).reduced (14, 12);

    header.removeFromRight (176); // Latenzanzeige
    qualityBox.setBounds (header.removeFromRight (getWidth() >= 1100 ? 210 : 180));
    header.removeFromRight (10);
    stereoButton.setBounds (header.removeFromRight (96));
    header.removeFromRight (4);
    coupledButton.setBounds (header.removeFromRight (110));

    // Moonshroom-Studio-Taste rechts neben dem Schriftzug
    header.removeFromLeft (18 + 118 + 96 - 14 + 12);
    header.removeFromRight (12);
    studioButton.setBounds (header.removeFromLeft (juce::jmin (176, header.getWidth())));

    studio.setBounds (getLocalBounds());

    area = area.reduced (12);

    if (stereoMode)
    {
        // Zwei große Phasen-Knöpfe außen, Analyse in der Mitte
        const int side = juce::jlimit (300, 420, (int) (area.getWidth() * 0.30));
        left.setBounds (area.removeFromLeft (side));
        area.removeFromLeft (12);
        right.setBounds (area.removeFromRight (side));
        area.removeFromRight (12);

        scope.setBounds (area.removeFromTop ((int) (area.getHeight() * 0.52)));
        area.removeFromTop (12);
        stereoField.setBounds (area);
    }
    else
    {
        // Ein großer Knopf links, Analyse breit rechts
        const int side = juce::jlimit (360, 500, (int) (area.getWidth() * 0.36));
        left.setBounds (area.removeFromLeft (side));
        area.removeFromLeft (12);

        scope.setBounds (area.removeFromTop ((int) (area.getHeight() * 0.56)));
        area.removeFromTop (12);
        stereoField.setBounds (area);
    }
}

} // namespace lp
