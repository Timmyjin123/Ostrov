// Ostrov — synthesis engines (v0.2).
// Each engine renders one layer of one voice (mono). The core calls an engine twice per note
// (two detuned layers) when Width > 0, so engine state is kept per "slot" (voice + layer).
//
//  Bloom  — PM where operators are soft single-cycle waves; modulator self-feedback, wave scan
//  Drift  — three tri↔saw↔square VCOs + sub into a warm ladder, slow drift on non-integer LFOs
//  Fold   — oscillators through a wave used as a transfer function, with bias (even harmonics)
//  Bell   — exciter + optional bowing into six modal resonators
//  Choir  — three-voice ensemble into four formant filters, vowel morph
//  Breath — noise into six tuned resonators plus a soft fundamental
//  Glass  — two 4-operator PM stacks with a morph between them
//  Tide   — three wavetable scanners drifting through all waves (evolving pads)
#pragma once
#include "Blocks.h"
#include "Wavetables.h"
#include <array>

namespace ostrov {

constexpr int kMaxVoices = 12;
constexpr int kMaxSlots = kMaxVoices * 2;

struct Macros { float timbre = 0.5f, color = 0.5f, motion = 0.3f, body = 0.5f; };

struct VoiceIn {
    float freq = 220.0f;
    float sr = 48000.0f;
    const float* env = nullptr;     // amplitude envelope 0..1 (applied by the core)
    const float* strike = nullptr;  // velocity transient + pressure, 0..~1.5
    float vel = 0.8f;
    int slot = 0;                   // voice index + 12 * layer
    int voice = 0;                  // voice index 0..11
    int n = 0;
};

class Engine {
public:
    explicit Engine(const WaveBank& w) : waves(w) {}
    virtual ~Engine() = default;
    virtual void reset(int slot) = 0;
    virtual void render(const VoiceIn& in, const Macros& m, float* out) = 0;
protected:
    const WaveBank& waves;
    // continuous scan across a list of waves
    float scan(const int* list, int count, float pos, float phase, float fn) const {
        pos = clampf(pos, 0.0f, 1.0f) * (float)(count - 1) * 0.9999f;
        const int a = (int)pos; const float f = pos - (float)a;
        const float x = waves.read(list[a], phase, fn);
        return f > 0.001f ? x + (waves.read(list[a + 1], phase, fn) - x) * f : x;
    }
};

// ---------------------------------------------------------------- Bloom
class BloomEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override { st[v] = {}; st[v].lfo.ph = 0.137f * v; }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.slot];
        const float fn = in.freq / in.sr;
        static const float ratios[6][2] = {{1.0f, 1.0f}, {1.0f, 2.0f}, {1.0f, 3.0f}, {2.0f, 3.0f}, {0.5f, 3.5f}, {1.41f, 2.83f}};
        const float rpos = clampf(m.body, 0.0f, 1.0f) * 5.999f;
        const int rb = (int)rpos;
        const float r1 = ratios[rb][0], r2 = ratios[rb][1];
        static const int carriers[6] = {W_Warm, W_Reed, W_Soft, W_Organ, W_Hollow, W_Glass};
        const float tim = std::pow(clampf(m.timbre, 0.0f, 1.0f), 1.25f);
        const float fb1 = 0.1f + 0.45f * tim;
        for (int i = 0; i < in.n; ++i) {
            const float e = in.env[i], k = in.strike[i];
            const float I = tim * 2.6f * (0.3f + 0.7f * e) + k * tim * 1.4f;   // index in cycles
            s.pc = WaveBank::wrap(s.pc + fn);
            s.p1 = WaveBank::wrap(s.p1 + fn * r1);
            s.p2 = WaveBank::wrap(s.p2 + fn * r2);
            const float m1 = waves.read(W_Sine, WaveBank::wrap(s.p1 + fb1 * s.m1z * 0.25f), fn * r1 * 2.0f);
            s.m1z = 0.5f * (s.m1z + m1);
            const float m2 = waves.read(W_Warm, WaveBank::wrap(s.p2 + m1 * I * 0.22f), fn * r2 * (1.0f + I));
            const float pm = I * (0.26f * m2 + 0.12f * m1) + 0.1f * m.motion * s.fb;
            const float scanPos = m.color + 0.3f * m.motion * s.lfo.sine(0.09f + 0.013f * in.voice, in.sr);
            const float car = scan(carriers, 6, scanPos, WaveBank::wrap(s.pc + pm), fn * (1.0f + I * 4.0f));
            s.fb += (car - s.fb) * 0.4f;
            out[i] = car;
        }
    }
