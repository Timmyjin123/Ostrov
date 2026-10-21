// Ostrov — synth core: voices, tuning, engines, tone and space.
#pragma once
#include "Engines.h"
#include "Effects.h"
#include "Tuning.h"
#include <memory>
#include <vector>

namespace ostrov {

enum EngineId { E_Bloom = 0, E_Drift, E_Fold, E_Bell, E_Choir, E_Breath, E_Count };
inline const char* engineName(int e) {
    static const char* n[] = {"Bloom", "Drift", "Fold", "Bell", "Choir", "Breath"};
    return (e >= 0 && e < E_Count) ? n[e] : "?";
}

struct Params {
    int engine = E_Bloom;
    Macros macros;
    float attack = 0.02f;     // seconds
    float release = 1.2f;     // seconds
    float drift = 0.3f;       // 0..1 slow per-voice pitch drift
    int spaceType = 0;        // 0 plate, 1 tape, 2 tape->plate
    float spaceMix = 0.35f;
    float spaceSize = 0.6f;
    float volumeDb = -6.0f;
    int root = 0;             // 0 = C
    uint16_t scaleMask = 0x0FFF;
    bool scaleLock = true;
    TuningMode tuning = TuningMode::JustAdaptive;
    float aftertouchAmt = 0.6f;
};

class Synth {
public:
    void prepare(float sampleRate, int maxBlock) {
        sr = sampleRate;
        if (!wavesBuilt) { waves.build(); wavesBuilt = true; }
        engines.clear();
        engines.emplace_back(std::make_unique<BloomEngine>(waves));
        engines.emplace_back(std::make_unique<DriftEngine>(waves));
        engines.emplace_back(std::make_unique<FoldEngine>(waves));
        engines.emplace_back(std::make_unique<BellEngine>(waves));
        engines.emplace_back(std::make_unique<ChoirEngine>(waves));
        engines.emplace_back(std::make_unique<BreathEngine>(waves));
        plate.prepare(sr); tape.prepare(sr);
        (void)maxBlock;
        for (int v = 0; v < kMaxVoices; ++v) {
            voices[v] = {};
            voices[v].idx = v;
            voices[v].lfo[0].ph = 0.13f * v; voices[v].lfo[1].ph = 0.41f + 0.07f * v; voices[v].lfo[2].ph = 0.77f + 0.03f * v;
        }
        for (auto& e : engines) for (int v = 0; v < kMaxVoices; ++v) e->reset(v);
        currentEngine = -1;
    }

    void setParams(const Params& p) { params = p; }
    const Params& getParams() const { return params; }

    // MIDI -------------------------------------------------------------
    void noteOn(int midi, float velocity) {
        const int note = params.scaleLock ? quantizeToScale(midi, params.root, params.scaleMask) : midi;
        int v = findVoice();
        Voice& vo = voices[v];
        const bool wasSilent = !vo.active;
        vo.active = true; vo.gate = true; vo.key = midi; vo.note = note;
        vo.vel = velocity; vo.strike = velocity; vo.press = 0.0f; vo.age = ++ageCounter;
        if (wasSilent) { vo.env = 0.0f; vo.cents = targetCents(vo); }
        if (currentEngine >= 0) engines[(size_t)currentEngine]->reset(v);
    }
    void noteOff(int midi) {
        for (auto& vo : voices) if (vo.active && vo.gate && vo.key == midi) vo.gate = false;
    }
    void allNotesOff() { for (auto& vo : voices) vo.gate = false; }
    void polyPressure(int midi, float p) { for (auto& vo : voices) if (vo.gate && vo.key == midi) vo.press = p; }
    void channelPressure(float p) { for (auto& vo : voices) if (vo.gate) vo.press = p; }
    void pitchBend(float semis) { bend = semis; }

    // Audio ------------------------------------------------------------
    void render(float* outL, float* outR, int n) {
        if (params.engine != currentEngine) {
            currentEngine = clampEngine(params.engine);
            for (int v = 0; v < kMaxVoices; ++v) engines[(size_t)currentEngine]->reset(v);
        }
        int done = 0;
        while (done < n) {
            const int chunk = std::min(kChunk, n - done);
            renderChunk(outL + done, outR + done, chunk);
            done += chunk;
        }
    }

    int activeVoices() const { int c = 0; for (auto& v : voices) c += v.active ? 1 : 0; return c; }
    // pitch class used as just-intonation reference (for UI)
    int referencePitchClass() const { return refPc; }

private:
    static constexpr int kChunk = 32;
    struct Voice {
        bool active = false, gate = false;
        int key = 60, note = 60, idx = 0;
        float vel = 0.8f, env = 0.0f, strike = 0.0f, press = 0.0f, pressSm = 0.0f, cents = 0.0f;
        uint32_t age = 0;
        Lfo lfo[3];
    };

    float sr = 48000.0f, bend = 0.0f;
    bool wavesBuilt = false;
    WaveBank waves;
    std::vector<std::unique_ptr<Engine>> engines;
    int currentEngine = -1;
    Params params;
    Voice voices[kMaxVoices];
    uint32_t ageCounter = 0;
    int refPc = 0;
    Macros mSm; bool mInit = false;
    float mixSm = 0.35f, sizeSm = 0.6f, volSm = 0.5f;
    Plate plate; TapeDelay tape;
    DcBlock dcL, dcR;
    float envBuf[kChunk], strikeBuf[kChunk], voiceBuf[kChunk];

    static int clampEngine(int e) { return e < 0 ? 0 : (e >= E_Count ? E_Count - 1 : e); }

