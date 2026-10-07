// MFR-Tannhäuser - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Offline checks (exit 1 on regressions, for CI):
//   1. voicing, DIRTY: drop-D riffing against the measured balance of the reference's distorted guitars
//   2. voicing, CLEAN: chords against the measured balance of the reference's clean verse guitar
//      (--fit searches the tone controls of both channels)
//   3. CLEAN stays clean at its default volume; DIRTY really distorts
//   4. switching channels mid-note doesn't jump
//   5. the gate shuts the hiss off between riffs
//   6. DOUBLE spreads the guitar wide, and folds back to mono at 0
//   7. held chords: 8 s chords must decay smoothly on both channels
//   8. full chain at defaults: finite, the two channels at similar levels
// Writes tann_demo.wav (stereo): clean chords, then the riff on the dirty channel.
#include "../Source/TannDSP.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

using namespace tann;
constexpr double kFs = 48000.0, kOs = kFs * 4;
static bool gFailed = false;
static void check (bool ok) { if (! ok) gFailed = true; }

// Targets: 1/3-octave balance of the reference's guitars (side channel), re 1 kHz.
// Distorted: both choruses averaged (they agree within 0.6 dB). Clean: the verse, steady
// (harmonic) bins only, smoothed over 3 bands; below 250 Hz the bass leaks in, so it starts there.
static const double kBands[] = { 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000 };
constexpr int kNumBands = 21, k1k = 10;
static const double kDirty[] = { 7.0, 4.5, 3.8, 3.9, 0.4, -1.2, -1.8, -3.2, -1.6, 0.2, 0.0, -0.1, 0.5, 0.8, 3.1, 2.9, 1.5, -2.2, -3.9, -4.7, -7.2 };
static const double kClean[] = { 0, 0, 0, 0, 2.8, 0.5, -3.4, -1.7, -2.4, 0.2, 0.0, 2.3, 3.2, 5.7, 5.0, 3.7, -3.4, -6.9, -11.8, 0, 0 };
constexpr int kCleanFrom = 4, kCleanTo = 18;   // 250 Hz .. 6.3 kHz

struct Pluck
{
    std::vector<float> buf; size_t pos = 0; float damp;
    Pluck (double hz, float amp, uint32_t& rng, float damping, float pick = 0.6f) : buf ((size_t) std::lround (kOs / hz)), damp (damping)
    {
        for (auto& v : buf) { rng = rng * 1664525u + 1013904223u; v = amp * ((float) (rng >> 9) / 4194304.0f - 1.0f); }
        float lp = 0; for (auto& v : buf) { lp += pick * (v - lp); v = lp; }
    }
    float tick() { const size_t nx = (pos + 1) % buf.size(); const float o = buf[pos]; buf[pos] = damp * 0.5f * (buf[pos] + buf[nx]); pos = nx; return o; }
};

// Drop-D riffing: eighth-note chugs on the open D (palm-muted), then ringing D5 / F5 / G5 power chords, 150 BPM.
static std::vector<float> riff (double seconds, float amp)
{
    std::vector<float> y ((size_t) (seconds * kOs), 0.0f);
    const double step = 60.0 / 150.0 / 2.0;
    static const double roots[] = { 73.42, 73.42, 87.31, 98.0 };
    uint32_t rng = 5;
    std::vector<Pluck> ringing;
    size_t next = 0; int n = 0;
    for (size_t i = 0; i < y.size(); ++i)
    {
        if (i == next)
        {
            ringing.clear();
            const int bar = (n / 8) % 4;
            const bool chug = bar == 0 || (n % 8) < 4;
            const double r = roots[bar];
            const float damp = chug ? 0.995f : 0.9997f;
            for (double f : { r, r * 1.4983, r * 2.0 })
                ringing.emplace_back (f, amp, rng, damp, 0.7f);
            ++n; next = (size_t) (n * step * kOs);
        }
        float s = 0; for (auto& p : ringing) s += p.tick();
        y[i] = s;
    }
    return y;
}

