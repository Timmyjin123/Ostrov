// Offline renderer: writes demo WAVs for every engine (no plugin host needed).
#include "../src/dsp/Synth.h"
#include "../src/dsp/Presets.h"
#include <cstdio>
#include <string>
#include <vector>
#include <xmmintrin.h>

using namespace ostrov;

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
    FILE* f = std::fopen(path.c_str(), "wb");
    const int n = (int)L.size();
    const int bytes = n * 4;
    auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(2); w32(sr); w32(sr * 4); w16(4); w16(16);
    std::fwrite("data", 1, 4, f); w32(bytes);
    for (int i = 0; i < n; ++i) {
        auto c = [](float x) { x = x > 1 ? 1 : (x < -1 ? -1 : x); return (int16_t)(x * 32767); };
        int16_t a = c(L[i]), b = c(R[i]);
        std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
    }
    std::fclose(f);
}

struct Ev { float t; int note; float vel; bool on; };

int main(int argc, char** argv) {
    _mm_setcsr(_mm_getcsr() | 0x8040); // FTZ/DAZ
    const int sr = 48000;
    const std::string outDir = argc > 1 ? argv[1] : ".";
    Synth syn;
    syn.prepare((float)sr, 512);

    // progression in D dorian; chords played as plain MIDI, scale lock + adaptive just intonation
    std::vector<Ev> evs;
    auto chord = [&](float t, float dur, std::initializer_list<int> notes, float vel) {
        float k = 0;
        for (int n : notes) { evs.push_back({t + k, n, vel, true}); evs.push_back({t + dur, n, 0, false}); k += 0.03f; }
    };
    chord(0.0f, 2.6f, {50, 57, 62, 65, 69}, 0.8f);
    chord(3.0f, 2.6f, {48, 55, 64, 67, 72}, 0.7f);
    chord(6.0f, 2.6f, {46, 53, 62, 65, 69}, 0.75f);
    // melody on top
    float t = 9.0f;
    for (int n : {74, 76, 77, 81, 79, 77, 76, 74}) { evs.push_back({t, n, 0.85f, true}); evs.push_back({t + 0.45f, n, 0, false}); t += 0.5f; }
    chord(9.0f, 4.0f, {50, 57, 62}, 0.6f);
    const float total = 15.5f;

    const std::string only = argc > 2 ? argv[2] : "";
    for (const auto& pr : factoryPresets()) {
        if (!only.empty() && only != pr.name) continue;
        syn.prepare((float)sr, 512);
        Params p;
        p.engine = pr.engine; p.macros = {pr.timbre, pr.color, pr.motion, pr.body};
        p.attack = pr.attack; p.release = pr.release; p.contour = pr.contour; p.modDecay = pr.modDecay;
        p.width = pr.width; p.drift = pr.drift; p.drive = pr.drive;
        p.spaceType = pr.spaceType; p.spaceMix = pr.spaceMix; p.spaceSize = pr.spaceSize; p.volumeDb = -3.0f;
        p.root = 2; p.scaleMask = scalePresets()[3].mask; p.scaleLock = true; p.tuning = TuningMode::JustAdaptive;
        syn.setParams(p);
        const int N = (int)(total * sr);
        std::vector<float> L((size_t)N), R((size_t)N);
        const int blk = 256;
        double peak = 0, sum = 0; bool bad = false;
        for (int pos = 0; pos < N; pos += blk) {
            const float t0 = (float)pos / sr, t1 = (float)(pos + blk) / sr;
            for (auto& ev : evs) if (ev.t >= t0 && ev.t < t1) { if (ev.on) syn.noteOn(ev.note, ev.vel); else syn.noteOff(ev.note); }
            const int n = std::min(blk, N - pos);
            syn.render(&L[(size_t)pos], &R[(size_t)pos], n);
        }
        for (int i = 0; i < N; ++i) {
            if (!std::isfinite(L[i]) || !std::isfinite(R[i])) bad = true;
            peak = std::max(peak, (double)std::max(std::fabs(L[i]), std::fabs(R[i])));
            sum += L[i] * L[i] + R[i] * R[i];
        }
        const double rms = std::sqrt(sum / (2.0 * N));
        std::printf("%-15s %-6s peak %.3f  rms %.1f dBFS %s\n", pr.name, engineName(pr.engine), peak, 20 * std::log10(rms + 1e-12), bad ? "NAN!" : "");
        std::string fn = pr.name; for (auto& c : fn) if (c == ' ') c = '_';
        writeWav(outDir + "/" + fn + ".wav", L, R, sr);
    }
    return 0;
}
