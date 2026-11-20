// TumultDSP.h - Klang-Engine fuer "Tumult" (Stromschlag Audio): Teil 1 der Stimmvervielfacher, Teil 2 das Gemurmel
// Reines C++ ohne JUCE, damit die Engine unabhaengig vom Plugin getestet und zu Hoerbeispielen gerendert werden kann.
//
// Aus EINER aufgenommenen Stimme wird eine Menschenmenge, die denselben Text ruft (zum Beispiel einen Sprechchor).
// Jede Person der Menge ist eine "Stimme" mit eigener Persoenlichkeit:
//
//   Einsatz-Versatz (wer ruft frueher, wer spaeter)   Tonlage (Maenner, Frauen, Jugendliche)
//   Klangfarbe (Brust, Naehe, Entfernung)             Lage im Raum (links/rechts, vorne/hinten)
//   Beteiligung (nicht jeder ruft jedes Mal mit)      Lautstaerke
//
// Staffelung: wenige Stimmen vorne (deutlich, hell, nahe am Einsatz), die Masse dahinter (dumpfer, leiser, breiter
// gestreut). Dahinter liegt ein Raum (Klein = Kneipe ... Stadion = Arena mit Echos).
//
// Personen bleiben bis NEUE LEUTE erhalten; nur Einsatz und Beteiligung variieren in Sprechpausen. Darum gibt es
// kein Knacken, und jede Wiederholung des Rufs klingt ein bisschen anders. Die Stimme laeuft mit einem festen Vorlauf
// von 500 ms (Latenz), damit die Menge auch VOR dem Einsatz der eigenen Stimme einsetzen kann. Jede Stimme hat ausserdem
// ihren begrenzten Zeitverlauf waehrend aktiver Sprache (keine unabhaengige Time-Stretch-Engine).
//
// Vordergrund: periodensynchrone Fenster trennen bei sicherer stimmhafter Analyse Tonlage, Formantlage und Zeitverlauf.
// Rueckfall und hintere Masse: bisherige Delay-Bearbeitung. Keine zusaetzlichen Bedienparameter.
// Teil 2, GEMURMEL: ein Dauerbett aus Gemurmel in fuenf Stufen (leises Raunen ... aufgebrachte Menge). INTENSITAET blendet
// stufenlos zwischen den Stufen. Jede Stufe besteht aus mehreren Aufnahmen; links und rechts laufen unabhaengige Spieler, die
// in zufaelliger Reihenfolge und an zufaelligen Stellen weiterlaufen und weich ineinander ueberblenden, so dass sich nichts
// hoerbar wiederholt. Das Bett geht durch denselben Raum wie die Menge.
//
// Block-Unabhaengigkeit: Alles wird in festen 16-Sample-Schritten gerechnet, der Zaehler gehoert zur Engine. Ergebnis
// ist bit-genau gleich, egal wie der Host den Strom in Bloecke teilt (bei gleichen Reglerwerten).
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include "VoiceModel.h"

