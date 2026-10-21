// Ostrov — synthesis engines.
// Each engine renders one voice (mono) per call. Ideas come from the hardware analysis:
//  Bloom  — phase modulation where operators are soft single-cycle waves, pressure-driven index
//  Drift  — tri↔saw VCO into a warm ladder, slow drift on non-integer LFO ratios
//  Fold   — oscillators fed into a wave used as a transfer function (wavefolder)
//  Bell   — exciter pinging modal resonators
//  Choir  — VCO into formant resonators (vowel morph)
//  Breath — noise into a bank of tuned resonators
#pragma once
#include "Blocks.h"
#include "Wavetables.h"
#include <array>

namespace ostrov {

constexpr int kMaxVoices = 12;

struct Macros { float timbre = 0.5f, color = 0.5f, motion = 0.3f, body = 0.5f; };

struct VoiceIn {
    float freq = 220.0f;
    float sr = 48000.0f;
    const float* env = nullptr;     // amplitude envelope 0..1 (applied by the core)
    const float* strike = nullptr;  // velocity transient + pressure, 0..~1.5
    float vel = 0.8f;
    int idx = 0;
    int n = 0;
};

class Engine {
public:
    explicit Engine(const WaveBank& w) : waves(w) {}
    virtual ~Engine() = default;
    virtual void reset(int v) = 0;
    virtual void render(const VoiceIn& in, const Macros& m, float* out) = 0;
protected:
    const WaveBank& waves;
};

// ---------------------------------------------------------------- Bloom
class BloomEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override { st[v] = {}; }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.idx];
        const float fn = in.freq / in.sr;
        static const float ratios[4][2] = {{1.0f, 2.0f}, {1.0f, 3.0f}, {2.0f, 5.0f}, {0.5f, 3.5f}};
        const int rb = std::min(3, (int)(m.body * 4.0f));
        const float r1 = ratios[rb][0], r2 = ratios[rb][1];
        static const int carriers[5] = {W_Warm, W_Reed, W_Soft, W_Organ, W_Glass};
        const float cpos = clampf(m.color, 0.0f, 1.0f) * 3.999f;
        const int c0 = (int)cpos; const float cf = cpos - (float)c0;
        const float tim = std::pow(clampf(m.timbre, 0.0f, 1.0f), 1.4f);
        const float detune = 1.0f + 0.0064f * m.motion;            // the ~11 cent ensemble layer
        const float fbAmt = 0.12f * m.motion;
        for (int i = 0; i < in.n; ++i) {
            const float e = in.env[i], k = in.strike[i];
            const float I = tim * (0.12f + 0.55f * e + 0.6f * k) * 1.3f;   // index in cycles
            s.pc = WaveBank::wrap(s.pc + fn);
            s.p1 = WaveBank::wrap(s.p1 + fn * r1);
            s.p2 = WaveBank::wrap(s.p2 + fn * r2);
            s.pl = WaveBank::wrap(s.pl + fn * detune);
            const float m1 = waves.read(W_Sine, s.p1, fn * r1);
            const float m2 = waves.read(W_Warm, WaveBank::wrap(s.p2 + m1 * I * 0.3f), fn * r2 * (1.0f + I));
            const float pm = I * (0.22f * m1 + 0.18f * m2) + fbAmt * s.fb;
            const float bw = fn * (1.0f + I * 4.0f);
            const float ph = WaveBank::wrap(s.pc + pm);
            float car = waves.read(carriers[c0], ph, bw);
            if (cf > 0.001f) car += (waves.read(carriers[c0 + 1], ph, bw) - car) * cf;
            s.fb += (car - s.fb) * 0.4f;
            const float layer = waves.read(carriers[c0], WaveBank::wrap(s.pl + pm * 0.5f), bw);
            out[i] = car * (1.0f - 0.3f * m.motion) + layer * 0.3f * m.motion;
        }
    }
private:
    struct St { float pc = 0, p1 = 0, p2 = 0, pl = 0.37f, fb = 0; };
    std::array<St, kMaxVoices> st{};
};