// Clean chords picked through (D, G/D, A, Bm shapes up the neck), eighths at 120 BPM.
static std::vector<float> chords (double seconds, float amp)
{
    static const double ch[4][4] = { { 146.8, 220.0, 293.7, 370.0 }, { 146.8, 246.9, 293.7, 392.0 },
                                      { 110.0, 220.0, 277.2, 329.6 }, { 123.5, 185.0, 246.9, 293.7 } };
    std::vector<float> y ((size_t) (seconds * kOs), 0.0f);
    const double step = 60.0 / 120.0 / 2.0;
    uint32_t rng = 11;
    std::vector<Pluck> ringing;
    size_t next = 0; int n = 0;
    for (size_t i = 0; i < y.size(); ++i)
    {
        if (i == next)
        {
            ringing.emplace_back (ch[(n / 8) % 4][n % 4], amp, rng, 0.9994f, 0.6f);
            if (ringing.size() > 6) ringing.erase (ringing.begin());
            ++n; next = (size_t) (n * step * kOs);
        }
        float s = 0; for (auto& p : ringing) s += p.tick();
        y[i] = s;
    }
    return y;
}

static void run (const Params& p, const std::vector<float>& in, std::vector<float>& L, std::vector<float>& R)
{
    Chain c; c.prepare (kOs); c.setParams (p);
    L.resize (in.size()); R.resize (in.size());
    for (size_t i = 0; i < in.size(); ++i) c.process (in[i], L[i], R[i]);
}

static double rmsDb (const std::vector<float>& x, size_t from, size_t to)
{
    double e = 0; for (size_t i = from; i < to; ++i) e += (double) x[i] * x[i];
    return 10 * std::log10 (e / (double) (to - from) + 1e-30);
}

static void bandLevels (const std::vector<float>& x, double* out)
{
    const int N = 16384;
    std::vector<double> acc (kNumBands, 0.0);
    for (size_t s = (size_t) (0.5 * kOs); s + N <= x.size(); s += N / 2)
        for (int b = 0; b < kNumBands; ++b)
        {
            const double lo = kBands[b] / std::pow (2.0, 1.0 / 6), hi = kBands[b] * std::pow (2.0, 1.0 / 6);
            for (double f = lo; f < hi; f += (hi - lo) / 12)
            {
                const double w = 2 * kPi * f / kOs, c = 2 * std::cos (w);
                double s1 = 0, s2 = 0;
                for (int i = 0; i < N; ++i)
                {
                    const double win = 0.5 - 0.5 * std::cos (2 * kPi * i / N);
                    const double s0 = x[s + (size_t) i] * win + c * s1 - s2; s2 = s1; s1 = s0;
                }
                acc[(size_t) b] += s1 * s1 + s2 * s2 - c * s1 * s2;
            }
        }
    const double ref = 10 * std::log10 (acc[k1k] + 1e-30);
    for (int b = 0; b < kNumBands; ++b) out[b] = 10 * std::log10 (acc[(size_t) b] + 1e-30) - ref;
}

static double fitError (const double* lv, const double* target, int from, int to)
{
    double e = 0; for (int b = from; b <= to; ++b) { const double d = lv[b] - target[b]; e += d * d; }
    return std::sqrt (e / (to - from + 1));
}

// Voicing is measured dry and mono (no room, no spring, no doubling) on the left output.
static Params voicingParams (bool dirty)
{
    Params p; p.dirty = dirty; p.room = 0; p.spring = 0; p.width = 0; p.gateDb = -90;
    return p;
}

static double goertzel (const std::vector<float>& x, size_t from, size_t n, double hz)
{
    const double w = 2 * kPi * hz / kOs, c = 2 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2 * kPi * (double) i / (double) n);
        const double s0 = x[from + i] * win + c * s1 - s2; s2 = s1; s1 = s0;
    }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}

static void writeWavStereo (const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    const uint32_t bytes = (uint32_t) L.size() * 4;
    f.write ("RIFF", 4); u32 (36 + bytes); f.write ("WAVEfmt ", 8);
    u32 (16); u16 (1); u16 (2); u32 (48000); u32 (48000 * 4); u16 (4); u16 (16);
    f.write ("data", 4); u32 (bytes);
    for (size_t i = 0; i < L.size(); ++i)
        for (float s : { L[i], R[i] })
        { const auto v = (int16_t) std::lround (std::clamp (s, -1.0f, 1.0f) * 32767.0f); f.write ((const char*) &v, 2); }
}

static void printRow (const char* name, const double* v, int from, int to)
{
    std::printf ("  %-6s", name);
    for (int b = from; b <= to; ++b) std::printf ("%+6.1f", v[b]);
    std::printf ("\n");
}