namespace tumult
{
constexpr float kPi = 3.14159265358979f;
constexpr int kChunk = 16;               // Steuerwerte und Stimmen-Schritt alle 16 Samples
constexpr int kMaxVoices = 64;
constexpr int kNumMurmurLevels = 5;     // Raunen, Gemurmel, angespannt, aufgebracht, kurz vor dem Ruf
constexpr float kMurmurXfadeSec = 3.0f;  // Ueberblendzeit zwischen zwei Aufnahmen
constexpr float kMurmurCal = 1.0f;       // Eichung des Gemurmels bei GEMURMEL 100 %
constexpr int kNumSizes = 4;             // Klein, Mittel, Gross, Stadion
constexpr float kLatencySec = 0.500f;    // fester Vorlauf: die Menge darf vor und nach der eigenen Stimme einsetzen
constexpr float kWindowSec = 0.060f;     // Fenster des Tonhoehen-Verschiebers
constexpr float kSpeedMax = 0.06f;       // groesste Abweichung des Sprechtempos (+-6 %) bei CHAOS 100 %
constexpr float kAccMaxSec = 0.08f;      // so weit darf eine Stimme durch ihr Tempo hoechstens davonlaufen
constexpr float kSilenceLevel = 0.0032f; // ca. -50 dBFS (Spitze)
constexpr int kMinDelay = 40;            // Samples; garantiert, dass nur schon geschriebenes Material gelesen wird
constexpr float kCrowdCal = 1.85f;       // Eichung: bei MENGE 70 % ist die Menge etwa so laut wie die Eingangsstimme

inline float finiteClamp (float x, float lo, float hi, float fallback) { return std::isfinite (x) ? std::clamp (x, lo, hi) : fallback; }
inline double validRate (double x) { return std::isfinite (x) && x >= 8000 && x <= 384000 ? x : 48000; }
inline float dbToLin (float db) { return db <= -79.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float logLerp (float a, float b, float t) { return a * std::pow (b / a, t); }
inline int sampleCount (double fs, float seconds) { return (int) std::lround (fs * (double) seconds); }

// Weiches Begrenzen nur ueber 0,8 (darunter unveraendert), schuetzt vor Uebersteuern bei Extremwerten
inline float softClip (float x)
{
    const float a = std::fabs (x);
    if (a < 0.8f) return x;
    const float s = x < 0.0f ? -1.0f : 1.0f;
    return s * (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f));
}

struct Rng
{
    uint32_t s;
    explicit Rng (uint32_t seed = 1) : s (seed ? seed : 1u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // [0,1)
    float bi() { return uni() * 2.0f - 1.0f; }                             // [-1,1)
    float gauss() { return (uni() + uni() + uni() + uni() - 2.0f) * 1.7320508f; }   // Mittelwert 0, Streuung ca. 1
};

//==============================================================================
// Vier Groessen: Zahl der Stimmen, Streuung des Einsatzes, Raum
struct SizeCfg
{
    int voices, front;        // Stimmen insgesamt, davon vorne (deutlich)
    float spread;             // groesster Einsatz-Versatz (s), bei CHAOS 100 %
    float width;              // Breite im Stereobild 0..1
    float backDark;           // Grenzfrequenz der entferntesten Stimmen (Hz)
    float rt60, lenScale, preSec, dampHz;   // Raum
    float echo1, g1, echo2, g2;             // einzelne Echos von den Raengen (Sekunden, Pegel)
};

inline const SizeCfg& sizeCfg (int i)
{
    static const SizeCfg table[kNumSizes] = {
        // voices front spread width backDark | rt60 len  pre    damp   echo1 g1   echo2 g2
        {  10,    2,   0.100f, 0.65f, 5500.0f,   0.45f, 0.6f, 0.003f, 6500.0f, 0.0f, 0.0f, 0.0f, 0.0f },   // Klein: Kneipe, Hinterhof
        {  24,    3,   0.180f, 0.85f, 4200.0f,   0.95f, 1.0f, 0.008f, 5200.0f, 0.0f, 0.0f, 0.0f, 0.0f },   // Mittel: Platz, Saal
        {  40,    4,   0.280f, 1.00f, 3400.0f,   1.80f, 1.6f, 0.015f, 4200.0f, 0.090f, 0.22f, 0.0f, 0.0f },   // Gross: Halle
        {  64,    6,   0.400f, 1.00f, 2800.0f,   3.40f, 2.6f, 0.028f, 3200.0f, 0.170f, 0.30f, 0.320f, 0.18f }   // Stadion
    };
    return table[std::clamp (i, 0, kNumSizes - 1)];
}

struct Params
{
    int size = 1;            // 0 Klein, 1 Mittel, 2 Gross, 3 Stadion
    float amount = 0.7f;     // MENGE: Lautstaerke der Menge, 0..1
    float chaos = 0.45f;     // CHAOS: 0 = fast im Gleichschritt, 1 = voellig ungeordnet
    float variety = 0.6f;    // VIELFALT: 0 = klingen alle aehnlich, 1 = Maenner, Frauen, Jugendliche gemischt
    float room = 0.35f;      // RAUM: Anteil des Raums
    bool wallaEnabled = true; // separater Schalter; Pegel bleibt erhalten
    bool choirEnabled = true; // CHOR an/aus: aus = nur Gemurmel (und Raum), keine Menge, keine Latenz
    float walla = 0.0f;      // GEMURMEL: Pegel des Gemurmel-Betts, 0 = aus
    float wallaInt = 0.5f;   // INTENSITAET: 0 = leises Raunen ... 1 = aufgebrachte Menge
    float ownDb = -3.0f;     // EIGENE STIMME in dB (-80 = aus)
    float outDb = 0.0f;      // OUTPUT in dB
};

inline Params sanitize (Params p)
{
    p.size = (int) std::lround (finiteClamp ((float) p.size, 0.0f, (float) (kNumSizes - 1), 1.0f));
    p.amount = finiteClamp (p.amount, 0.0f, 1.0f, 0.7f);
    p.chaos = finiteClamp (p.chaos, 0.0f, 1.0f, 0.45f);
    p.variety = finiteClamp (p.variety, 0.0f, 1.0f, 0.6f);
    p.room = finiteClamp (p.room, 0.0f, 1.0f, 0.35f);
    p.walla = finiteClamp (p.walla, 0.0f, 1.0f, 0.0f);
    p.wallaInt = finiteClamp (p.wallaInt, 0.0f, 1.0f, 0.5f);
    p.ownDb = finiteClamp (p.ownDb, -80.0f, 12.0f, -3.0f);
    p.outDb = finiteClamp (p.outDb, -60.0f, 24.0f, 0.0f);
    return p;
}

//==============================================================================
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void clear() { z1 = z2 = 0; }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void lowpass (float fc, float Q, float fs)
    {
        fc = std::clamp (fc, 20.0f, 0.45f * fs);
        const float w = 2.0f * kPi * fc / fs, c = std::cos (w), al = std::sin (w) / (2.0f * Q), a0 = 1.0f + al;
        b0 = (1.0f - c) * 0.5f / a0; b1 = (1.0f - c) / a0; b2 = b0;
        a1 = -2.0f * c / a0; a2 = (1.0f - al) / a0;
    }
    void peaking (float fc, float Q, float gainDb, float fs)
    {
        fc = std::clamp (fc, 20.0f, 0.45f * fs);
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 2.0f * kPi * fc / fs, c = std::cos (w), al = std::sin (w) / (2.0f * Q), a0 = 1.0f + al / A;
        b0 = (1.0f + al * A) / a0; b1 = -2.0f * c / a0; b2 = (1.0f - al * A) / a0;
        a1 = -2.0f * c / a0; a2 = (1.0f - al / A) / a0;
    }
};

// Raum: Feedback-Delay-Netzwerk mit 8 Leitungen, Vorlauf, einzelnen Echos und weichem Umschalten der Groesse
class Reverb
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        for (int i = 0; i < N; ++i)
            line[i].assign ((size_t) sampleCount (sampleRate, kBaseMs[i] * 0.001f * kMaxScale) + 8, 0.0f);
        pre.assign ((size_t) sampleCount (sampleRate, 0.5f) + 8, 0.0f);
        hpCoef = 1.0f - std::exp (-2.0f * kPi * 180.0f / fs);
        reset();
        apply (1);
    }
    void reset()
    {
        for (int i = 0; i < N; ++i) { std::fill (line[i].begin(), line[i].end(), 0.0f); pos[i] = 0; lp[i] = 0.0f; }
        std::fill (pre.begin(), pre.end(), 0.0f);
        prePos = 0; hpL = hpR = 0.0f; fade = 1.0f;
    }
    void setSize (int size) { wanted = std::clamp (size, 0, kNumSizes - 1); }
    void configure (int size) { reset(); apply (std::clamp (size, 0, kNumSizes - 1)); }

    // wl, wr: nur der Raumanteil (ohne Eingang)
    void process (float in, float& wl, float& wr)
    {
        if (wanted != cfg)
        {
            fade -= 1.0f / (0.02f * fs);
            if (fade <= 0.0f)
            {
                fade = 0.0f;
                for (int i = 0; i < N; ++i) { std::fill (line[i].begin(), line[i].end(), 0.0f); lp[i] = 0.0f; }
                apply (wanted);
            }
        }
        else if (fade < 1.0f)
            fade = std::min (1.0f, fade + 1.0f / (0.03f * fs));

        // Vorlauf und Echos
        pre[(size_t) prePos] = in;
        const int preSize = (int) pre.size();
        auto tap = [&] (int d) { int r = prePos - d; if (r < 0) r += preSize; return pre[(size_t) r]; };
        float x = tap (preLen);
        if (e1 > 0) x += g1 * tap (e1);
        if (e2 > 0) x += g2 * tap (e2);
        if (++prePos >= preSize) prePos = 0;

        float y[N], a[N];
        for (int i = 0; i < N; ++i) a[i] = y[i] = line[i][(size_t) pos[i]];
        for (int h = 1; h < N; h *= 2)
            for (int i = 0; i < N; i += 2 * h)
                for (int j = i; j < i + h; ++j) { const float u = a[j], w = a[j + h]; a[j] = u + w; a[j + h] = u - w; }
        for (int i = 0; i < N; ++i)
        {
            const float v = 0.5f * x * kInSign[i] + g[i] * 0.35355339f * a[i];
            lp[i] += damp * (v - lp[i]);
            line[i][(size_t) pos[i]] = lp[i];
            if (++pos[i] >= len[i]) pos[i] = 0;
        }
        float l = 0, r = 0;
        for (int i = 0; i < N; ++i) { l += kOutL[i] * y[i]; r += kOutR[i] * y[i]; }
        l *= 0.35355339f * fade; r *= 0.35355339f * fade;
        hpL += hpCoef * (l - hpL); hpR += hpCoef * (r - hpR);   // Dumpfheit unter ca. 180 Hz raus
        wl = l - hpL; wr = r - hpR;
    }

