// Ostrov — own band-limited single-cycle waves (additive, mip-mapped)
#pragma once
#include "Blocks.h"
#include <vector>
#include <array>

namespace ostrov {

enum WaveId {
    W_Sine = 0, W_Warm, W_Soft, W_Hollow, W_Reed, W_Glass, W_Bright, W_Organ,
    W_Saw, W_Tri, W_Square, W_Count
};

class WaveBank {
public:
    static constexpr int kSize = 2048;          // samples per cycle
    static constexpr int kLevels = 10;          // max harmonics 512,256,...,1

    void build() {
        tables.assign((size_t)W_Count * kLevels * (kSize + 1), 0.0f);
        for (int w = 0; w < W_Count; ++w)
            for (int l = 0; l < kLevels; ++l)
                buildOne(w, l);
    }

    // phase in cycles [0,1). freqNorm = f/sr chooses the mip level.
    float read(int wave, float phase, float freqNorm) const {
        const int lvl = levelFor(freqNorm);
        const float* t = table(wave, lvl);
        const float p = phase * kSize;
        const int i = (int)p;
        const float fr = p - (float)i;
        return t[i] + (t[i + 1] - t[i]) * fr;
    }

    static float wrap(float ph) { return ph - std::floor(ph); }

private:
    std::vector<float> tables;

    const float* table(int w, int l) const { return &tables[((size_t)w * kLevels + l) * (kSize + 1)]; }
    float* table(int w, int l) { return &tables[((size_t)w * kLevels + l) * (kSize + 1)]; }

    static int maxHarm(int level) { return 512 >> level; }

    static int levelFor(float freqNorm) {
        // highest harmonic count H with H * f < 0.45 sr
        const float hmax = 0.45f / std::max(freqNorm, 1e-6f);
        int l = 0;
        while (l < kLevels - 1 && (float)maxHarm(l) > hmax) ++l;
        return l;
    }

    // Harmonic recipe: amplitude (signed) for harmonic n of wave w
    static float amp(int w, int n) {
        const float fn = (float)n;
        switch (w) {
            case W_Sine:   return n == 1 ? 1.0f : 0.0f;
            case W_Warm:   // dark, all harmonics, slight 2nd boost
                return (n == 2 ? 1.4f : 1.0f) / std::pow(fn, 2.1f);
            case W_Soft:   // soft square: odd, rolled off
                return (n % 2) ? std::exp(-fn / 9.0f) / fn : 0.0f;
            case W_Hollow: // triangle-ish with a lifted 3rd
                return (n % 2) ? ((n == 3 ? 2.2f : 1.0f) / (fn * fn)) * ((n / 2) % 2 ? -1.0f : 1.0f) : 0.0f;
            case W_Reed:   // formant ripple, even-rich
                return (1.0f + 0.7f * std::cos(fn * 0.85f)) * std::exp(-fn / 18.0f) / std::pow(fn, 1.1f);
            case W_Glass: { // sparse bright partials
                switch (n) { case 1: return 1.0f; case 2: return 0.32f; case 4: return 0.28f;
                             case 7: return 0.16f; case 11: return 0.09f; case 16: return 0.05f; default: return 0.0f; }
            }
            case W_Bright: return std::exp(-fn / 40.0f) / fn * (n % 2 ? 1.0f : 0.8f);
            case W_Organ: {
                switch (n) { case 1: return 1.0f; case 2: return 0.65f; case 3: return 0.45f; case 4: return 0.3f;
                             case 6: return 0.18f; case 8: return 0.14f; default: return 0.0f; }
            }
            case W_Saw:    return 1.0f / fn;
            case W_Tri:    return (n % 2) ? (((n / 2) % 2) ? -1.0f : 1.0f) / (fn * fn) : 0.0f;
            case W_Square: return (n % 2) ? 1.0f / fn : 0.0f;
        }
        return 0.0f;
    }

    void buildOne(int w, int l) {
        float* t = table(w, l);
        const int H = std::min(maxHarm(l), kSize / 2 - 1);
        static std::vector<double> sinT;
        if (sinT.empty()) {
            sinT.resize(kSize);
            for (int i = 0; i < kSize; ++i) sinT[i] = std::sin(2.0 * 3.14159265358979 * i / kSize);
        }
        std::vector<double> acc(kSize, 0.0);
        for (int n = 1; n <= H; ++n) {
            const double a = amp(w, n);
            if (a == 0.0) continue;
            // gentle top roll-off inside the level to avoid Gibbs ringing
            const double sigma = (H > 4) ? std::sin(kPi * n / (H + 1.0)) / (kPi * n / (H + 1.0)) : 1.0;
            for (int i = 0; i < kSize; ++i)
                acc[i] += a * sigma * sinT[(size_t)((long)n * i % kSize)];
        }
        double mx = 1e-9;
        for (double v : acc) mx = std::max(mx, std::fabs(v));
        for (int i = 0; i < kSize; ++i) t[i] = (float)(acc[i] / mx);
        t[kSize] = t[0];
    }
};

} // namespace ostrov