// ---------------------------------------------------------------- Drift
class DriftEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override {
        auto& s = st[v];
        s.f.reset(); s.p1 = 0; s.p2 = 0.31f; s.p3 = 0.0f;
        s.l[0].ph = 0.1f * v; s.l[1].ph = 0.37f + 0.05f * v; s.l[2].ph = 0.71f; s.l[3].ph = 0.53f;
    }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.idx];
        // non-integer LFO ratios (cf. 0.946 / 0.56 / 0.405 in the analysed engine)
        const float base = 0.31f * (1.0f + 0.07f * in.idx);
        const float morph = clampf(m.color, 0.0f, 1.0f) * 1.999f;   // tri -> saw -> square
        const int w0 = morph < 1.0f ? W_Tri : W_Saw;
        const int w1 = morph < 1.0f ? W_Saw : W_Square;
        const float wf = morph - std::floor(morph);
        const float tim = clampf(m.timbre, 0.0f, 1.0f);
        for (int i = 0; i < in.n; ++i) {
            const float l0 = s.l[0].sine(base, in.sr), l1 = s.l[1].sine(base * 0.946f, in.sr);
            const float l2 = s.l[2].sine(base * 0.56f, in.sr), l3 = s.l[3].sine(base * 0.405f, in.sr);
            const float driftC = m.motion * (4.0f * l0 + 2.0f * l2);              // cents
            const float f1 = in.freq * std::exp2(driftC / 1200.0f);
            const float f2 = f1 * std::exp2((m.motion * 9.0f * (0.7f + 0.3f * l1) + 3.0f) / 1200.0f);
            const float fn1 = f1 / in.sr, fn2 = f2 / in.sr;
            s.p1 = WaveBank::wrap(s.p1 + fn1);
            s.p2 = WaveBank::wrap(s.p2 + fn2);
            s.p3 = WaveBank::wrap(s.p3 + fn1 * 0.5f);
            auto osc = [&](float p, float fn) {
                const float a = waves.read(w0, p, fn);
                return a + (waves.read(w1, p, fn) - a) * wf;
            };
            const float x = 0.5f * (osc(s.p1, fn1) + osc(s.p2, fn2)) + 0.25f * waves.read(W_Sine, s.p3, fn1 * 0.5f);
            if ((i & 7) == 0) {
                const float k = in.strike[i], e = in.env[i];
                float fc = in.freq * (1.2f + tim * tim * 70.0f) * (1.0f + 1.8f * k * tim + 0.4f * e)
                           * (1.0f + 0.25f * m.motion * l3);
                s.f.set(fc, m.body * 0.92f, in.sr);
            }
            out[i] = s.f.process(x * 0.8f) * 2.9f;
        }
    }
private:
    struct St { float p1 = 0, p2 = 0, p3 = 0; WarmLadder f; Lfo l[4]; };
    std::array<St, kMaxVoices> st{};
};

// ---------------------------------------------------------------- Fold
class FoldEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override { auto& s = st[v]; s = {}; s.aa.reset(); s.post.reset(); s.lfo.ph = 0.13f * v; }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.idx];
        const float fn = in.freq / in.sr * 0.5f;        // 2x oversampled
        const float tim = std::pow(clampf(m.timbre, 0.0f, 1.0f), 1.3f);
        static const int transfers[4] = {W_Sine, W_Warm, W_Hollow, W_Glass};
        const float cpos = clampf(m.color, 0.0f, 1.0f) * 2.999f;
        const int t0 = (int)cpos; const float tf = cpos - (float)t0;
        const float ratio = 0.5f + std::floor(clampf(m.body, 0.0f, 1.0f) * 3.999f) * 0.5f; // 0.5,1,1.5,2
        s.aa.set(in.sr * 0.45f, 0.6f, in.sr * 2.0f);
        s.post.set(clampf(in.freq * 3.0f + 9000.0f * (0.3f + 0.7f * (1.0f - tim * 0.5f)), 200.0f, 16000.0f), 0.7f, in.sr);
        for (int i = 0; i < in.n; ++i) {
            const float e = in.env[i], k = in.strike[i];
            const float lfo = s.lfo.sine(0.21f, in.sr);
            float drive = 0.35f + tim * 5.0f * (0.45f + 0.55f * e) + 0.8f * k * tim;
            drive *= 1.0f + 0.3f * m.motion * lfo;
            float acc = 0.0f;
            for (int os = 0; os < 2; ++os) {
                s.p1 = WaveBank::wrap(s.p1 + fn);
                s.p2 = WaveBank::wrap(s.p2 + fn * ratio * (1.0f + 0.002f * m.motion));
                const float x = (waves.read(W_Sine, s.p1, fn) + 0.6f * waves.read(W_Warm, s.p2, fn * ratio)) * drive;
                // the wave is used as a periodic transfer function: endless folding
                const float tp = WaveBank::wrap(x * 0.125f);
                const float bw = 0.5f / 64.0f;
                float y = waves.read(transfers[t0], tp, bw);
                if (tf > 0.001f) y += (waves.read(transfers[t0 + 1], tp, bw) - y) * tf;
                acc = s.aa.lp(y);
            }
            out[i] = s.post.lp(acc) * 0.9f;
        }
    }