private:
    struct St { float pc = 0, p1 = 0, p2 = 0, fb = 0, m1z = 0; Lfo lfo; };
    std::array<St, kMaxSlots> st{};
};

// ---------------------------------------------------------------- Drift
class DriftEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override {
        auto& s = st[v];
        s.f.reset(); s.p[0] = 0; s.p[1] = 0.31f; s.p[2] = 0.67f; s.sub = 0.0f; s.nz.seed(55u + 3u * v);
        for (int k = 0; k < 4; ++k) s.l[k].ph = 0.1f * v + 0.23f * k;
    }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.slot];
        const float base = 0.31f * (1.0f + 0.07f * in.voice);
        const float morph = clampf(m.color, 0.0f, 1.0f) * 1.999f;   // tri -> saw -> square
        const int w0 = morph < 1.0f ? W_Tri : W_Saw;
        const int w1 = morph < 1.0f ? W_Saw : W_Square;
        const float wf = morph - std::floor(morph);
        const float tim = clampf(m.timbre, 0.0f, 1.0f);
        for (int i = 0; i < in.n; ++i) {
            const float l0 = s.l[0].sine(base, in.sr), l1 = s.l[1].sine(base * 0.946f, in.sr);
            const float l2 = s.l[2].sine(base * 0.56f, in.sr), l3 = s.l[3].sine(base * 0.405f, in.sr);
            const float driftC = m.motion * (5.0f * l0 + 3.0f * l2);
            const float det = 3.0f + m.motion * 12.0f * (0.75f + 0.25f * l1);
            float x = 0.0f;
            for (int o = 0; o < 3; ++o) {
                const float c = driftC + (o == 0 ? 0.0f : (o == 1 ? det : -det));
                const float fn = in.freq * std::exp2(c / 1200.0f) / in.sr;
                s.p[o] = WaveBank::wrap(s.p[o] + fn);
                const float a = waves.read(w0, s.p[o], fn);
                x += a + (waves.read(w1, s.p[o], fn) - a) * wf;
            }
            const float fnSub = in.freq * 0.5f / in.sr;
            s.sub = WaveBank::wrap(s.sub + fnSub);
            x = x * 0.36f + 0.32f * waves.read(W_Sine, s.sub, fnSub) + 0.015f * s.nz.next();
            if ((i & 7) == 0) {
                const float k = in.strike[i], e = in.env[i];
                const float fc = in.freq * (1.0f + tim * tim * 90.0f) * (1.0f + 2.2f * k * tim + 0.3f * e)
                                 * (1.0f + 0.3f * m.motion * l3) + 40.0f;
                s.f.set(fc, m.body * 0.95f, in.sr);
            }
            out[i] = s.f.process(x * 1.1f) * 2.6f;
        }
    }
private:
    struct St { float p[3] = {0, 0, 0}; float sub = 0; WarmLadder f; Lfo l[4]; Noise nz; };
    std::array<St, kMaxSlots> st{};
};

// ---------------------------------------------------------------- Fold
class FoldEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override { auto& s = st[v]; s = {}; s.aa.reset(); s.post.reset(); s.lfo.ph = 0.13f * v; s.lfo2.ph = 0.61f + 0.07f * v; }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.slot];
        const float fn = in.freq / in.sr * 0.5f;        // 2x oversampled
        const float tim = std::pow(clampf(m.timbre, 0.0f, 1.0f), 1.2f);
        static const int transfers[5] = {W_Sine, W_Warm, W_Hollow, W_Organ, W_Glass};
        const float bias = (clampf(m.body, 0.0f, 1.0f) - 0.5f) * 0.9f;
        s.aa.set(in.sr * 0.42f, 0.6f, in.sr * 2.0f);
        s.post.set(clampf(in.freq * 6.0f + 6000.0f + 8000.0f * tim, 300.0f, 18000.0f), 0.6f, in.sr);
        for (int i = 0; i < in.n; ++i) {
            const float e = in.env[i], k = in.strike[i];
            const float lfo = s.lfo.sine(0.17f + 0.02f * in.voice, in.sr);
            const float lfo2 = s.lfo2.sine(0.071f, in.sr);
            float drive = 0.4f + tim * 8.0f * (0.4f + 0.6f * e) + 1.5f * k * tim;
            drive *= 1.0f + 0.35f * m.motion * lfo;
            const float shapePos = m.color + 0.15f * m.motion * lfo2;
            float acc = 0.0f;
            for (int os = 0; os < 2; ++os) {
                s.p1 = WaveBank::wrap(s.p1 + fn);
                s.p2 = WaveBank::wrap(s.p2 + fn * 0.5f * (1.0f + 0.0015f * m.motion));
                const float x = (waves.read(W_Sine, s.p1, fn) + 0.55f * waves.read(W_Warm, s.p2, fn * 0.5f)) * drive + bias * drive * 0.5f;
                const float y = scan(transfers, 5, shapePos, WaveBank::wrap(x * 0.125f), 1.0f / 128.0f);
                acc = s.aa.lp(y);
            }
            out[i] = s.dc.process(s.post.lp(acc)) * 0.95f;
        }
    }
