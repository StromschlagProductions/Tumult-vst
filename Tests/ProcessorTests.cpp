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
        CHECK (p->getNumPrograms() == 9);
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
        setParam ("wallaEnabled", 1.0f);
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
        setParam ("wallaEnabled", 0.0f); setParam ("walla", 0.0f); setParam ("amount", 0.7f); setParam ("own", -3.0f); setParam ("room", 0.35f);
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
        for (auto* id : { "size", "amount", "chaos", "variety", "walla", "wallaInt", "wallaEnabled", "room", "own", "output" })
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

    // Wertefelder: Prozent, Dezimalkomma, aus und Intensitaetsnamen sind round-trip-faehig.
    {
        auto checkText = [&] (const char* id, const juce::String& text, float expected)
        {
            auto* param = p->apvts.getParameter (id);
            const float actual = param->convertFrom0to1 (param->getValueForText (text));
            CHECK (std::abs (actual - expected) < 1e-5f);
        };
        for (auto* id : { "amount", "chaos", "variety", "room", "walla", "wallaInt" })
        {
            checkText (id, "70 %", 0.7f);
            checkText (id, "12,5 %", 0.125f);
        }
        checkText ("own", "aus", -60.0f); checkText ("own", "-3,5 dB", -3.5f);
        checkText ("walla", "aus", 0.0f); checkText ("wallaInt", "angespannt", 0.5f);
        checkText ("chaos", "totales Chaos", 1.0f);
    }

    // Mono: identische Kanaele bleiben bei hohen gueltigen Pegeln sicher begrenzt.
    for (double fs : { 44100.0, 48000.0 })
    {
        layout.inputBuses.set (0, juce::AudioChannelSet::mono());
        layout.outputBuses.set (0, juce::AudioChannelSet::mono());
        CHECK (p->setBusesLayout (layout));
        auto set = [&] (const char* id, float value) { auto* v = p->apvts.getParameter (id); v->setValueNotifyingHost (v->convertTo0to1 (value)); };
        set ("amount", 0); set ("room", 0); set ("own", 6); set ("output", 12); set ("wallaEnabled", 0);
        p->prepareToPlay (fs, 512);
        juce::AudioBuffer<float> b (1, 512);
        double peak = 0;
        for (int block = 0; block < 120; ++block)
        {
            for (int i = 0; i < 512; ++i) b.setSample (0, i, 0.8f * std::sin (0.1f * (float) (block * 512 + i)));
            { NoAllocations guard; p->processBlock (b, midi); }
            for (int i = 0; i < 512; ++i) { const float x = b.getSample (0, i); CHECK (std::abs (x) <= 1.0f); peak = std::max (peak, (double) std::abs (x)); }
        }
        CHECK (peak > 0.9);
    }

    // Generatorbetrieb mit deaktiviertem Eingang; Schalter, Projektzustand und Pegel sind unabhaengig.
    {
        layout.inputBuses.set (0, juce::AudioChannelSet::disabled());
        layout.outputBuses.set (0, juce::AudioChannelSet::stereo());
        CHECK (p->setBusesLayout (layout));
        p->setCurrentProgram (8);
        CHECK (std::isfinite (p->getTailLengthSeconds()));
        auto* sw = p->apvts.getParameter ("wallaEnabled"); sw->setValueNotifyingHost (1);
        CHECK (std::isinf (p->getTailLengthSeconds()));
        p->prepareToPlay (48000, 256);
        juce::AudioBuffer<float> b (2, 256);
        double e = 0;
        for (int block = 0; block < 400; ++block)
        {
            // Der Generator darf nicht von zufaelligem Inhalt unbenutzter Eingangskanaele abhaengen.
            fill (b, block * 256);
            { NoAllocations guard; p->processBlock (b, midi); }
            for (int i = 0; i < 256; ++i) e += (double) b.getSample (0, i) * b.getSample (0, i);
        }
        CHECK (e > 1e-3);
        const float level = p->apvts.getRawParameterValue ("walla")->load();
        sw->setValueNotifyingHost (0);
        for (int block = 0; block < 400; ++block) { b.clear(); p->processBlock (b, midi); }
        CHECK (p->apvts.getRawParameterValue ("walla")->load() == level);
        double tail = 0; for (int i = 0; i < 256; ++i) tail += std::abs (b.getSample (0, i));
        CHECK (tail < 1e-4);
        layout.inputBuses.set (0, juce::AudioChannelSet::stereo()); CHECK (p->setBusesLayout (layout));
    }

    // State nach prepareToPlay: die gewaehlten Personen werden auch ohne erneutes Prepare uebernommen.
    {
        p->setCurrentProgram (2); p->rerollCrowd(); p->uiWidth.store (2460);
        juce::MemoryBlock state; p->getStateInformation (state);
        auto q = std::make_unique<TumultProcessor>();
        q->prepareToPlay (48000, 256);
        q->setStateInformation (state.getData(), (int) state.getSize());
        CHECK (q->uiWidth.load() == 2460);
        p->prepareToPlay (48000, 256);
        juce::AudioBuffer<float> a (2, 256), b (2, 256);
        // Das Bett hat seinen eigenen laufenden Abspielzustand; hier wird die Besetzung verglichen.
        for (auto* proc : { p.get(), q.get() }) proc->apvts.getParameter ("wallaEnabled")->setValueNotifyingHost (0);
        for (int block = 0; block < 400; ++block)
        {
            fill (a, block * 256); b.makeCopyOf (a);
            { NoAllocations guard; p->processBlock (a, midi); }
            { NoAllocations guard; q->processBlock (b, midi); }
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) CHECK (a.getSample (ch, i) == b.getSample (ch, i));
        }
        // Migration alter 1.2-Zustaende: Gemurmel bleibt an, wenn bisher ein Pegel eingestellt war.
        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
        CHECK (xml != nullptr);
        auto tree = juce::ValueTree::fromXml (*xml);
        tree.removeChild (tree.getChildWithProperty ("id", "wallaEnabled"), nullptr);
        juce::MemoryBlock legacy;
        auto oldXml = tree.createXml(); juce::AudioProcessor::copyXmlToBinary (*oldXml, legacy);
        q->setStateInformation (legacy.getData(), (int) legacy.getSize());
        CHECK (q->apvts.getRawParameterValue ("wallaEnabled")->load() == 1.0f);
    }

    // Host-Bypass behaelt die gemeldeten 500 ms bei und erhaelt Stereo.
    for (double fs : { 44100.0, 48000.0 })
    {
        p->setCurrentProgram (8);
        layout.inputBuses.set (0, juce::AudioChannelSet::stereo());
        layout.outputBuses.set (0, juce::AudioChannelSet::stereo()); CHECK (p->setBusesLayout (layout));
        p->prepareToPlay (fs, (int) fs);
        juce::AudioBuffer<float> b (2, (int) fs); b.clear();
        b.setSample (0, 100, 0.5f); b.setSample (1, 100, -0.25f);
        { NoAllocations guard; p->processBlockBypassed (b, midi); }
        const int at = 100 + (int) std::lround (fs * 0.5);
        CHECK (std::abs (b.getSample (0, at) - 0.5f) < 1e-6f);
        CHECK (std::abs (b.getSample (1, at) + 0.25f) < 1e-6f);
        for (int i = 0; i < at; ++i) CHECK (std::abs (b.getSample (0, i)) < 1e-12f);
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
