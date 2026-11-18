// render_demo.cpp - rendert Hoerbeispiele von TUMULT aus der echten Engine (Source/TumultDSP.h) als WAV.
// Aufruf: render_demo <Zielordner> [eingang.wav]
// Ohne Eingabedatei dient ein sprachaehnliches Testsignal (vier Silben); mit Datei (16/24/32 Bit oder Float, Mono oder
// Stereo, PCM) die eigene Aufnahme. Je Groesse entsteht eine Datei, dazu werden Pegel und Streuung gemessen.
#include "../Source/TumultDSP.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#if defined (__SSE__) || defined (_M_X64) || defined (_M_IX86_FP)
 #include <xmmintrin.h>
#endif

namespace
{
void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int fs)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) { std::perror (path.c_str()); return; }
    const uint32_t n = (uint32_t) l.size(), dataBytes = n * 2 * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (3); w16 (2); w32 ((uint32_t) fs); w32 ((uint32_t) fs * 8); w16 (8); w16 (32);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i) { std::fwrite (&l[i], 4, 1, f); std::fwrite (&r[i], 4, 1, f); }
    std::fclose (f);
}

// Minimaler WAV-Leser (PCM 16/24/32 oder Float 32), liefert Mono
bool readWav (const std::string& path, std::vector<float>& mono, int& fs)
{
    FILE* f = std::fopen (path.c_str(), "rb");
    if (! f) return false;
    char id[4]; uint32_t size;
    if (std::fread (id, 1, 4, f) != 4 || std::memcmp (id, "RIFF", 4)) { std::fclose (f); return false; }
    std::fread (&size, 4, 1, f); std::fread (id, 1, 4, f);
    uint16_t fmt = 0, ch = 1, bits = 16; uint32_t rate = 48000;
    while (std::fread (id, 1, 4, f) == 4 && std::fread (&size, 4, 1, f) == 1)
    {
        if (! std::memcmp (id, "fmt ", 4))
        {
            std::vector<char> b (size); std::fread (b.data(), 1, size, f);
            std::memcpy (&fmt, &b[0], 2); std::memcpy (&ch, &b[2], 2); std::memcpy (&rate, &b[4], 4); std::memcpy (&bits, &b[14], 2);
        }
        else if (! std::memcmp (id, "data", 4))
        {
            std::vector<unsigned char> b (size); size_t got = std::fread (b.data(), 1, size, f); b.resize (got);
            const int bytes = bits / 8; const size_t frames = got / (size_t) (bytes * ch);
            mono.assign (frames, 0.0f);
            for (size_t i = 0; i < frames; ++i)
            {
                double sum = 0;
                for (int c = 0; c < ch; ++c)
                {
                    const unsigned char* p = &b[(i * ch + (size_t) c) * (size_t) bytes];
                    double v = 0;
                    if (fmt == 3 && bits == 32) { float x; std::memcpy (&x, p, 4); v = x; }
                    else if (bits == 16) v = (int16_t) (p[0] | (p[1] << 8)) / 32768.0;
                    else if (bits == 24) { int32_t x = (p[0] << 8) | (p[1] << 16) | ((uint32_t) p[2] << 24); v = (x >> 8) / 8388608.0; }
                    else if (bits == 32) { int32_t x; std::memcpy (&x, p, 4); v = x / 2147483648.0; }
                    sum += v;
                }
                mono[i] = (float) (sum / ch);
            }
            fs = (int) rate; std::fclose (f); return true;
        }
        else std::fseek (f, (long) size + (size & 1), SEEK_CUR);
    }
    std::fclose (f); return false;
}

std::vector<float> testChant (int fs, int total)
{
    std::vector<float> x ((size_t) total, 0.0f);
    double ph = 0; const int syl = (int) (0.18 * fs), gap = (int) (0.07 * fs);
    for (int rep = 0; rep < 3; ++rep)
    {
        int pos = (int) ((0.2 + rep * 2.2) * fs);
        for (int s = 0; s < 4; ++s)
        {
            for (int i = 0; i < syl && pos + i < total; ++i)
            {
                const double t = (double) i / syl, e = std::sin (3.14159265358979 * t), f0 = 130.0 + 10.0 * s;
                ph += 2.0 * 3.14159265358979 * f0 / fs;
                double v = 0;
                for (int h = 1; h <= 20; ++h)
                {
                    const double fh = h * f0; double w = 1.0 / h;
                    if (fh > 600 && fh < 1200) w *= 3;
                    if (fh > 2200 && fh < 3200) w *= 2;
                    v += w * std::sin (h * ph);
                }
                x[(size_t) (pos + i)] = 0.3f * (float) (e * v * 0.25);
            }
            pos += syl + gap;
        }
    }
    return x;
}

double rmsOf (const std::vector<float>& x) { double s = 0; for (float v : x) s += (double) v * v; return std::sqrt (s / (double) x.size() + 1e-30); }
}

int main (int argc, char** argv)
{
    // Wie der Plugin-Callback: sehr kleine Hallreste duerfen Offline-Renderings nicht ausbremsen.
   #if defined (__SSE__) || defined (_M_X64) || defined (_M_IX86_FP)
    _mm_setcsr (_mm_getcsr() | 0x8040u);
   #endif
    if (argc < 2) { std::fprintf (stderr, "Usage: render_demo <Zielordner> [eingang.wav]\n"); return 1; }
    const std::string dir = argv[1];
    int fs = 48000;
    std::vector<float> in;
    if (argc > 2 && ! readWav (argv[2], in, fs)) { std::fprintf (stderr, "Eingabe nicht lesbar: %s\n", argv[2]); return 1; }
    if (in.empty()) in = testChant (fs, fs * 8);
    in.resize (in.size() + (size_t) fs * 5, 0.0f);   // Platz fuer den Nachhall
    const char* names[tumult::kNumSizes] = { "klein", "mittel", "gross", "stadion" };
    for (int size = 0; size < tumult::kNumSizes; ++size)
    {
        tumult::Engine e; e.prepare (fs, 7);
        tumult::Params p; p.size = size;
        auto l = in, r = in;
        e.process (l.data(), r.data(), (int) l.size(), p);
        const double inDb = 20.0 * std::log10 (rmsOf (in)), outDb = 20.0 * std::log10 (0.5 * (rmsOf (l) + rmsOf (r)));
        std::printf ("%-8s Eingang %.1f dB, Ausgang %.1f dB, neu gewuerfelt: %d\n", names[size], inDb, outDb, e.rerollCount());
        writeWav (dir + "/tumult-" + names[size] + ".wav", l, r, fs);
    }
    return 0;
}