private:
    struct St { float p1 = 0, p2 = 0; Svf aa, post; Lfo lfo; };
    std::array<St, kMaxVoices> st{};
};

// ---------------------------------------------------------------- Bell
class BellEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override {
        auto& s = st[v];
        for (auto& r : s.r) r.reset();
        s.ph = 0; s.cnt = 0; s.nz.seed(1234u + 77u * v);
        for (int k = 0; k < 4; ++k) s.l[k].ph = 0.21f * k + 0.05f * v;
    }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.idx];
        static const float sets[4][4] = {
            {1.0f, 2.76f, 5.40f, 8.93f},   // bell / bar
            {1.0f, 2.32f, 4.25f, 6.63f},   // glass
            {1.0f, 2.0f,  3.0f,  4.0f},    // harmonic (soft mallet)
            {1.0f, 1.51f, 2.27f, 3.41f},   // gong-ish
        };
        const float cpos = clampf(m.color, 0.0f, 1.0f) * 2.999f;
        const int c0 = (int)cpos; const float cf = cpos - (float)c0;
        const float decay = 0.25f + m.body * m.body * 7.0f;
        const float tim = clampf(m.timbre, 0.0f, 1.0f);
        static const float amps[4] = {1.0f, 0.6f, 0.38f, 0.22f};
        for (int i = 0; i < in.n; ++i) {
            if ((s.cnt++ & 15) == 0) {
                for (int k = 0; k < 4; ++k) {
                    const float ratio = lerpf(sets[c0][k], sets[c0 + 1][k], cf);
                    const float shimmer = 1.0f + 0.004f * m.motion * s.l[k].sine(0.3f + 0.17f * k, in.sr / 16.0f);
                    s.r[k].set(in.freq * ratio * shimmer, decay / std::pow((float)(k + 1), 0.8f), in.sr);
                }
            }
            const float k = in.strike[i];
            s.ph = WaveBank::wrap(s.ph + in.freq * 3.7f / in.sr);
            const float click = waves.read(W_Sine, WaveBank::wrap(s.ph + 0.4f * tim * waves.read(W_Sine, s.ph, 0.0f)), 0.0f);
            const float ex = k * k * k * (click * (0.3f + 0.7f * tim) + s.nz.next() * 0.5f * tim);
            float y = 0.0f;
            for (int p = 0; p < 4; ++p) y += s.r[p].process(ex) * amps[p];
            out[i] = y * 14.0f;
        }
    }
private:
    struct St { Resonator r[4]; float ph = 0; unsigned cnt = 0; Noise nz; Lfo l[4]; };
    std::array<St, kMaxVoices> st{};
};