private:
    static constexpr int N = 8;
    static constexpr float kMaxScale = 2.6f;
    static constexpr float kBaseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.3f, 79.1f };
    static constexpr float kInSign[N] = { 1, -1, 1, 1, -1, 1, -1, -1 };
    static constexpr float kOutL[N] = { 1, 1, -1, 1, -1, -1, 1, -1 };
    static constexpr float kOutR[N] = { 1, -1, 1, 1, -1, 1, -1, -1 };

    void apply (int size)
    {
        cfg = wanted = size;
        const auto& c = sizeCfg (size);
        for (int i = 0; i < N; ++i)
        {
            len[i] = std::max (16, sampleCount (fs, kBaseMs[i] * 0.001f * c.lenScale));
            len[i] = std::min (len[i], (int) line[i].size());
            pos[i] = 0;
            g[i] = std::pow (10.0f, -3.0f * (float) len[i] / (c.rt60 * fs));
        }
        damp = 1.0f - std::exp (-2.0f * kPi * c.dampHz / fs);
        preLen = std::max (1, sampleCount (fs, c.preSec));
        e1 = c.g1 > 0 ? sampleCount (fs, c.echo1) : 0; g1 = c.g1;
        e2 = c.g2 > 0 ? sampleCount (fs, c.echo2) : 0; g2 = c.g2;
    }

    float fs = 48000.0f;
    std::vector<float> line[N];
    std::vector<float> pre;
    int len[N] = {}, pos[N] = {};
    float lp[N] = {}, g[N] = {};
    float damp = 0.5f, hpCoef = 0.02f, hpL = 0.0f, hpR = 0.0f;
    int preLen = 1, prePos = 0, e1 = 0, e2 = 0, cfg = 1, wanted = 1;
    float g1 = 0.0f, g2 = 0.0f, fade = 1.0f;
};

// Zwei vorreservierte Raeume: alter Nachhall bleibt waehrend des Uebergangs erhalten.
class Room
{
public:
    void prepare (double rate)
    {
        fs = (float) rate;
        for (auto& r : rooms) r.prepare (rate);
        reset();
    }
    void reset()
    {
        for (auto& r : rooms) r.configure (1);
        current = 0; size = wanted = nextSize = 1; transition = false; first = true; mix = 0;
    }
    void setSize (int target) { wanted = std::clamp (target, 0, kNumSizes - 1); }
    void process (float input, float& l, float& r)
    {
        if (first) { rooms[current].configure (wanted); size = wanted; first = false; }
        if (! transition && size != wanted)
        {
            nextSize = wanted;
            rooms[1 - current].configure (nextSize);
            transition = true; mix = 0;
        }
        rooms[current].process (input, l, r);
        if (! transition) return;
        float nl, nr;
        rooms[1 - current].process (input, nl, nr);
        // 500 ms lassen auch dem Stadion Zeit, seine ersten Reflexionen aufzubauen.
        const float w = mix * mix * (3.0f - 2.0f * mix);
        l = lerp (l, nl, w); r = lerp (r, nr, w);
        mix = std::min (1.0f, mix + 1.0f / (0.5f * fs));
        if (mix >= 1.0f) { current = 1 - current; size = nextSize; transition = false; }
    }
private:
    Reverb rooms[2];
    float fs = 48000, mix = 0;
    int current = 0, size = 1, wanted = 1, nextSize = 1;
    bool transition = false, first = true;
};

//==============================================================================
// Die Gemurmel-Aufnahmen (mono, int16): fuenf Stufen mit je mehreren Aufnahmen. Wird vom Plugin einmal geladen und von allen
// Instanzen geteilt; die Engine veraendert sie nie.
struct MurmurBank
{
    double rate = 24000.0;
    std::vector<std::vector<int16_t>> level[kNumMurmurLevels];
    bool usable() const
    {
        if (! (rate >= 8000.0 && rate <= 96000.0)) return false;
        for (const auto& l : level) { if (l.empty()) return false; for (const auto& t : l) if (t.size() < 64) return false; }
        return true;
    }
};

//==============================================================================
class Engine
{
    friend struct EngineTestAccess; // gezielter Regressionstest der Phrasenzeit, keine GUI-Schnittstelle
public:
    Engine() = default;
    Engine (const Engine&) = delete;
    Engine& operator= (const Engine&) = delete;

    void prepare (double sampleRate, uint32_t seed = 1)
    {
        fs = (float) validRate (sampleRate);
        latency = sampleCount (fs, kLatencySec);
        window = (float) sampleCount (fs, kWindowSec);
        int n = 1;
        while (n < sampleCount (fs, 1.4f) + 64) n *= 2;
        ring.assign ((size_t) n, 0.0f);
        ownSide.assign ((size_t) latency, 0.0f);
        mask = (uint32_t) n - 1u;
        rev.prepare (fs);
        analysis.prepare (fs);
        aGain = 1.0f - std::exp (-(float) kChunk / (0.04f * fs));
        aDelay = 1.0f - std::exp (-(float) kChunk / (0.25f * fs));
        aRatio = 1.0f - std::exp (-(float) kChunk / (0.12f * fs));
        aMix = 1.0f - std::exp (-(float) kChunk / (0.30f * fs));
        envRelease = std::exp (-1.0f / (0.03f * fs));
        settleRate = 1.0f / (0.3f * fs);
        driftDecay = 1.0f - std::exp (-(float) kChunk / (1.111111f * fs));
        driftNoise = 0.0127f * std::sqrt (driftDecay / 0.0003f);
        filterStep = 1.0f / (0.03f * fs);
        baseSeed = seed ? seed : 1u;
        reset();
    }

