#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>

namespace lp
{

/** Ein Messpunkt: Ausgang, zeitgleich ausgerichteter Eingang und musikalische Position. */
struct AnalysisSample
{
    float outL = 0.0f, outR = 0.0f;
    float inL = 0.0f, inR = 0.0f;
    double ppq = -1.0;        // Position in Vierteln am Ausgang, < 0: Transport steht
    float bpm = 0.0f;         // 0: unbekannt
    float beatsPerBar = 4.0f;
};

/**
    Sperrfreie Übergabe vom Audio-Thread (ein Schreiber) an die Oberfläche (ein Leser).
    Ist der Puffer voll oder keine Oberfläche offen, werden Daten verworfen.
*/
class AnalysisFifo
{
public:
    explicit AnalysisFifo (int capacity = 1 << 15) : fifo (capacity), data ((size_t) capacity) {}

    /** Audio-Thread */
    void push (const AnalysisSample* samples, int count) noexcept
    {
        if (! active.load (std::memory_order_relaxed))
            return;

        int s1, n1, s2, n2;
        fifo.prepareToWrite (count, s1, n1, s2, n2);

        for (int i = 0; i < n1; ++i) data[(size_t) (s1 + i)] = samples[i];
        for (int i = 0; i < n2; ++i) data[(size_t) (s2 + i)] = samples[n1 + i];

        fifo.finishedWrite (n1 + n2);
    }

    /** Oberfläche: holt bis zu maxCount Punkte, gibt die Anzahl zurück. */
    int pull (AnalysisSample* dest, int maxCount) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (maxCount, s1, n1, s2, n2);

        for (int i = 0; i < n1; ++i) dest[i] = data[(size_t) (s1 + i)];
        for (int i = 0; i < n2; ++i) dest[n1 + i] = data[(size_t) (s2 + i)];

        fifo.finishedRead (n1 + n2);
        return n1 + n2;
    }

    /** Oberfläche meldet sich an/ab. Beim Anmelden werden alte Daten verworfen. */
    void setActive (bool shouldBeActive) noexcept
    {
        if (shouldBeActive)
            fifo.reset();

        active.store (shouldBeActive);
    }

private:
    juce::AbstractFifo fifo;
    std::vector<AnalysisSample> data;
    std::atomic<bool> active { false };
};

} // namespace lp
