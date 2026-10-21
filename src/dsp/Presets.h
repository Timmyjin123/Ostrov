// Ostrov — factory presets
#pragma once
#include <array>

namespace ostrov {

struct Preset {
    const char* name;
    int engine;
    float timbre, color, motion, body;
    float attack, release, contour, modDecay;
    float width, drift, drive;
    int spaceType; float spaceMix, spaceSize;
};

// engine: 0 Bloom 1 Drift 2 Fold 3 Bell 4 Choir 5 Breath 6 Glass 7 Tide
// space:  0 Plate 1 Tape 2 Tape+Plate 3 Shimmer
inline const std::array<Preset, 22>& factoryPresets() {
    static const std::array<Preset, 22> p = {{
        //  name               eng  tim   col   mot   body   atk    rel   cont   mdec  width drift drive sp  mix   size
        {"Morning Bloom",       0, 0.45f, 0.20f, 0.55f, 0.20f, 0.35f, 2.5f, -0.30f, 1.8f, 0.70f, 0.30f, 0.15f, 3, 0.45f, 0.70f},
        {"Glass Pluck",         0, 0.30f, 0.80f, 0.30f, 0.45f, 0.002f, 1.2f, 0.65f, 0.35f, 0.40f, 0.15f, 0.25f, 1, 0.35f, 0.45f},
        {"Soft Keys",           0, 0.25f, 0.10f, 0.25f, 0.00f, 0.004f, 1.4f, 0.45f, 0.70f, 0.35f, 0.20f, 0.30f, 0, 0.25f, 0.50f},
        {"Ratio Bells",         0, 0.55f, 0.55f, 0.40f, 0.85f, 0.002f, 2.5f, 0.40f, 1.20f, 0.50f, 0.10f, 0.10f, 0, 0.40f, 0.70f},
        {"Island Bass",         1, 0.25f, 0.55f, 0.20f, 0.35f, 0.003f, 0.4f, 0.50f, 0.25f, 0.00f, 0.10f, 0.45f, 0, 0.10f, 0.30f},
        {"Drift Pad",           1, 0.35f, 0.45f, 0.75f, 0.30f, 0.90f, 3.5f, -0.25f, 2.5f, 0.80f, 0.55f, 0.20f, 2, 0.40f, 0.65f},
        {"Warm Brass",          1, 0.30f, 0.50f, 0.35f, 0.25f, 0.05f, 0.8f, 0.45f, 0.45f, 0.50f, 0.25f, 0.35f, 0, 0.25f, 0.50f},
        {"Acid Shore",          1, 0.20f, 0.95f, 0.15f, 0.80f, 0.002f, 0.3f, 0.70f, 0.18f, 0.00f, 0.05f, 0.60f, 1, 0.25f, 0.35f},
        {"Sun Fold",            2, 0.35f, 0.25f, 0.45f, 0.55f, 0.02f, 1.2f, 0.35f, 0.60f, 0.45f, 0.20f, 0.20f, 1, 0.30f, 0.45f},
        {"Rust Fold",           2, 0.65f, 0.70f, 0.60f, 0.85f, 0.01f, 0.9f, 0.20f, 0.40f, 0.30f, 0.15f, 0.40f, 2, 0.30f, 0.50f},
        {"Temple Bell",         3, 0.45f, 0.05f, 0.40f, 0.80f, 0.001f, 4.0f, 0.00f, 1.0f, 0.40f, 0.10f, 0.10f, 0, 0.45f, 0.80f},
        {"Glass Rain",          3, 0.70f, 0.40f, 0.30f, 0.35f, 0.001f, 1.5f, 0.00f, 1.0f, 0.60f, 0.05f, 0.10f, 1, 0.40f, 0.55f},
        {"Bowed Glass",         3, 0.90f, 0.35f, 0.55f, 0.95f, 0.60f, 3.0f, 0.00f, 1.0f, 0.60f, 0.15f, 0.10f, 3, 0.45f, 0.70f},
        {"Choir of Mist",       4, 0.30f, 0.45f, 0.55f, 0.40f, 0.70f, 3.0f, -0.15f, 2.0f, 0.75f, 0.25f, 0.10f, 3, 0.50f, 0.75f},
        {"Vowel Lead",          4, 0.20f, 0.65f, 0.60f, 0.55f, 0.03f, 0.8f, 0.30f, 0.40f, 0.20f, 0.15f, 0.30f, 1, 0.30f, 0.50f},
        {"Wind Strings",        5, 0.75f, 0.10f, 0.50f, 0.45f, 0.80f, 2.5f, 0.00f, 1.0f, 0.70f, 0.20f, 0.10f, 2, 0.40f, 0.65f},
        {"Sea Breath",          5, 0.40f, 0.40f, 0.70f, 0.70f, 1.20f, 3.5f, -0.20f, 2.0f, 0.80f, 0.30f, 0.05f, 3, 0.55f, 0.75f},
        {"Crystal Stack",       6, 0.50f, 0.30f, 0.45f, 0.00f, 0.005f, 2.0f, 0.45f, 0.80f, 0.50f, 0.15f, 0.15f, 0, 0.35f, 0.65f},
        {"Morph Keys",          6, 0.40f, 0.60f, 0.70f, 0.55f, 0.01f, 1.6f, 0.35f, 0.90f, 0.45f, 0.20f, 0.25f, 2, 0.30f, 0.55f},
        {"Tide Pad",            7, 0.40f, 0.35f, 0.70f, 0.40f, 1.50f, 4.0f, 0.00f, 1.0f, 0.80f, 0.35f, 0.15f, 2, 0.45f, 0.70f},
        {"Deep Tide",           7, 0.20f, 0.15f, 0.50f, 0.80f, 2.00f, 5.0f, 0.00f, 1.0f, 0.60f, 0.40f, 0.25f, 0, 0.40f, 0.85f},
        {"Shimmer Field",       7, 0.55f, 0.60f, 0.85f, 0.25f, 1.00f, 4.5f, -0.20f, 3.0f, 0.90f, 0.30f, 0.05f, 3, 0.55f, 0.80f},
    }};
    return p;
}

} // namespace ostrov
