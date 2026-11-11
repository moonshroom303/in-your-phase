#include "Analysis/ScopeAssembler.h"
#include "DSP/PhaseRotator.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <complex>
#include <iostream>
#include <random>

using namespace lp;

namespace
{
constexpr double fs = 48000.0;
constexpr int block = 256;

/** Verarbeitet zwei Kanäle blockweise durch den Rotor. */
void run (PhaseRotator& rotor, std::vector<float>& l, std::vector<float>& r, int blockSize = block)
{
    const int n = (int) l.size();
    for (int pos = 0; pos < n; pos += blockSize)
    {
        const int len = juce::jmin (blockSize, n - pos);
        float* chans[] = { l.data() + pos, r.data() + pos };
        juce::AudioBuffer<float> buf (chans, 2, len);
        rotor.process (buf, nullptr);
    }
}

std::unique_ptr<PhaseRotator> makeRotor (Quality q, ChannelParams left, ChannelParams right, double sr = fs)
{
    auto rotor = std::make_unique<PhaseRotator>();
    rotor->setQuality (q);
    rotor->prepare (sr, block, 2);
    rotor->setParams (0, left);
    rotor->setParams (1, right);
    rotor->reset(); // Glättung überspringen
    return rotor;
}

std::vector<float> noise (int n, unsigned seed)
{
    std::mt19937 gen (seed);
    std::uniform_real_distribution<float> dist (-0.5f, 0.5f);
    std::vector<float> v ((size_t) n);
    for (auto& x : v)
        x = dist (gen);
    return v;
}

/** Komplexer Frequenzgang einer Impulsantwort bei f. */
std::complex<double> dft (const std::vector<float>& h, double f, double sr = fs)
{
    std::complex<double> sum = 0.0;
    const double w = juce::MathConstants<double>::twoPi * f / sr;
    for (size_t n = 0; n < h.size(); ++n)
        sum += (double) h[n] * std::polar (1.0, -w * (double) n);
    return sum;
}

double wrapDeg (double d)
{
    d = std::fmod (d + 180.0, 360.0);
    if (d < 0.0) d += 360.0;
    return d - 180.0;
}
} // namespace

class InYourPhaseTests : public juce::UnitTest
{
public:
    InYourPhaseTests() : juce::UnitTest ("In Your Phase", "inyourphase") {}

    void runTest() override
    {
        beginTest ("Hilbert-Entwurf ist antisymmetrisch, gerade Abstände sind null");
        {
            auto h = designHilbert (1025);
            const int M = 512;
            double maxAsym = 0.0, maxEven = 0.0;
            for (int i = 0; i < 1025; ++i)
            {
                maxAsym = juce::jmax (maxAsym, (double) std::abs (h[(size_t) i] + h[(size_t) (1024 - i)]));
                if ((i - M) % 2 == 0)
                    maxEven = juce::jmax (maxEven, (double) std::abs (h[(size_t) i]));
            }
            expect (maxAsym < 1.0e-7);
            expect (maxEven == 0.0);
        }

        beginTest ("Partitionierte Faltung entspricht direkter Faltung (Latenz = Partitionsgröße)");
        {
            const int P = 128, taps = 1000, n = 6000;
            auto h = noise (taps, 1);
            auto x = noise (n, 2);

            const int order = 8;
            juce::dsp::FFT fft (order);
            auto kernel = makeKernel (h, P, fft);

            PartitionedConvolver conv;
            conv.prepare (P, kernel.numPartitions);
            conv.setKernel (&kernel);

            double maxErr = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const float y = conv.processSample (x[(size_t) i]);
                double ref = 0.0;
                for (int k = 0; k < taps; ++k)
                {
                    const int idx = i - P - k;
                    if (idx >= 0)
                        ref += (double) h[(size_t) k] * x[(size_t) idx];
                }
                maxErr = juce::jmax (maxErr, std::abs (ref - (double) y));
            }
            expect (maxErr < 2.0e-4, "Abweichung " + juce::String (maxErr));
        }

