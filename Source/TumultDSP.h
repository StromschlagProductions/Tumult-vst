// TumultDSP.h - Klang-Engine fuer "Tumult" (Stromschlag Audio), Teil 1: der Stimmvervielfacher
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
// Die Persoenlichkeiten werden NUR in einer Sprechpause neu gewuerfelt (wenn nichts mehr nachklingt). Darum gibt es
// kein Knacken, und jede Wiederholung des Rufs klingt ein bisschen anders. Die Stimme laeuft mit einem festen Vorlauf
// von 200 ms (Latenz), damit die Menge auch VOR dem Einsatz der eigenen Stimme einsetzen kann.
//
// Block-Unabhaengigkeit: Alles wird in festen 16-Sample-Schritten gerechnet, der Zaehler gehoert zur Engine. Ergebnis
// ist bit-genau gleich, egal wie der Host den Strom in Bloecke teilt (bei gleichen Reglerwerten).
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tumult
{
constexpr float kPi = 3.14159265358979f;
constexpr int kChunk = 16;               // Steuerwerte und Stimmen-Schritt alle 16 Samples
constexpr int kMaxVoices = 64;
constexpr int kNumSizes = 4;             // Klein, Mittel, Gross, Stadion
constexpr float kLatencySec = 0.200f;    // fester Vorlauf: die Menge darf vor und nach der eigenen Stimme einsetzen
constexpr float kWindowSec = 0.060f;     // Fenster des Tonhoehen-Verschiebers
constexpr float kHoldSec = 0.430f;       // so lange muss Stille sein, bevor die Menge neu gewuerfelt wird
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
    float spread;             // groesster Einsatz-Versatz (s), bei SYNCHRON 0 %
    float width;              // Breite im Stereobild 0..1
    float backDark;           // Grenzfrequenz der entferntesten Stimmen (Hz)
    float rt60, lenScale, preSec, dampHz;   // Raum
    float echo1, g1, echo2, g2;             // einzelne Echos von den Raengen (Sekunden, Pegel)
};

inline const SizeCfg& sizeCfg (int i)
{
    static const SizeCfg table[kNumSizes] = {
        // voices front spread width backDark | rt60 len  pre    damp   echo1 g1   echo2 g2
        {  10,    2,   0.050f, 0.65f, 5500.0f,   0.45f, 0.6f, 0.003f, 6500.0f, 0.0f, 0.0f, 0.0f, 0.0f },   // Klein: Kneipe, Hinterhof
        {  24,    3,   0.085f, 0.85f, 4200.0f,   0.95f, 1.0f, 0.008f, 5200.0f, 0.0f, 0.0f, 0.0f, 0.0f },   // Mittel: Platz, Saal
        {  40,    4,   0.120f, 1.00f, 3400.0f,   1.80f, 1.6f, 0.015f, 4200.0f, 0.090f, 0.22f, 0.0f, 0.0f },   // Gross: Halle
        {  64,    6,   0.150f, 1.00f, 2800.0f,   3.40f, 2.6f, 0.028f, 3200.0f, 0.170f, 0.30f, 0.320f, 0.18f }   // Stadion
    };
    return table[std::clamp (i, 0, kNumSizes - 1)];
}

struct Params
{
    int size = 1;            // 0 Klein, 1 Mittel, 2 Gross, 3 Stadion
    float amount = 0.7f;     // MENGE: Lautstaerke der Menge, 0..1
    float tight = 0.55f;     // SYNCHRON: 0 = ungeordnet, 1 = fast im Gleichschritt
    float variety = 0.6f;    // VIELFALT: 0 = klingen alle aehnlich, 1 = Maenner, Frauen, Jugendliche gemischt
    float room = 0.35f;      // RAUM: Anteil des Raums
    float ownDb = -3.0f;     // EIGENE STIMME in dB (-80 = aus)
    float outDb = 0.0f;      // OUTPUT in dB
};