    int findVoice() {
        for (int v = 0; v < kMaxVoices; ++v) if (!voices[v].active) return v;
        int best = -1; uint32_t bestAge = 0xFFFFFFFFu;
        for (int v = 0; v < kMaxVoices; ++v)   // oldest released voice first
            if (!voices[v].gate && voices[v].age < bestAge) { best = v; bestAge = voices[v].age; }
        if (best >= 0) return best;
        for (int v = 0; v < kMaxVoices; ++v)
            if (voices[v].age < bestAge) { best = v; bestAge = voices[v].age; }
        return best;
    }

    float targetCents(const Voice& vo) const {
        switch (params.tuning) {
            case TuningMode::Equal: return 0.0f;
            case TuningMode::JustRoot: return justCents(vo.note - params.root);
            case TuningMode::JustAdaptive: return justCents(vo.note - refPc);
        }
        return 0.0f;
    }

    void updateReference() {
        if (params.tuning == TuningMode::JustRoot) { refPc = params.root; return; }
        int lowest = 1000;
        for (auto& vo : voices) if (vo.active && vo.gate && vo.note < lowest) lowest = vo.note;
        if (lowest < 1000) refPc = ((lowest % 12) + 12) % 12;
    }

    void renderChunk(float* L, float* R, int n) {
        for (int i = 0; i < n; ++i) { L[i] = 0.0f; R[i] = 0.0f; }
        // smooth macros per chunk
        const float mc = onePoleCoeff(0.03f, sr / (float)kChunk);
        if (!mInit) { mSm = params.macros; mInit = true; }
        mSm.timbre += (params.macros.timbre - mSm.timbre) * mc;
        mSm.color  += (params.macros.color  - mSm.color)  * mc;
        mSm.motion += (params.macros.motion - mSm.motion) * mc;
        mSm.body   += (params.macros.body   - mSm.body)   * mc;
        updateReference();

        Engine& eng = *engines[(size_t)currentEngine];
        const float aC = onePoleCoeff(std::max(params.attack, 0.001f) * 0.4f, sr);
        const float rC = onePoleCoeff(std::max(params.release, 0.01f) * 0.3f, sr);
        const float sC = onePoleCoeff(0.22f, sr);
        const float centsC = onePoleCoeff(params.tuning == TuningMode::JustAdaptive ? 0.08f : 0.01f, sr / (float)kChunk);
        const float lfoSr = sr / (float)kChunk;

        for (auto& vo : voices) {
            if (!vo.active) continue;
            vo.cents += (targetCents(vo) - vo.cents) * centsC;
            const float drift = params.drift * (5.0f * vo.lfo[0].sine(0.071f + 0.011f * vo.idx, lfoSr)
                                              + 3.0f * vo.lfo[1].sine(0.13f + 0.017f * vo.idx, lfoSr)
                                              + 2.0f * vo.lfo[2].sine(0.23f, lfoSr)) ;
            const float freq = noteToHz((float)vo.note + bend + (vo.cents + drift) * 0.01f);
            for (int i = 0; i < n; ++i) {
                const float target = vo.gate ? 1.0f : 0.0f;
                vo.env += (target - vo.env) * (target > vo.env ? aC : rC);
                vo.strike -= vo.strike * sC;
                vo.pressSm += (vo.press - vo.pressSm) * 0.002f;
                envBuf[i] = vo.env;
                strikeBuf[i] = vo.strike + vo.pressSm * params.aftertouchAmt;
            }
            VoiceIn in; in.freq = freq; in.sr = sr; in.env = envBuf; in.strike = strikeBuf;
            in.vel = vo.vel; in.idx = vo.idx; in.n = n;
            eng.render(in, mSm, voiceBuf);
            // stereo spread by voice index, collapsing to centre at low level
            const float spread = 0.2f + 0.06f * (float)vo.idx;
            for (int i = 0; i < n; ++i) {
                const float e = envBuf[i];
                const float pan = 0.5f + (spread - 0.5f) * std::min(1.0f, e * 3.0f);
                const float g = e * (0.55f + 0.45f * vo.vel) * 0.28f;
                const float s = voiceBuf[i] * g;
                L[i] += s * std::sqrt(1.0f - pan);
                R[i] += s * std::sqrt(pan);
            }
            if (!vo.gate && vo.env < 1e-4f) vo.active = false;
        }

        // tone: subtle asymmetric warmth + soft limiting
        const float pc = onePoleCoeff(0.05f, sr / (float)kChunk);
        mixSm += (params.spaceMix - mixSm) * pc;
        sizeSm += (params.spaceSize - sizeSm) * pc;
        volSm += (std::pow(10.0f, params.volumeDb / 20.0f) - volSm) * pc;
        for (int i = 0; i < n; ++i) {
            float l = dcL.process(asymSat(L[i] * 1.2f, 0.06f));
            float r = dcR.process(asymSat(R[i] * 1.2f, 0.06f));
            l = fastTanh(l); r = fastTanh(r);
            float wl = 0, wr = 0;
            if (params.spaceType == 0) {
                plate.process(l, r, sizeSm, wl, wr);
            } else if (params.spaceType == 1) {
                tape.process(l, r, sizeSm, wl, wr);
            } else {
                float tl, tr;
                tape.process(l, r, sizeSm * 0.8f, tl, tr);
                plate.process(l * 0.5f + tl, r * 0.5f + tr, sizeSm, wl, wr);
            }
            L[i] = (l + wl * mixSm * 1.2f) * volSm;
            R[i] = (r + wr * mixSm * 1.2f) * volSm;
        }
    }
};

} // namespace ostrov
