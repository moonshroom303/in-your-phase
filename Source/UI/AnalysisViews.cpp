#include "AnalysisViews.h"

namespace lp
{

//==============================================================================
// ScopeView

ScopeView::ScopeView (juce::ValueTree settingsTree) : settings (settingsTree)
{
    addAndMakeVisible (modeBox);
    modeBox.addItem ("TAKT", 1);
    modeBox.addItem ("TRIGGER", 2);
    modeBox.setTooltip (juce::String::fromUTF8 ("Takt: an das Raster des Hosts gebunden · Trigger: ab steigendem Nulldurchgang"));
    modeBox.onChange = [this]
    {
        settings.setProperty ("scopeMode", modeBox.getSelectedId() == 2 ? 1 : 0, nullptr);
        applySettings();
    };

    addAndMakeVisible (timeBox);
    timeBox.setTooltip (juce::String::fromUTF8 ("Länge eines Bildes"));
    timeBox.onChange = [this]
    {
        const int idx = timeBox.getSelectedId() - 1;
        if (idx < 0)
            return;
        settings.setProperty (settings.getProperty ("scopeMode", 0).operator int() == 1 ? "scopeTrigger" : "scopeSync", idx, nullptr);
        applySettings();
    };

    addAndMakeVisible (inputButton);
    inputButton.setClickingTogglesState (true);
    inputButton.setTooltip ("Eingang zum Vergleich grau einblenden");
    inputButton.onClick = [this]
    {
        settings.setProperty ("scopeInput", inputButton.getToggleState(), nullptr);
        repaint();
    };

    applySettings();
}

void ScopeView::applySettings()
{
    const bool trigger = (int) settings.getProperty ("scopeMode", 0) == 1;
    const int index = (int) settings.getProperty (trigger ? "scopeTrigger" : "scopeSync", trigger ? 3 : 2);

    modeBox.setSelectedId (trigger ? 2 : 1, juce::dontSendNotification);

    timeBox.clear (juce::dontSendNotification);
    const auto& names = trigger ? ScopeAssembler::triggerNames() : ScopeAssembler::syncNames();
    for (int i = 0; i < names.size(); ++i)
        timeBox.addItem (names[i], i + 1);
    timeBox.setSelectedId (index + 1, juce::dontSendNotification);

    inputButton.setToggleState ((bool) settings.getProperty ("scopeInput", false), juce::dontSendNotification);

    assembler.setMode (trigger ? ScopeAssembler::Mode::trigger : ScopeAssembler::Mode::sync, index);
    hasFrame = false;
    repaint();
}

void ScopeView::refresh()
{
    if (! assembler.takeFrame (frame))
        return;

    hasFrame = true;

    float peak = 0.0f;
    const bool showInput = inputButton.getToggleState();
    for (int c = 0; c < ScopeAssembler::columns; ++c)
    {
        for (auto* t : { &frame.outL, &frame.outR })
            if (t->has (c))
                peak = juce::jmax (peak, std::abs (t->lo[(size_t) c]), std::abs (t->hi[(size_t) c]));

        if (showInput)
            for (auto* t : { &frame.inL, &frame.inR })
                if (t->has (c))
                    peak = juce::jmax (peak, std::abs (t->lo[(size_t) c]), std::abs (t->hi[(size_t) c]));
    }

    // Automatische Skalierung: schnell nach oben, langsam nach unten
    const float target = juce::jmax (0.02f, peak * 1.1f);
    displayScale = target > displayScale ? target : displayScale + (target - displayScale) * 0.08f;
    repaint();
}

juce::Rectangle<float> ScopeView::plotArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f).withTrimmedTop (50.0f);
}

void ScopeView::resized()
{
    auto header = getLocalBounds().reduced (10).withTrimmedTop (28).removeFromTop (24);
    inputButton.setBounds (header.removeFromRight (86));
    header.removeFromRight (6);
    timeBox.setBounds (header.removeFromRight (100));
    header.removeFromRight (6);
    modeBox.setBounds (header.removeFromRight (100));
}

