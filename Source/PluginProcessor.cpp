#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

static juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

juce::StringArray TumultProcessor::sizeNames()
{
    return { "Klein", "Mittel", u8 ("Gro\xc3\x9f"), "Stadion" };   // Reihenfolge wie tumult::sizeCfg()
}

namespace
{
constexpr float kOwnMin = -60.0f, kOwnMax = 6.0f;   // EIGENE STIMME in dB, ganz links = aus

struct Preset { const char* name; int size; float amount, chaos, variety, room, ownDb, walla, wallaInt; bool enabled = true; };
const Preset kPresets[] = {
    { "Kneipe",                    0, 0.70f, 0.60f, 0.70f, 0.30f, -3.0f, 0.50f, 0.25f },
    { "Kleine Demo",               1, 0.70f, 0.35f, 0.55f, 0.20f, -3.0f, 0.35f, 0.50f },
    { "Protestzug",                1, 0.75f, 0.40f, 0.65f, 0.30f, -3.0f, 0.45f, 0.70f },
    { "Marktplatz",                1, 0.70f, 0.75f, 0.90f, 0.45f, -6.0f, 0.50f, 0.35f },
    { "Gro\xc3\x9f" "demo auf dem Platz", 2, 0.75f, 0.40f, 0.75f, 0.40f, -3.0f, 0.45f, 0.75f },
    { "Sporthalle",                2, 0.75f, 0.50f, 0.80f, 0.55f, -6.0f, 0.40f, 0.50f },
    { "Stadion-Chor",              3, 0.80f, 0.25f, 0.80f, 0.45f, -6.0f, 0.35f, 0.60f },
    { "Stadion-Jubel",             3, 0.80f, 0.80f, 1.00f, 0.55f, -80.0f, 0.40f, 0.90f },
    { "Ausgangslage",              1, 0.70f, 0.45f, 0.60f, 0.35f, -3.0f, 0.50f, 0.50f, false },
};
// Die Gemurmel-Aufnahmen (eingebettet) werden einmal dekodiert und von allen Instanzen geteilt
static std::shared_ptr<const tumult::MurmurBank> murmurBank()
{
    static const std::shared_ptr<const tumult::MurmurBank> bank = []() -> std::shared_ptr<const tumult::MurmurBank>
    {
        struct Item { int level; const char* data; int size; };
        const Item items[] = {
            { 0, BinaryData::L1_a_ogg, BinaryData::L1_a_oggSize }, { 0, BinaryData::L1_b_ogg, BinaryData::L1_b_oggSize },
            { 0, BinaryData::L1_c_ogg, BinaryData::L1_c_oggSize }, { 0, BinaryData::L1_d_ogg, BinaryData::L1_d_oggSize },
            { 1, BinaryData::L2_a_ogg, BinaryData::L2_a_oggSize }, { 1, BinaryData::L2_b_ogg, BinaryData::L2_b_oggSize },
            { 1, BinaryData::L2_c_ogg, BinaryData::L2_c_oggSize }, { 1, BinaryData::L2_d_ogg, BinaryData::L2_d_oggSize },
            { 2, BinaryData::L3_a_ogg, BinaryData::L3_a_oggSize },
            { 3, BinaryData::L4_a_ogg, BinaryData::L4_a_oggSize }, { 3, BinaryData::L4_b_ogg, BinaryData::L4_b_oggSize },
            { 3, BinaryData::L4_c_ogg, BinaryData::L4_c_oggSize },
            { 4, BinaryData::L5_a_ogg, BinaryData::L5_a_oggSize }, { 4, BinaryData::L5_b_ogg, BinaryData::L5_b_oggSize } };
        juce::OggVorbisAudioFormat format;
        auto b = std::make_shared<tumult::MurmurBank>();
        double rate = 0.0;
        for (const auto& it : items)
        {
            std::unique_ptr<juce::AudioFormatReader> reader (format.createReaderFor (new juce::MemoryInputStream (it.data, (size_t) it.size, false), true));
            if (reader == nullptr || reader->numChannels < 1 || reader->lengthInSamples < 1024) return nullptr;
            if (rate == 0.0) rate = reader->sampleRate;
            if (reader->sampleRate != rate) return nullptr;
            const int n = (int) reader->lengthInSamples;
            juce::AudioBuffer<float> buf (1, n);
            reader->read (&buf, 0, n, 0, true, false);
            std::vector<int16_t> y ((size_t) n);
            for (int i = 0; i < n; ++i)
                y[(size_t) i] = (int16_t) juce::roundToInt (juce::jlimit (-1.0f, 1.0f, buf.getSample (0, i)) * 32767.0f);
            b->level[it.level].push_back (std::move (y));
        }
        b->rate = rate;
        return b->usable() ? b : nullptr;
    }();
    return bank;
}

constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}