private:
    struct St { float p1 = 0, p2 = 0; Svf aa, post; Lfo lfo, lfo2; DcBlock dc; };
    std::array<St, kMaxSlots> st{};
};

// ---------------------------------------------------------------- Bell
class BellEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override {
        auto& s = st[v];
        for (auto& r : s.r) r.reset();
        s.ph = 0; s.cnt = 0; s.nz.seed(1234u + 77u * v); s.bow.reset();
        for (int k = 0; k < 6; ++k) s.l[k].ph = 0.21f * k + 0.05f * v;
    }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.slot];
        static const float sets[4][6] = {
            {1.0f, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f},  // bar / bell
            {1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 12.53f},   // glass bowl
            {1.0f, 2.0f,  3.0f,  4.0f,  5.0f,  6.0f},     // harmonic mallet
            {1.0f, 1.51f, 2.27f, 3.41f, 4.56f, 5.89f},    // gong
        };
        const float cpos = clampf(m.color, 0.0f, 1.0f) * 2.999f;
        const int c0 = (int)cpos; const float cf = cpos - (float)c0;
        const float decay = 0.3f + m.body * m.body * 9.0f;
        const float tim = clampf(m.timbre, 0.0f, 1.0f);
        const float tilt = 1.3f - tim;   // brighter strike -> more upper partials
        if (s.cnt == 0) s.bow.set(in.freq * 2.0f, 0.7f, in.sr);
        for (int i = 0; i < in.n; ++i) {
            if ((s.cnt++ & 15) == 0) {
                for (int k = 0; k < 6; ++k) {
                    const float ratio = lerpf(sets[c0][k], sets[c0 + 1][k], cf);
                    const float shimmer = 1.0f + 0.005f * m.motion * s.l[k].sine(0.25f + 0.17f * k, in.sr / 16.0f);
                    s.r[k].set(in.freq * ratio * shimmer, decay / std::pow((float)(k + 1), 0.7f), in.sr);
                    s.amp[k] = std::pow((float)(k + 1), -tilt);
                }
            }
            const float k = in.strike[i], e = in.env[i];
            s.ph = WaveBank::wrap(s.ph + in.freq * 3.7f / in.sr);
            const float click = waves.read(W_Sine, WaveBank::wrap(s.ph + 0.5f * tim * waves.read(W_Sine, s.ph, 0.0f)), 0.0f);
            const float n = s.nz.next();
            float ex = k * k * k * (click * (0.4f + 0.6f * tim) + n * 0.6f * tim);
            // bowing: continuous friction noise while held, grows with timbre
            ex += s.bow.bp(n) * e * tim * tim * 0.05f;
            float y = 0.0f;
            for (int p = 0; p < 6; ++p) y += s.r[p].process(ex) * s.amp[p];
            out[i] = y * 22.0f;
        }
    }
private:
    struct St { Resonator r[6]; float amp[6] = {1, .6f, .4f, .3f, .2f, .1f}; float ph = 0; unsigned cnt = 0; Noise nz; Lfo l[6]; Svf bow; };
    std::array<St, kMaxSlots> st{};
};

