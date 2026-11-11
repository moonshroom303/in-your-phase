#pragma once

#include "../Analysis/ScopeAssembler.h"
#include "LimeLookAndFeel.h"

namespace lp
{

/**
    Getaktetes Oszilloskop. Takt-Modus: ein Bild pro Notenlänge, an das Raster des
    Hosts gebunden. Trigger-Modus: feste Zeit ab steigendem Nulldurchgang.
    L in Lime, R in Cyan, wahlweise der Eingang als graue Spur zum Vergleich.
*/
class ScopeView : public juce::Component
{
public:
    explicit ScopeView (juce::ValueTree settingsTree);

    void setSampleRate (double sr) { assembler.setSampleRate (sr); }
    void push (const AnalysisSample& s) noexcept { assembler.push (s); }
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void applySettings();
    juce::Rectangle<float> plotArea() const;

    juce::ValueTree settings;
    ScopeAssembler assembler;
    ScopeAssembler::Frame frame;
    bool hasFrame = false;
    float displayScale = 1.0f;

    juce::ComboBox modeBox, timeBox;
    juce::TextButton inputButton { "EINGANG" };
};

/**
    Stereofeld: Goniometer (Lissajous, Mono senkrecht, Gegenphase waagerecht)
    mit Nachleuchten, dazu Korrelationsmesser und Balance.
*/
class StereoFieldView : public juce::Component
{
public:
    void setSampleRate (double sr) { analysis.setSampleRate (sr); }
    void push (float l, float r) noexcept;
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Rectangle<float> scopeArea() const;

    StereoAnalysis analysis;
    juce::Image trail;
    std::vector<juce::Point<float>> pending;
    float scale = 1.0f;
    float shownCorrelation = 0.0f;
};

} // namespace lp