juce::AudioProcessorValueTreeState::ParameterLayout TumultProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Prozentanzeigen und Texteingaben verwenden dieselbe Einheit (auch Dezimalkomma).
    auto number = [] (String text, float fallback)
    {
        text = text.trim().replaceCharacter (',', '.');
        if (! text.containsAnyOf ("0123456789")) return fallback;
        return text.getFloatValue();
    };
    auto parsePercent = [number] (const String& text)
    { return jlimit (0.0f, 1.0f, number (text, 0.0f) * 0.01f); };
    auto percent = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction (parsePercent);
    auto decibel = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (v, 1) + " dB"; })
        .withValueFromStringFunction ([number] (const String& text) { return number (text, 0.0f); });
    auto ownText = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return v <= kOwnMin + 0.05f ? String ("aus") : String (v, 1) + " dB"; })
        .withValueFromStringFunction ([number] (const String& text)
        { return text.trim().equalsIgnoreCase ("aus") ? kOwnMin : number (text, kOwnMin); });
    auto wallaText = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return v <= 0.001f ? String ("aus") : String (roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction (parsePercent);
    auto intText = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction ([parsePercent] (const String& text)
        {
            const char* names[] = { "Raunen", "Gemurmel", "angespannt", "aufgebracht", "Tumult" };
            for (int i = 0; i < 5; ++i) if (text.trim().equalsIgnoreCase (names[i])) return (float) i * 0.25f;
            return parsePercent (text);
        });
    auto chaosText = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { const int pct = roundToInt (v * 100.0f);
            return pct <= 0 ? u8 ("im Gleichschritt") : pct >= 100 ? String ("totales Chaos") : String (pct) + " %"; })
        .withValueFromStringFunction ([parsePercent] (const String& text)
        {
            if (text.trim().equalsIgnoreCase ("totales Chaos")) return 1.0f;
            return parsePercent (text);
        });

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "size", 1 }, u8 ("Gr\xc3\xb6\xc3\x9f" "e"), sizeNames(), 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Menge",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "chaos", 1 }, "Chaos",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.45f, chaosText));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "variety", 1 }, "Vielfalt",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "room", 1 }, "Raum",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.35f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "walla", 1 }, "Gemurmel",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.5f, wallaText));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "wallaInt", 1 }, u8 ("Intensit\xc3\xa4t"),
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.5f, intText));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "own", 1 }, "Eigene Stimme",
                                                       NormalisableRange<float> (kOwnMin, kOwnMax), -3.0f, ownText));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 12.0f), 0.0f, decibel));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "wallaEnabled", 1 }, "Gemurmel an", false));
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
    pWalla = apvts.getRawParameterValue ("walla");
    pWallaInt = apvts.getRawParameterValue ("wallaInt");
    pWallaEnabled = apvts.getRawParameterValue ("wallaEnabled");
    // Jede Instanz bekommt ihre eigene Menge (zwei Spuren mit Tumult rufen nicht im Gleichschritt dieselben Leute)
    seed.store ((juce::Random::getSystemRandom().nextInt() | 1) & 0x7fffffff);
}

