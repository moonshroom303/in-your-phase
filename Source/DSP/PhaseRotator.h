#pragma once

#include "HilbertConvolver.h"

#include <array>
#include <atomic>

namespace lp
{

/** Genauigkeit der Hilbert-Transformation: mehr Taps reichen tiefer in den Bass, kosten aber Latenz. */
enum class Quality
{
    precise = 0,   // ab ca. 12 Hz, ca. 88 ms Latenz
    balanced = 1,  // ab ca. 25 Hz, ca. 45 ms
    fast = 2       // ab ca. 100 Hz, ca. 13 ms
};

struct ChannelParams
{
    float phaseDeg = 0.0f;  // -180 ... +180, frequenzunabhängige Phasendrehung
    float delayMs = 0.0f;   // Feinverzögerung zur Zeitausrichtung
    float gainDb = 0.0f;
    bool invert = false;    // Polarität
};

/**
    Frequenzunabhängige Phasendrehung um einen beliebigen Winkel (stufenlos, 360°):

        y = cos(φ) · x(t - D) + sin(φ) · H{x}(t - D)

    H ist ein linearphasiger FIR-Hilbert-Transformator. Bei φ = 0 kommt das Signal
    unverändert (nur verzögert) heraus, bei φ = 180° invertiert. Danach folgen
    Pegel/Polarität und eine Feinverzögerung mit Lagrange-Interpolation.

    Echtzeitsicher: prepare() legt alles an, process() alloziert nichts.
    Qualitätswechsel passieren synchron an einer Blockgrenze mit kurzem Aus- und
    Einblenden, auch beim Offline-Rendern.
*/
class PhaseRotator
{
public:
    static constexpr int maxChannels = 2;
    static constexpr double maxDelayMs = 25.0;

    static int tapsFor (Quality q, double sampleRate) noexcept;
    static int partitionSizeFor (double sampleRate) noexcept;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;

    /** Wechselt die Qualität. Darf vom Audio-Thread aufgerufen werden. */
    void setQuality (Quality q) noexcept { targetQuality.store ((int) q); }
    Quality getTargetQuality() const noexcept { return (Quality) targetQuality.load(); }

    /** Latenz in Samples für eine Qualitätsstufe (Hilbert-Mitte + Partitionsgröße). */
    int latencyFor (Quality q) const noexcept;
    int getLatencySamples() const noexcept { return latencyFor (getTargetQuality()); }

    void setParams (int channel, const ChannelParams& params) noexcept;

    /**
        Verarbeitet io an Ort und Stelle. alignedInput (optional, gleiche Größe)
        bekommt das Eingangssignal, um die Verarbeitungslatenz verzögert, für die Analyse.
    */
    void process (juce::AudioBuffer<float>& io, juce::AudioBuffer<float>* alignedInput) noexcept;

private:
    struct Channel
    {
        PartitionedConvolver convolver;
        std::vector<float> directRing;
        int writePos = 0;

        double phaseCurrent = 0.0;
        double phaseTarget = 0.0;
        float cosValue = 1.0f;
        float sinValue = 0.0f;

        juce::SmoothedValue<float> gain;
        juce::SmoothedValue<float> delaySamples;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> fineDelay;
    };

    void updateFade (int numSamples) noexcept;

    double sampleRate = 48000.0;
    int partitionSize = 128;
    int numActiveChannels = 2;
    double phaseCoeff = 0.0;

    std::array<PartitionedKernel, 3> kernels;
    std::array<Channel, maxChannels> channels;

    std::atomic<int> targetQuality { (int) Quality::balanced };
    int activeQuality = (int) Quality::balanced;
    int activeLatency = 0;

    // Aus- und Einblenden beim Qualitätswechsel
    enum class FadeState { running, fadingOut, fadingIn };
    FadeState fadeState = FadeState::running;
    int fadeLength = 480;
    int fadePos = 0;
    std::vector<float> fadeGains;
};

} // namespace lp
