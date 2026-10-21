// Ostrov — scales, scale lock and just-intonation tuning
#pragma once
#include <cstdint>
#include <cmath>
#include <array>

namespace ostrov {

struct ScalePreset { const char* name; uint16_t mask; }; // bit i = semitone i above root

inline uint16_t maskOf(std::initializer_list<int> degrees) {
    uint16_t m = 0;
    for (int d : degrees) m |= (uint16_t)(1u << d);
    return m;
}

inline const std::array<ScalePreset, 14>& scalePresets() {
    static const std::array<ScalePreset, 14> p = {{
        {"Chromatic",      0x0FFF},
        {"Major",          maskOf({0,2,4,5,7,9,11})},
        {"Minor",          maskOf({0,2,3,5,7,8,10})},
        {"Dorian",         maskOf({0,2,3,5,7,9,10})},
        {"Phrygian",       maskOf({0,1,3,5,7,8,10})},
        {"Lydian",         maskOf({0,2,4,6,7,9,11})},
        {"Mixolydian",     maskOf({0,2,4,5,7,9,10})},
        {"Major Penta",    maskOf({0,2,4,7,9})},
        {"Minor Penta",    maskOf({0,3,5,7,10})},
        {"Hirajoshi",      maskOf({0,2,3,7,8})},
        {"In Sen",         maskOf({0,1,5,7,10})},
        {"Whole Tone",     maskOf({0,2,4,6,8,10})},
        {"Harmonic Minor", maskOf({0,2,3,5,7,8,11})},
        {"Open Fifths",    maskOf({0,2,7})},
    }};
    return p;
}

enum class TuningMode { Equal = 0, JustRoot = 1, JustAdaptive = 2 };

// 5-limit just ratios relative to a reference pitch class
inline float justCents(int interval) {
    static const float ratios[12] = { 1.0f, 16.f/15.f, 9.f/8.f, 6.f/5.f, 5.f/4.f, 4.f/3.f,
                                      45.f/32.f, 3.f/2.f, 8.f/5.f, 5.f/3.f, 9.f/5.f, 15.f/8.f };
    interval = ((interval % 12) + 12) % 12;
    return 1200.0f * std::log2(ratios[interval]) - 100.0f * (float)interval;
}

// Move a MIDI note to the nearest enabled scale degree (ties go up)
inline int quantizeToScale(int note, int root, uint16_t mask) {
    if ((mask & 0x0FFF) == 0) return note;
    for (int d = 0; d <= 6; ++d) {
        const int up = note + d, dn = note - d;
        if ((mask >> (((up - root) % 12 + 12) % 12)) & 1) return up;
        if ((mask >> (((dn - root) % 12 + 12) % 12)) & 1) return dn;
    }
    return note;
}

inline float noteToHz(float note) { return 440.0f * std::exp2((note - 69.0f) / 12.0f); }

} // namespace ostrov