    // Gemurmel-Aufnahmen setzen (nicht im Audio-Thread aufrufen); ohne Bank bleibt das Gemurmel stumm
    void setMurmurBank (std::shared_ptr<const MurmurBank> b)
    {
        bank = (b && b->usable()) ? std::move (b) : nullptr;
        initMurmur();
    }

    void reset (uint32_t seed = 0)
    {
        if (seed != 0) baseSeed = seed;
        pendingSeed.store (0, std::memory_order_relaxed);
        manualReroll.store (false, std::memory_order_relaxed);
        peopleDirty = true; phraseIndex = 0;
        std::fill (ownSide.begin(), ownSide.end(), 0.0f); ownPos = 0;
        std::fill (ring.begin(), ring.end(), 0.0f);
        wpos = 0;
        rev.reset();
        analysis.reset();
        rng = Rng (baseSeed);
        for (auto& q : v) { q = Voice(); }
        chunkPos = 0;
        std::fill (std::begin (stageL), std::end (stageL), 0.0f);
        std::fill (std::begin (stageR), std::end (stageR), 0.0f);
        env = 0.0f; silentRun = 1 << 29; rerollDone = false; snap = true; norm = 0.0f; visRef = 0.0f;
        ownCur = outCur = wetCur = bypassCur = 0.0f; ownInit = false;
        choirCur = vgCur = 1.0f; choirOn = true; choirInit = false;
        rerolls = 0; visCounter = 0;
        initMurmur();
        for (int i = 0; i < kMaxVoices; ++i) { visLevel[i].store (0.0f); visOn[i].store (0.0f); }
    }

    int latencySamples() const { return latency; }
    // Dem Host gemeldete Latenz: ohne Chor gibt es keinen Vorlauf
    int hostLatencySamples() const { return choirOn ? latency : 0; }
    // Wuerfelt die Menge in der naechsten Sprechpause neu (Knopf "Neue Leute")
    void requestReroll() { manualReroll.store (true, std::memory_order_release); }
    void requestSeed (uint32_t seed) { pendingSeed.store (seed ? seed : 1u, std::memory_order_release); }
    int rerollCount() const { return rerolls; }

    // Fuer die Anzeige (Oberflaeche): Lage und Aktivitaet jeder Stimme
    float voicePan (int i) const { return visPan[i].load (std::memory_order_relaxed); }
    float voiceDepth (int i) const { return visDepth[i].load (std::memory_order_relaxed); }
    float voiceOn (int i) const { return visOn[i].load (std::memory_order_relaxed); }
    float voiceLevel (int i) const { return visLevel[i].load (std::memory_order_relaxed); }

    // Eingang (links/rechts, bei Mono-Spuren beide gleich) wird durch den Ausgang ersetzt
    void process (float* l, float* r, int n, const Params& pIn, bool monoOutput = false, bool bypassed = false)
    {
        const Params p = sanitize (pIn);
        const float ownTarget = dbToLin (p.ownDb), outTarget = dbToLin (p.outDb);
        const float wetTarget = p.room * p.room * kRoomGain;
        if (! ownInit) { ownCur = ownTarget; outCur = outTarget; wetCur = wetTarget; ownInit = true; }
        const float aSmooth = 1.0f - std::exp (-1.0f / (0.015f * fs));
        const float choirTarget = p.choirEnabled ? 1.0f : 0.0f;
        if (! choirInit) { choirCur = vgCur = choirTarget; choirInit = true; }
        choirOn = p.choirEnabled;
        rev.setSize (p.size);

        for (int i = 0; i < n; ++i)
        {
            if (chunkPos == 0) renderChunk (p);

            const float il = std::isfinite (l[i]) ? l[i] : 0.0f;
            const float ir = std::isfinite (r[i]) ? r[i] : 0.0f;
            const float in = 0.5f * il + 0.5f * ir;
            const float sideNow = 0.5f * il - 0.5f * ir;
            const float sideDelayed = ownSide[(size_t) ownPos];
            ownSide[(size_t) ownPos] = sideNow;
            if (++ownPos == latency) ownPos = 0;
            const float mag = std::fabs (in);
            env = std::max (mag, env * envRelease);
            if (env < kSilenceLevel) { if (silentRun < (1 << 30)) ++silentRun; }
            else { silentRun = 0; rerollDone = false; }

            const float dryDelayed = ring[(wpos - (uint32_t) latency) & mask];
            choirCur += aSmooth * (choirTarget - choirCur);
            if (std::fabs (choirCur - choirTarget) < 1e-4f) choirCur = choirTarget;
            // Mit Chor kommt die eigene Stimme um den Vorlauf verzoegert, ohne Chor direkt (weicher Uebergang)
            const float dry = lerp (in, dryDelayed, choirCur);
            const float side = lerp (sideNow, sideDelayed, choirCur);
            ring[wpos & mask] = in;
            analysis.push (in, wpos);
            ++wpos;

            ownCur += aSmooth * (ownTarget - ownCur);
            outCur += aSmooth * (outTarget - outCur);
            wetCur += aSmooth * (wetTarget - wetCur);
            const float sl = stageL[chunkPos] + (dry + side) * ownCur;
            const float sr = stageR[chunkPos] + (dry - side) * ownCur;
            float wl, wr;
            rev.process (0.5f * (sl + sr), wl, wr);
            const float ol = (sl + wetCur * wl) * outCur;
            const float oright = (sr + wetCur * wr) * outCur;
            const float bypassTarget = bypassed ? 1.0f : 0.0f;
            bypassCur += aSmooth * (bypassTarget - bypassCur);
            if (std::fabs (bypassCur - bypassTarget) < 1e-4f) bypassCur = bypassTarget;
            if (monoOutput)
                l[i] = r[i] = lerp (softClip (0.5f * ol + 0.5f * oright), dry, bypassCur);
            else
            {
                l[i] = lerp (softClip (ol), dry + side, bypassCur);
                r[i] = lerp (softClip (oright), dry - side, bypassCur);
            }
            if (++chunkPos == kChunk) chunkPos = 0;
        }
    }

private:
    static constexpr float kRoomGain = 3.0f;

