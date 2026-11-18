#include "PluginProcessor.h"
#include "TestSupport.h"
#include <memory>

static void fill (juce::AudioBuffer<float>& b, int offset)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (c, i, 0.25f * std::sin (0.04f * (float) (i + offset)) * (((i + offset) / 6000) % 2 == 0 ? 1.0f : 0.0f));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto p = std::make_unique<TumultProcessor>();

    // Stereo (Standard), Mono rein/Stereo raus und reine Mono-Spur werden akzeptiert, Surround nicht
    auto layout = p->getBusesLayout();
    layout.inputBuses.set (0, juce::AudioChannelSet::mono());
    layout.outputBuses.set (0, juce::AudioChannelSet::stereo());
    CHECK (p->setBusesLayout (layout));
    layout.outputBuses.set (0, juce::AudioChannelSet::mono());
    CHECK (p->setBusesLayout (layout));
    layout.inputBuses.set (0, juce::AudioChannelSet::create5point1());
    layout.outputBuses.set (0, juce::AudioChannelSet::create5point1());
    CHECK (! p->setBusesLayout (layout));
    layout.inputBuses.set (0, juce::AudioChannelSet::stereo());
    layout.outputBuses.set (0, juce::AudioChannelSet::stereo());
    CHECK (p->setBusesLayout (layout));
    p->prepareToPlay (48000, 64);

    // Die Latenz wird dem Host gemeldet (500 ms)
    CHECK (p->getLatencySamples() == 24000);

    juce::MidiBuffer midi;

    // Alle Presets laden, jedes liefert endliche, begrenzte Ausgabe bei jeder Blockgroesse ohne Speicheranforderung
    {
        CHECK (p->getNumPrograms() == 8);
        juce::AudioBuffer<float> b (2, 8193);
        double energy = 0;
        int offset = 0;
        for (int prog = 0; prog < p->getNumPrograms(); ++prog)
        {
            p->setCurrentProgram (prog);
            CHECK (p->getProgramName (prog).isNotEmpty());
            for (int round = 0; round < 8; ++round)
                for (int n : { 0, 1, 7, 31, 64, 513, 8193 })
                {
                    b.setSize (2, n, false, false, true);
                    fill (b, offset); offset += n;
                    { NoAllocations guard; p->processBlock (b, midi); }
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < n; ++i) { const float x = b.getSample (c, i); CHECK (std::isfinite (x) && std::abs (x) <= 1.05f); energy += (double) x * x; }
                }
        }
        CHECK (energy > 0.0);
    }

    // Gemurmel: die eingebetteten Aufnahmen werden geladen; ohne Eingang kommt hoerbares, endliches Material auf beiden Seiten
    {
        auto setParam = [&] (const char* id, float v)
        {
            auto* prm = p->apvts.getParameter (id);
            CHECK (prm != nullptr);
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        };
        setParam ("amount", 0.0f); setParam ("own", -60.0f); setParam ("room", 0.0f); setParam ("walla", 0.7f);
        double lowE = 0, highE = 0;
        for (float x : { 0.0f, 1.0f })
        {
            setParam ("wallaInt", x);
            p->reset();
            juce::AudioBuffer<float> b (2, 4800);
            double e = 0, d = 0, l = 0, r = 0;
            for (int round = 0; round < 40; ++round)
            {
                b.clear();
                { NoAllocations guard; p->processBlock (b, midi); }
                if (round >= 10)
                    for (int i = 1; i < 4800; ++i)
                    {
                        const float a2 = b.getSample (0, i), c2 = b.getSample (1, i);
                        CHECK (std::isfinite (a2) && std::isfinite (c2));
                        e += (double) a2 * a2; l += (double) a2 * a2; r += (double) c2 * c2;
                        const double dd = a2 - b.getSample (0, i - 1); d += dd * dd;
                    }
            }
            CHECK (l > 1e-3 && r > 1e-3);
            (x == 0.0f ? lowE : highE) = d / e;   // Anteil hoher Frequenzen
        }
        CHECK (highE > lowE);   // die obere Stufe klingt heller als die untere
        setParam ("walla", 0.0f); setParam ("amount", 0.7f); setParam ("own", -3.0f); setParam ("room", 0.35f);
        p->reset();
    }

    // Eigene Stimme, Menge und Raum aus: es bleibt Stille
    {
        for (auto* id : { "amount", "room" })
            p->apvts.getParameter (id)->setValueNotifyingHost (0.0f);
        p->apvts.getParameter ("own")->setValueNotifyingHost (0.0f);   // ganz links = aus
        p->prepareToPlay (48000, 64);
        juce::AudioBuffer<float> b (2, 4096);
        double energy = 0;
        for (int round = 0; round < 20; ++round)
        {
            fill (b, round * 4096);
            p->processBlock (b, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 4096; ++i) energy += std::abs (b.getSample (c, i));
        }
        CHECK (energy == 0.0);
    }

    // Reine Mono-Spur, auch mit einem Block, der groesser ist als vorbereitet
    {
        p->setCurrentProgram (2);
        layout.inputBuses.set (0, juce::AudioChannelSet::mono());
        layout.outputBuses.set (0, juce::AudioChannelSet::mono());
        CHECK (p->setBusesLayout (layout));
        p->prepareToPlay (48000, 64);
        juce::AudioBuffer<float> b (1, 32769);
        double e = 0;
        for (int round = 0; round < 3; ++round)
        {
            fill (b, round * 32769);
            { NoAllocations guard; p->processBlock (b, midi); }
            for (int i = 0; i < b.getNumSamples(); ++i) { const float x = b.getSample (0, i); CHECK (std::isfinite (x)); e += (double) x * x; }
        }
        CHECK (e > 0.0);
        layout.inputBuses.set (0, juce::AudioChannelSet::mono());
        layout.outputBuses.set (0, juce::AudioChannelSet::stereo());
        CHECK (p->setBusesLayout (layout));   // Mono rein, Stereo raus
        p->prepareToPlay (48000, 64);
        juce::AudioBuffer<float> st (2, 4096);
        st.clear();
        for (int i = 0; i < 4096; ++i) st.setSample (0, i, 0.2f * std::sin (0.05f * (float) i));
        { NoAllocations guard; p->processBlock (st, midi); }
        for (int i = 0; i < 4096; ++i) CHECK (std::isfinite (st.getSample (1, i)));
        layout.inputBuses.set (0, juce::AudioChannelSet::stereo());
        layout.outputBuses.set (0, juce::AudioChannelSet::stereo());
        CHECK (p->setBusesLayout (layout));
        p->prepareToPlay (48000, 64);
    }

    // Zustand speichern und laden: Regler, Look, Fenstergroesse, Preset und die Menge (Startwert) bleiben erhalten
    {
        p->setCurrentProgram (6);
        p->uiTheme.store (1); p->uiWidth.store (900);
        juce::MemoryBlock state;
        p->getStateInformation (state);

        auto q = std::make_unique<TumultProcessor>();
        q->setStateInformation (state.getData(), (int) state.getSize());
        CHECK (q->getCurrentProgram() == 6);
        CHECK (q->uiTheme.load() == 1);
        CHECK (q->uiWidth.load() == 900);
        for (auto* id : { "size", "amount", "chaos", "variety", "walla", "wallaInt", "room", "own", "output" })
            CHECK (q->apvts.getRawParameterValue (id)->load() == p->apvts.getRawParameterValue (id)->load());
        CHECK ((int) q->apvts.getRawParameterValue ("size")->load() == 3);

        // gleiche Eingabe, gleicher Zustand: bit-genau gleiche Menge
        p->prepareToPlay (48000, 512); q->prepareToPlay (48000, 512);
        juce::AudioBuffer<float> a (2, 512), c (2, 512);
        for (int round = 0; round < 40; ++round)
        {
            fill (a, round * 512); c.makeCopyOf (a);
            p->processBlock (a, midi); q->processBlock (c, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i) CHECK (a.getSample (ch, i) == c.getSample (ch, i));
        }

        // Kaputter oder fremder Zustand wird ignoriert
        const char junk[] = "not a state";
        q->setStateInformation (junk, (int) sizeof (junk));
        CHECK (q->getCurrentProgram() == 6);
    }

    // Die Oberflaeche laesst sich anlegen und wieder abbauen (alle Looks)
    {
        for (int theme = 0; theme < 2; ++theme)
        {
            p->uiTheme.store (theme);
            std::unique_ptr<juce::AudioProcessorEditor> editor (p->createEditor());
            CHECK (editor != nullptr);
            editor->setSize (820, 420);
        }
    }

    return 0;
}
