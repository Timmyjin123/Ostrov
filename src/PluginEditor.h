#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ostrov_ui {
struct Palette {
    static juce::Colour bg()     { return juce::Colour(0xff111318); }
    static juce::Colour panel()  { return juce::Colour(0xff181b22); }
    static juce::Colour line()   { return juce::Colour(0xff2a2f3a); }
    static juce::Colour text()   { return juce::Colour(0xffe9e4d8); }
    static juce::Colour dim()    { return juce::Colour(0xff737a88); }
    static juce::Colour accent() { return juce::Colour(0xff5ccfc0); }   // sea
    static juce::Colour warm()   { return juce::Colour(0xffe8a86b); }   // sand / root
};

class LookAndFeel : public juce::LookAndFeel_V4 {
public:
    LookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

class Knob : public juce::Component {
public:
    Knob(juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, bool big);
    void resized() override;
    void paint(juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String label; bool big;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class ScaleRing : public juce::Component {
public:
    explicit ScaleRing(OstrovProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
private:
    OstrovProcessor& proc;
    juce::Point<float> dotPos(int deg) const;
};
} // namespace ostrov_ui

class OstrovEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit OstrovEditor(OstrovProcessor&);
    ~OstrovEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    OstrovProcessor& proc;
    ostrov_ui::LookAndFeel lnf;
    juce::OwnedArray<juce::TextButton> engineButtons;
    std::unique_ptr<ostrov_ui::Knob> timbre, color, motion, body, attack, release, drift, mix, size, volume;
    juce::ComboBox spaceType, root, scale, tuning;
    juce::ToggleButton lock { "Scale Lock" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> spaceAtt, rootAtt, scaleAtt, tuningAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lockAtt;
    ostrov_ui::ScaleRing ring;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OstrovEditor)
};
