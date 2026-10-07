// MFR-Tannhäuser - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Late-'90s Swedish art-punk guitar, two channels, run 4x oversampled:
//
//   guitar -> gate -> CLEAN: solid-state combo -> spring -> 1x12
//                  or DIRTY: [boost] -> British 50 W head -> 4x12
//          -> dynamic close mic -> double-track spread -> room mic -> 2-inch tape -> L/R
//
// Pure C++ (no JUCE).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace tann
{

constexpr double kPi = 3.14159265358979323846;
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
    void lowpass (double fs, double f, double q)
    { const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q); set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
    void highpass (double fs, double f, double q)
    { const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q); set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
    void peaking (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowshelf (double fs, double f, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos (w), s = std::sqrt (A) * std::sin (w) * std::sqrt (2.0);
        set (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
             (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
    }
    void highshelf (double fs, double f, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos (w), s = std::sqrt (A) * std::sin (w) * std::sqrt (2.0);
        set (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
             (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
    }
private:
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    { b0 = float (B0 / A0); b1 = float (B1 / A0); b2 = float (B2 / A0); a1 = float (A1 / A0); a2 = float (A2 / A0); }
};

struct OnePole
{
    float a = 0, z = 0;
    void setCutoff (double fs, double f) { a = float (1 - std::exp (-2 * kPi * f / fs)); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { z += a * (x - z); return x - z; }
    void reset() { z = 0; }
};

//==============================================================================
// Passive treble/middle/bass network (Yeh & Smith 2006 analog model, bilinear),
// with the British head's component values. Double precision: at 4x oversampling
// the poles sit very close to z = 1, and single precision goes unstable.
class ToneStack
{
public:
    void set (double fs, double treble, double middle, double bass)
    {
        const double R1 = 220e3, R2 = 1e6, R3 = 22e3, R4 = 33e3;      // treble, bass, middle pots; slope resistor
        const double C1 = 470e-12, C2 = 22e-9, C3 = 22e-9;
        const double t = std::clamp (treble, 0.0, 1.0), m = std::clamp (middle, 0.0, 1.0);
        const double l = std::exp ((std::clamp (bass, 0.0, 1.0) - 1.0) * 3.4);

        const double b1 = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
        const double b2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
                        + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                        + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
        const double b3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                        - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4
                        + t * l * C1 * C2 * C3 * R1 * R2 * R4;
        const double a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
        const double a2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                        - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
                        + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
        const double a3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                        - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
                        + l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;

        const double c = 2 * fs, c2 = c * c, c3 = c2 * c;
        const double B0 = -b1 * c - b2 * c2 - b3 * c3, B1 = -b1 * c + b2 * c2 + 3 * b3 * c3;
        const double B2 = b1 * c + b2 * c2 - 3 * b3 * c3, B3 = b1 * c - b2 * c2 + b3 * c3;
        const double A0 = -1 - a1 * c - a2 * c2 - a3 * c3, A1 = -3 - a1 * c + a2 * c2 + 3 * a3 * c3;
        const double A2 = -3 + a1 * c + a2 * c2 - 3 * a3 * c3, A3 = -1 + a1 * c - a2 * c2 + a3 * c3;
        nb0 = B0 / A0; nb1 = B1 / A0; nb2 = B2 / A0; nb3 = B3 / A0;
        na1 = A1 / A0; na2 = A2 / A0; na3 = A3 / A0;
    }
    float process (float in)
    {
        const double x = in;
        const double y = nb0 * x + s1;
        s1 = nb1 * x - na1 * y + s2;
        s2 = nb2 * x - na2 * y + s3;
        s3 = nb3 * x - na3 * y;
        if (! std::isfinite (y)) { reset(); return 0.0f; }
        return (float) y;
    }
    void reset() { s1 = s2 = s3 = 0; }
private:
    double nb0 = 0, nb1 = 0, nb2 = 0, nb3 = 0, na1 = 0, na2 = 0, na3 = 0, s1 = 0, s2 = 0, s3 = 0;
};

//==============================================================================
// Small FDN reverb (8 lines, Householder mixing, damped feedback, stereo taps),
// used here with a short decay as the room mic.
class Plate
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        static const double ms[kLines] = { 29.7, 37.1, 41.1, 43.7, 53.9, 61.3, 67.9, 79.3 };
        for (int i = 0; i < kLines; ++i)
        {
            lines[(size_t) i].assign ((size_t) (ms[i] * 0.001 * fs) + 1, 0.0f);
            pos[(size_t) i] = 0;
            damp[(size_t) i].setCutoff (fs, 4500);
        }
        pre.assign ((size_t) (0.015 * fs) + 1, 0.0f);
        prePos = 0;
        inHP.setCutoff (fs, 200);
        setDecay (1.8);
        reset();
    }
    void reset()
    {
        for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
        std::fill (pre.begin(), pre.end(), 0.0f);
        for (auto& d : damp) d.reset();
        inHP.reset();
    }
    void setDecay (double rt60)
    {
        for (int i = 0; i < kLines; ++i)
            gain[(size_t) i] = (float) std::pow (10.0, -3.0 * (double) lines[(size_t) i].size() / (rt60 * fs));
    }
    void process (float x, float& outL, float& outR)
    {
        // Pre-delay, and keep the low end out of the tail.
        const float in = pre[(size_t) prePos];
        pre[(size_t) prePos] = inHP.hp (x);
        if (++prePos == (int) pre.size()) prePos = 0;

        std::array<float, kLines> o {};
        float sum = 0;
        for (int i = 0; i < kLines; ++i) { o[(size_t) i] = lines[(size_t) i][(size_t) pos[(size_t) i]]; sum += o[(size_t) i]; }
        const float h = sum * (2.0f / kLines);   // Householder: v - 2/N * sum
        for (int i = 0; i < kLines; ++i)
        {
            const float fb = damp[(size_t) i].lp ((o[(size_t) i] - h) * gain[(size_t) i]) + in * 0.35f;
            lines[(size_t) i][(size_t) pos[(size_t) i]] = fb;
            if (++pos[(size_t) i] == (int) lines[(size_t) i].size()) pos[(size_t) i] = 0;
        }
        outL = o[0] - o[2] + o[4] - o[6];
        outR = o[1] - o[3] + o[5] - o[7];
    }
private:
    static constexpr int kLines = 8;
    double fs = 192000;
    std::array<std::vector<float>, kLines> lines;
    std::array<int, kLines> pos {};
    std::array<float, kLines> gain {};
    std::array<OnePole, kLines> damp;
    std::vector<float> pre;
    int prePos = 0;
    OnePole inHP;
};

//==============================================================================
// A tiny spring tank: dispersive allpasses for the 'boing', a recirculating
// delay, and band-limiting.
class Spring
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& a : ap) { a.z = 0; }
        line.assign ((size_t) (0.036 * fs) + 1, 0.0f);
        line2.assign ((size_t) (0.041 * fs) + 1, 0.0f);
        pos = pos2 = 0;
        lp.lowpass (fs, 3800, 0.7); hp.highpass (fs, 300, 0.7);
        reset();
    }
    void reset()
    {
        for (auto& a : ap) a.z = 0;
        std::fill (line.begin(), line.end(), 0.0f);
        std::fill (line2.begin(), line2.end(), 0.0f);
        lp.reset(); hp.reset();
    }
    void process (float x, float& outL, float& outR)
    {
        float v = hp.process (x);
        for (auto& a : ap) v = a.process (v);                      // dispersion: highs arrive first, the chirp
        const float d1 = line[(size_t) pos], d2 = line2[(size_t) pos2];
        line[(size_t) pos] = lp.process (v + 0.62f * d2);
        line2[(size_t) pos2] = v * 0.7f + 0.6f * d1;
        if (++pos == (int) line.size()) pos = 0;
        if (++pos2 == (int) line2.size()) pos2 = 0;
        outL = d1; outR = d2;
    }
private:
    struct Allpass { float z = 0; float process (float x) { const float y = -0.65f * x + z; z = x + 0.65f * y; return y; } };
    double fs = 192000;
    std::array<Allpass, 24> ap {};
    std::vector<float> line, line2;
    int pos = 0, pos2 = 0;
    Biquad lp, hp;
};

//==============================================================================
// A circular delay line read at fractional delays (linear interpolation).
class DelayLine
{
public:
    void setMax (size_t samples) { buf.assign (samples + 4, 0.0f); w = 0; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    void push (float x) { buf[w] = x; if (++w == buf.size()) w = 0; }
    float read (double d) const   // d = samples behind the newest pushed sample
    {
        const double size = (double) buf.size();
        d = std::clamp (d, 0.0, size - 3.0);
        double rp = (double) w - 1.0 - d;
        if (rp < 0) rp += size;
        const size_t i = (size_t) rp;
        const float f = (float) (rp - (double) i);
        const size_t j = i + 1 == buf.size() ? 0 : i + 1;
        return buf[i] + (buf[j] - buf[i]) * f;
    }
private:
    std::vector<float> buf { 0.0f, 0.0f, 0.0f, 0.0f };
    size_t w = 0;
};

//==============================================================================
struct Params
{
    float gateDb = -70;                  // -90 = off
    bool  dirty = true;                  // channel: false = CLEAN (solid-state combo), true = DIRTY (British head)

    // CLEAN channel
    float cVolume = 4.0f;                // 0..10
    float cBass = 5.0f, cMid = 5.0f, cTreble = 5.0f;   // 0..10, 5 = flat
    bool  cBright = false;
    float spring = 0.2f;                 // 0..1

    // DIRTY channel
    bool  boost = false;
    float preamp = 6.0f, master = 6.0f;  // 0..10
    float dBass = 5.0f, dMiddle = 6.0f, dTreble = 6.0f, presence = 5.0f;   // 0..10

    // Shared
    float room = 0.25f;                  // 0..1, close mic -> room mic
    float tape = 0.4f;                   // 0..1
    float width = 1.0f;                  // 0..1, double-track spread
    float outputDb = -3;
};

class Chain
{
public:
    void prepare (double oversampledRate)
    {
        fs = oversampledRate;
        gateDecay = float (std::exp (-1.0 / (0.03 * fs)));
        gateOpen = float (1 - std::exp (-1.0 / (0.0005 * fs)));
        gateClose = float (1 - std::exp (-1.0 / (0.06 * fs)));

        // CLEAN: solid-state combo, 1x12.
        cIn.highpass (fs, 60, 0.7);
        // 1x12 voicing fitted to the reference's clean verse guitar.
        cCabHP.highpass (fs, 120, 1.0); cCabDip.peaking (fs, 355, 2.5, -2.0); cCabPeak.peaking (fs, 2420, 1.2, 10.0);
        cCabLP1.lowpass (fs, 3170, 0.63); cCabLP2.lowpass (fs, 8750, 1.5);
        spring.prepare (fs);

        // DIRTY: boost, British head, 4x12.
        bIn.setCutoff (fs, 30); bHump.setCutoff (fs, 720); bLP.lowpass (fs, 6000, 0.7);
        couple1.setCutoff (fs, 30); couple2.setCutoff (fs, 25); couple3.setCutoff (fs, 20);
        tight.highpass (fs, 60, 0.7);
        resonance.lowshelf (fs, 100, 2.0);
        // 4x12 voicing fitted to the reference's distorted guitars: low-mid body, a scoop at 440 Hz,
        // a narrow presence peak near 2.8 kHz (the close mic), then a gentle roll-off.
        dCabHP.highpass (fs, 68, 1.2); dCabThump.peaking (fs, 285, 0.3, 5.0); dCabScoop.peaking (fs, 440, 1.06, -8.2);
        dCabBite.peaking (fs, 2770, 2.5, 5.25); dCabLP1.lowpass (fs, 11200, 1.5); dCabLP2.lowpass (fs, 5400, 0.4);

        // Shared: dynamic close mic, room mic, doubler, tape.
        micProx.lowshelf (fs, 150, 1.5); micPres.peaking (fs, 4500, 1.2, 3.0);
        room.prepare (fs); room.setDecay (0.55);
        dblL.setMax ((size_t) (0.03 * fs)); dblR.setMax ((size_t) (0.03 * fs));
        tapeHFL.highshelf (fs, 9000, -2.0); tapeHFR.highshelf (fs, 9000, -2.0);
        fade = (float) (1.0 / (0.012 * fs));
        setParams (params);
        reset();
    }
    void reset()
    {
        env = 0; gate = 0; lfoA = 0; lfoB = 0.37;
        for (auto* o : { &bIn, &bHump, &couple1, &couple2, &couple3 }) o->reset();
        for (auto* b : { &cIn, &cBassF, &cMidF, &cTrebleF, &cBrightF, &cCabHP, &cCabDip, &cCabPeak, &cCabLP1, &cCabLP2,
                         &bLP, &bTone, &bright, &tight, &presence, &resonance,
                         &dCabHP, &dCabThump, &dCabScoop, &dCabBite, &dCabLP1, &dCabLP2, &micProx, &micPres, &tapeHFL, &tapeHFR }) b->reset();
        stack.reset(); spring.reset(); room.reset(); dblL.reset(); dblR.reset();
        dirtyMix = params.dirty ? 1.0f : 0.0f;
    }
    void setParams (const Params& p)
    {
        params = p;
        gateOn = p.gateDb > -89.5f;
        gateThresh = dbToGain (p.gateDb);

        const float cv = std::clamp (p.cVolume / 10.0f, 0.0f, 1.0f);
        cGain = 0.4f + 5.0f * cv * cv;
        cBassF.lowshelf (fs, 100, (p.cBass - 5.0f) * 2.4f);
        cMidF.peaking (fs, 800, 0.7, (p.cMid - 5.0f) * 2.0f);
        cTrebleF.highshelf (fs, 3000, (p.cTreble - 5.0f) * 2.4f);
        cBrightF.highshelf (fs, 4000, p.cBright ? 4.0 : 0.0);

        bGain = 4.0f;
        bTone.highshelf (fs, 1200, 2.0);
        const float pv = std::clamp (p.preamp / 10.0f, 0.0f, 1.0f);
        preGain = 1.0f + 60.0f * pv * pv;
        bright.highshelf (fs, 2500, 6.0 * (1.0 - pv));
        stack.set (fs, p.dTreble / 10.0, p.dMiddle / 10.0, p.dBass / 10.0);
        const float mv = std::clamp (p.master / 10.0f, 0.0f, 1.0f);
        masterGain = 0.3f + 4.0f * mv * mv;
        presence.highshelf (fs, 3500, (p.presence - 5.0f) * 1.6f);

        tapeDrive = 1.0f + 3.0f * p.tape;
        outGain = dbToGain (p.outputDb);
    }

    void process (float x, float& outL, float& outR)
    {
        if (gateOn)
        {
            env = std::max (std::abs (x), env * gateDecay);
            const float target = env > gateThresh ? 1.0f : 0.0f;
            gate += (target > gate ? gateOpen : gateClose) * (target - gate);
            x *= gate;
        }

        dirtyMix = std::clamp (dirtyMix + (params.dirty ? fade : -fade), 0.0f, 1.0f);
        float clean = 0, dirty = 0;

        if (dirtyMix < 1.0f)
        {
            // Solid-state combo: an op-amp stage that stays linear until it hits the rails, flat-ish tone controls.
            float c = cIn.process (x) * cGain;
            c = c / std::pow (1.0f + std::pow (std::abs (c), 6.0f), 1.0f / 6.0f);
            c = cBrightF.process (cTrebleF.process (cMidF.process (cBassF.process (c))));
            float sl, sr;
            spring.process (c, sl, sr);
            c += params.spring * 1.5f * 0.5f * (sl + sr);
            c = cCabLP2.process (cCabLP1.process (cCabPeak.process (cCabDip.process (cCabHP.process (c)))));
            clean = c * 4.0f;   // level-matched to the DIRTY channel at the defaults
        }
        if (dirtyMix > 0.0f)
        {
            float v = bIn.hp (x);
            if (params.boost)
            {
                // Mid-hump boost: gain only above ~720 Hz, soft diode clip.
                float h = bHump.hp (v) * 12.0f;
                h = 0.6f * h / std::sqrt (1.0f + (h / 0.6f) * (h / 0.6f));
                v = bTone.process (bLP.process (v + h)) * bGain;
            }
            // Cascaded preamp: a bright first stage, then a cold-biased second one.
            float a = bright.process (tight.process (v)) * preGain;
            a = a > 0 ? std::tanh (1.3f * a) / 1.3f : std::tanh (0.9f * a) / 0.9f;
            a = couple1.hp (a) * 6.0f;
            a = a > 0 ? std::tanh (0.7f * a) / 0.7f : std::tanh (2.2f * a) / 2.2f;
            a = couple2.hp (a);
            a = stack.process (a) * 3.0f;
            a = couple3.hp (a) * masterGain;
            a = std::tanh (a);   // EL34 push-pull
            a = resonance.process (presence.process (a));
            dirty = dCabLP2.process (dCabLP1.process (dCabBite.process (dCabScoop.process (dCabThump.process (dCabHP.process (a)))))) * 0.5f;
        }
        float y = clean + dirtyMix * (dirty - clean);

        // Dynamic close mic.
        y = micPres.process (micProx.process (y));

        // Double-track: two slightly drifting, differently delayed copies hard left and right.
        dblL.push (y); dblR.push (y);
        lfoA += 0.23 / fs; lfoA -= std::floor (lfoA);
        lfoB += 0.31 / fs; lfoB -= std::floor (lfoB);
        const double ms = 0.001 * fs;
        const float dl = dblL.read ((2.0 + 0.6 * std::sin (2 * kPi * lfoA)) * ms);
        const float dr = dblR.read ((14.0 + 0.8 * std::sin (2 * kPi * lfoB)) * ms);
        float l = y + params.width * (dl - y);
        float r = y + params.width * (dr - y);

        // Room mic.
        float rl, rr;
        room.process (0.5f * (l + r), rl, rr);
        const float wet = params.room * 0.6f;
        l = (1.0f - 0.4f * params.room) * l + wet * rl;
        r = (1.0f - 0.4f * params.room) * r + wet * rr;

        // 2-inch tape.
        auto tape = [k = 0.4f * tapeDrive] (float s) { return std::tanh (s * k) / k; };   // saturates loud peaks, not normal playing
        outL = tapeHFL.process (tape (l)) * outGain;
        outR = tapeHFR.process (tape (r)) * outGain;
        if (! std::isfinite (outL) || ! std::isfinite (outR)) { reset(); outL = outR = 0; }
    }

    float getGate() const { return gateOn ? gate : 1.0f; }

private:
    double fs = 192000, lfoA = 0, lfoB = 0.37;
    Params params;
    bool gateOn = true;
    float env = 0, gate = 0, gateThresh = 0, gateDecay = 0, gateOpen = 0, gateClose = 0;
    Biquad cIn, cBassF, cMidF, cTrebleF, cBrightF, cCabHP, cCabDip, cCabPeak, cCabLP1, cCabLP2;
    Spring spring;
    OnePole bIn, bHump, couple1, couple2, couple3;
    Biquad bLP, bTone, bright, tight, presence, resonance, dCabHP, dCabThump, dCabScoop, dCabBite, dCabLP1, dCabLP2;
    ToneStack stack;
    Biquad micProx, micPres, tapeHFL, tapeHFR;
    Plate room;
    DelayLine dblL, dblR;
    float cGain = 1, bGain = 4, preGain = 20, masterGain = 2, tapeDrive = 2, outGain = 1, dirtyMix = 1, fade = 0.001f;
};

} // namespace tann