// ---------------------------------------------------------------- Choir
class ChoirEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override {
        auto& s = st[v];
        for (auto& f : s.f) f.reset();
        for (int k = 0; k < 3; ++k) { s.p[k] = 0.31f * k; s.vib[k].ph = 0.11f * v + 0.29f * k; }
        s.t = 0; s.cnt = 0; s.nz.seed(999u + 31u * v); s.drift.ph = 0.23f * v;
    }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.slot];
        // formant centres (Hz), published average male vowels: u o a e i (+ F4)
        static const float F[5][4] = {
            {300, 870, 2240, 3300}, {570, 840, 2410, 3350}, {730, 1090, 2440, 3400},
            {530, 1840, 2480, 3500}, {270, 2290, 3010, 3600}};
        static const float A[4] = {1.0f, 0.6f, 0.36f, 0.2f};
        static const float Q[4] = {6.0f, 8.0f, 11.0f, 13.0f};
        static const float detC[3] = {0.0f, 1.0f, -1.0f};
        static const float vibR[3] = {5.3f, 4.9f, 5.7f};
        const float tim = clampf(m.timbre, 0.0f, 1.0f);
        const float scale = 0.78f + 0.55f * clampf(m.body, 0.0f, 1.0f);
        const float spread = 4.0f + 10.0f * m.motion;   // ensemble detune in cents
        for (int i = 0; i < in.n; ++i) {
            s.t += 1.0f / in.sr;
            const float vibAmt = (0.002f + m.motion * 0.006f) * clampf(s.t * 2.0f - 0.2f, 0.0f, 1.0f);
            float src = 0.0f;
            for (int k = 0; k < 3; ++k) {
                const float vib = 1.0f + vibAmt * s.vib[k].sine(vibR[k], in.sr);
                const float fn = in.freq * vib * std::exp2(detC[k] * spread / 1200.0f) / in.sr;
                s.p[k] = WaveBank::wrap(s.p[k] + fn);
                src += waves.read(k == 0 ? W_Saw : W_Bright, s.p[k], fn);
            }
            src *= 0.42f;
            const float breath = s.nz.next() * (0.08f + 0.5f * tim);
            src = src * (1.0f - 0.35f * tim) + breath * (0.5f + 0.5f * in.env[i]);
            if ((s.cnt++ & 15) == 0) {
                const float dr = 0.1f * m.motion * s.drift.sine(0.11f, in.sr / 16.0f);
                const float vpos = clampf(m.color + dr, 0.0f, 1.0f) * 3.999f;
                const int v0 = (int)vpos; const float vf = vpos - (float)v0;
                for (int k = 0; k < 4; ++k)
                    s.f[k].set(lerpf(F[v0][k], F[v0 + 1][k], vf) * scale, Q[k] * (0.75f + 0.5f * tim), in.sr);
            }
            float y = 0.0f;
            for (int k = 0; k < 4; ++k) y += s.f[k].bp(src) * A[k];
            out[i] = y * 3.6f;
        }
    }
private:
    struct St { Svf f[4]; float p[3] = {0, 0, 0}; float t = 0; unsigned cnt = 0; Noise nz; Lfo vib[3], drift; };
    std::array<St, kMaxSlots> st{};
};

// ---------------------------------------------------------------- Breath
class BreathEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override {
        auto& s = st[v];
        for (auto& r : s.r) r.reset();
        s.tone.reset(); s.cnt = 0; s.nz.seed(4242u + 13u * v); s.ph = 0;
        for (int k = 0; k < 6; ++k) s.fl[k].ph = 0.29f * k + 0.07f * v;
    }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.slot];
        const float tim = clampf(m.timbre, 0.0f, 1.0f);
        const float q = 4.0f + tim * tim * 260.0f;
        const float stretch = clampf(m.color, 0.0f, 1.0f) * 0.03f;
        const float tilt = 2.0f - 1.4f * clampf(m.body, 0.0f, 1.0f);
        const float norm = 0.55f * std::sqrt(q) / (1.0f + tim * 2.0f);
        s.tone.set(900.0f + m.body * 10000.0f, 0.6f, in.sr);
        static const float rates[6] = {0.7f, 1.13f, 1.71f, 2.3f, 2.9f, 3.7f};
        const float fn = in.freq / in.sr;
        for (int i = 0; i < in.n; ++i) {
            if ((s.cnt++ & 15) == 0) {
                for (int k = 0; k < 6; ++k) {
                    const float kk = (float)(k + 1);
                    s.r[k].set(in.freq * kk * (1.0f + stretch * (kk - 1.0f) * (kk - 1.0f)), q, in.sr);
                    s.amp[k] = std::pow(kk, -tilt) * (1.0f + 0.7f * m.motion * s.fl[k].sine(rates[k], in.sr / 16.0f));
                }
            }
            const float n = s.tone.lp(s.nz.next());
            float y = 0.0f;
            for (int k = 0; k < 6; ++k) y += s.r[k].bp(n) * s.amp[k];
            s.ph = WaveBank::wrap(s.ph + fn);
            const float fund = waves.read(W_Warm, s.ph, fn) * 0.12f * tim;
            out[i] = (y * norm * 14.0f + fund * 6.0f) * (1.0f + 0.5f * in.strike[i]);
        }
    }
