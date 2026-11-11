#pragma once

#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>

namespace lp
{

/**
    Linearphasiger FIR-Hilbert-Transformator: ungerade Länge, antisymmetrisch,
    Kaiser-Fenster. Verschiebt alle Frequenzen um genau 90°, die Mitte liegt bei
    Tap (numTaps - 1) / 2.
*/
std::vector<float> designHilbert (int numTaps, double kaiserBeta = 8.0);

/** Vorberechnete Spektren eines FIR-Filters, in Blöcke der Länge partitionSize zerlegt. */
struct PartitionedKernel
{
    int numTaps = 0;
    int partitionSize = 0;
    int numPartitions = 0;
    std::vector<std::vector<float>> spectra; // pro Partition (P + 1) komplexe Werte, interleaved
};

/** Zerlegt taps in Partitionen und transformiert sie. fft muss die Größe 2 * partitionSize haben. */
PartitionedKernel makeKernel (const std::vector<float>& taps, int partitionSize, const juce::dsp::FFT& fft);

/**
    Gleichmäßig partitionierte Faltung (overlap-save) für einen Kanal.
    Latenz: genau partitionSize Samples. Der Kernel lässt sich jederzeit wechseln,
    die Eingangshistorie bleibt erhalten, solange der neue Kernel nicht mehr
    Partitionen hat als bei prepare() angegeben.
*/
class PartitionedConvolver
{
public:
    void prepare (int partitionSize, int maxPartitions);
    void reset() noexcept;
    void setKernel (const PartitionedKernel* newKernel) noexcept { kernel = newKernel; }

    float processSample (float x) noexcept
    {
        inputBlock[(size_t) (P + pos)] = x;
        const float y = outputBlock[(size_t) pos];

        if (++pos == P)
        {
            processPartition();
            pos = 0;
        }

        return y;
    }

private:
    void processPartition() noexcept;

    int P = 0;
    int maxParts = 0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> inputBlock;   // 2P: vorheriger + aktueller Block
    std::vector<float> outputBlock;  // P
    std::vector<float> fftBuffer;    // 4P (2 * FFT-Größe)
    std::vector<float> accum;        // 2 * (P + 1)
    std::vector<std::vector<float>> history; // Eingangsspektren der letzten maxParts Blöcke
    int historyHead = 0;
    int pos = 0;
    const PartitionedKernel* kernel = nullptr;
};

} // namespace lp
