// Regressionstests der TUMULT-Klang-Engine (ohne JUCE). Pruefen Verhalten, nicht Klang: das entscheidet das Ohr.
#include "TumultDSP.h"
#include "TestSupport.h"
#include <limits>
#include <vector>

using tumult::Params;

static double rms (const std::vector<float>& x, size_t a, size_t b)
{
    double s = 0;
    for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) (b - a) + 1e-30);
}
static double db (double ratio) { return 20.0 * std::log10 (ratio + 1e-30); }

// Ein sprachaehnliches Signal: vier Silben mit Obertoenen und zwei Formantbereichen (kein echter Text, aber ein gutes Testsignal)
static std::vector<float> chant (double fs, int total, float level = 0.3f, double startSec = 0.1)
{
    std::vector<float> x ((size_t) total, 0.0f);
    double ph = 0;
    const int syl = (int) (0.18 * fs), gap = (int) (0.07 * fs);
    int pos = (int) (startSec * fs);
    for (int s = 0; s < 4; ++s)
    {
        for (int i = 0; i < syl && pos + i < total; ++i)
        {
            const double t = (double) i / syl, e = std::sin (3.14159265358979 * t);
            const double f0 = 130.0 + 10.0 * s;
            ph += 2.0 * 3.14159265358979 * f0 / fs;
            double v = 0;
            for (int h = 1; h <= 20; ++h)
            {
                const double fh = h * f0;
                double w = 1.0 / h;
                if (fh > 600 && fh < 1200) w *= 3;
                if (fh > 2200 && fh < 3200) w *= 2;
                v += w * std::sin (h * ph);
            }
            x[(size_t) (pos + i)] = level * (float) (e * v * 0.25);
        }
        pos += syl + gap;
    }
    return x;
}

// Rendert in Bloecken der Groesse "block" (0 = alles auf einmal)
static void render (double fs, const Params& p, const std::vector<float>& in, int block, std::vector<float>& l, std::vector<float>& r, uint32_t seed = 7)
{
    tumult::Engine e; e.prepare (fs, seed);
    l = in; r = in;
    const int n = (int) in.size(), step = block > 0 ? block : n;
    for (int at = 0; at < n; at += step)
    {
        const int m = std::min (step, n - at);
        NoAllocations guard;
        e.process (l.data() + at, r.data() + at, m, p);
    }
}