inline Params sanitize (Params p)
{
    p.size = (int) std::lround (finiteClamp ((float) p.size, 0.0f, (float) (kNumSizes - 1), 1.0f));
    p.amount = finiteClamp (p.amount, 0.0f, 1.0f, 0.7f);
    p.tight = finiteClamp (p.tight, 0.0f, 1.0f, 0.55f);
    p.variety = finiteClamp (p.variety, 0.0f, 1.0f, 0.6f);
    p.room = finiteClamp (p.room, 0.0f, 1.0f, 0.35f);
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

//==============================================================================
class Engine
{
public:
    Engine() = default;
    Engine (const Engine&) = delete;
    Engine& operator= (const Engine&) = delete;

    void prepare (double sampleRate, uint32_t seed = 1)
    {
        fs = (float) validRate (sampleRate);
        latency = sampleCount (fs, kLatencySec);
        window = (float) sampleCount (fs, kWindowSec);
        hold = sampleCount (fs, kHoldSec);
        int n = 1;
        while (n < sampleCount (fs, 0.6f) + 64) n *= 2;
        ring.assign ((size_t) n, 0.0f);
        mask = (uint32_t) n - 1u;
        rev.prepare (fs);
        aGain = 1.0f - std::exp (-(float) kChunk / (0.04f * fs));
        aDelay = 1.0f - std::exp (-(float) kChunk / (0.25f * fs));
        aRatio = 1.0f - std::exp (-(float) kChunk / (0.12f * fs));
        envRelease = std::exp (-1.0f / (0.03f * fs));
        settleRate = 1.0f / (0.3f * fs);
        baseSeed = seed ? seed : 1u;
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        wpos = 0;
        rev.reset();
        rng = Rng (baseSeed);
        for (auto& q : v) { q = Voice(); }
        chunkPos = 0;
        std::fill (std::begin (stageL), std::end (stageL), 0.0f);
        std::fill (std::begin (stageR), std::end (stageR), 0.0f);
        env = 0.0f; silentRun = hold; rerollDone = false; snap = true; norm = 0.0f;
        ownCur = outCur = wetCur = 0.0f; ownInit = false;
        rerolls = 0;
        for (int i = 0; i < kMaxVoices; ++i) { visLevel[i].store (0.0f); visOn[i].store (0.0f); }
    }

    int latencySamples() const { return latency; }
    // Wuerfelt die Menge in der naechsten Sprechpause neu (Knopf "Neue Leute")
    void requestReroll() { manualReroll.store (true, std::memory_order_release); }
    int rerollCount() const { return rerolls; }

    // Fuer die Anzeige (Oberflaeche): Lage und Aktivitaet jeder Stimme
    float voicePan (int i) const { return visPan[i].load (std::memory_order_relaxed); }
    float voiceDepth (int i) const { return visDepth[i].load (std::memory_order_relaxed); }
    float voiceOn (int i) const { return visOn[i].load (std::memory_order_relaxed); }
    float voiceLevel (int i) const { return visLevel[i].load (std::memory_order_relaxed); }

    // Eingang (links/rechts, bei Mono-Spuren beide gleich) wird durch den Ausgang ersetzt
    void process (float* l, float* r, int n, const Params& pIn)
    {
        const Params p = sanitize (pIn);
        const float ownTarget = dbToLin (p.ownDb), outTarget = dbToLin (p.outDb);
        const float wetTarget = p.room * p.room * kRoomGain;
        if (! ownInit) { ownCur = ownTarget; outCur = outTarget; wetCur = wetTarget; ownInit = true; }
        const float aSmooth = 1.0f - std::exp (-1.0f / (0.015f * fs));
        rev.setSize (p.size);

        for (int i = 0; i < n; ++i)
        {
            if (chunkPos == 0) renderChunk (p);

            float in = 0.5f * (l[i] + r[i]);
            if (! std::isfinite (in)) in = 0.0f;
            const float mag = std::fabs (in);
            env = std::max (mag, env * envRelease);
            if (env < kSilenceLevel) { if (silentRun < (1 << 30)) ++silentRun; }
            else { silentRun = 0; rerollDone = false; }

            const float dry = ring[(wpos - (uint32_t) latency) & mask];
            ring[wpos & mask] = in;
            ++wpos;

            ownCur += aSmooth * (ownTarget - ownCur);
            outCur += aSmooth * (outTarget - outCur);
            wetCur += aSmooth * (wetTarget - wetCur);
            const float sl = stageL[chunkPos] + dry * ownCur;
            const float sr = stageR[chunkPos] + dry * ownCur;
            float wl, wr;
            rev.process (0.5f * (sl + sr), wl, wr);
            l[i] = softClip ((sl + wetCur * wl) * outCur);
            r[i] = softClip ((sr + wetCur * wr) * outCur);
            if (++chunkPos == kChunk) chunkPos = 0;
        }
    }

private:
    static constexpr float kRoomGain = 3.0f;

    struct Voice
    {
        // Persoenlichkeit (wird nur in der Sprechpause neu gewuerfelt)
        bool participates = true;
        float jitter = 0, semis = 0, pan = 0, depthU = 0.5f, levelDb = 0, hpHz = 200, peakHz = 1500, peakDb = 0, lpJit = 1;
        // Laufzeit
        float delay = 0, ratio = 1, gL = 0, gR = 0, phase = 0.5f, driftD = 0, driftG = 0, hpState = 0, hpA = 0.02f, vis = 0;
        int filtSize = -1;
        Biquad lp, pk;
    };

    void reroll()
    {
        ++rerolls;
        for (int i = 0; i < kMaxVoices; ++i)
        {
            auto& q = v[(size_t) i];
            q.participates = i < 6 ? true : rng.uni() < 0.88f;
            q.jitter = std::clamp (rng.gauss() * 0.45f, -1.0f, 1.0f);
            const float kind = rng.uni();   // Maenner, Frauen, Jugendliche, Sonstige
            const float mean = kind < 0.35f ? -0.5f : kind < 0.75f ? 0.5f : kind < 0.88f ? 1.1f : 0.0f;
            q.semis = std::clamp (mean + rng.gauss() * 0.25f, -1.0f, 1.0f);
            q.pan = rng.bi();
            q.depthU = rng.uni();
            q.levelDb = rng.gauss() * 2.0f;
            q.hpHz = logLerp (100.0f, 320.0f, rng.uni());
            q.peakHz = logLerp (700.0f, 3200.0f, rng.uni());
            q.peakDb = rng.bi() * 4.0f;
            q.lpJit = 0.85f + 0.3f * rng.uni();
            q.filtSize = -1;
        }
        snap = true;
    }

    void setupFilters (Voice& q, int index, const SizeCfg& c, int size)
    {
        const bool front = index < c.front;
        const float depth = front ? q.depthU * 0.25f : 0.3f + 0.7f * q.depthU;
        const float lpHz = lerp (11000.0f, c.backDark, depth) * q.lpJit;
        q.lp.lowpass (lpHz, 0.6f, fs);
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
        // Neu wuerfeln nur, wenn nichts mehr nachklingt
        if (silentRun >= hold)
        {
            const bool manual = manualReroll.exchange (false, std::memory_order_acq_rel);
            if (! rerollDone || manual) { reroll(); rerollDone = true; }
        }

        const auto& c = sizeCfg (p.size);
        const float spread = c.spread * (1.0f - 0.85f * p.tight);
        const float semisScale = 0.8f + 4.2f * p.variety;
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
            if (q.filtSize != p.size) setupFilters (q, i, c, p.size);

            q.driftD += -0.0003f * q.driftD + 0.0127f * rng.bi();
            q.driftG += -0.0003f * q.driftG + 0.0127f * rng.bi();

            const float depth = front ? q.depthU * 0.25f : 0.3f + 0.7f * q.depthU;
            const float jitter = q.jitter * spread * (front ? 0.5f : 1.0f);
            tD[i] = std::max ((float) kMinDelay, (float) latency + (jitter + q.driftD * 0.004f) * fs - 0.5f * window);
            const float semis = q.semis * semisScale * (front ? 0.5f : 1.0f);
            tR[i] = std::fabs (semis) < 0.5f ? 1.0f : std::pow (2.0f, semis / 12.0f);
            const float db = q.levelDb + q.driftG * 1.5f + (front ? 0.0f : -4.0f - 8.0f * depth);
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
            const float tL = amp * std::cos (angle), tRr = amp * std::sin (angle);

            if (snap) { q.delay = tD[i]; q.ratio = tR[i]; q.gL = tL; q.gR = tRr; }
            // Eine nicht beteiligte Stimme, die schon ganz leise ist, braucht nicht gerechnet zu werden
            if (tL == 0.0f && tRr == 0.0f && q.gL < 1e-7f && q.gR < 1e-7f)
            {
                q.gL = q.gR = 0.0f; q.vis *= 0.9f;
                q.delay = tD[i]; q.ratio = tR[i];   // unhoerbar: gleich auf den Zielwert, damit beim Zuschalten nichts gleitet
                publish (i, c, front, false);
                continue;
            }

            const float D0 = q.delay, G0L = q.gL, G0R = q.gR;
            q.delay += aDelay * (tD[i] - q.delay);
            q.ratio += aRatio * (tR[i] - q.ratio);
            q.gL += aGain * (tL - q.gL);
            q.gR += aGain * (tRr - q.gR);
            const float dD = (q.delay - D0) * (1.0f / (float) kChunk);
            const float dL = (q.gL - G0L) * (1.0f / (float) kChunk), dR = (q.gR - G0R) * (1.0f / (float) kChunk);

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
                q.hpState += q.hpA * (x - q.hpState);
                x = q.pk.process (q.lp.process (x - q.hpState));
                peak = std::max (peak, std::fabs (x));
                stageL[k] += x * (G0L + dL * (float) k);
                stageR[k] += x * (G0R + dR * (float) k);
            }
            q.vis = std::max (q.vis * 0.9f, peak * (q.gL + q.gR));
            publish (i, c, front, active[i]);
        }
        snap = false;
    }

    void publish (int i, const SizeCfg& c, bool front, bool on)
    {
        const auto& q = v[(size_t) i];
        visPan[i].store (q.pan * c.width * (front ? 0.7f : 1.0f), std::memory_order_relaxed);
        visDepth[i].store (front ? q.depthU * 0.25f : 0.3f + 0.7f * q.depthU, std::memory_order_relaxed);
        visOn[i].store (on && i < c.voices ? 1.0f : 0.0f, std::memory_order_relaxed);
        visLevel[i].store (std::min (1.0f, q.vis * 3.0f), std::memory_order_relaxed);
    }

    float fs = 48000.0f, window = 2880.0f;
    int latency = 9600, hold = 20640;
    std::vector<float> ring;
    uint32_t mask = 0, wpos = 0;
    Reverb rev;
    std::array<Voice, kMaxVoices> v;
    Rng rng { 1 };
    uint32_t baseSeed = 1;

    float stageL[kChunk] = {}, stageR[kChunk] = {};
    int chunkPos = 0;
    float env = 0.0f, envRelease = 0.99f, settleRate = 1e-4f;
    int silentRun = 0, rerolls = 0;
    bool rerollDone = false, snap = true, ownInit = false;
    std::atomic<bool> manualReroll { false };
    float norm = 0.0f, ownCur = 0.0f, outCur = 0.0f, wetCur = 0.0f;
    float aGain = 0.01f, aDelay = 0.001f, aRatio = 0.003f;

    std::array<std::atomic<float>, kMaxVoices> visPan {}, visDepth {}, visOn {}, visLevel {};
};
}
