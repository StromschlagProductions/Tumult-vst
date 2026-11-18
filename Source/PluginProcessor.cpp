#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

juce::StringArray TumultProcessor::sizeNames()
{
    return { "Klein", "Mittel", u8 ("Gro\xc3\x9f"), "Stadion" };   // Reihenfolge wie tumult::sizeCfg()
}

namespace
{
constexpr float kOwnMin = -60.0f, kOwnMax = 6.0f;   // EIGENE STIMME in dB, ganz links = aus

struct Preset { const char* name; int size; float amount, chaos, variety, room, ownDb; };
const Preset kPresets[] = {
    { "Kneipe",                    0, 0.70f, 0.60f, 0.70f, 0.30f, -3.0f },
    { "Kleine Demo",               1, 0.70f, 0.35f, 0.55f, 0.20f, -3.0f },
    { "Protestzug",                1, 0.75f, 0.40f, 0.65f, 0.30f, -3.0f },
    { "Marktplatz",                1, 0.70f, 0.75f, 0.90f, 0.45f, -6.0f },
    { "Gro\xc3\x9f" "demo auf dem Platz", 2, 0.75f, 0.40f, 0.75f, 0.40f, -3.0f },
    { "Sporthalle",                2, 0.75f, 0.50f, 0.80f, 0.55f, -6.0f },
    { "Stadion-Chor",              3, 0.80f, 0.25f, 0.80f, 0.45f, -6.0f },
    { "Stadion-Jubel",             3, 0.80f, 0.80f, 1.00f, 0.55f, -80.0f },
};
constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}

juce::AudioProcessorValueTreeState::ParameterLayout TumultProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto decibel = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (v, 1) + " dB"; });
    auto ownText = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return v <= kOwnMin + 0.05f ? String ("aus") : String (v, 1) + " dB"; });
    auto chaosText = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { const int pct = roundToInt (v * 100.0f);
                            return pct <= 0 ? u8 ("im Gleichschritt") : pct >= 100 ? String ("totales Chaos") : String (pct) + " %"; });

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "size", 1 }, u8 ("Gr\xc3\xb6\xc3\x9f" "e"), sizeNames(), 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Menge",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "chaos", 1 }, "Chaos",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.45f, chaosText));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "variety", 1 }, "Vielfalt",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "room", 1 }, "Raum",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.35f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "own", 1 }, "Eigene Stimme",
                                                       NormalisableRange<float> (kOwnMin, kOwnMax), -3.0f, ownText));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 12.0f), 0.0f, decibel));
    return layout;
}

TumultProcessor::TumultProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TUMULT", createLayout())
{
    pSize = apvts.getRawParameterValue ("size");
    pAmount = apvts.getRawParameterValue ("amount");
    pChaos = apvts.getRawParameterValue ("chaos");
    pVariety = apvts.getRawParameterValue ("variety");
    pRoom = apvts.getRawParameterValue ("room");
    pOwn = apvts.getRawParameterValue ("own");
    pOut = apvts.getRawParameterValue ("output");
    // Jede Instanz bekommt ihre eigene Menge (zwei Spuren mit Tumult rufen nicht im Gleichschritt dieselben Leute)
    seed.store ((juce::Random::getSystemRandom().nextInt() | 1) & 0x7fffffff);
}

void TumultProcessor::prepareToPlay (double sampleRate, int maxBlock)
{
    engine.prepare (sampleRate, seed.load());
    const size_t n = (size_t) juce::jmax (maxBlock, 4096);
    scratchL.assign (n, 0.0f);
    scratchR.assign (n, 0.0f);
    setLatencySamples (engine.latencySamples());   // fester Vorlauf, der Host gleicht ihn aus
}

void TumultProcessor::reset() { engine.reset(); }

bool TumultProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (in == out) return true;
    return in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo();   // Mono rein, Stereo raus
}

void TumultProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numSamples == 0 || numCh == 0)
        return;

    tumult::Params p;
    p.size = juce::roundToInt (tumult::finiteClamp (pSize->load(), 0.0f, (float) (tumult::kNumSizes - 1), 1.0f));
    p.amount = pAmount->load();
    p.chaos = pChaos->load();
    p.variety = pVariety->load();
    p.room = pRoom->load();
    const float own = pOwn->load();
    p.ownDb = own <= kOwnMin + 0.05f ? -80.0f : own;   // ganz links = aus
    p.outDb = pOut->load();

    float* left = buffer.getWritePointer (0);

    if (numCh > 1)
    {
        // Mono rein, Stereo raus: die Stimme kommt auf beide Seiten, die Menge ersetzt den Eingang
        if (getTotalNumInputChannels() == 1)
            buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
        engine.process (left, buffer.getWritePointer (1), numSamples, p);
        for (int ch = 2; ch < numCh; ++ch)
            buffer.clear (ch, 0, numSamples);
    }
    else
    {
        // reine Mono-Spur: die Menge wird zu Mono zusammengemischt (in vorreservierten Abschnitten)
        if (scratchL.empty()) { buffer.clear(); return; }
        for (int offset = 0; offset < numSamples;)
        {
            const int count = std::min ((int) scratchL.size(), numSamples - offset);
            std::copy_n (left + offset, count, scratchL.data());
            std::copy_n (left + offset, count, scratchR.data());
            engine.process (scratchL.data(), scratchR.data(), count, p);
            for (int i = 0; i < count; ++i)
                left[offset + i] = 0.70710678f * (scratchL[(size_t) i] + scratchR[(size_t) i]);
            offset += count;
        }
    }
}

//==============================================================================
int TumultProcessor::getNumPrograms() { return kNumPresets; }

const juce::String TumultProcessor::getProgramName (int i)
{
    return i >= 0 && i < kNumPresets ? u8 (kPresets[i].name) : juce::String();
}

void TumultProcessor::setCurrentProgram (int i)
{
    if (i < 0 || i >= kNumPresets) return;
    currentProgram = i;
    const auto& pr = kPresets[i];
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set ("size", (float) pr.size);
    set ("amount", pr.amount);
    set ("chaos", pr.chaos);
    set ("variety", pr.variety);
    set ("room", pr.room);
    set ("own", pr.ownDb <= -60.0f ? kOwnMin : pr.ownDb);
}

void TumultProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("theme", uiTheme.load(), nullptr);
    state.setProperty ("uiW", uiWidth.load(), nullptr);
    state.setProperty ("uiH", uiHeight.load(), nullptr);
    state.setProperty ("program", currentProgram.load(), nullptr);
    state.setProperty ("seed", (juce::int64) seed.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TumultProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName ("TUMULT"))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            uiTheme.store ((int) state.getProperty ("theme", 0) == 1 ? 1 : 0);
            const int width = juce::jlimit (340, 2040, (int) state.getProperty ("uiW", 680));
            uiWidth.store (width);
            uiHeight.store (juce::roundToInt ((float) width * 420.0f / 680.0f));
            currentProgram.store (juce::jlimit (0, kNumPresets - 1, (int) state.getProperty ("program", 0)));
            // Dieselbe Menge wie beim Speichern: die Zusammensetzung haengt am gespeicherten Startwert
            const auto saved = (juce::int64) state.getProperty ("seed", (juce::int64) 0);
            if (saved > 0 && saved <= 0x7fffffff) seed.store ((uint32_t) saved);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* TumultProcessor::createEditor() { return new TumultEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TumultProcessor(); }