private:
    struct St { Svf r[6]; Svf tone; unsigned cnt = 0; Noise nz; Lfo fl[6]; float amp[6] = {1, .5f, .3f, .2f, .15f, .1f}; float ph = 0; };
    std::array<St, kMaxSlots> st{};
};

// ---------------------------------------------------------------- Glass (dual 4-op PM stacks with morph)
class GlassEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override { st[v] = {}; st[v].lfo.ph = 0.19f * v; }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.slot];
        static const float setsA[3][4] = {{1, 2, 3, 4}, {1, 1, 2, 5}, {1, 0.5f, 1.5f, 2}};
        static const float setsB[3][4] = {{1, 3.5f, 5.1f, 7.2f}, {1, 2.76f, 4.1f, 6.3f}, {1, 1.41f, 2.83f, 4.24f}};
        const int si = std::min(2, (int)(clampf(m.body, 0.0f, 1.0f) * 3.0f));
        const float tim = std::pow(clampf(m.timbre, 0.0f, 1.0f), 1.2f);
        const float fn = in.freq / in.sr;
        for (int i = 0; i < in.n; ++i) {
            const float e = in.env[i], k = in.strike[i];
            const float I = tim * 1.25f * (0.35f + 0.65f * e) + k * tim * 0.6f;
            const float morph = clampf(m.color + 0.35f * m.motion * s.lfo.sine(0.07f + 0.01f * in.voice, in.sr), 0.0f, 1.0f);
            auto stack = [&](const float* r, float* ph) {
                float mod = 0.0f;
                for (int o = 3; o >= 0; --o) {
                    ph[o] = WaveBank::wrap(ph[o] + fn * r[o]);
                    const float a = o == 0 ? 1.0f : I * (o == 3 ? 0.5f : 0.35f);
                    mod = waves.read(W_Sine, WaveBank::wrap(ph[o] + mod), fn * r[o] * (1.0f + I * 3.0f)) * a;
                }
                return mod;
            };
            const float a = stack(setsA[si], s.pa);
            const float b = stack(setsB[si], s.pb);
            out[i] = (a * (1.0f - morph) + b * morph) * 0.9f;
        }
    }
private:
    struct St { float pa[4] = {0, 0, 0, 0}, pb[4] = {0, 0, 0, 0}; Lfo lfo; };
    std::array<St, kMaxSlots> st{};
};

// ---------------------------------------------------------------- Tide (wave scanners for evolving pads)
class TideEngine : public Engine {
public:
    using Engine::Engine;
    void reset(int v) override {
        auto& s = st[v]; s = {};
        for (int k = 0; k < 3; ++k) { s.p[k] = 0.27f * k; s.l[k].ph = 0.17f * v + 0.33f * k; }
        s.f.reset();
    }
    void render(const VoiceIn& in, const Macros& m, float* out) override {
        auto& s = st[in.slot];
        static const int list[9] = {W_Sine, W_Warm, W_Hollow, W_Soft, W_Reed, W_Organ, W_Glass, W_Bright, W_Saw};
        static const float mult[3] = {1.0f, 2.0f, 0.5f};
        static const float rate[3] = {0.043f, 0.031f, 0.057f};
        const float tim = clampf(m.timbre, 0.0f, 1.0f);
        const float layer = clampf(m.body, 0.0f, 1.0f);
        const float lvl[3] = {1.0f, 0.55f * layer, 0.6f * layer};
        for (int i = 0; i < in.n; ++i) {
            float y = 0.0f;
            for (int k = 0; k < 3; ++k) {
                if (lvl[k] < 0.001f) continue;
                const float fn = in.freq * mult[k] * (1.0f + 0.0008f * (float)k * m.motion) / in.sr;
                s.p[k] = WaveBank::wrap(s.p[k] + fn);
                const float pos = m.color + 0.45f * m.motion * s.l[k].tri(rate[k] * (1.0f + 0.1f * in.voice), in.sr);
                y += scan(list, 9, pos, s.p[k], fn) * lvl[k];
            }
            if ((s.cnt++ & 7) == 0)
                s.f.set(in.freq * (1.5f + 40.0f * tim * tim) * (1.0f + 0.8f * in.strike[i]) + 80.0f, 0.8f, in.sr);
            out[i] = s.f.lp(y * 0.6f) * 1.3f;
        }
    }
private:
    struct St { float p[3] = {0, 0, 0}; Lfo l[3]; Svf f; unsigned cnt = 0; };
    std::array<St, kMaxSlots> st{};
};

} // namespace ostrov