int main (int argc, char** argv)
{
    const bool fit = argc > 1 && std::strcmp (argv[1], "--fit") == 0;
    const auto riffIn = riff (4.0, 0.12f);
    const auto chordIn = chords (4.0, 0.1f);

    // 1-2. Voicing.
    for (bool dirty : { true, false })
    {
        const auto& in = dirty ? riffIn : chordIn;
        const double* target = dirty ? kDirty : kClean;
        const int from = dirty ? 0 : kCleanFrom, to = dirty ? kNumBands - 1 : kCleanTo;
        std::vector<float> L, R; run (voicingParams (dirty), in, L, R);
        double lv[kNumBands]; bandLevels (L, lv);
        std::printf ("voicing, %s channel at defaults vs reference (dB re 1 kHz):\n  band  ", dirty ? "DIRTY" : "CLEAN");
        for (int b = from; b <= to; ++b) std::printf ("%6.0f", kBands[b]);
        std::printf ("\n");
        printRow ("ref", target, from, to);
        printRow ("ours", lv, from, to);
        const double e = fitError (lv, target, from, to);
        std::printf ("  RMS error: %.1f dB\n", e);
        if (! fit) check (e < (dirty ? 2.0 : 2.5));

        if (fit)
        {
            double best = 1e9; Params bp;
            const std::vector<float> shortIn (in.begin(), in.begin() + (long) (2.5 * kOs));
            for (float b = 2; b <= 8; b += 1.5f)
                for (float m = 2; m <= 8; m += 1.5f)
                    for (float t = 2; t <= 8; t += 1.5f)
                        for (float x : { 3.0f, 5.0f, 7.0f })   // presence (dirty) or volume (clean)
                        {
                            Params p = voicingParams (dirty);
                            if (dirty) { p.dBass = b; p.dMiddle = m; p.dTreble = t; p.presence = x; }
                            else       { p.cBass = b; p.cMid = m; p.cTreble = t; p.cVolume = x; }
                            std::vector<float> l2, r2; run (p, shortIn, l2, r2);
                            double v[kNumBands]; bandLevels (l2, v);
                            const double err = fitError (v, target, from, to);
                            if (err < best) { best = err; bp = p; }
                        }
            if (dirty) std::printf ("  best fit: bass %.1f middle %.1f treble %.1f presence %.0f -> %.1f dB\n", bp.dBass, bp.dMiddle, bp.dTreble, bp.presence, best);
            else       std::printf ("  best fit: bass %.1f mid %.1f treble %.1f volume %.0f -> %.1f dB\n", bp.cBass, bp.cMid, bp.cTreble, bp.cVolume, best);
        }
    }
    if (fit) return 0;

    // 3. Clean vs dirty distortion on a single note.
    {
        std::vector<float> in ((size_t) (1.5 * kOs));
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.1f * (float) std::sin (2 * kPi * 147.0 * (double) i / kOs);
        auto thd = [&] (bool dirty)
        {
            std::vector<float> L, R; run (voicingParams (dirty), in, L, R);
            const size_t n = (size_t) (0.5 * kOs), from = L.size() - n;
            const double fund = goertzel (L, from, n, 147.0);
            double harm = 0; for (int k = 2; k <= 10; ++k) harm += goertzel (L, from, n, 147.0 * k);
            return std::sqrt (harm / (fund + 1e-30));
        };
        const double tc = thd (false), td = thd (true);
        std::printf ("distortion at -20 dBFS: CLEAN %.1f %%, DIRTY %.0f %%\n", 100 * tc, 100 * td);
        check (tc < 0.03 && td > 0.15);
    }

    // 4. Channel switch mid-note: the biggest sample step around the switch stays like normal playing.
    {
        Chain c; Params p; p.room = 0; p.spring = 0; c.prepare (kOs); c.setParams (p);
        double worstNormal = 0, worstSwitch = 0, prevL = 0;
        for (size_t i = 0; i < chordIn.size(); ++i)
        {
            if (i == chordIn.size() / 2) { p.dirty = false; c.setParams (p); }
            float l, r; c.process (chordIn[i], l, r);
            const double step = std::abs (l - prevL); prevL = l;
            if (i > (size_t) (0.5 * kOs))
                (i >= chordIn.size() / 2 && i < chordIn.size() / 2 + (size_t) (0.05 * kOs) ? worstSwitch : worstNormal)
                    = std::max (i >= chordIn.size() / 2 && i < chordIn.size() / 2 + (size_t) (0.05 * kOs) ? worstSwitch : worstNormal, step);
        }
        std::printf ("channel switch: biggest step %.4f (normal playing %.4f)\n", worstSwitch, worstNormal);
        check (worstSwitch < 1.5 * worstNormal);
    }

    // 5. Gate: a riff, then 2 s of pickup hiss.
    {
        auto in = riff (2.0, 0.12f);
        in.resize ((size_t) (4.0 * kOs), 0.0f);
        uint32_t rng = 3;
        for (auto& v : in) { rng = rng * 1664525u + 1013904223u; v += 3e-4f * ((float) (rng >> 9) / 4194304.0f - 1.0f); }
        Params on; on.room = 0; Params off = on; off.gateDb = -90;
        std::vector<float> L1, R1, L2, R2; run (on, in, L1, R1); run (off, in, L2, R2);
        const size_t a = (size_t) (3.0 * kOs);
        const double gated = rmsDb (L1, a, in.size()), open = rmsDb (L2, a, in.size());
        std::printf ("gate: hiss after the riff %.0f dBFS (gate off: %.0f dBFS)\n", gated, open);
        check (open - gated > 30.0);
    }

    // 6. Double-track width.
    {
        for (float w : { 1.0f, 0.0f })
        {
            Params p; p.width = w; p.room = 0;
            std::vector<float> L, R; run (p, riffIn, L, R);
            double lr = 0, ll = 0, rr = 0;
            for (size_t i = (size_t) (0.5 * kOs); i < L.size(); ++i) { lr += (double) L[i] * R[i]; ll += (double) L[i] * L[i]; rr += (double) R[i] * R[i]; }
            const double corr = lr / std::sqrt (ll * rr);
            std::printf ("double %.0f: L/R correlation %.2f\n", w * 10, corr);
            check (w > 0.5f ? corr < 0.6 : corr > 0.999);
        }
    }

    // 7. Held chords.
    for (bool dirty : { false, true })
    {
        std::vector<float> in ((size_t) (8.0 * kOs), 0.0f);
        uint32_t rng = 9;
        for (double f : { 73.42, 110.0, 146.8, 185.0, 220.0, 293.7 })
        {
            Pluck pl (f, 0.08f, rng, 0.99995f);
            for (auto& v : in) v += pl.tick();
        }
        Params p; p.dirty = dirty;
        std::vector<float> L, R; run (p, in, L, R);
        const size_t win = (size_t) (0.25 * kOs);
        double prev = 0, worst = 0; bool ok = true;
        for (size_t s0 = 0; s0 + win <= in.size(); s0 += win)
        {
            const double db = rmsDb (L, s0, s0 + win);
            if (! std::isfinite (db)) ok = false;
            if (s0 >= 2 * win) worst = std::max (worst, std::abs (db - prev));
            prev = db;
        }
        std::printf ("held D chord, %s: biggest 0.25 s level step %.1f dB\n", dirty ? "DIRTY" : "CLEAN", worst);
        check (ok && worst < 3.0);
    }

    // 8. Levels and the demo.
    {
        std::vector<float> L1, R1, L2, R2;
        Params clean; clean.dirty = false; run (clean, chords (6.0, 0.1f), L1, R1);
        Params dirty; run (dirty, riff (6.0, 0.12f), L2, R2);
        const double cleanRms = rmsDb (L1, (size_t) kOs, L1.size()), dirtyRms = rmsDb (L2, (size_t) kOs, L2.size());
        L1.insert (L1.end(), L2.begin(), L2.end()); R1.insert (R1.end(), R2.begin(), R2.end());
        std::vector<float> dl, dr; double peak = 0; bool finite = true;
        float a1 = 0, a2 = 0, b1 = 0, b2 = 0;
        for (size_t i = 0; i < L1.size(); ++i)
        {
            a1 += 0.25f * (L1[i] - a1); a2 += 0.25f * (a1 - a2); b1 += 0.25f * (R1[i] - b1); b2 += 0.25f * (b1 - b2);
            if (i % 4 == 3) { dl.push_back (a2); dr.push_back (b2); finite &= std::isfinite (a2) && std::isfinite (b2);
                              peak = std::max ({ peak, (double) std::abs (a2), (double) std::abs (b2) }); }
        }
        std::printf ("defaults: CLEAN RMS %.1f dBFS, DIRTY RMS %.1f dBFS, demo peak %.1f dBFS, %s\n",
                     cleanRms, dirtyRms, 20 * std::log10 (peak + 1e-12), finite ? "finite" : "NON-FINITE");
        check (finite && peak < 1.0 && peak > 0.1 && std::abs (cleanRms - dirtyRms) < 6.0);
        writeWavStereo ("tann_demo.wav", dl, dr);
    }

    std::printf ("\n%s\n", gFailed ? "CHECKS FAILED" : "all checks passed");
    return gFailed ? 1 : 0;
}