void TumultProcessor::prepareToPlay (double sampleRate, int maxBlock)
{
    engine.prepare (sampleRate, seed.load());
    engine.setMurmurBank (murmurBank());   // wird beim ersten Mal dekodiert, nie im Audio-Thread
    const size_t n = (size_t) juce::jmax (maxBlock, 4096);
    scratchL.assign (n, 0.0f);
    scratchR.assign (n, 0.0f);
    setLatencySamples (engine.latencySamples());   // fester Vorlauf, der Host gleicht ihn aus
}

void TumultProcessor::reset() { engine.reset (seed.load()); }

void TumultProcessor::rerollCrowd()
{
    const uint32_t next = (uint32_t) ((juce::Random::getSystemRandom().nextInt() | 1) & 0x7fffffff);
    seed.store (next);
    engine.requestSeed (next);
}

double TumultProcessor::getTailLengthSeconds() const
{
    // Das Bett erzeugt auch auf einer leeren Spur dauerhaft Audio. JUCE meldet VST3 kInfiniteTail.
    return pWallaEnabled != nullptr && pWallaEnabled->load() >= 0.5f
        ? std::numeric_limits<double>::infinity() : 5.0;
}

bool TumultProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (in.isDisabled() || in == out) return true;
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
    p.walla = pWalla->load();
    p.wallaEnabled = pWallaEnabled->load() >= 0.5f;
    p.wallaInt = pWallaInt->load();
    const float own = pOwn->load();
    p.ownDb = own <= kOwnMin + 0.05f ? -80.0f : own;   // ganz links = aus
    p.outDb = pOut->load();

    if (getTotalNumInputChannels() == 0) buffer.clear();
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
            engine.process (scratchL.data(), scratchR.data(), count, p, true);
            for (int i = 0; i < count; ++i)
                left[offset + i] = scratchL[(size_t) i];
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
    set ("walla", pr.walla);
    set ("wallaEnabled", pr.enabled ? 1.0f : 0.0f);
    set ("wallaInt", pr.wallaInt);
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
    state.setProperty ("stateVersion", 2, nullptr);
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
            const int width = juce::jlimit (kUiWidth / 2, kUiWidth * 3, (int) state.getProperty ("uiW", 820));
            uiWidth.store (width);
            uiHeight.store (juce::roundToInt ((float) width * 420.0f / 820.0f));
            currentProgram.store (juce::jlimit (0, kNumPresets - 1, (int) state.getProperty ("program", 0)));
            // Dieselbe Menge wie beim Speichern: die Zusammensetzung haengt am gespeicherten Startwert
            const auto saved = (juce::int64) state.getProperty ("seed", (juce::int64) 0);
            if (saved > 0 && saved <= 0x7fffffff)
            {
                seed.store ((uint32_t) saved);
                engine.requestSeed ((uint32_t) saved);
            }
            // Alte Projekte behalten ihren Gemurmelpegel und ihr bisheriges An/Aus-Verhalten.
            bool hasSwitch = false;
            float oldWalla = 0.0f;
            for (auto child : state)
            {
                const auto id = child.getProperty ("id").toString();
                if (id == "wallaEnabled") hasSwitch = true;
                if (id == "walla") oldWalla = (float) child.getProperty ("value", 0.0f);
            }
            if (! hasSwitch)
            {
                juce::ValueTree sw ("PARAM");
                sw.setProperty ("id", "wallaEnabled", nullptr);
                sw.setProperty ("value", oldWalla > 0.0f ? 1.0f : 0.0f, nullptr);
                state.addChild (sw, -1, nullptr);
                // In alten Zustaenden vor 1.2 gab es auch den Pegel noch nicht.
                if (state.getChildWithProperty ("id", "walla").isValid() == false)
                {
                    juce::ValueTree level ("PARAM"); level.setProperty ("id", "walla", nullptr);
                    level.setProperty ("value", 0.0f, nullptr); state.addChild (level, -1, nullptr);
                }
            }
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* TumultProcessor::createEditor() { return new TumultEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TumultProcessor(); }