    struct Voice
    {
        // Persoenlichkeit (wird nur in der Sprechpause neu gewuerfelt)
        bool participates = true;
        float phraseDb = 0, formant = 1.0f, formantN = 0.0f;
        EpochVoice epoch;
        float jitter = 0, speedN = 0, semis = 0, pan = 0, depthU = 0.5f, levelDb = 0, hpHz = 200, peakHz = 1500, peakDb = 0, lpJit = 1;
        // Laufzeit
        float speechEnv = 0, filterMix = 1;
        Biquad oldLp;
        float acc = 0, delay = 0, ratio = 1, gL = 0, gR = 0, phase = 0.5f, driftD = 0, driftG = 0, hpState = 0, hpA = 0.02f, vis = 0;
        int filtSize = -1;
        Biquad lp, pk;
    };

    void reroll()
    {
        ++rerolls;
        Rng people (baseSeed);
        Rng phrase (baseSeed ^ (0x9e3779b9u * ++phraseIndex));
        for (int i = 0; i < kMaxVoices; ++i)
        {
            auto& q = v[(size_t) i];
            if (peopleDirty)
            {
                const float kind = people.uni();
                const float mean = kind < 0.35f ? -0.5f : kind < 0.75f ? 0.5f : kind < 0.88f ? 1.1f : 0.0f;
                q.semis = std::clamp (mean + people.gauss() * 0.25f, -1.0f, 1.0f);
                q.pan = people.bi(); q.depthU = people.uni(); q.levelDb = people.gauss() * 2.0f;
                q.hpHz = logLerp (100.0f, 320.0f, people.uni());
                q.peakHz = logLerp (700.0f, 3200.0f, people.uni());
                q.peakDb = people.bi() * 4.0f; q.lpJit = 0.85f + 0.3f * people.uni();
                q.filtSize = -1;
                q.formantN = Rng (baseSeed ^ ((uint32_t) (i + 1) * 2246822519u)).bi();
            }
            q.participates = i < 6 || phrase.uni() < 0.88f;
            q.jitter = std::clamp (phrase.gauss() * 0.45f, -1.0f, 1.0f);
            q.speedN = std::clamp (phrase.gauss() * 0.6f, -1.0f, 1.0f);
            q.phraseDb = phrase.bi();
            q.acc = 0.0f; q.speechEnv = 0.0f;
        }
        peopleDirty = false;
        snap = true;
    }

    void setupFilters (Voice& q, int index, const SizeCfg& c, int size)
    {
        const bool front = index < c.front;
        const float depth = front ? q.depthU * 0.25f : 0.3f + 0.7f * q.depthU;
        const float lpHz = lerp (11000.0f, c.backDark, depth) * q.lpJit;
        const bool firstFilter = q.filtSize < 0;
        // Eine laufende Ueberblendung erst abschliessen; danach folgt der neueste Zielwert.
        q.oldLp = q.lp;
        q.lp.lowpass (lpHz, 0.6f, fs);
        q.filterMix = firstFilter ? 1.0f : 0.0f;
        q.pk.peaking (q.peakHz, 0.9f, q.peakDb, fs);
        q.hpA = 1.0f - std::exp (-2.0f * kPi * q.hpHz / fs);
        q.filtSize = size;
    }

