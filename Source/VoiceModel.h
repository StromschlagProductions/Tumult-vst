#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace tumult
{
// Gemeinsame, kausale Periodizitaetsanalyse. Der gemeldete Zeitstempel liegt in
// der Fenstermitte; die bereits vorhandene Plugin-Latenz liefert den Vorlauf.
class VoiceAnalysis
{
public:
    struct Frame { uint32_t time = 0; float period = 0, confidence = 0; };
    void prepare (double sampleRate)
    {
        fs = std::isfinite (sampleRate) && sampleRate >= 8000 && sampleRate <= 384000 ? (float) sampleRate : 48000.0f;
        blendStep = 1.0f - std::exp (-1.0f / (0.025f * fs));
        stride = std::max (1, (int) std::lround (fs / 12000.0f));
        const float rate = fs / (float) stride;
        minLag = std::max (2, (int) (rate / 500.0f));
        maxLag = std::min (255, (int) std::ceil (rate / 70.0f));
        width = maxLag * 2;
        hop = std::max (1, (int) std::lround (fs * 0.005f));
        lpA = 1.0f - std::exp (-6.28318530718f * 2000.0f / fs);
        hpA = 1.0f - std::exp (-6.28318530718f * 65.0f / fs);
        for (int i = 0; i <= 1024; ++i)
            hann[(size_t) i] = 0.5f - 0.5f * std::cos (6.28318530718f * (float) i / 1024.0f);
        reset();
    }
    void reset()
    {
        samples.fill (0); frames.fill (Frame());
        decPos = decCount = stridePos = hopPos = framePos = frameCount = 0;
        low1 = low2 = high = previousPeriod = 0;
    }
    void push (float input, uint32_t time)
    {
        if (! std::isfinite (input)) input = 0;
        high += hpA * (input - high);
        low1 += lpA * (input - high - low1);
        low2 += lpA * (low1 - low2);
        if (++stridePos >= stride)
        {
            stridePos = 0; samples[(size_t) decPos] = low2;
            decPos = (decPos + 1) & 1023;
            decCount = std::min (1024, decCount + 1);
        }
        if (++hopPos < hop) return;
        hopPos = 0;
        Frame result;
        result.time = time - (uint32_t) ((width + maxLag) * stride / 2);
        if (decCount >= width + maxLag + 1)
        {
            float energy = 0;
            for (int i = 0; i < width + maxLag; ++i)
            {
                scratch[(size_t) i] = samples[(size_t) ((decPos - 1 - i) & 1023)];
                energy += scratch[(size_t) i] * scratch[(size_t) i];
            }
            if (energy > (float) (width + maxLag) * 1e-7f)
            {
                double running = 0;
                difference[0] = 1;
                for (int lag = 1; lag <= maxLag; ++lag)
                {
                    float sum = 0;
                    for (int i = 0; i < width; ++i)
                    {
                        const float d = scratch[(size_t) i] - scratch[(size_t) (i + lag)];
                        sum += d * d;
                    }
                    running += sum;
                    difference[(size_t) lag] = running > 1e-20 ? (float) (sum * lag / running) : 1.0f;
                }
                for (int lag = minLag; lag < maxLag; ++lag)
                    if (difference[(size_t) lag] < 0.15f
                        && difference[(size_t) lag] <= difference[(size_t) (lag - 1)]
                        && difference[(size_t) lag] < difference[(size_t) (lag + 1)])
                    {
                        const float l = difference[(size_t) (lag - 1)], m = difference[(size_t) lag], r = difference[(size_t) (lag + 1)];
                        const float denominator = l - 2 * m + r;
                        const float fine = std::abs (denominator) > 1e-8f ? std::clamp (0.5f * (l - r) / denominator, -0.5f, 0.5f) : 0;
                        result.period = ((float) lag + fine) * (float) stride;
                        result.confidence = std::clamp (1.0f - m, 0.0f, 1.0f);
                        if (previousPeriod > 0 && std::abs (result.period / previousPeriod - 1) > 0.08f)
                            result.confidence *= 0.2f;
                        previousPeriod = result.period;
                        break;
                    }
            }
        }
        frames[(size_t) framePos] = result;
        framePos = (framePos + 1) & 511;
        frameCount = std::min (512, frameCount + 1);
    }
    Frame at (uint32_t time) const
    {
        if (frameCount == 0) return {};
        const auto& latest = frames[(size_t) ((framePos - 1) & 511)];
        const int distance = (int32_t) (latest.time - time);
        const int back = (int) std::lround ((double) distance / hop);
        if (back < 0 || back >= frameCount) return {};
        const auto f = frames[(size_t) ((framePos - 1 - back) & 511)];
        return std::abs ((int32_t) (f.time - time)) <= 2 * hop ? f : Frame();
    }
    float window (float phase) const
    {
        const float x = std::clamp (phase, 0.0f, 1.0f) * 1024;
        const int i = std::min ((int) x, 1023);
        return hann[(size_t) i] + (hann[(size_t) (i + 1)] - hann[(size_t) i]) * (x - (float) i);
    }
    float sampleRate() const { return fs; }
    float crossfadeStep() const { return blendStep; }
private:
    std::array<float, 1024> samples {}, scratch {};
    std::array<float, 256> difference {};
    std::array<Frame, 512> frames {};
    std::array<float, 1025> hann {};
    int stride = 4, minLag = 24, maxLag = 172, width = 344, hop = 240;
    int decPos = 0, decCount = 0, stridePos = 0, hopPos = 0, framePos = 0, frameCount = 0;
    float blendStep = 0.001f;
    float fs = 48000, lpA = 0.2f, hpA = 0.01f, low1 = 0, low2 = 0, high = 0, previousPeriod = 0;
};

// Periodensynchrone Fenster: Abstand der Synthese-Epochen regelt die Tonhoehe,
// Abspielrate innerhalb eines Fensters regelt separat die spektrale Huelle.
// Keine Heap-Allokationen. Bei unsicherer Analyse weicher Rueckfall auf fallback.
class EpochVoice
{
public:
    void reset() { *this = EpochVoice(); }
    template <typename Read>
    float process (float fallback, uint32_t time, float meanDelay, float ratio, float formant,
                   VoiceAnalysis::Frame frame, bool enabled, const VoiceAnalysis& analysis, Read&& read)
    {
        ratio = std::isfinite (ratio) ? std::clamp (ratio, 0.7f, 1.45f) : 1.0f;
        formant = std::isfinite (formant) ? std::clamp (formant, 0.85f, 1.18f) : 1.0f;
        if (! std::isfinite (fallback)) fallback = 0;
        const bool voiced = enabled && std::isfinite (frame.confidence) && std::isfinite (frame.period)
            && std::isfinite (meanDelay) && frame.confidence > 0.85f && frame.period >= 12
            && frame.period <= analysis.sampleRate() / 60.0f
            && meanDelay > frame.period * 3.0f + 32 && meanDelay < analysis.sampleRate() * 1.3f;
        const float target = voiced ? 1.0f : 0.0f;
        const float smooth = analysis.crossfadeStep();
        blend += smooth * (target - blend);
        if (std::abs (blend - target) < 1e-5f) blend = target;
        if (! voiced) due = 0;
        else if (due <= 0)
        {
            Grain* free = nullptr;
            for (auto& grain : grains) if (! grain.active) { free = &grain; break; }
            if (free != nullptr)
            {
                const float radius = frame.period / formant;
                const float centerDelay = meanDelay - radius;
                uint32_t mark = time - (uint32_t) std::lround (centerDelay);
                bool fresh = ! haveMark || std::fabs ((double) (int32_t) (time - previousMark)) > analysis.sampleRate() * 0.2;
                if (! fresh)
                {
                    const float movement = (float) (int32_t) (time - previousMark) - centerDelay;
                    mark = previousMark + (uint32_t) (int32_t) std::lround (std::round (movement / frame.period) * frame.period);
                }
                const int search = std::max (2, (int) std::lround (frame.period * (fresh ? 0.55f : 0.18f)));
                float best = -1e30f; uint32_t bestMark = mark;
                for (int j = -search; j <= search; ++j)
                {
                    const uint32_t candidate = mark + (uint32_t) j;
                    const float y = read (time, (float) (int32_t) (time - candidate));
                    // Abstand nur als kleiner Gleichstandsbrecher, keine Pegelabhaengigkeit.
                    const float score = y * (1.0f - 0.02f * std::abs ((float) j) / (float) search);
                    if (score > best) { best = score; bestMark = candidate; }
                }
                previousMark = bestMark; haveMark = true;
                *free = { bestMark, 0, radius, formant, true };
            }
            due += frame.period / ratio;
        }
        if (voiced) due -= 1;
        float sum = 0, weights = 0;
        for (auto& grain : grains)
        {
            if (! grain.active) continue;
            const float phase = grain.age / (2.0f * grain.radius);
            if (phase >= 1) { grain.active = false; continue; }
            const float w = analysis.window (phase);
            const float offset = (grain.age - grain.radius) * grain.rate;
            const float delay = (float) (int32_t) (time - grain.mark) - offset;
            sum += w * read (time, delay); weights += w;
            grain.age += 1;
        }
        const float rebuilt = weights > 0.1f ? sum / weights : fallback;
        if (! voiced && blend == 0) haveMark = false;
        return fallback + blend * (rebuilt - fallback);
    }
    float voicedMix() const { return blend; }
private:
    struct Grain { uint32_t mark = 0; float age = 0, radius = 1, rate = 1; bool active = false; };
    std::array<Grain, 16> grains {};
    float due = 0, blend = 0;
    uint32_t previousMark = 0;
    bool haveMark = false;
};
}