// ---------------------------------------------------------------- Choir
class ChoirEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override {
        auto& s = st[v];
        for (auto& f : s.f) f.reset();
        s.p1 = 0; s.p2 = 0.5f; s.t = 0; s.cnt = 0; s.nz.seed(999u + 31u * v);
        s.vib.ph = 0.11f * v; s.drift.ph = 0.23f * v;
    }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.idx];
        // Formant centre frequencies (Hz), classic published male-vowel averages: u o a e i
        static const float F[5][3] = {
            {300, 870, 2240}, {570, 840, 2410}, {730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}};
        static const float A[3] = {1.0f, 0.55f, 0.32f};
        static const float Q[3] = {7.0f, 9.0f, 12.0f};
        const float tim = clampf(m.timbre, 0.0f, 1.0f);
        const float scale = 0.8f + 0.5f * clampf(m.body, 0.0f, 1.0f);
        for (int i = 0; i < in.n; ++i) {
            s.t += 1.0f / in.sr;
            const float vibAmt = m.motion * 0.006f * clampf(s.t * 2.5f - 0.3f, 0.0f, 1.0f);
            const float vib = 1.0f + vibAmt * s.vib.sine(5.3f, in.sr);
            const float fn = in.freq * vib / in.sr;
            s.p1 = WaveBank::wrap(s.p1 + fn);
            s.p2 = WaveBank::wrap(s.p2 + fn * 2.0f * 1.0015f);
            float src = waves.read(W_Saw, s.p1, fn) * 0.8f + waves.read(W_Saw, s.p2, fn * 2.0f) * 0.2f;
            const float breath = s.nz.next() * (0.15f + 0.6f * tim);
            src = src * (1.0f - 0.45f * tim) + breath * (0.4f + 0.4f * in.env[i]);
            if ((s.cnt++ & 15) == 0) {
                const float dr = 0.08f * m.motion * s.drift.sine(0.13f, in.sr / 16.0f);
                const float vpos = clampf(m.color + dr, 0.0f, 1.0f) * 3.999f;
                const int v0 = (int)vpos; const float vf = vpos - (float)v0;
                for (int k = 0; k < 3; ++k)
                    s.f[k].set(lerpf(F[v0][k], F[v0 + 1][k], vf) * scale, Q[k] * (0.7f + 0.6f * tim), in.sr);
            }
            float y = 0.0f;
            for (int k = 0; k < 3; ++k) y += s.f[k].bp(src) * A[k];
            out[i] = y * 3.4f;
        }
    }
private:
    struct St { Svf f[3]; float p1 = 0, p2 = 0, t = 0; unsigned cnt = 0; Noise nz; Lfo vib, drift; };
    std::array<St, kMaxVoices> st{};
};

// ---------------------------------------------------------------- Breath
class BreathEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override {
        auto& s = st[v];
        for (auto& r : s.r) r.reset();
        s.tone.reset(); s.cnt = 0; s.nz.seed(4242u + 13u * v);
        for (int k = 0; k < 4; ++k) s.fl[k].ph = 0.29f * k + 0.07f * v;
    }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.idx];
        const float tim = clampf(m.timbre, 0.0f, 1.0f);
        const float q = 3.0f + tim * tim * 220.0f;
        const float stretch = clampf(m.color, 0.0f, 1.0f) * 0.035f;
        const float tilt = 2.2f - 1.6f * clampf(m.body, 0.0f, 1.0f);
        const float norm = 0.55f * std::sqrt(q) / (1.0f + tim * 2.0f);
        s.tone.set(700.0f + m.body * 9000.0f, 0.6f, in.sr);
        static const float rates[4] = {0.7f, 1.13f, 1.71f, 2.3f};
        for (int i = 0; i < in.n; ++i) {
            if ((s.cnt++ & 15) == 0) {
                for (int k = 0; k < 4; ++k) {
                    const float kk = (float)(k + 1);
                    s.r[k].set(in.freq * kk * (1.0f + stretch * (kk - 1.0f) * (kk - 1.0f)), q, in.sr);
                    s.amp[k] = std::pow(kk, -tilt) * (1.0f + 0.6f * m.motion * s.fl[k].sine(rates[k], in.sr / 16.0f));
                }
            }
            const float n = s.tone.lp(s.nz.next());
            float y = 0.0f;
            for (int k = 0; k < 4; ++k) y += s.r[k].bp(n) * s.amp[k];
            out[i] = y * norm * 14.0f * (1.0f + 0.5f * in.strike[i]);
        }
    }
private:
    struct St { Svf r[4]; Svf tone; unsigned cnt = 0; Noise nz; Lfo fl[4]; float amp[4] = {1, 0.5f, 0.3f, 0.2f}; };
    std::array<St, kMaxVoices> st{};
};

} // namespace ostrov