    float readTap (uint32_t t, float d) const
    {
        const int di = (int) d;
        const float f = d - (float) di;
        const uint32_t i0 = t - (uint32_t) di;
        const float ym1 = ring[(i0 + 1u) & mask], y0 = ring[i0 & mask], y1 = ring[(i0 - 1u) & mask], y2 = ring[(i0 - 2u) & mask];
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * f + c2) * f + c1) * f + y0;
    }

    void renderChunk (const Params& p)
    {
        const auto& c = sizeCfg (p.size);
        const float spread = c.spread * (0.1f + 0.9f * p.chaos);
        const float accMax = kAccMaxSec * p.chaos * fs;
        // Neu wuerfeln nur, wenn nichts mehr nachklingt: der spaeteste Nachzuegler braucht Vorlauf + Streuung + Tempo-Drift + Fenster
        const int hold = latency + (int) ((spread + kAccMaxSec * p.chaos + kWindowSec + 0.01f) * fs);
        if (silentRun >= hold)
        {
            const bool manual = manualReroll.exchange (false, std::memory_order_acq_rel);
            const uint32_t requested = pendingSeed.exchange (0, std::memory_order_acq_rel);
            if (requested != 0 || manual)
            {
                baseSeed = requested != 0 ? requested : Rng (baseSeed).next();
                peopleDirty = true; phraseIndex = 0;
                rng = Rng (baseSeed);
            }
            if (! rerollDone || manual || requested != 0) { reroll(); rerollDone = true; }
        }

        const float semisScale = 0.8f + 5.2f * p.variety;
        const float amountGain = p.amount * p.amount * 2.0f * kCrowdCal;
        const uint32_t t0 = wpos;

        float tD[kMaxVoices], tR[kMaxVoices], tG[kMaxVoices];
        bool active[kMaxVoices];
        float power = 0.0f;
        for (int i = 0; i < kMaxVoices; ++i)
        {
            auto& q = v[(size_t) i];
            const bool front = i < c.front;
            active[i] = i < c.voices && q.participates;
            if (q.filtSize != p.size && q.filterMix >= 1.0f) setupFilters (q, i, c, p.size);

            // Eigenes Sprechtempo: die Stimme laeuft langsam davon (oder hinterher), bis zur Sperre kAccMaxSec
            if (q.speechEnv >= kSilenceLevel)
                q.acc = std::clamp (q.acc + (-q.speedN * kSpeedMax * p.chaos) * (float) kChunk, -accMax, accMax);
            else q.acc = std::clamp (q.acc, -accMax, accMax);
            q.driftD += -driftDecay * q.driftD + driftNoise * rng.bi();
            q.driftG += -driftDecay * q.driftG + driftNoise * rng.bi();

            const float depth = front ? q.depthU * 0.25f : 0.3f + 0.7f * q.depthU;
            const float jitter = q.jitter * spread * (front ? 0.5f : 1.0f);
            tD[i] = std::max ((float) kMinDelay, (float) latency + (jitter + q.driftD * 0.004f) * fs + q.acc - 0.5f * window);
            const float semis = q.semis * semisScale * (front ? 0.5f : 1.0f);
            tR[i] = std::fabs (semis) < 0.001f ? 1.0f : std::pow (2.0f, semis / 12.0f);
            const float db = q.levelDb + q.phraseDb + q.driftG * 1.5f + (front ? 0.0f : -4.0f - 8.0f * depth);
            tG[i] = active[i] ? dbToLin (db) : 0.0f;
            power += tG[i] * tG[i];
        }
        const float normTarget = 1.0f / std::sqrt (std::max (power, 1e-6f));
        if (snap) norm = normTarget;
        norm += aGain * (normTarget - norm);

        for (int k = 0; k < kChunk; ++k) stageL[k] = stageR[k] = 0.0f;

        for (int i = 0; i < kMaxVoices; ++i)
        {
            auto& q = v[(size_t) i];
            const bool front = i < c.front;
            const float amp = tG[i] * norm * amountGain;
            const float angle = (q.pan * c.width * (front ? 0.7f : 1.0f) + 1.0f) * (kPi * 0.25f);
            const float tL = amp > 0.0f ? amp * std::cos (angle) : 0.0f;
            const float tRr = amp > 0.0f ? amp * std::sin (angle) : 0.0f;

            if (snap) { q.delay = tD[i]; q.ratio = tR[i]; q.gL = tL; q.gR = tRr; }
            // Eine nicht beteiligte Stimme, die schon ganz leise ist, braucht nicht gerechnet zu werden
            if (tL == 0.0f && tRr == 0.0f && q.gL < 1e-7f && q.gR < 1e-7f)
            {
                q.gL = q.gR = 0.0f; q.vis *= 0.995f; q.speechEnv = 0.0f;
                if (q.epoch.voicedMix() > 0) q.epoch.reset();
                q.delay = tD[i]; q.ratio = tR[i];   // unhoerbar: gleich auf den Zielwert, damit beim Zuschalten nichts gleitet
                continue;
            }

            const float D0 = q.delay, G0L = q.gL, G0R = q.gR;
            q.delay += aDelay * (tD[i] - q.delay);
            q.ratio += aRatio * (tR[i] - q.ratio);
            q.gL += aGain * (tL - q.gL);
            q.gR += aGain * (tRr - q.gR);
            const float dD = (q.delay - D0) * (1.0f / (float) kChunk);
            const float dL = (q.gL - G0L) * (1.0f / (float) kChunk), dR = (q.gR - G0R) * (1.0f / (float) kChunk);

            VoiceAnalysis::Frame voiceFrame;
            if (front || q.epoch.voicedMix() > 0)
            {
                const float formantTarget = std::pow (2.0f, q.formantN * 2.0f * p.variety / 12.0f);
                q.formant += aRatio * (formantTarget - q.formant);
                voiceFrame = analysis.at (t0 - (uint32_t) std::lround (D0 + 0.5f * window));
            }
            const bool single = tR[i] == 1.0f && std::fabs (q.ratio - 1.0f) < 0.003f;
            const float inc = single ? 0.0f : (1.0f - q.ratio) / window;
            float peak = 0.0f;
            for (int k = 0; k < kChunk; ++k)
            {
                float ph = q.phase;
                if (single)
                {
                    const float diff = 0.5f - ph;
                    if (std::fabs (diff) <= settleRate) ph = 0.5f; else ph += diff > 0 ? settleRate : -settleRate;
                }
                else
                {
                    ph += inc;
                    if (ph >= 1.0f) ph -= 1.0f; else if (ph < 0.0f) ph += 1.0f;
                }
                q.phase = ph;
                const float D = D0 + dD * (float) k;
                const uint32_t t = t0 + (uint32_t) k;
                const float tri = 1.0f - std::fabs (2.0f * ph - 1.0f), gA = tri * tri * (3.0f - 2.0f * tri), gB = 1.0f - gA;   // weiche Ueberblendung, beide Haelften ergeben immer 1
                float x = gA * readTap (t, D + ph * window);
                if (gB > 1e-4f)
                {
                    float pb = ph + 0.5f;
                    if (pb >= 1.0f) pb -= 1.0f;
                    x += gB * readTap (t, D + pb * window);
                }
                if (front || q.epoch.voicedMix() > 0)
                {
                    const float meanDelay = D + 0.5f * window;
                    x = q.epoch.process (x, t, meanDelay, q.ratio, q.formant, voiceFrame, front, analysis,
                        [this] (uint32_t at, float delay) { return readTap (at, delay); });
                }
                q.speechEnv = std::max (std::fabs (x), q.speechEnv * envRelease);
                q.hpState += q.hpA * (x - q.hpState);
                const float high = x - q.hpState;
                float filtered = q.lp.process (high);
                if (q.filterMix < 1.0f)
                {
                    filtered = lerp (q.oldLp.process (high), filtered, q.filterMix);
                    q.filterMix = std::min (1.0f, q.filterMix + filterStep);
                }
                x = q.pk.process (filtered);
                peak = std::max (peak, std::fabs (x));
                stageL[k] += x * (G0L + dL * (float) k);
                stageR[k] += x * (G0R + dR * (float) k);
            }
            q.vis = std::max (q.vis * 0.995f, peak * (q.gL + q.gR));   // Huellkurve mit ca. 70 ms Abklingzeit
        }
        const bool publishNow = snap || ++visCounter >= 8;
        if (publishNow) visCounter = 0;
        snap = false;
        {
            // CHOR aus: die Stimmen werden weich ausgeblendet, das Gemurmel kommt danach dazu
            const float target = p.choirEnabled ? 1.0f : 0.0f;
            const float g0 = vgCur;
            vgCur += (1.0f - std::exp (-(float) kChunk / (0.015f * fs))) * (target - vgCur);
            if (std::fabs (vgCur - target) < 1e-4f) vgCur = target;
            if (g0 != 1.0f || vgCur != 1.0f)
                for (int k = 0; k < kChunk; ++k)
                {
                    const float g = g0 + (vgCur - g0) * (float) (k + 1) / (float) kChunk;
                    stageL[k] *= g; stageR[k] *= g;
                }
        }
        renderMurmur (p);

        // Anzeige: Helligkeit relativ zur lautesten Stimme der letzten Sekunden (unabhaengig von MENGE und Eingangspegel)
        float loudest = 0.0f;
        for (const auto& q : v) loudest = std::max (loudest, q.vis);
        visRef = std::max (visRef * 0.99995f, loudest);
        if (publishNow)
            for (int i = 0; i < kMaxVoices; ++i) publish (i, c, i < c.front, active[i]);
    }

    void publish (int i, const SizeCfg& c, bool front, bool on)
    {
        const auto& q = v[(size_t) i];
        visPan[i].store (q.pan * c.width * (front ? 0.7f : 1.0f), std::memory_order_relaxed);
        visDepth[i].store (front ? q.depthU * 0.25f : 0.3f + 0.7f * q.depthU, std::memory_order_relaxed);
        visOn[i].store (on && i < c.voices ? 1.0f : 0.0f, std::memory_order_relaxed);
        visLevel[i].store (std::min (1.0f, q.vis / (visRef + 1e-6f)), std::memory_order_relaxed);
    }

    //==========================================================================
    // Gemurmel
    struct MHead { int take = 0; double pos = 0.0, start = 0.0, end = 0.0; };
    struct TakeOrder { std::array<int, 64> order {}; int count = 0, next = 0; };
    struct Visit { int take = -1; double start = 0; };
    struct VisitHistory { std::array<Visit, 16> recent {}; int next = 0, count = 0; };
    struct MurmurGap
    {
        Rng random { 1 };
        int remaining = 0, total = 1;
        bool active = false;
        void prepare (float fs, uint32_t seed)
        {
            random = Rng (seed); active = false;
            remaining = total = std::max (1, (int) std::lround (fs * (0.15f + 0.55f * random.uni())));
        }
        float tick (float fs)
        {
            if (remaining <= 0)
            {
                active = ! active;
                const float seconds = active ? 0.9f + 1.5f * random.uni() : 1.8f + 3.0f * random.uni();
                remaining = total = std::max (1, (int) std::lround (seconds * fs));
            }
            const float phase = 1.0f - (float) remaining / (float) total;
            --remaining;
            if (! active) return 0;
            const float a = std::clamp (phase / 0.22f, 0.0f, 1.0f);
            const float r = std::clamp ((1.0f - phase) / 0.35f, 0.0f, 1.0f);
            return a * a * (3 - 2 * a) * r * r * (3 - 2 * r);
        }
    };
    struct MPlayer { MHead a, b; bool fading = false; float t = 0.0f; };

    float readTake (const std::vector<int16_t>& y, double pos) const
    {
        const int n = (int) y.size();
        const int i = (int) pos;
        const float f = (float) (pos - (double) i);
        auto at = [&] (int k) { return (float) y[(size_t) std::clamp (k, 0, n - 1)] * (1.0f / 32768.0f); };
        const float ym1 = at (i - 1), y0 = at (i), y1 = at (i + 1), y2 = at (i + 2);
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * f + c2) * f + c1) * f + y0;
    }

    int nextTake (int lv, int side, int avoid)
    {
        const int n = (int) bank->level[lv].size();
        if (n > 64)
        {
            if (avoid < 0) return (int) (mrng.uni() * (float) n) % n;
            const int r = (int) (mrng.uni() * (float) (n - 1)) % (n - 1);
            return r >= avoid ? r + 1 : r;
        }
        auto& order = takeOrder[lv][side];
        if (order.count != n || order.next >= n)
        {
            order.count = n; order.next = 0;
            for (int i = 0; i < n; ++i) order.order[(size_t) i] = i;
            for (int i = n - 1; i > 0; --i)
                std::swap (order.order[(size_t) i], order.order[(size_t) ((int) (mrng.uni() * (float) (i + 1)))]);
            if (n > 1 && order.order[0] == avoid)
                std::swap (order.order[0], order.order[(size_t) (1 + (int) (mrng.uni() * (float) (n - 1)))]);
        }
        return order.order[(size_t) order.next++];
    }

    MHead newHead (int lv, int side, int avoid)
    {
        MHead head; head.take = nextTake (lv, side, avoid);
        const auto& take = bank->level[lv][(size_t) head.take];
        const double length = std::max (1.0, std::min ((double) take.size() - 2.0 * mStep,
            mXf * (2.2 + 2.3 * mrng.uni())));
        const double room = std::max (0.0, (double) take.size() - length - 2.0 * mStep);
        auto& history = visits[lv];
        const auto& other = mp[lv][1 - side];
        const auto& current = mp[lv][side];
        double bestScore = -1, chosen = 0;
        for (int candidate = 0; candidate < 12; ++candidate)
        {
            const double start = mrng.uni() * room;
            double score = room;
            for (int i = 0; i < history.count; ++i)
                if (history.recent[(size_t) i].take == head.take)
                    score = std::min (score, std::abs (start - history.recent[(size_t) i].start));
            auto avoidHead = [&] (const MHead& h)
            {
                if (h.end > 0 && h.take == head.take) score = std::min (score, 0.6 * std::abs (start - h.pos));
            };
            avoidHead (other.a); if (other.fading) avoidHead (other.b); avoidHead (current.a);
            if (score > bestScore) { bestScore = score; chosen = start; }
        }
        head.pos = head.start = chosen; head.end = chosen + length;
        history.recent[(size_t) history.next] = { head.take, chosen };
        history.next = (history.next + 1) % (int) history.recent.size();
        history.count = std::min ((int) history.recent.size(), history.count + 1);
        return head;
    }

    void initMurmur()
    {
        mrng = Rng (baseSeed * 2654435761u + 12345u);
        mInit = false;
        for (auto& w : mW) w = 0.0f;
        mG = 0.0f; mDensity = 1.0f;
        for (int lv = 0; lv < kNumMurmurLevels; ++lv)
        {
            visits[lv] = VisitHistory();
            for (int side = 0; side < 2; ++side) { mp[lv][side] = MPlayer(); takeOrder[lv][side] = TakeOrder(); }
        }
        for (int side = 0; side < 2; ++side)
            gaps[side].prepare (fs, baseSeed ^ (0x85ebca6bu * (uint32_t) (side + 1)));
        if (! bank) return;
        mStep = bank->rate / (double) fs;
        size_t minLen = ~(size_t) 0;
        for (const auto& l : bank->level) for (const auto& t : l) minLen = std::min (minLen, t.size());
        mXf = std::min (kMurmurXfadeSec * bank->rate, (double) minLen * 0.25);
        mXfInc = (float) (mStep / mXf);
        for (int lv = 0; lv < kNumMurmurLevels; ++lv)
            for (int s = 0; s < 2; ++s)
            {
                auto& pl = mp[lv][s];
                pl.a = newHead (lv, s, -1);
            }
    }

    float tickPlayer (int lv, int side)
    {
        auto& pl = mp[lv][side];
        const auto& takes = bank->level[lv];
        const auto& ta = takes[(size_t) pl.a.take];
        const float ya = readTake (ta, pl.a.pos);
        pl.a.pos += mStep;
        if (pl.fading)
        {
            const float yb = readTake (takes[(size_t) pl.b.take], pl.b.pos);
            pl.b.pos += mStep;
            pl.t += mXfInc;
            if (pl.t >= 1.0f) { pl.a = pl.b; pl.fading = false; return yb; }
            const float sm = pl.t * pl.t * (3.0f - 2.0f * pl.t);
            return ya * std::sqrt (1.0f - sm) + yb * std::sqrt (sm);   // unabhaengige Aufnahmen: gleiche Leistung
        }
        if (pl.a.pos > pl.a.end - mXf)
        {
            pl.b = newHead (lv, side, pl.a.take);
            pl.t = 0.0f;
            pl.fading = true;
        }
        return ya;
    }

    // Rechnet kChunk Samples Gemurmel und legt sie auf stageL/stageR (dann geht es mit durch den Raum)
    void renderMurmur (const Params& p)
    {
        if (! bank) return;
        const float x4 = p.wallaInt * (float) (kNumMurmurLevels - 1);
        const int lo = std::min ((int) x4, kNumMurmurLevels - 2);
        const float fr = x4 - (float) lo;
        float wT[kNumMurmurLevels] = {};
        wT[lo] = std::cos (fr * kPi * 0.5f);
        wT[lo + 1] = std::sin (fr * kPi * 0.5f);
        const float tilt = dbToLin (3.0f * p.wallaInt);   // je aufgebrachter, desto etwas lauter
        const float gT = p.wallaEnabled ? p.walla * p.walla * kMurmurCal * tilt : 0.0f;
        const float densityX = std::clamp (p.wallaInt * 4.0f, 0.0f, 1.0f);
        const float densityTarget = densityX * densityX * (3.0f - 2.0f * densityX);
        if (! mInit)
        {
            for (int i = 0; i < kNumMurmurLevels; ++i) mW[i] = wT[i];
            mG = 0; mDensity = densityTarget; mInit = true;
        }
        const float density0 = mDensity;
        mDensity += aMix * (densityTarget - mDensity);
        if (std::abs (mDensity - densityTarget) < 1e-6f) mDensity = densityTarget;
        const float densityStep = (mDensity - density0) / (float) kChunk;

        float w0[kNumMurmurLevels], dw[kNumMurmurLevels];
        for (int i = 0; i < kNumMurmurLevels; ++i)
        {
            w0[i] = mW[i];
            mW[i] += aMix * (wT[i] - mW[i]);
        }
        float weightPower = 0.0f;
        for (float w : mW) weightPower += w * w;
        const float weightNorm = 1.0f / std::sqrt (std::max (weightPower, 1e-12f));
        for (int i = 0; i < kNumMurmurLevels; ++i)
        {
            mW[i] *= weightNorm;
            dw[i] = (mW[i] - w0[i]) / (float) kChunk;
        }
        const float g0 = mG;
        mG += aGain * (gT - mG);
        const float dg = (mG - g0) * (1.0f / (float) kChunk);
        if (gT < 1e-6f && mG < 1e-6f) { mG = 0.0f; return; }
        int act[kNumMurmurLevels], na = 0;
        for (int i = 0; i < kNumMurmurLevels; ++i) if (w0[i] > 1e-5f || mW[i] > 1e-5f) act[na++] = i;
        for (int k = 0; k < kChunk; ++k)
        {
            const float g = g0 + dg * (float) k;
            float l = 0.0f, r = 0.0f;
            for (int j = 0; j < na; ++j)
            {
                const int lv = act[j];
                const float w = (w0[lv] + dw[lv] * (float) k) * g;
                l += w * tickPlayer (lv, 0);
                r += w * tickPlayer (lv, 1);
            }
            const float density = density0 + densityStep * (float) k;
            const float quietGain = 0.70710678f + 0.29289322f * density;
            // Bei 0 % einzelne weiche Gruppen mit Pausen, ab 25 % das volle Dauerbett.
            const float leftMotion = gaps[0].tick (fs), rightMotion = gaps[1].tick (fs);
            stageL[k] += l * quietGain * (density + (1 - density) * leftMotion);
            stageR[k] += r * quietGain * (density + (1 - density) * rightMotion);
        }
    }

    std::shared_ptr<const MurmurBank> bank;
    MPlayer mp[kNumMurmurLevels][2];
    TakeOrder takeOrder[kNumMurmurLevels][2];
    VisitHistory visits[kNumMurmurLevels];
    MurmurGap gaps[2];
    float mDensity = 1;
    Rng mrng { 7 };
    double mStep = 0.5, mXf = 72000.0;
    float mXfInc = 1e-5f, mW[kNumMurmurLevels] = {}, mG = 0.0f, aMix = 0.001f;
    bool mInit = false;

    float fs = 48000.0f, window = 2880.0f;
    int latency = 24000;
    std::vector<float> ring, ownSide;
    int ownPos = 0;
    uint32_t mask = 0, wpos = 0;
    Room rev;
    VoiceAnalysis analysis;
    std::array<Voice, kMaxVoices> v;
    Rng rng { 1 };
    uint32_t baseSeed = 1, phraseIndex = 0;
    std::atomic<uint32_t> pendingSeed { 0 };
    bool peopleDirty = true;

    float stageL[kChunk] = {}, stageR[kChunk] = {};
    int chunkPos = 0;
    float env = 0.0f, envRelease = 0.99f, settleRate = 1e-4f;
    int silentRun = 0, rerolls = 0, visCounter = 0;
    bool rerollDone = false, snap = true, ownInit = false;
    std::atomic<bool> manualReroll { false };
    float bypassCur = 0.0f;
    float choirCur = 1.0f, vgCur = 1.0f;
    bool choirOn = true, choirInit = false;
    float norm = 0.0f, ownCur = 0.0f, outCur = 0.0f, wetCur = 0.0f, visRef = 0.0f;
    float driftDecay = 0.0003f, driftNoise = 0.0127f, filterStep = 0.001f;
    float aGain = 0.01f, aDelay = 0.001f, aRatio = 0.003f;

    std::array<std::atomic<float>, kMaxVoices> visPan {}, visDepth {}, visOn {}, visLevel {};
};
}