void ScopeView::paint (juce::Graphics& g)
{
    using namespace juce;

    Theme::drawPanel (g, getLocalBounds().toFloat(), String::fromUTF8 ("Oszilloskop · getaktet"));

    const auto plot = plotArea();
    g.setColour (Theme::plot);
    g.fillRoundedRectangle (plot, 4.0f);

    // Raster
    const float cy = plot.getCentreY();
    g.setColour (Theme::grid);
    g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * 0.25f), plot.getX(), plot.getRight());
    g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * 0.75f), plot.getX(), plot.getRight());
    g.setColour (Theme::grid.brighter (0.4f));
    g.drawHorizontalLine ((int) cy, plot.getX(), plot.getRight());

    g.setFont (Theme::mono (10.5f));

    if (! hasFrame || frame.synced)
    {
        const double length = hasFrame ? frame.lengthBeats : 1.0;
        const double beatsPerBar = hasFrame ? frame.beatsPerBar : 4.0;
        const bool subBeat = length <= 1.0;
        const int divisions = subBeat ? 4 : juce::jmax (1, (int) std::round (length));

        for (int i = 0; i <= divisions; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) divisions;
            const bool bar = ! subBeat && i % juce::jmax (1, (int) std::round (beatsPerBar)) == 0;
            g.setColour (bar ? Theme::frame.withAlpha (0.45f) : Theme::grid.brighter (subBeat ? 0.1f : 0.25f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());

            if (! subBeat && i < divisions)
            {
                g.setColour (Theme::dim);
                g.drawText (String (i + 1), Rectangle<float> (x + 3.0f, plot.getY() + 2.0f, 30.0f, 14.0f), Justification::left);
            }
        }
    }
    else
    {
        for (int i = 0; i <= 10; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / 10.0f;
            g.setColour (Theme::grid.brighter (i % 5 == 0 ? 0.3f : 0.05f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        }
        g.setColour (Theme::dim);
        g.drawText ("0", plot.withTrimmedLeft (4.0f).removeFromBottom (16.0f), Justification::left);
        g.drawText (String (frame.lengthMs, 0) + " ms", plot.withTrimmedRight (4.0f).removeFromBottom (16.0f), Justification::right);
    }

    if (! hasFrame)
    {
        g.setColour (Theme::dim);
        g.setFont (Theme::sans (13.0f));
        g.drawText (String::fromUTF8 ("Warte auf Signal …"), plot, Justification::centred);
        return;
    }

    const float halfHeight = plot.getHeight() * 0.46f;
    auto mapY = [&] (float v) { return cy - v / displayScale * halfHeight; };

    auto drawTrace = [&] (const ScopeAssembler::Trace& t, Colour colour)
    {
        Path mid;
        bool started = false;

        for (int c = 0; c < ScopeAssembler::columns; ++c)
        {
            if (! t.has (c))
                continue;

            const float x = plot.getX() + plot.getWidth() * ((float) c + 0.5f) / (float) ScopeAssembler::columns;
            const float yTop = mapY (t.hi[(size_t) c]);
            const float yBottom = mapY (t.lo[(size_t) c]);

            g.setColour (colour.withAlpha (colour.getFloatAlpha() * 0.45f));
            g.fillRect (x - 0.6f, yTop, 1.2f, jmax (1.0f, yBottom - yTop));

            const float ym = mapY (0.5f * (t.lo[(size_t) c] + t.hi[(size_t) c]));
            if (! started)
            {
                mid.startNewSubPath (x, ym);
                started = true;
            }
            else
            {
                mid.lineTo (x, ym);
            }
        }

        g.setColour (colour);
        g.strokePath (mid, PathStrokeType (1.4f));
    };

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    if (inputButton.getToggleState())
    {
        drawTrace (frame.inL, Colours::white.withAlpha (0.28f));
        drawTrace (frame.inR, Colours::white.withAlpha (0.18f));
    }

    drawTrace (frame.outR, Theme::cyan.withAlpha (0.9f));
    drawTrace (frame.outL, Theme::lime);
    g.restoreState();

    // Legende und Skala
    auto legend = plot.reduced (8.0f, 4.0f).removeFromBottom (16.0f);
    g.setFont (Theme::mono (11.0f, true));
    g.setColour (Theme::lime);
    g.drawText ("L", legend.removeFromLeft (14.0f), Justification::left);
    g.setColour (Theme::cyan);
    g.drawText ("R", legend.removeFromLeft (14.0f), Justification::left);
    if (inputButton.getToggleState())
    {
        g.setColour (Colours::white.withAlpha (0.5f));
        g.drawText ("EIN", legend.removeFromLeft (34.0f), Justification::left);
    }

    g.setColour (Theme::dim);
    g.setFont (Theme::mono (10.5f));
    auto info = plot.reduced (8.0f, 4.0f).removeFromTop (14.0f);
    g.drawText (String::fromUTF8 ("±") + String (displayScale, 2), info, Justification::right);
    if (frame.synced)
        g.drawText (frame.fromHostTransport ? "TRANSPORT" : "INTERNE UHR", info.withTrimmedRight (60.0f), Justification::right);
}

//==============================================================================
// StereoFieldView

void StereoFieldView::push (float l, float r) noexcept
{
    analysis.push (l, r);
    if (pending.size() < 8192)
        pending.emplace_back (l, r);
}

juce::Rectangle<float> StereoFieldView::scopeArea() const
{
    auto area = getLocalBounds().toFloat().reduced (12.0f).withTrimmedTop (26.0f);
    const bool wide = area.getWidth() > area.getHeight() * 1.35f;
    const float size = wide ? juce::jmin (area.getHeight(), area.getWidth() * 0.55f)
                            : juce::jmin (area.getWidth(), area.getHeight() - 92.0f);
    return wide ? area.removeFromLeft (size).withSizeKeepingCentre (size, size)
                : area.removeFromTop (size).withSizeKeepingCentre (size, size);
}

void StereoFieldView::resized()
{
    const auto area = scopeArea().toNearestInt();
    if (area.getWidth() > 0 && area.getHeight() > 0)
    {
        trail = juce::Image (juce::Image::RGB, area.getWidth(), area.getHeight(), true);
        juce::Graphics ig (trail);
        ig.fillAll (Theme::plot);
    }
}

void StereoFieldView::refresh()
{
    if (! trail.isValid())
    {
        pending.clear();
        return;
    }

    const float target = juce::jmax (0.02f, analysis.getPeak());
    scale = target > scale ? target : scale + (target - scale) * 0.05f;

    juce::Graphics ig (trail);
    ig.setColour (Theme::plot.withAlpha (0.20f)); // Nachleuchten
    ig.fillAll();

    const float w = (float) trail.getWidth(), h = (float) trail.getHeight();
    const float radius = juce::jmin (w, h) * 0.46f;
    const float cx = w * 0.5f, cy = h * 0.5f;
    const float k = 0.70710678f / scale * radius;

    ig.setColour (Theme::lime.withAlpha (0.55f));
    for (const auto& p : pending)
    {
        const float x = cx + (p.y - p.x) * k;   // rechts: R
        const float y = cy - (p.x + p.y) * k;   // oben: Mono
        ig.fillRect (x - 0.75f, y - 0.75f, 1.5f, 1.5f);
    }
    pending.clear();

    shownCorrelation += (analysis.getCorrelation() - shownCorrelation) * 0.25f;
    repaint();
}

void StereoFieldView::paint (juce::Graphics& g)
{
    using namespace juce;

    Theme::drawPanel (g, getLocalBounds().toFloat(), "Stereofeld");

    const auto area = scopeArea();
    if (trail.isValid())
        g.drawImage (trail, area);

    // Achsen
    const auto c = area.getCentre();
    const float radius = jmin (area.getWidth(), area.getHeight()) * 0.46f;
    g.setColour (Theme::frame.withAlpha (0.4f));
    g.drawRoundedRectangle (area, 4.0f, 1.0f);
    g.setColour (Theme::grid.brighter (0.3f));
    g.drawEllipse (Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c), 1.0f);
    const float d = radius * 0.7071f;
    g.drawLine (c.x - d, c.y - d, c.x + d, c.y + d, 1.0f);
    g.drawLine (c.x + d, c.y - d, c.x - d, c.y + d, 1.0f);
    g.setColour (Theme::grid.brighter (0.6f));
    g.drawLine (c.x, c.y - radius, c.x, c.y + radius, 1.0f);
    g.drawLine (c.x - radius, c.y, c.x + radius, c.y, 1.0f);

    g.setFont (Theme::mono (11.0f, true));
    g.setColour (Theme::lime);
    g.drawText ("L", Rectangle<float> (c.x - d - 16.0f, c.y - d - 16.0f, 14.0f, 14.0f), Justification::centred);
    g.setColour (Theme::cyan);
    g.drawText ("R", Rectangle<float> (c.x + d + 2.0f, c.y - d - 16.0f, 14.0f, 14.0f), Justification::centred);
    g.setColour (Theme::dim);
    g.drawText ("M", Rectangle<float> (c.x - 7.0f, area.getY() + 2.0f, 14.0f, 14.0f), Justification::centred);
    g.drawText ("-S", Rectangle<float> (area.getX() + 3.0f, c.y - 16.0f, 22.0f, 14.0f), Justification::left);
    g.drawText ("+S", Rectangle<float> (area.getRight() - 25.0f, c.y - 16.0f, 22.0f, 14.0f), Justification::right);

    // Messbalken: neben oder unter dem Goniometer
    auto rest = getLocalBounds().toFloat().reduced (12.0f).withTrimmedTop (26.0f);
    const bool wide = rest.getWidth() > rest.getHeight() * 1.35f;
    auto meters = wide ? rest.withTrimmedLeft (area.getWidth() + 16.0f) : rest.withTrimmedTop (area.getHeight() + 12.0f);
    meters = meters.withSizeKeepingCentre (meters.getWidth(), jmin (meters.getHeight(), 92.0f));

    auto drawBar = [&] (Rectangle<float> r, const String& caption, float value, float minV, float maxV,
                        const String& valueText, bool negativeIsBad)
    {
        auto top = r.removeFromTop (16.0f);
        g.setFont (Theme::sans (11.0f, true));
        g.setColour (Theme::dim);
        g.drawText (caption, top, Justification::left);
        g.setFont (Theme::mono (12.0f, true));
        g.setColour (Theme::cyan);
        g.drawText (valueText, top, Justification::right);

        auto bar = r.removeFromTop (14.0f);
        g.setColour (Theme::plot);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (Theme::frame.withAlpha (0.35f));
        g.drawRoundedRectangle (bar, 3.0f, 1.0f);

        auto toX = [&] (float v) { return jmap (jlimit (minV, maxV, v), minV, maxV, bar.getX(), bar.getRight()); };
        const float zero = toX (0.0f);
        const float x = toX (value);
        const bool bad = negativeIsBad && value < 0.0f;
        g.setColour ((bad ? Theme::danger : Theme::lime).withAlpha (0.85f));
        g.fillRect (Rectangle<float> (jmin (zero, x), bar.getY() + 2.0f, std::abs (x - zero), bar.getHeight() - 4.0f));
        g.setColour (Theme::text);
        g.fillRect (x - 1.0f, bar.getY(), 2.0f, bar.getHeight());
        g.setColour (Theme::dim.withAlpha (0.8f));
        g.fillRect (zero - 0.5f, bar.getY(), 1.0f, bar.getHeight());

        auto scaleRow = r.removeFromTop (14.0f);
        g.setFont (Theme::mono (10.0f));
        g.setColour (Theme::dim);
        g.drawText (String (minV, 0), scaleRow, Justification::left);
        g.drawText ("0", scaleRow, Justification::centred);
        g.drawText ((maxV > 0 ? "+" : "") + String (maxV, 0), scaleRow, Justification::right);
    };

    auto first = meters.removeFromTop (46.0f);
    meters.removeFromTop (4.0f);
    drawBar (first, "KORRELATION", shownCorrelation, -1.0f, 1.0f,
             (shownCorrelation >= 0.0f ? "+" : "") + String (shownCorrelation, 2), true);

    const float balance = analysis.getBalanceDb();
    const String balanceText = std::abs (balance) < 0.1f ? String ("MITTE")
                             : (balance > 0.0f ? "R +" : "L +") + String (std::abs (balance), 1) + " dB";
    drawBar (meters.removeFromTop (46.0f), "BALANCE", balance, -12.0f, 12.0f, balanceText, false);
}

} // namespace lp
