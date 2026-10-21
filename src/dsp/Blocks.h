// Ostrov — basic DSP building blocks
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace ostrov {

constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Cubic soft saturator y = x - x^3/3, clamped at ±sqrt(3)  (smooth, odd harmonics)
inline float cubicSat(float x) {
    x = clampf(x, -1.7320508f, 1.7320508f);
    return x - x * x * x * (1.0f / 3.0f);
}

// Asymmetric "tape/tube" curve: adds a touch of 2nd harmonic
inline float asymSat(float x, float a) { return x + a * (1.0f - x * x) - a; }

inline float fastTanh(float x) {
    x = clampf(x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Simple one-pole smoother
struct Smoother {
    float y = 0.0f;
    float process(float target, float coeff) { y += (target - y) * coeff; return y; }
};

inline float onePoleCoeff(float timeSec, float sr) {
    if (timeSec <= 0.0f) return 1.0f;
    return 1.0f - std::exp(-1.0f / (timeSec * sr));
}

// xorshift32 noise in [-1,1)
struct Noise {
    uint32_t s = 0x9E3779B9u;
    void seed(uint32_t v) { s = v ? v : 0x9E3779B9u; }
    float next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (int32_t)s * (1.0f / 2147483648.0f);
    }
};

// Sine LFO with phase in cycles
struct Lfo {
    float ph = 0.0f;
    float sine(float hz, float sr) {
        ph += hz / sr; ph -= std::floor(ph);
        return std::sin(ph * kTwoPi);
    }
    float tri(float hz, float sr) {
        ph += hz / sr; ph -= std::floor(ph);
        return 4.0f * std::fabs(ph - 0.5f) - 1.0f;
    }
};

// Zero-delay-feedback state-variable filter (Simper). bp is normalised to unity peak.
struct Svf {
    float ic1 = 0.0f, ic2 = 0.0f;
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    void set(float fc, float q, float sr) {
        fc = clampf(fc, 10.0f, sr * 0.45f);
        g = std::tan(kPi * fc / sr);
        k = 1.0f / std::max(q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0.0f; }
    // returns band-pass normalised to 0 dB peak
    float bp(float x) {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v1 * k;
    }
    float lp(float x) {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }
};

// Two-pole resonator with decay time (modal / bell partial). Input gain normalised.
struct Resonator {
    float y1 = 0.0f, y2 = 0.0f, b1 = 0.0f, b2 = 0.0f, gain = 0.0f;
    void set(float freq, float decaySec, float sr) {
        freq = clampf(freq, 20.0f, sr * 0.45f);
        const float r = std::exp(-1.0f / (std::max(decaySec, 0.001f) * sr));
        const float w = kTwoPi * freq / sr;
        b1 = 2.0f * r * std::cos(w);
        b2 = -r * r;
        gain = (1.0f - r * r) * 0.5f / std::max(std::sin(w), 0.01f) ;
    }
    void reset() { y1 = y2 = 0.0f; }
    float process(float x) {
        const float y = x * gain + b1 * y1 + b2 * y2;
        y2 = y1; y1 = y;
        return y;
    }
};

// 4-pole ladder low-pass with cubic saturation in the feedback path (2x oversampled).
// Character: resonance pushed through x - x^3/3, cutoff mapped with a square law.
struct WarmLadder {
    float s[4] = {0, 0, 0, 0};
    float fbState = 0.0f;
    float g = 0.1f, res = 0.0f;
    void reset() { for (float& v : s) v = 0.0f; fbState = 0.0f; }
    void set(float fc, float resonance, float sr) {
        fc = clampf(fc, 20.0f, sr * 0.42f);
        const float wc = kTwoPi * fc / (2.0f * sr); // 2x oversampled
        g = wc / (1.0f + wc);
        res = clampf(resonance, 0.0f, 1.0f) * 3.9f;
    }
    float tick(float x) {
        const float fb = cubicSat(s[3] * res * 0.5f) * 2.0f;
        float in = x - fb;
        in = cubicSat(in);
        s[0] += g * (in - s[0]);
        s[1] += g * (s[0] - s[1]);
        s[2] += g * (s[1] - s[2]);
        s[3] += g * (s[2] - s[3]);
        return s[3];
    }
    float process(float x) {
        tick(x);
        // gain compensation for resonance
        return tick(x) * (1.0f + res * 0.35f);
    }
};

// DC blocker
struct DcBlock {
    float x1 = 0.0f, y1 = 0.0f;
    float process(float x) { const float y = x - x1 + 0.9975f * y1; x1 = x; y1 = y; return y; }
};

} // namespace ostrov
