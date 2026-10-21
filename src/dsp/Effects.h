// Ostrov — space effects: modulated plate (Dattorro topology) and tape delay
#pragma once
#include "Blocks.h"
#include <vector>

namespace ostrov {

struct DelayLine {
    std::vector<float> buf; int mask = 0, w = 0;
    void init(int minLen) {
        int n = 1; while (n < minLen + 4) n <<= 1;
        buf.assign((size_t)n, 0.0f); mask = n - 1; w = 0;
    }
    void clear() { std::fill(buf.begin(), buf.end(), 0.0f); }
    void push(float x) { buf[(size_t)w] = x; w = (w + 1) & mask; }
    float tap(int d) const { return buf[(size_t)((w - 1 - d) & mask)]; }
    float tapFrac(float d) const {
        const int i = (int)d; const float f = d - (float)i;
        const float a = tap(i), b = tap(i + 1);
        return a + (b - a) * f;
    }
};

// Allpass built on a delay line: y = -g*x + d; push(x + g*y)
struct Allpass {
    DelayLine d; int len = 1;
    void init(int l) { len = l; d.init(l + 64); }
    float process(float x, float g) {
        const float del = d.tap(len - 1);
        const float v = x + g * del;   // feedback form
        d.push(v);
        return del - g * v;
    }
    float processMod(float x, float g, float extra) {
        const float del = d.tapFrac((float)(len - 1) + extra);
        const float v = x + g * del;
        d.push(v);
        return del - g * v;
    }
};

class Plate {
public:
    void prepare(float sampleRate) {
        sr = sampleRate;
        const float s = sr / 29761.0f;
        auto L = [&](int n) { return std::max(1, (int)(n * s)); };
        in1.init(L(142)); in2.init(L(107)); in3.init(L(379)); in4.init(L(277));
        apL.init(L(672)); apR.init(L(908));
        dL1.init(L(4453)); dR1.init(L(4217));
        ap2L.init(L(1800)); ap2R.init(L(2656));
        dL2.init(L(3720)); dR2.init(L(3163));
        lenL1 = L(4453); lenR1 = L(4217); lenL2 = L(3720); lenR2 = L(3163);
        excursion = 16.0f * s;
        pre.init((int)(0.03f * sr));
        scale = s;
        clear();
    }
    void clear() {
        for (auto* a : {&in1, &in2, &in3, &in4, &apL, &apR, &ap2L, &ap2R}) a->d.clear();
        for (auto* d : {&dL1, &dR1, &dL2, &dR2, &pre}) d->clear();
        bwState = dampL = dampR = fbL = fbR = 0.0f;
    }
    // size 0..1 -> decay; returns wet stereo
    void process(float inL, float inR, float size, float& outL, float& outR) {
        const float decay = 0.35f + 0.6f * size;
        const float damping = 0.45f - 0.35f * size;
        pre.push(0.5f * (inL + inR));
        float x = pre.tap((int)(0.012f * sr));
        bwState += (x - bwState) * 0.75f;
        x = bwState;
        x = in1.process(x, 0.75f); x = in2.process(x, 0.75f);
        x = in3.process(x, 0.625f); x = in4.process(x, 0.625f);
        lfoPh += 0.9f / sr; if (lfoPh >= 1.0f) lfoPh -= 1.0f;
        const float modA = excursion * (0.5f + 0.5f * std::sin(kTwoPi * lfoPh));
        const float modB = excursion * (0.5f + 0.5f * std::cos(kTwoPi * lfoPh * 0.77f));
        // left half
        float l = apL.processMod(x + fbR, -0.7f, modA);
        dL1.push(l);
        l = dL1.tap(lenL1 - 1);
        dampL += (l - dampL) * (1.0f - damping);
        l = ap2L.process(dampL * decay, 0.5f);
        dL2.push(l);
        // right half
        float r = apR.processMod(x + fbL, -0.7f, modB);
        dR1.push(r);
        r = dR1.tap(lenR1 - 1);
        dampR += (r - dampR) * (1.0f - damping);
        r = ap2R.process(dampR * decay, 0.5f);
        dR2.push(r);
        fbL = dL2.tap(lenL2 - 1) * decay;
        fbR = dR2.tap(lenR2 - 1) * decay;
        auto T = [&](int n) { return std::max(0, (int)(n * scale)); };
        outL = 0.6f * (dR1.tap(T(266)) + dR1.tap(T(2974)) - ap2R.d.tap(T(1913)) + dR2.tap(T(1996))
                       - dL1.tap(T(1990)) - ap2L.d.tap(T(187)) - dL2.tap(T(1066)));
        outR = 0.6f * (dL1.tap(T(353)) + dL1.tap(T(3627)) - ap2L.d.tap(T(1228)) + dL2.tap(T(2673))
                       - dR1.tap(T(2111)) - ap2R.d.tap(T(335)) - dR2.tap(T(121)));
    }
private:
    float sr = 48000.0f, scale = 1.0f, excursion = 16.0f, lfoPh = 0.0f;
    Allpass in1, in2, in3, in4, apL, apR, ap2L, ap2R;
    DelayLine dL1, dR1, dL2, dR2, pre;
    int lenL1 = 1, lenR1 = 1, lenL2 = 1, lenR2 = 1;
    float bwState = 0, dampL = 0, dampR = 0, fbL = 0, fbR = 0;
};

class TapeDelay {
public:
    void prepare(float sampleRate) {
        sr = sampleRate;
        l.init((int)(4.0f * sr)); r.init((int)(4.0f * sr));
        timeL = timeR = 0.4f * sr;
        lpL = lpR = 0.0f; noise.seed(777u);
    }
    void clear() { l.clear(); r.clear(); lpL = lpR = 0.0f; }
    void process(float inL, float inR, float size, float& outL, float& outR) {
        const float base = (0.12f + 0.9f * size) * sr;
        // slow glide of delay time -> pitch bends like a tape machine when changed
        timeL += (base - timeL) * 0.00003f;
        timeR += (base * 1.075f - timeR) * 0.00003f;
        wow.ph += 0.45f / sr; wow.ph -= std::floor(wow.ph);
        flut.ph += 5.9f / sr; flut.ph -= std::floor(flut.ph);
        const float mod = 0.0018f * sr * std::sin(kTwoPi * wow.ph) + 0.00015f * sr * std::sin(kTwoPi * flut.ph)
                        + 0.00003f * sr * noise.next();
        const float yl = l.tapFrac(clampf(timeL + mod, 1.0f, (float)l.mask - 4.0f));
        const float yr = r.tapFrac(clampf(timeR - mod * 0.8f, 1.0f, (float)r.mask - 4.0f));
        const float fb = 0.25f + 0.55f * size;
        lpL += (yl - lpL) * 0.35f;
        lpR += (yr - lpR) * 0.35f;
        const float fl = fastTanh((lpL * 0.8f + lpR * 0.2f) * fb * 1.2f) / 1.2f;
        const float fr = fastTanh((lpR * 0.8f + lpL * 0.2f) * fb * 1.2f) / 1.2f;
        l.push(inL + dcL.process(fl));
        r.push(inR + dcR.process(fr));
        outL = yl; outR = yr;
    }
private:
    float sr = 48000.0f, timeL = 1, timeR = 1, lpL = 0, lpR = 0;
    DelayLine l, r; Lfo wow, flut; Noise noise; DcBlock dcL, dcR;
};

// Octave-up pitch shifter: two crossfaded read heads sweeping a short window
class OctaveUp {
public:
    void prepare(float sr) { win = (int)(0.06f * sr); d.init(win * 2 + 8); ph = 0.0f; }
    void clear() { d.clear(); }
    float process(float x) {
        d.push(x);
        ph += 1.0f / (float)win; if (ph >= 1.0f) ph -= 1.0f;
        float y = 0.0f;
        for (int h = 0; h < 2; ++h) {
            float p = ph + 0.5f * (float)h; if (p >= 1.0f) p -= 1.0f;
            const float delay = (1.0f - p) * (float)win + 2.0f;     // shrinking delay -> 2x speed
            y += d.tapFrac(delay) * std::sin(kPi * p);
        }
        return y;
    }
private:
    DelayLine d; int win = 2048; float ph = 0.0f;
};

// Stereo ensemble: three modulated taps per side (BBD-style string ensemble)
class Ensemble {
public:
    void prepare(float sampleRate) { sr = sampleRate; l.init((int)(0.05f * sr)); r.init((int)(0.05f * sr)); }
    void clear() { l.clear(); r.clear(); }
    void process(float& L, float& R, float amount) {
        l.push(L); r.push(R);
        if (amount <= 0.001f) return;
        static const float rates[3] = {0.33f, 0.51f, 0.73f};
        float wl = 0.0f, wr = 0.0f;
        for (int k = 0; k < 3; ++k) {
            ph[k] += rates[k] / sr; if (ph[k] >= 1.0f) ph[k] -= 1.0f;
            const float a = std::sin(kTwoPi * (ph[k] + 0.333f * (float)k));
            const float b = std::sin(kTwoPi * (ph[k] + 0.333f * (float)k + 0.25f));
            wl += l.tapFrac((0.011f + 0.003f * a) * sr);
            wr += r.tapFrac((0.011f + 0.003f * b) * sr);
        }
        L = L * (1.0f - 0.4f * amount) + wl * 0.33f * amount * 0.9f;
        R = R * (1.0f - 0.4f * amount) + wr * 0.33f * amount * 0.9f;
    }
private:
    float sr = 48000.0f, ph[3] = {0.0f, 0.3f, 0.6f};
    DelayLine l, r;
};

} // namespace ostrov