        for (auto q : { Quality::precise, Quality::balanced, Quality::fast })
        {
            beginTest ("0° lässt das Signal unverändert, nur verzögert (Qualität " + juce::String ((int) q) + ")");
            auto rotorHolder = makeRotor (q, {}, {});
            auto& rotor = *rotorHolder;
            const int L = rotor.latencyFor (q);
            auto inL = noise (L + 6000, 3), inR = noise (L + 6000, 4);
            auto l = inL, r = inR;
            run (rotor, l, r);

            double maxErr = 0.0;
            for (int i = L; i < (int) l.size(); ++i)
            {
                maxErr = juce::jmax (maxErr, (double) std::abs (l[(size_t) i] - inL[(size_t) (i - L)]));
                maxErr = juce::jmax (maxErr, (double) std::abs (r[(size_t) i] - inR[(size_t) (i - L)]));
            }
            expect (maxErr < 1.0e-5, "Abweichung " + juce::String (maxErr));
        }

        beginTest (juce::String::fromUTF8 ("180° invertiert das Signal"));
        {
            ChannelParams p;
            p.phaseDeg = 180.0f;
            auto rotorHolder = makeRotor (Quality::balanced, p, p);
            auto& rotor = *rotorHolder;
            const int L = rotor.latencyFor (Quality::balanced);
            auto in = noise (L + 4000, 5);
            auto l = in, r = in;
            run (rotor, l, r);

            double maxErr = 0.0;
            for (int i = L; i < (int) l.size(); ++i)
                maxErr = juce::jmax (maxErr, (double) std::abs (l[(size_t) i] + in[(size_t) (i - L)]));
            expect (maxErr < 1.0e-4, "Abweichung " + juce::String (maxErr));
        }

        struct Band { Quality q; std::vector<double> freqs; };
        const Band bands[] = {
            { Quality::precise,  { 20.0, 40.0, 100.0, 1000.0, 10000.0, 18000.0 } },
            { Quality::balanced, { 40.0, 100.0, 440.0, 1000.0, 5000.0, 12000.0, 18000.0 } },
            { Quality::fast,     { 150.0, 440.0, 2000.0, 15000.0 } },
        };

        for (const auto& band : bands)
        {
            for (float theta : { 30.0f, 90.0f, -120.0f, 179.0f })
            {
                beginTest ("Konstante Phasendrehung " + juce::String (theta, 0) + juce::String::fromUTF8 ("° (Qualität ")
                           + juce::String ((int) band.q) + ")");
                ChannelParams p;
                p.phaseDeg = theta;
                auto rotorHolder = makeRotor (band.q, p, p);
            auto& rotor = *rotorHolder;
                const int L = rotor.latencyFor (band.q);

                std::vector<float> l ((size_t) (2 * L + 2048), 0.0f), r = l;
                l[0] = 1.0f;
                run (rotor, l, r);

                double worstPhase = 0.0, worstMag = 0.0;
                for (double f : band.freqs)
                {
                    const auto X = dft (l, f);
                    const double w = juce::MathConstants<double>::twoPi * f / fs;
                    // Erwartet: e^{-jωL} · e^{-jθ}
                    const double phase = juce::radiansToDegrees (std::arg (X) + w * L);
                    const double err = std::abs (wrapDeg (phase + theta));
                    const double magDb = 20.0 * std::log10 (std::abs (X));
                    worstPhase = juce::jmax (worstPhase, err);
                    worstMag = juce::jmax (worstMag, std::abs (magDb));
                }

                expect (worstPhase < 0.5, "Phasenfehler " + juce::String (worstPhase, 3) + juce::String::fromUTF8 ("°"));
                expect (worstMag < 0.1, "Pegelfehler " + juce::String (worstMag, 3) + " dB");
            }
        }

        beginTest (juce::String::fromUTF8 ("Phasendrehung bei 44,1 und 96 kHz"));
        for (double sr : { 44100.0, 96000.0 })
        {
            ChannelParams p;
            p.phaseDeg = 90.0f;
            auto rotorHolder = makeRotor (Quality::balanced, p, p, sr);
            auto& rotor = *rotorHolder;
            const int L = rotor.latencyFor (Quality::balanced);
            std::vector<float> l ((size_t) (2 * L + 2048), 0.0f), r = l;
            l[0] = 1.0f;
            run (rotor, l, r);

            for (double f : { 50.0, 1000.0, 15000.0 })
            {
                const auto X = dft (l, f, sr);
                const double w = juce::MathConstants<double>::twoPi * f / sr;
                const double err = std::abs (wrapDeg (juce::radiansToDegrees (std::arg (X) + w * L) + 90.0));
                expect (err < 0.5, juce::String (sr) + " Hz, " + juce::String (f) + " Hz: " + juce::String (err, 3));
            }
        }

