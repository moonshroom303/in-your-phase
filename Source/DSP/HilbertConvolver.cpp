#include "HilbertConvolver.h"

#include <cmath>

namespace lp
{

namespace
{
double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    const double q = x * x / 4.0;
    for (int k = 1; k < 64; ++k)
    {
        term *= q / ((double) k * (double) k);
        sum += term;
        if (term < sum * 1.0e-12)
            break;
    }
    return sum;
}
} // namespace

std::vector<float> designHilbert (int numTaps, double kaiserBeta)
{
    jassert (numTaps % 2 == 1 && numTaps >= 3);

    const int M = (numTaps - 1) / 2;
    const double norm = besselI0 (kaiserBeta);
    std::vector<float> h ((size_t) numTaps, 0.0f);

    for (int i = 0; i < numTaps; ++i)
    {
        const int n = i - M;
        if (n % 2 == 0)
            continue; // gerade Abstände zur Mitte sind beim idealen Hilbert-Filter null

        const double ideal = 2.0 / (juce::MathConstants<double>::pi * n);
        const double r = (double) n / (double) M;
        const double window = besselI0 (kaiserBeta * std::sqrt (juce::jmax (0.0, 1.0 - r * r))) / norm;
        h[(size_t) i] = (float) (ideal * window);
    }

    return h;
}

PartitionedKernel makeKernel (const std::vector<float>& taps, int partitionSize, const juce::dsp::FFT& fft)
{
    jassert (fft.getSize() == 2 * partitionSize);

    PartitionedKernel k;
    k.numTaps = (int) taps.size();
    k.partitionSize = partitionSize;
    k.numPartitions = (k.numTaps + partitionSize - 1) / partitionSize;

    const int fftSize = fft.getSize();
    std::vector<float> buffer ((size_t) (2 * fftSize), 0.0f);

    // Skalierung der Rücktransformation messen (je nach FFT-Engine 1 oder fftSize)
    buffer[0] = 1.0f;
    fft.performRealOnlyForwardTransform (buffer.data(), true);
    fft.performRealOnlyInverseTransform (buffer.data());
    const float scale = buffer[0];
    jassert (scale > 0.0f);

    const int numFloats = 2 * (partitionSize + 1);

    for (int p = 0; p < k.numPartitions; ++p)
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);

        for (int i = 0; i < partitionSize; ++i)
        {
            const int t = p * partitionSize + i;
            if (t < k.numTaps)
                buffer[(size_t) i] = taps[(size_t) t];
        }

        fft.performRealOnlyForwardTransform (buffer.data(), true);

        std::vector<float> spectrum ((size_t) numFloats);
        for (int i = 0; i < numFloats; ++i)
            spectrum[(size_t) i] = buffer[(size_t) i] / scale;

        k.spectra.push_back (std::move (spectrum));
    }

    return k;
}

void PartitionedConvolver::prepare (int partitionSize, int maxPartitions)
{
    P = partitionSize;
    maxParts = juce::jmax (1, maxPartitions);

    int order = 0;
    while ((1 << order) < 2 * P)
        ++order;

    fft = std::make_unique<juce::dsp::FFT> (order);

    inputBlock.assign ((size_t) (2 * P), 0.0f);
    outputBlock.assign ((size_t) P, 0.0f);
    fftBuffer.assign ((size_t) (4 * P), 0.0f);
    accum.assign ((size_t) (2 * (P + 1)), 0.0f);
    history.assign ((size_t) maxParts, std::vector<float> ((size_t) (2 * (P + 1)), 0.0f));
    reset();
}

void PartitionedConvolver::reset() noexcept
{
    std::fill (inputBlock.begin(), inputBlock.end(), 0.0f);
    std::fill (outputBlock.begin(), outputBlock.end(), 0.0f);
    for (auto& h : history)
        std::fill (h.begin(), h.end(), 0.0f);
    historyHead = 0;
    pos = 0;
}

void PartitionedConvolver::processPartition() noexcept
{
    // Eingangsspektrum aus [vorheriger Block, aktueller Block]
    std::copy (inputBlock.begin(), inputBlock.end(), fftBuffer.begin());
    std::fill (fftBuffer.begin() + 2 * P, fftBuffer.end(), 0.0f);
    fft->performRealOnlyForwardTransform (fftBuffer.data(), true);

    const int numFloats = 2 * (P + 1);
    auto& newest = history[(size_t) historyHead];
    std::copy (fftBuffer.begin(), fftBuffer.begin() + numFloats, newest.begin());

    std::fill (accum.begin(), accum.end(), 0.0f);

    if (kernel != nullptr)
    {
        const int parts = juce::jmin (kernel->numPartitions, maxParts);

        for (int k = 0; k < parts; ++k)
        {
            int idx = historyHead - k;
            if (idx < 0)
                idx += maxParts;

            const float* x = history[(size_t) idx].data();
            const float* h = kernel->spectra[(size_t) k].data();
            float* a = accum.data();

            for (int b = 0; b < numFloats; b += 2)
            {
                const float xr = x[b], xi = x[b + 1];
                const float hr = h[b], hi = h[b + 1];
                a[b]     += xr * hr - xi * hi;
                a[b + 1] += xr * hi + xi * hr;
            }
        }
    }

    std::copy (accum.begin(), accum.end(), fftBuffer.begin());
    std::fill (fftBuffer.begin() + numFloats, fftBuffer.end(), 0.0f);
    fft->performRealOnlyInverseTransform (fftBuffer.data());

    // overlap-save: die zweite Hälfte ist gültig
    std::copy (fftBuffer.begin() + P, fftBuffer.begin() + 2 * P, outputBlock.begin());

    // aktueller Block wird zum vorherigen
    std::copy (inputBlock.begin() + P, inputBlock.end(), inputBlock.begin());

    if (++historyHead == maxParts)
        historyHead = 0;
}

} // namespace lp
