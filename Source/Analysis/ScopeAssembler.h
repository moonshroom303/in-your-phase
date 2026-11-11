#pragma once

#include "AnalysisFifo.h"

#include <array>
#include <cmath>

namespace lp
{

/**
    Setzt aus dem Messdatenstrom Oszilloskop-Bilder zusammen.

    Takt-Modus: ein Bild umfasst eine Notenlänge (1/16 bis 2 Takte) und beginnt
    immer auf dem Raster des Hosts, das Bild steht also im Takt still. Läuft der
    Transport nicht, zählt eine interne Uhr mit dem Host-Tempo (sonst 120 BPM).

    Trigger-Modus: ein Bild umfasst eine feste Zeit und startet bei einem
    steigenden Nulldurchgang des linken Kanals.

    Pro Bildspalte werden Minimum und Maximum gespeichert, so bleibt die
    Darstellung bei jeder Länge gleich detailliert.
*/
class ScopeAssembler
{
public:
    static constexpr int columns = 512;

    enum class Mode { sync, trigger };

    struct Trace
    {
        std::array<float, columns> lo {};
        std::array<float, columns> hi {};
        void clear() noexcept { lo.fill (1.0f); hi.fill (-1.0f); } // lo > hi: Spalte leer
        bool has (int c) const noexcept { return lo[(size_t) c] <= hi[(size_t) c]; }
    };

    struct Frame
    {
        Trace outL, outR, inL, inR;
        bool synced = true;
        double lengthBeats = 1.0;   // Takt-Modus
        double lengthMs = 20.0;     // Trigger-Modus
        double beatsPerBar = 4.0;
        bool fromHostTransport = false;
    };

    static const juce::StringArray& syncNames();     // "1/16" … "2 Takte"
    static const juce::StringArray& triggerNames();  // "2 ms" … "100 ms"

    ScopeAssembler();

    void setSampleRate (double sr) noexcept { sampleRate = sr; }
    void setMode (Mode m, int timebaseIndex) noexcept;
    Mode getMode() const noexcept { return mode; }
    int getTimebaseIndex() const noexcept { return timebase; }

    void push (const AnalysisSample& s) noexcept;

    /** true, wenn seit dem letzten Aufruf ein neues Bild fertig wurde. */
    bool takeFrame (Frame& out) noexcept;

private:
    double lengthInBeats (double beatsPerBar) const noexcept;
    double triggerMs() const noexcept;
    void startFrame() noexcept;
    void publish() noexcept;
    void addToColumn (int column, const AnalysisSample& s) noexcept;

    double sampleRate = 48000.0;
    Mode mode = Mode::sync;
    int timebase = 2;

    Frame current, finished;
    bool hasFinished = false;
    bool currentHasData = false;

    // Takt-Modus
    long long currentIndex = -1;
    double internalPpq = 0.0;
    bool lastWasHost = false;

    // Trigger-Modus
    bool capturing = false;
    long long captured = 0;
    long long waited = 0;
    float previousL = 0.0f;
};

/** Stereofeld-Kennzahlen: Korrelation, Balance, Spitzenpegel. */
class StereoAnalysis
{
public:
    void setSampleRate (double sr) noexcept
    {
        coeff = 1.0 - std::exp (-1.0 / (0.3 * sr)); // ca. 300 ms Mittelung
    }

    void push (float l, float r) noexcept
    {
        sLR += coeff * ((double) l * r - sLR);
        sLL += coeff * ((double) l * l - sLL);
        sRR += coeff * ((double) r * r - sRR);
        // Länge des Stereo-Vektors: Mono und hart gepannte Spitzen enden beide genau am Kreis
        peak = juce::jmax (peak * 0.99995f, std::sqrt (l * l + r * r));
    }

    /** -1 (gegenphasig) … 0 (unkorreliert) … +1 (mono) */
    float getCorrelation() const noexcept
    {
        const double denom = std::sqrt (sLL * sRR);
        return denom > 1.0e-12 ? (float) juce::jlimit (-1.0, 1.0, sLR / denom) : 0.0f;
    }

    /** Balance in dB: positiv = rechts lauter */
    float getBalanceDb() const noexcept
    {
        if (sLL < 1.0e-14 && sRR < 1.0e-14)
            return 0.0f;
        return (float) (10.0 * std::log10 ((sRR + 1.0e-14) / (sLL + 1.0e-14)));
    }

    /** Spitzenwert von √(L² + R²), skaliert das Goniometer */
    float getPeak() const noexcept { return peak; }

private:
    double coeff = 1.0e-4;
    double sLR = 0.0, sLL = 0.0, sRR = 0.0;
    float peak = 0.0f;
};

} // namespace lp