        beginTest (juce::String::fromUTF8 ("Feinverzögerung verschiebt um die eingestellte Zeit"));
        {
            ChannelParams p;
            p.delayMs = 1.0f; // 48 Samples
            auto rotorHolder = makeRotor (Quality::fast, p, {});
            auto& rotor = *rotorHolder;
            const int L = rotor.latencyFor (Quality::fast);
            std::vector<float> l ((size_t) (L + 400), 0.0f), r = l;
            l[0] = 1.0f;
            run (rotor, l, r);

            const auto peak = std::max_element (l.begin(), l.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); });
            expect ((int) (peak - l.begin()) == L + 48, "Spitze bei " + juce::String ((int) (peak - l.begin())));
            expectWithinAbsoluteError (*peak, 1.0f, 1.0e-4f);
        }

        beginTest (juce::String::fromUTF8 ("Polarität und Pegel"));
        {
            ChannelParams p;
            p.invert = true;
            p.gainDb = 6.0f;
            auto rotorHolder = makeRotor (Quality::fast, p, {});
            auto& rotor = *rotorHolder;
            const int L = rotor.latencyFor (Quality::fast);
            auto in = noise (L + 2000, 6);
            auto l = in, r = in;
            run (rotor, l, r);

            const float g = juce::Decibels::decibelsToGain (6.0f);
            double maxErr = 0.0;
            for (int i = L; i < (int) l.size(); ++i)
                maxErr = juce::jmax (maxErr, (double) std::abs (l[(size_t) i] + g * in[(size_t) (i - L)]));
            expect (maxErr < 1.0e-4);
        }

        beginTest ("Kanäle sind unabhängig");
        {
            ChannelParams p;
            p.phaseDeg = 90.0f;
            auto rotorHolder = makeRotor (Quality::fast, p, {});
            auto& rotor = *rotorHolder;
            const int L = rotor.latencyFor (Quality::fast);
            auto in = noise (L + 3000, 7);
            auto l = in, r = in;
            run (rotor, l, r);

            double maxErrR = 0.0, diffL = 0.0;
            for (int i = L; i < (int) l.size(); ++i)
            {
                maxErrR = juce::jmax (maxErrR, (double) std::abs (r[(size_t) i] - in[(size_t) (i - L)]));
                diffL = juce::jmax (diffL, (double) std::abs (l[(size_t) i] - in[(size_t) (i - L)]));
            }
            expect (maxErrR < 1.0e-5, "R wurde verändert");
            expect (diffL > 0.1, "L wurde nicht gedreht");
        }

        beginTest ("Qualitätswechsel: neue Latenz, danach wieder exakt");
        {
            ChannelParams p;
            p.phaseDeg = 90.0f;
            auto rotorHolder = makeRotor (Quality::balanced, p, p);
            auto& rotor = *rotorHolder;
            auto warm = noise (20000, 8);
            auto wr = warm;
            run (rotor, warm, wr);

            rotor.setQuality (Quality::fast);
            std::vector<float> silence (48000, 0.0f), sr2 = silence;
            run (rotor, silence, sr2); // Aus-/Einblenden und Ausklingen

            const int L = rotor.latencyFor (Quality::fast);
            expect (rotor.getLatencySamples() == L);

            std::vector<float> l ((size_t) (2 * L + 2048), 0.0f), r = l;
            l[0] = 1.0f;
            run (rotor, l, r);

            const auto X = dft (l, 1000.0);
            const double w = juce::MathConstants<double>::twoPi * 1000.0 / fs;
            const double err = std::abs (wrapDeg (juce::radiansToDegrees (std::arg (X) + w * L) + 90.0));
            expect (err < 0.5, "Phasenfehler nach Wechsel " + juce::String (err, 3));
        }

        beginTest (juce::String::fromUTF8 ("Winkeländerung ohne Sprung, über ±180° auf dem kurzen Weg"));
        {
            auto rotorHolder = makeRotor (Quality::fast, {}, {});
            auto& rotor = *rotorHolder;
            const int n = 48000;
            std::vector<float> l ((size_t) n), r ((size_t) n);
            for (int i = 0; i < n; ++i)
                l[(size_t) i] = r[(size_t) i] = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / fs);

            const auto original = l;
            // Erst auf 179°, dann über die Grenze auf -179°
            ChannelParams p;
            for (int pos = 0; pos < n; pos += block)
            {
                if (pos == 9600)  { p.phaseDeg = 179.0f;  rotor.setParams (0, p); }
                if (pos == 24064) { p.phaseDeg = -179.0f; rotor.setParams (0, p); }
                float* chans[] = { l.data() + pos, r.data() + pos };
                juce::AudioBuffer<float> buf (chans, 2, juce::jmin (block, n - pos));
                rotor.process (buf, nullptr);
            }

            float maxStep = 0.0f;
            for (int i = 4000; i < n; ++i)
                maxStep = juce::jmax (maxStep, std::abs (l[(size_t) i] - l[(size_t) (i - 1)]));
            const float sineStep = 0.5f * (float) (juce::MathConstants<double>::twoPi * 1000.0 / fs);
            expect (maxStep < sineStep * 1.3f, "Sprung " + juce::String (maxStep, 4));

            // Beim Wechsel 179 -> -179 darf der Pegel nicht einbrechen (kein Umweg über 0°)
            double minRms = 1.0;
            for (int start = 24064; start < 24064 + 4800; start += 48)
            {
                double sum = 0.0;
                for (int i = start; i < start + 48; ++i)
                    sum += (double) l[(size_t) i] * l[(size_t) i];
                minRms = juce::jmin (minRms, std::sqrt (sum / 48.0));
            }
            expect (minRms > 0.30, "Pegeleinbruch auf " + juce::String (minRms, 3));
        }

        beginTest ("Plugin: gekoppelter Modus, Zustand, Latenz");
        {
            InYourPhaseProcessor proc;
            auto set = [&] (const char* id, float plain)
            {
                auto* param = proc.apvts.getParameter (id);
                param->setValueNotifyingHost (param->convertTo0to1 (plain));
            };

            set (ParamIDs::quality, 2.0f);
            proc.prepareToPlay (fs, block);
            PhaseRotator ref;
            ref.prepare (fs, block, 2);
            expect (proc.getLatencySamples() == ref.latencyFor (Quality::fast));

            set (ParamIDs::mode, 0.0f);     // gekoppelt
            set (ParamIDs::phaseL, 90.0f);
            set (ParamIDs::phaseR, 0.0f);   // wird ignoriert

            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            auto in = noise (block * 80, 9);
            double maxDiff = 0.0;
            for (int b = 0; b < 80; ++b)
            {
                for (int i = 0; i < block; ++i)
                    buf.setSample (0, i, in[(size_t) (b * block + i)]), buf.setSample (1, i, in[(size_t) (b * block + i)]);
                proc.processBlock (buf, midi);
                if (b > 40)
                    for (int i = 0; i < block; ++i)
                        maxDiff = juce::jmax (maxDiff, (double) std::abs (buf.getSample (0, i) - buf.getSample (1, i)));
            }
            expect (maxDiff < 1.0e-5, "R folgt L nicht: " + juce::String (maxDiff));

            // Zustand speichern und in neuer Instanz laden
            set (ParamIDs::mode, 1.0f);
            set (ParamIDs::phaseR, -37.5f);
            set (ParamIDs::delayL, 2.25f);
            set (ParamIDs::invertR, 1.0f);
            proc.apvts.state.setProperty ("scopeSync", 4, nullptr);

            juce::MemoryBlock state;
            proc.getStateInformation (state);

            InYourPhaseProcessor other;
            other.setStateInformation (state.getData(), (int) state.getSize());
            expectWithinAbsoluteError (other.apvts.getRawParameterValue (ParamIDs::phaseR)->load(), -37.5f, 0.01f);
            expectWithinAbsoluteError (other.apvts.getRawParameterValue (ParamIDs::delayL)->load(), 2.25f, 0.001f);
            expect (other.apvts.getRawParameterValue (ParamIDs::invertR)->load() > 0.5f);
            expect (other.isStereoMode());
            expect ((int) other.apvts.state.getProperty ("scopeSync", -1) == 4);

            // Busse: mono und stereo, keine Mischformen
            juce::AudioProcessor::BusesLayout stereo, mono, mixed;
            stereo.inputBuses.add (juce::AudioChannelSet::stereo());
            stereo.outputBuses.add (juce::AudioChannelSet::stereo());
            mono.inputBuses.add (juce::AudioChannelSet::mono());
            mono.outputBuses.add (juce::AudioChannelSet::mono());
            mixed.inputBuses.add (juce::AudioChannelSet::mono());
            mixed.outputBuses.add (juce::AudioChannelSet::stereo());
            expect (proc.checkBusesLayoutSupported (stereo));
            expect (proc.checkBusesLayoutSupported (mono));
            expect (! proc.checkBusesLayoutSupported (mixed));
        }

        beginTest ("Oszilloskop im Takt: ein Bild pro Viertel, am Raster ausgerichtet");
        {
            ScopeAssembler scope;
            scope.setSampleRate (fs);
            scope.setMode (ScopeAssembler::Mode::sync, 2); // 1/4

            int frames = 0;
            ScopeAssembler::Frame frame;
            const double beatsPerSample = 120.0 / 60.0 / fs;
            for (int i = 0; i < (int) (2.5 / beatsPerSample); ++i)
            {
                AnalysisSample s;
                s.ppq = i * beatsPerSample;
                s.bpm = 120.0f;
                s.outL = (float) std::fmod (s.ppq, 1.0); // Sägezahn pro Viertel
                s.outR = -s.outL;
                scope.push (s);
                if (scope.takeFrame (frame))
                {
                    ++frames;
                    expect (frame.fromHostTransport);
                    expectWithinAbsoluteError (frame.lengthBeats, 1.0, 1.0e-9);
                    // Erste Spalte nahe 0, letzte nahe 1: das Bild beginnt genau auf dem Schlag
                    expect (frame.outL.lo[0] < 0.01f);
                    expect (frame.outL.hi[ScopeAssembler::columns - 1] > 0.99f);
                }
            }
            expect (frames == 2, "Bilder: " + juce::String (frames));
        }

        beginTest ("Oszilloskop ohne Transport: interne Uhr mit Host-Tempo");
        {
            ScopeAssembler scope;
            scope.setSampleRate (fs);
            scope.setMode (ScopeAssembler::Mode::sync, 4); // 1 Takt
            int frames = 0;
            ScopeAssembler::Frame frame;
            for (int i = 0; i < (int) (fs * 4.2); ++i) // 4,2 s bei 120 BPM, 4/4 = 2 Takte
            {
                AnalysisSample s;
                s.ppq = -1.0;
                s.bpm = 120.0f;
                scope.push (s);
                if (scope.takeFrame (frame))
                {
                    ++frames;
                    expect (! frame.fromHostTransport);
                    expectWithinAbsoluteError (frame.lengthBeats, 4.0, 1.0e-9);
                }
            }
            expect (frames == 2, "Bilder: " + juce::String (frames));
        }

        beginTest ("Oszilloskop im Trigger-Modus startet am steigenden Nulldurchgang");
        {
            ScopeAssembler scope;
            scope.setSampleRate (fs);
            scope.setMode (ScopeAssembler::Mode::trigger, 1); // 5 ms
            ScopeAssembler::Frame frame;
            bool got = false;
            for (int i = 0; i < 4800 && ! got; ++i)
            {
                AnalysisSample s;
                s.outL = (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * i / fs + 1.0);
                scope.push (s);
                got = scope.takeFrame (frame);
            }
            expect (got);
            expect (std::abs (frame.outL.lo[0]) < 0.08f, "Start bei " + juce::String (frame.outL.lo[0]));
            // Bei 5 ms gibt es weniger Samples als Spalten: nächste gefüllte Spalte nehmen
            int next = 20;
            while (next < ScopeAssembler::columns && ! frame.outL.has (next))
                ++next;
            expect (next < ScopeAssembler::columns && frame.outL.hi[(size_t) next] > frame.outL.hi[0], "nicht steigend");
        }

        beginTest ("Korrelation und Balance");
        {
            auto check = [this] (float sign, float gainR, float expectedCorr, float expectedBalance)
            {
                StereoAnalysis a;
                a.setSampleRate (fs);
                auto n1 = noise (48000, 10);
                for (auto v : n1)
                    a.push (v, sign * gainR * v);
                expectWithinAbsoluteError (a.getCorrelation(), expectedCorr, 0.01f);
                expectWithinAbsoluteError (a.getBalanceDb(), expectedBalance, 0.1f);
            };
            check (1.0f, 1.0f, 1.0f, 0.0f);
            check (-1.0f, 1.0f, -1.0f, 0.0f);
            check (1.0f, 2.0f, 1.0f, 6.02f);

            StereoAnalysis u;
            u.setSampleRate (fs);
            auto a = noise (96000, 11), b = noise (96000, 12);
            for (size_t i = 0; i < a.size(); ++i)
                u.push (a[i], b[i]);
            expect (std::abs (u.getCorrelation()) < 0.1f, "unkorreliert: " + juce::String (u.getCorrelation()));
        }

        beginTest (juce::String::fromUTF8 ("Moonshroom-Studio-Karte: Taste öffnet, Links führen zum richtigen Ziel"));
        {
            InYourPhaseProcessor proc;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            auto* editor = dynamic_cast<InYourPhaseEditor*> (ed.get());
            expect (editor != nullptr);

            auto& overlay = editor->getStudioOverlay();
            expect (! overlay.isVisible(), "anfangs geschlossen");
            expect (editor->getStudioButton().isVisible());
            expect (editor->getStudioButton().getRight() <= editor->getWidth() - 600, "Taste im linken Teil der Kopfzeile");

            editor->getStudioButton().onClick();
            expect (overlay.isVisible(), "Taste oeffnet die Karte");
            expect (overlay.getBounds() == editor->getLocalBounds());

            juce::StringArray opened;
            overlay.panel.setOpener ([&] (const juce::String& u) { opened.add (u); });

            for (int i = 0; i < MoonshroomPanel::numLinks; ++i)
                overlay.panel.getLinkButton (i).onClick();
            overlay.panel.getLogoButton().onClick();

            expectEquals (opened.size(), 5);
            expectEquals (opened[0], juce::String ("https://moonshroom.moonshroom303.chatgpt.site/"));
            expectEquals (opened[1], juce::String ("https://www.soundcloud.com/moonshroom303"));
            expectEquals (opened[2], juce::String ("https://www.youtube.com/@moonshroom303"));
            expectEquals (opened[3], juce::String ("https://shroomsloops-studio-231026.moonshroom303.chatgpt.site/#"));
            expectEquals (opened[4], opened[0]);

            expectEquals (overlay.panel.getLinkButton (3).getButtonText(), juce::String ("ShroomsLoops"));

            overlay.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            expect (! overlay.isVisible(), "Escape schliesst");

            // Kleinste Fenstergröße: Taste stößt nicht an die Moduswahl
            editor->setSize (1000, 640);
            auto* coupled = [&]() -> juce::Component* {
                for (auto* c : editor->getChildren())
                    if (auto* b = dynamic_cast<juce::TextButton*> (c))
                        if (b->getButtonText() == "GEKOPPELT") return b;
                return nullptr; }();
            expect (coupled != nullptr);
            if (coupled != nullptr)
                expect (editor->getStudioButton().getRight() < coupled->getX(), "keine Ueberlappung bei 1000 px");
            expect (editor->getStudioButton().getWidth() >= 140, "Taste breit genug: " + juce::String (editor->getStudioButton().getWidth()));
        }
    }
};

static InYourPhaseTests tests;

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTests ({ &tests });

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (auto* r = runner.getResult (i))
            failures += r->failures;

    std::cout << (failures == 0 ? "ALLE TESTS BESTANDEN" : "FEHLGESCHLAGEN: " + std::to_string (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