int main()
{
    NoDenormals denormals;

    // 1) Alle Abtastraten und Groessen: endlich, begrenzt, und unabhaengig von der Blockaufteilung (bit-genau)
    for (double fs : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int size = 0; size < tumult::kNumSizes; ++size)
        {
            Params p; p.size = size; p.variety = 1.0f; p.tight = 0.2f; p.room = 0.6f; p.ownDb = 0.0f;
            const auto in = chant (fs, (int) (fs * 1.6) + 13);
            std::vector<float> a, b, c, dd;
            render (fs, p, in, 0, a, dd);
            render (fs, p, in, 137, b, dd);
            render (fs, p, in, 1, c, dd);
            CHECK (a == b);
            CHECK (a == c);
            double peak = 0;
            for (float x : a) { CHECK (std::isfinite (x)); peak = std::max (peak, (double) std::abs (x)); }
            CHECK (peak > 0.01 && peak <= 1.05);
        }

    // 2) Ungueltige Werte (NaN, unendlich, riesig) duerfen nichts kaputt machen
    {
        const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
        Params p;
        p.size = 999; p.amount = nan; p.tight = inf; p.variety = -inf; p.room = nan; p.ownDb = inf; p.outDb = nan;
        std::vector<float> l = chant (48000, 48000), r = l;
        tumult::Engine e; e.prepare (48000, 3);
        e.process (l.data(), r.data(), 48000, p);
        for (size_t i = 0; i < l.size(); ++i) CHECK (std::isfinite (l[i]) && std::isfinite (r[i]) && std::abs (l[i]) <= 1.05f);
        std::vector<float> xl (4800, nan), xr (4800, nan);   // auch ein ungueltiger Eingang darf nur ihn selbst treffen
        e.process (xl.data(), xr.data(), 4800, Params());
        std::vector<float> yl = chant (48000, 48000), yr = yl;
        e.process (yl.data(), yr.data(), 48000, Params());
        for (size_t i = 0; i < yl.size(); ++i) CHECK (std::isfinite (yl[i]) && std::isfinite (yr[i]));
        tumult::Engine e2; e2.prepare (std::numeric_limits<double>::quiet_NaN());   // ungueltige Abtastrate -> 48 kHz
        e2.process (l.data(), r.data(), 64, Params());
        CHECK (e2.latencySamples() == 9600);
    }

    // 3) Latenz: die eigene Stimme kommt exakt kLatencySec spaeter, sonst nichts, und ohne Menge, Raum und Eigenstimme herrscht Stille
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::vector<float> in ((size_t) fs, 0.0f);
        in[1000] = 0.5f;
        Params p; p.amount = 0.0f; p.room = 0.0f; p.ownDb = 0.0f; p.outDb = 0.0f;
        std::vector<float> l, r;
        render (fs, p, in, 256, l, r);
        const int latency = (int) std::lround (fs * tumult::kLatencySec);
        for (size_t i = 0; i < l.size(); ++i)
        {
            const float expected = (int) i == 1000 + latency ? 0.5f : 0.0f;
            CHECK (l[i] == expected && r[i] == expected);
        }
        p.ownDb = -80.0f;   // aus
        render (fs, p, chant (fs, (int) fs), 256, l, r);
        for (size_t i = 0; i < l.size(); ++i) CHECK (l[i] == 0.0f && r[i] == 0.0f);
    }

    // 4) Pegel: bei den Standardwerten ist die Menge (ohne Eigenstimme, ohne Raum) etwa so laut wie die Eingangsstimme
    for (int size = 0; size < tumult::kNumSizes; ++size)
    {
        Params p; p.size = size; p.ownDb = -80.0f; p.room = 0.0f;
        const auto in = chant (48000, 48000 * 4);
        std::vector<float> l, r;
        render (48000, p, in, 480, l, r);
        const double both = std::sqrt (0.5 * (rms (l, 0, l.size()) * rms (l, 0, l.size()) + rms (r, 0, r.size()) * rms (r, 0, r.size())));
        const double rel = db (both / rms (in, 0, in.size()));
        CHECK (rel > -3.0 && rel < 2.0);
        // MENGE regelt den Pegel (quadratisch, damit der untere Bereich fein einstellbar ist)
        Params quiet = p; quiet.amount = 0.35f;
        std::vector<float> ql, qr;
        render (48000, quiet, in, 480, ql, qr);
        CHECK (db (rms (ql, 0, ql.size()) / rms (l, 0, l.size())) < -9.0);
    }

    // 5) Zeitliche Streuung: eine kurze Rauschsalve wird in der grossen Menge breiter verschmiert als in der kleinen,
    //    und SYNCHRON zieht sie zusammen
    {
        tumult::Rng rg (9);
        const int n = 48000 * 3;
        std::vector<float> in ((size_t) n, 0.0f);
        for (int i = 14400; i < 14400 + 2400; ++i) in[(size_t) i] = 0.3f * rg.bi();
        auto span = [&] (int size, float tight)
        {
            Params p; p.size = size; p.tight = tight; p.ownDb = -80.0f; p.room = 0.0f; p.variety = 0.6f;
            std::vector<float> l, r;
            render (48000, p, in, 480, l, r);
            double tot = 0; for (float x : l) tot += (double) x * x;
            double c = 0; int a = -1, b = -1;
            for (int i = 0; i < n; ++i)
            {
                c += (double) l[(size_t) i] * l[(size_t) i];
                if (a < 0 && c > 0.1 * tot) a = i;
                if (b < 0 && c > 0.9 * tot) { b = i; break; }
            }
            return 1000.0 * (b - a) / 48000.0;
        };
        const double klein = span (0, 0.0f), stadion = span (3, 0.0f), stadionTight = span (3, 1.0f);
        CHECK (stadion > klein + 40.0);
        CHECK (stadionTight < stadion - 40.0);
    }

    // 6) Stereo und Mono-Kompatibilitaet: die Menge ist breit, die Mono-Summe loescht sich nicht aus
    for (int size = 0; size < tumult::kNumSizes; ++size)
    {
        Params p; p.size = size; p.ownDb = -80.0f; p.room = 0.0f;
        std::vector<float> l, r;
        render (48000, p, chant (48000, 48000 * 4), 480, l, r);
        double ll = 0, rr = 0, lr = 0, ss = 0;
        for (size_t i = 0; i < l.size(); ++i) { ll += (double) l[i] * l[i]; rr += (double) r[i] * r[i]; lr += (double) l[i] * r[i]; ss += (double) (l[i] + r[i]) * (l[i] + r[i]); }
        const double corr = lr / std::sqrt (ll * rr);
        const double sumDb = 10.0 * std::log10 (ss / (ll + rr));   // 0 dB = unabhaengig, +3 dB = identisch, -inf = Ausloeschung
        CHECK (corr > 0.0 && corr < 0.95);
        CHECK (sumDb > 1.0 && sumDb < 3.05);
    }

    // 7) Raum: Stadion klingt deutlich laenger nach als Klein
    {
        auto tail = [] (int size)
        {
            Params p; p.size = size; p.room = 0.5f; p.ownDb = 0.0f;
            auto in = chant (48000, 48000 * 9);   // Rufe in den ersten 1,3 s, danach Stille
            std::vector<float> l, r;
            render (48000, p, in, 480, l, r);
            return rms (l, (size_t) (48000 * 3.4), (size_t) (48000 * 3.9));
        };
        CHECK (db (tail (3) / tail (0)) > 20.0);
        CHECK (tail (3) > tail (1));
        CHECK (tail (1) > tail (0));
    }

    // 8) Neu wuerfeln nur in der Sprechpause: im Dauerton nie, nach einer Pause bei jedem neuen Ruf; jeder Ruf klingt anders
    {
        tumult::Rng rg (5);
        std::vector<float> noise (48000 * 6), nr;
        for (auto& x : noise) x = 0.1f * rg.bi();
        tumult::Engine e; e.prepare (48000, 5);
        nr = noise;
        e.process (noise.data(), nr.data(), (int) noise.size(), Params());
        CHECK (e.rerollCount() == 1);

        const int n = 48000 * 4;
        const auto phrase = chant (48000, 48000 * 12 / 10);
        std::vector<float> in ((size_t) n, 0.0f);
        for (size_t i = 0; i < phrase.size(); ++i) { in[i] = phrase[i]; in[i + 48000 * 2] = phrase[i]; }
        Params p; p.ownDb = -80.0f; p.room = 0.0f;
        std::vector<float> l, r;
        tumult::Engine e2; e2.prepare (48000, 5);
        l = in; r = in;
        e2.process (l.data(), r.data(), n, p);
        CHECK (e2.rerollCount() == 3);
        double d = 0, s = 0;
        for (size_t i = 0; i < phrase.size(); ++i) { const double x = (double) l[i] - l[i + 96000]; d += x * x; s += (double) l[i] * l[i]; }
        CHECK (d > 0.5 * s);   // klingt nicht gleich

        // "Neue Leute": der Wunsch wird in der naechsten Pause erfuellt
        e2.requestReroll();
        std::vector<float> quietL (48000, 0.0f), quietR (48000, 0.0f);
        e2.process (quietL.data(), quietR.data(), 48000, p);
        CHECK (e2.rerollCount() == 4);
    }

    // 9) Keine Knackser: ein reiner Ton bleibt ein Ton, auch wenn GROESSE, SYNCHRON und VIELFALT laufend automatisiert werden
    {
        const int n = 48000 * 6;
        std::vector<float> in ((size_t) n);
        for (int i = 0; i < n; ++i) in[(size_t) i] = 0.3f * (float) std::sin (2.0 * 3.14159265358979 * 200.0 * i / 48000.0);
        for (int automate = 0; automate < 2; ++automate)
        {
            tumult::Engine e; e.prepare (48000, 5);
            Params p; p.ownDb = -80.0f; p.room = 0.0f; p.variety = 1.0f;
            auto l = in, r = in;
            for (int at = 0; at < n; at += 480)
            {
                if (automate)
                {
                    const int k = at / 480;
                    p.size = (k / 40) % 4; p.tight = (float) (k % 200) / 200.0f; p.variety = (float) ((k + 50) % 300) / 300.0f;
                    p.amount = 0.4f + 0.5f * (float) ((k * 7) % 100) / 100.0f;
                }
                e.process (l.data() + at, r.data() + at, 480, p);
            }
            double hf = 0, tot = 0;
            for (size_t i = 48000 * 3 / 2; i < l.size(); ++i)
            {
                const double d2 = (double) l[i] - 2.0 * l[i - 1] + l[i - 2];
                hf += d2 * d2; tot += (double) l[i] * l[i];
            }
            CHECK (hf / tot < 5e-6);   // ein 200-Hz-Ton hat hier etwa 5e-7; Knackser wuerden ein Vielfaches ergeben
        }
    }

    // 10) Wilde Automation: beliebige Regler in jedem Block, die Ausgabe bleibt endlich und begrenzt
    {
        tumult::Rng rg (11);
        tumult::Engine e; e.prepare (44100, 2);
        auto in = chant (44100, 44100 * 10);
        auto l = in, r = in;
        for (int at = 0; at + 256 <= (int) l.size(); at += 256)
        {
            Params p;
            p.size = (int) (rg.uni() * 4.0f); p.amount = rg.uni(); p.tight = rg.uni(); p.variety = rg.uni(); p.room = rg.uni();
            p.ownDb = -80.0f + 92.0f * rg.uni(); p.outDb = -30.0f + 40.0f * rg.uni();
            NoAllocations guard;
            e.process (l.data() + at, r.data() + at, 256, p);
        }
        for (size_t i = 0; i < l.size(); ++i) CHECK (std::isfinite (l[i]) && std::isfinite (r[i]) && std::abs (l[i]) <= 1.05f && std::abs (r[i]) <= 1.05f);
    }

    // 11) Anzeige: Die Zahl der angezeigten Stimmen passt zur Groesse (einige Stimmen beteiligen sich nicht)
    {
        const int expected[4] = { 10, 24, 40, 64 };
        for (int size = 0; size < 4; ++size)
        {
            tumult::Engine e; e.prepare (48000, 21);
            Params p; p.size = size;
            auto l = chant (48000, 48000 / 2), r = l;
            e.process (l.data(), r.data(), (int) l.size(), p);
            int on = 0;
            for (int i = 0; i < tumult::kMaxVoices; ++i)
            {
                const float pan = e.voicePan (i), depth = e.voiceDepth (i);
                CHECK (std::isfinite (pan) && pan >= -1.0f && pan <= 1.0f);
                CHECK (std::isfinite (depth) && depth >= 0.0f && depth <= 1.0f);
                if (e.voiceOn (i) > 0.5f) ++on;
            }
            CHECK (on <= expected[size] && on >= (int) (0.7f * (float) expected[size]));
        }
    }

    return 0;
}
