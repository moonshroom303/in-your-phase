#include "PhaseRotator.h"

#include <cmath>

namespace lp
{

namespace
{
double wrap180 (double deg) noexcept
{
    deg = std::fmod (deg + 180.0, 360.0);
    if (deg < 0.0)
        deg += 360.0;
    return deg - 180.0;
}
} // namespace

int PhaseRotator::tapsFor (Quality q, double sr) noexcept
{
    // Basislängen bei 48 kHz, mit der Samplerate skaliert, damit die Bassgrenze gleich bleibt
    static constexpr int base[] = { 8193, 4097, 1025 };
    int taps = (int) std::lround (base[(int) q] * sr / 48000.0);
    return taps | 1; // ungerade
}

int PhaseRotator::partitionSizeFor (double sr) noexcept
{
    if (sr <= 64000.0)  return 128;
    if (sr <= 128000.0) return 256;
    return 512;
}

int PhaseRotator::latencyFor (Quality q) const noexcept
{
    return (tapsFor (q, sampleRate) - 1) / 2 + partitionSize;
}

void PhaseRotator::prepare (double sr, int maxBlockSize, int numChannels)
{
    sampleRate = sr;
    partitionSize = partitionSizeFor (sr);
    numActiveChannels = juce::jlimit (1, maxChannels, numChannels);

    int order = 0;
    while ((1 << order) < 2 * partitionSize)
        ++order;

    juce::dsp::FFT fft (order);
    int maxPartitions = 0;
    int maxLatency = 0;

    for (int q = 0; q < 3; ++q)
    {
        kernels[(size_t) q] = makeKernel (designHilbert (tapsFor ((Quality) q, sr)), partitionSize, fft);
        maxPartitions = juce::jmax (maxPartitions, kernels[(size_t) q].numPartitions);
        maxLatency = juce::jmax (maxLatency, latencyFor ((Quality) q));
    }

    activeQuality = targetQuality.load();
    activeLatency = latencyFor ((Quality) activeQuality);

    const int maxDelay = (int) std::ceil (maxDelayMs * 0.001 * sr) + 4;

    for (auto& ch : channels)
    {
        ch.convolver.prepare (partitionSize, maxPartitions);
        ch.convolver.setKernel (&kernels[(size_t) activeQuality]);
        ch.directRing.assign ((size_t) maxLatency + 1, 0.0f);
        ch.writePos = 0;

        ch.gain.reset (sr, 0.02);
        ch.delaySamples.reset (sr, 0.05);

        ch.fineDelay.setMaximumDelayInSamples (maxDelay);
        ch.fineDelay.prepare ({ sr, (juce::uint32) juce::jmax (1, maxBlockSize), 1 });
    }

    // Winkel gleitet mit ca. 20 ms Zeitkonstante zum Ziel
    phaseCoeff = 1.0 - std::exp (-1.0 / (0.02 * sr));

    fadeLength = juce::jmax (16, (int) (0.01 * sr));
    fadeGains.assign ((size_t) juce::jmax (1, maxBlockSize), 1.0f);

    reset();
}

void PhaseRotator::reset() noexcept
{
    for (auto& ch : channels)
    {
        ch.convolver.reset();
        std::fill (ch.directRing.begin(), ch.directRing.end(), 0.0f);
        ch.writePos = 0;
        ch.phaseCurrent = ch.phaseTarget;
        ch.cosValue = (float) std::cos (juce::degreesToRadians (ch.phaseCurrent));
        ch.sinValue = (float) std::sin (juce::degreesToRadians (ch.phaseCurrent));
        ch.gain.setCurrentAndTargetValue (ch.gain.getTargetValue());
        ch.delaySamples.setCurrentAndTargetValue (ch.delaySamples.getTargetValue());
        ch.fineDelay.reset();
    }

    fadeState = FadeState::running;
    fadePos = 0;
}

void PhaseRotator::setParams (int channel, const ChannelParams& p) noexcept
{
    if (! juce::isPositiveAndBelow (channel, maxChannels))
        return;

    auto& ch = channels[(size_t) channel];
    ch.phaseTarget = wrap180 ((double) p.phaseDeg);

    const float g = juce::Decibels::decibelsToGain (p.gainDb) * (p.invert ? -1.0f : 1.0f);
    ch.gain.setTargetValue (g);

    const float d = (float) (juce::jlimit (0.0, maxDelayMs, (double) p.delayMs) * 0.001 * sampleRate);
    ch.delaySamples.setTargetValue (d);
}

void PhaseRotator::updateFade (int n) noexcept
{
    // Umschalten nur an Blockgrenzen, wenn das Ausblenden fertig ist
    if (fadeState == FadeState::fadingOut && fadePos >= fadeLength)
    {
        activeQuality = targetQuality.load();
        activeLatency = latencyFor ((Quality) activeQuality);

        for (auto& ch : channels)
            ch.convolver.setKernel (&kernels[(size_t) activeQuality]);

        fadeState = FadeState::fadingIn;
        fadePos = 0;
    }

    if (fadeState == FadeState::running && targetQuality.load() != activeQuality)
    {
        fadeState = FadeState::fadingOut;
        fadePos = 0;
    }

    for (int i = 0; i < n; ++i)
    {
        switch (fadeState)
        {
            case FadeState::running:
                fadeGains[(size_t) i] = 1.0f;
                break;

            case FadeState::fadingOut:
                fadeGains[(size_t) i] = 1.0f - (float) juce::jmin (fadePos, fadeLength) / (float) fadeLength;
                ++fadePos;
                break;

            case FadeState::fadingIn:
                fadeGains[(size_t) i] = (float) juce::jmin (fadePos, fadeLength) / (float) fadeLength;
                if (++fadePos >= fadeLength)
                    fadeState = FadeState::running;
                break;
        }
    }
}

void PhaseRotator::process (juce::AudioBuffer<float>& io, juce::AudioBuffer<float>* alignedInput) noexcept
{
    const int n = juce::jmin (io.getNumSamples(), (int) fadeGains.size());
    const int numCh = juce::jmin (io.getNumChannels(), numActiveChannels);

    updateFade (n);

    for (int c = 0; c < numCh; ++c)
    {
        auto& ch = channels[(size_t) c];
        float* data = io.getWritePointer (c);
        float* aligned = (alignedInput != nullptr && c < alignedInput->getNumChannels())
                             ? alignedInput->getWritePointer (c) : nullptr;

        const int ringSize = (int) ch.directRing.size();

        for (int i = 0; i < n; ++i)
        {
            const float x = data[i];

            // Hilbert-Anteil, zentriert bei activeLatency
            const float h = ch.convolver.processSample (x);

            // Direkter Anteil, gleich weit verzögert
            ch.directRing[(size_t) ch.writePos] = x;
            int r = ch.writePos - activeLatency;
            if (r < 0)
                r += ringSize;
            const float d = ch.directRing[(size_t) r];
            if (++ch.writePos == ringSize)
                ch.writePos = 0;

            // Winkel auf dem kürzesten Weg zum Ziel gleiten lassen
            const double diff = wrap180 (ch.phaseTarget - ch.phaseCurrent);
            if (diff != 0.0)
            {
                if (std::abs (diff) < 1.0e-4)
                    ch.phaseCurrent = ch.phaseTarget;
                else
                    ch.phaseCurrent = wrap180 (ch.phaseCurrent + diff * phaseCoeff);

                const double rad = juce::degreesToRadians (ch.phaseCurrent);
                ch.cosValue = (float) std::cos (rad);
                ch.sinValue = (float) std::sin (rad);
            }

            float y = ch.cosValue * d + ch.sinValue * h;
            y *= ch.gain.getNextValue();

            ch.fineDelay.pushSample (0, y);
            y = ch.fineDelay.popSample (0, ch.delaySamples.getNextValue());

            data[i] = y * fadeGains[(size_t) i];

            if (aligned != nullptr)
                aligned[i] = d;
        }
    }
}

} // namespace lp
