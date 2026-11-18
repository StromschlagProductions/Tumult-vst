#include "VoiceModel.h"
#include "TestSupport.h"
#include <limits>
#include <vector>

static std::vector<float> vowel (double fs)
{
    std::vector<float> x ((size_t) (fs * 3), 0);
    for (double p = 0; p + 1 < (double) x.size(); p += fs / 120.0)
    {
        const int i = (int) p; const float f = (float) (p - i);
        x[(size_t) i] += 1 - f; x[(size_t) (i + 1)] += f;
    }
    for (auto f : { std::array<double, 2>{700,90}, {1250,110}, {2600,160} })
    {
        const double r = std::exp (-3.14159265358979 * f[1] / fs);
        const double a = 2 * r * std::cos (6.28318530717959 * f[0] / fs);
        double y1 = 0, y2 = 0;
        for (auto& sample : x) { const double y = (1 - r) * sample + a * y1 - r * r * y2; y2 = y1; y1 = y; sample = (float) y; }
    }
    float peak = 0; for (float y : x) peak = std::max (peak, std::abs (y));
    for (float& y : x) y *= 0.3f / peak;
    return x;
}

static std::vector<float> render (double fs, const std::vector<float>& input, float ratio, float formant, float slope = 0, uint32_t start = 0)
{
    tumult::VoiceAnalysis analysis; analysis.prepare (fs);
    tumult::EpochVoice voice;
    std::vector<float> out (input.size());
    NoAllocations guard;
    for (size_t i = 0; i < input.size(); ++i)
    {
        const uint32_t t = start + (uint32_t) i;
        analysis.push (input[i], t);
        const float delay = (float) (0.5 * fs) + slope * (float) i;
        const auto frame = analysis.at (t - (uint32_t) std::lround (delay));
        auto read = [&] (uint32_t at, float d)
        {
            CHECK (d > 1.0f); // Analyse-/Synthesefenster duerfen nicht in zukuenftige Samples greifen.
            const double p = (double) (int32_t) (at - start) - d;
            const int k = (int) std::floor (p); const float f = (float) (p - k);
            auto get = [&] (int n) { return n >= 0 && n < (int) input.size() ? input[(size_t) n] : 0.0f; };
            return get (k) * (1 - f) + get (k + 1) * f;
        };
        const float raw = read (t, delay);
        out[i] = voice.process (raw, t, delay, ratio, formant, frame, true, analysis, read);
        CHECK (std::isfinite (out[i]) && std::abs (out[i]) < 1.0f);
    }
    CHECK (voice.voicedMix() > 0.95f);
    return out;
}

static double pitch (const std::vector<float>& x, double fs, double target)
{
    const int low = (int) (fs / target * 0.9), high = (int) (fs / target * 1.1);
    std::vector<double> corr ((size_t) high + 2);
    for (int lag = low - 1; lag <= high + 1; ++lag)
    {
        double lr = 0, ll = 0, rr = 0;
        for (int i = (int) fs; i < (int) (fs * 1.25); i += 2)
        {
            const double a = x[(size_t) i], b = x[(size_t) (i + lag)]; lr += a * b; ll += a * a; rr += b * b;
        }
        corr[(size_t) lag] = lr / std::sqrt (ll * rr + 1e-30);
    }
    int best = low; for (int lag = low + 1; lag <= high; ++lag) if (corr[(size_t) lag] > corr[(size_t) best]) best = lag;
    const double l = corr[(size_t) (best - 1)], m = corr[(size_t) best], r = corr[(size_t) (best + 1)];
    const double fine = std::abs (l - 2 * m + r) > 1e-12 ? 0.5 * (l - r) / (l - 2 * m + r) : 0;
    return fs / (best + fine);
}

static double powerAt (const std::vector<float>& x, double fs, double hz)
{
    const double c = std::cos (6.28318530717959 * hz / fs), s = std::sin (6.28318530717959 * hz / fs);
    double re = 0, im = 0, co = 1, si = 0;
    for (int i = (int) fs; i < (int) (2 * fs); ++i)
    {
        re += x[(size_t) i] * co; im += x[(size_t) i] * si;
        const double next = co * c - si * s; si = si * c + co * s; co = next;
    }
    return re * re + im * im;
}

int main()
{
    NoDenormals denormals;
    // Periodizitaet und Zeitstempel funktionieren auch ueber den uint32-Samplezaehler-Wrap.
    for (double fs : { 44100.0, 48000.0 })
    {
        const auto in = vowel (fs);
        const auto plain = render (fs, in, 1.2f, 1.0f);
        const auto higher = render (fs, in, 1.2f, 1.1f);
        const auto lower = render (fs, in, 1.2f, 0.9f);
        const auto slower = render (fs, in, 1.2f, 1.0f, 0.03f);
        const auto wrapped = render (fs, in, 1.2f, 1.0f, 0.0f, 0xffffff00u);
        for (auto* out : { &plain, &higher, &lower, &slower, &wrapped }) CHECK (std::abs (pitch (*out, fs, 144) - 144) < 1.5);
        // Formantlage aendert die spektrale Huelle, nicht den Epochenabstand/F0.
        const double highRatio = powerAt (higher, fs, 864) / powerAt (higher, fs, 720);
        const double plainHigh = powerAt (plain, fs, 864) / powerAt (plain, fs, 720);
        const double lowRatio = powerAt (lower, fs, 576) / powerAt (lower, fs, 720);
        const double plainLow = powerAt (plain, fs, 576) / powerAt (plain, fs, 720);
        CHECK (highRatio > 2 * plainHigh);
        CHECK (lowRatio > 2 * plainLow);
        CHECK (plain == wrapped);
    }
    // Unperiodisches Rauschen wird nicht als verlässlich stimmhaft erzwungen.
    {
        tumult::VoiceAnalysis analysis; analysis.prepare (48000);
        uint32_t random = 7; int voiced = 0;
        for (uint32_t i = 0; i < 96000; ++i)
        {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            const float noise = ((float) (random >> 8) / 16777216.0f - 0.5f) * 0.3f;
            analysis.push (noise, i);
            if (i > 24000 && analysis.at (i - 12000).confidence > 0.85f) ++voiced;
        }
        CHECK (voiced < 96000 / 20);
        analysis.push (std::numeric_limits<float>::quiet_NaN(), 96000);
        CHECK (std::isfinite (analysis.at (84000).confidence));
    }
    // Ohne periodische Analyse bleibt der explizite Rueckfallpfad unveraendert.
    {
        tumult::VoiceAnalysis analysis; analysis.prepare (48000);
        tumult::EpochVoice voice;
        for (uint32_t t = 0; t < 48000; ++t)
        {
            const float raw = 0.2f * std::sin (0.017f * (float) t);
            const float out = voice.process (raw, t, 24000, 1.2f, 0.9f, {}, true, analysis,
                [] (uint32_t, float) { return 0.0f; });
            CHECK (out == raw);
        }
    }
    return 0;
}
