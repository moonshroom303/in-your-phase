#include "ScopeAssembler.h"

namespace lp
{

const juce::StringArray& ScopeAssembler::syncNames()
{
    static const juce::StringArray names { "1/16", "1/8", "1/4", "1/2", "1 Takt", "2 Takte" };
    return names;
}

const juce::StringArray& ScopeAssembler::triggerNames()
{
    static const juce::StringArray names { "2 ms", "5 ms", "10 ms", "20 ms", "50 ms", "100 ms" };
    return names;
}

ScopeAssembler::ScopeAssembler()
{
    startFrame();
}

void ScopeAssembler::setMode (Mode m, int timebaseIndex) noexcept
{
    const int maxIndex = (m == Mode::sync ? syncNames() : triggerNames()).size() - 1;
    timebaseIndex = juce::jlimit (0, maxIndex, timebaseIndex);

    if (m == mode && timebaseIndex == timebase)
        return;

    mode = m;
    timebase = timebaseIndex;
    currentIndex = -1;
    capturing = false;
    waited = 0;
    startFrame();
}

double ScopeAssembler::lengthInBeats (double beatsPerBar) const noexcept
{
    switch (timebase)
    {
        case 0:  return 0.25;
        case 1:  return 0.5;
        case 2:  return 1.0;
        case 3:  return 2.0;
        case 4:  return beatsPerBar;
        default: return 2.0 * beatsPerBar;
    }
}

double ScopeAssembler::triggerMs() const noexcept
{
    static constexpr double ms[] = { 2.0, 5.0, 10.0, 20.0, 50.0, 100.0 };
    return ms[juce::jlimit (0, 5, timebase)];
}

void ScopeAssembler::startFrame() noexcept
{
    current.outL.clear();
    current.outR.clear();
    current.inL.clear();
    current.inR.clear();
    currentHasData = false;
}

void ScopeAssembler::publish() noexcept
{
    if (currentHasData)
    {
        finished = current;
        hasFinished = true;
    }

    startFrame();
}

bool ScopeAssembler::takeFrame (Frame& out) noexcept
{
    if (! hasFinished)
        return false;

    out = finished;
    hasFinished = false;
    return true;
}

void ScopeAssembler::addToColumn (int c, const AnalysisSample& s) noexcept
{
    c = juce::jlimit (0, columns - 1, c);

    // Erste Werte einer leeren Spalte setzen beide Grenzen
    auto addValue = [c] (Trace& t, float v)
    {
        if (t.lo[(size_t) c] > t.hi[(size_t) c])
        {
            t.lo[(size_t) c] = v;
            t.hi[(size_t) c] = v;
        }
        else
        {
            t.lo[(size_t) c] = juce::jmin (t.lo[(size_t) c], v);
            t.hi[(size_t) c] = juce::jmax (t.hi[(size_t) c], v);
        }
    };

    addValue (current.outL, s.outL);
    addValue (current.outR, s.outR);
    addValue (current.inL, s.inL);
    addValue (current.inR, s.inR);
    currentHasData = true;
}

void ScopeAssembler::push (const AnalysisSample& s) noexcept
{
    if (mode == Mode::sync)
    {
        const double bpm = s.bpm > 0.0f ? (double) s.bpm : 120.0;
        const double beatsPerBar = s.beatsPerBar > 0.0f ? (double) s.beatsPerBar : 4.0;
        const double length = lengthInBeats (beatsPerBar);
        const bool host = s.ppq >= 0.0;

        double ppq;
        if (host)
        {
            ppq = s.ppq;
            internalPpq = ppq; // nahtlos weiterzählen, wenn der Transport stoppt
        }
        else
        {
            internalPpq += bpm / 60.0 / sampleRate;
            ppq = internalPpq;
        }

        if (host != lastWasHost)
        {
            currentIndex = -1;
            lastWasHost = host;
            startFrame();
        }

        const auto index = (long long) std::floor (ppq / length);

        if (index != currentIndex)
        {
            if (currentIndex >= 0)
                publish();
            else
                startFrame();

            currentIndex = index;
            current.synced = true;
            current.lengthBeats = length;
            current.beatsPerBar = beatsPerBar;
            current.fromHostTransport = host;
        }

        const double position = (ppq - (double) index * length) / length;
        addToColumn ((int) (position * columns), s);
        return;
    }

    // Trigger-Modus
    const double windowSamples = juce::jmax (1.0, triggerMs() * 0.001 * sampleRate);

    if (! capturing)
    {
        const bool rising = previousL < 0.0f && s.outL >= 0.0f;
        previousL = s.outL;
        ++waited;

        // Ohne Nulldurchgang (Stille, Rauschen) nach einer Weile trotzdem zeichnen
        if (! rising && waited < (long long) (windowSamples * 4.0))
            return;

        capturing = true;
        captured = 0;
        waited = 0;
        startFrame();
        current.synced = false;
        current.lengthMs = triggerMs();
    }

    previousL = s.outL;
    addToColumn ((int) ((double) captured / windowSamples * columns), s);

    if (++captured >= (long long) windowSamples)
    {
        publish();
        capturing = false;
    }
}

} // namespace lp
