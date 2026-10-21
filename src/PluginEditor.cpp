#include "PluginEditor.h"

using namespace ostrov_ui;
using juce::Colour;

static juce::Font uiFont(float h, bool bold = false) {
    return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain));
}

// ------------------------------------------------------------------ LookAndFeel
LookAndFeel::LookAndFeel() {
    setColour(juce::ComboBox::backgroundColourId, Palette::panel());
    setColour(juce::ComboBox::textColourId, Palette::text());
    setColour(juce::ComboBox::outlineColourId, Palette::line());
    setColour(juce::ComboBox::arrowColourId, Palette::dim());
    setColour(juce::PopupMenu::backgroundColourId, Palette::panel());
    setColour(juce::PopupMenu::textColourId, Palette::text());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::accent().withAlpha(0.25f));
    setColour(juce::PopupMenu::highlightedTextColourId, Palette::text());
    setColour(juce::Slider::textBoxTextColourId, Palette::dim());
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::textColourId, Palette::text());
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) {
    auto b = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h).reduced(4.0f);
    const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float thick = juce::jmax(2.5f, r * 0.09f);
    const float ang = a0 + pos * (a1 - a0);
    juce::Path bgArc; bgArc.addCentredArc(c.x, c.y, r - thick, r - thick, 0.0f, a0, a1, true);
    g.setColour(Palette::line()); g.strokePath(bgArc, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path arc; arc.addCentredArc(c.x, c.y, r - thick, r - thick, 0.0f, a0, ang, true);
    g.setColour(Palette::accent()); g.strokePath(arc, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float inner = r - thick * 2.6f;
    g.setColour(Palette::panel().brighter(0.08f)); g.fillEllipse(c.x - inner, c.y - inner, inner * 2, inner * 2);
    const juce::Point<float> tip(c.x + std::sin(ang) * inner * 0.72f, c.y - std::cos(ang) * inner * 0.72f);
    g.setColour(Palette::text()); g.fillEllipse(tip.x - thick * 0.6f, tip.y - thick * 0.6f, thick * 1.2f, thick * 1.2f);
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    const bool on = b.getToggleState();
    g.setColour(on ? Palette::accent().withAlpha(0.18f) : (over ? Palette::panel().brighter(0.06f) : Palette::panel()));
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(on ? Palette::accent() : Palette::line());
    g.drawRoundedRectangle(r, 6.0f, down ? 1.6f : 1.0f);
}

void LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) {
    g.setColour(b.getToggleState() ? Palette::text() : Palette::dim());
    g.setFont(uiFont(14.0f, b.getToggleState()));
    g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) {
    auto r = juce::Rectangle<float>(0, 0, (float)w, (float)h).reduced(0.5f);
    g.setColour(Palette::panel()); g.fillRoundedRectangle(r, 5.0f);
    g.setColour(Palette::line()); g.drawRoundedRectangle(r, 5.0f, 1.0f);
    juce::Path arrow; const float ax = (float)w - 14.0f, ay = (float)h * 0.5f;
    arrow.addTriangle(ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour(Palette::dim()); g.fillPath(arrow);
}

juce::Font LookAndFeel::getComboBoxFont(juce::ComboBox&) { return uiFont(13.5f); }

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool, bool) {
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft(r.getHeight()).reduced(5.0f);
    g.setColour(Palette::line()); g.drawRoundedRectangle(box, 4.0f, 1.0f);
    if (b.getToggleState()) { g.setColour(Palette::accent()); g.fillRoundedRectangle(box.reduced(3.0f), 2.5f); }
    g.setColour(Palette::text()); g.setFont(uiFont(13.5f));
    g.drawText(b.getButtonText(), r.withTrimmedLeft(4), juce::Justification::centredLeft);
}

// ------------------------------------------------------------------ Knob
Knob::Knob(juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& l, bool isBig)
    : label(l), big(isBig) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setPopupDisplayEnabled(true, true, nullptr);
    addAndMakeVisible(slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(s, id, slider);
}
void Knob::resized() { slider.setBounds(getLocalBounds().withTrimmedBottom(big ? 22 : 18)); }
void Knob::paint(juce::Graphics& g) {
    g.setColour(big ? Palette::text() : Palette::dim());
    g.setFont(uiFont(big ? 14.5f : 12.0f, big));
    g.drawText(label, getLocalBounds().removeFromBottom(big ? 22 : 18), juce::Justification::centred);
}

// ------------------------------------------------------------------ ScaleRing
static const char* kNoteNames[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

juce::Point<float> ScaleRing::dotPos(int deg) const {
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = juce::jmin(getWidth(), getHeight()) * 0.5f - 22.0f;
    const float a = juce::MathConstants<float>::twoPi * (float)deg / 12.0f;
    return { c.x + std::sin(a) * r, c.y - std::cos(a) * r };
}

void ScaleRing::paint(juce::Graphics& g) {
    const auto& ap = proc.apvts;
    const int root = (int)ap.getRawParameterValue("root")->load();
    const int sc = (int)ap.getRawParameterValue("scale")->load();
    const int tuning = (int)ap.getRawParameterValue("tuning")->load();
    const uint16_t mask = proc.currentMask();
    const int ref = tuning == 0 ? -1 : (tuning == 1 ? root : proc.referencePitchClass());
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = juce::jmin(getWidth(), getHeight()) * 0.5f - 22.0f;

    g.setColour(Palette::line()); g.drawEllipse(c.x - r, c.y - r, r * 2, r * 2, 1.0f);
    // lines between enabled degrees: the shape of the scale
    juce::Path shape; bool first = true;
    for (int d = 0; d < 12; ++d) if ((mask >> d) & 1) {
        const auto p = dotPos(d);
        if (first) { shape.startNewSubPath(p); first = false; } else shape.lineTo(p);
    }
    shape.closeSubPath();
    g.setColour(Palette::accent().withAlpha(0.08f)); g.fillPath(shape);
    g.setColour(Palette::accent().withAlpha(0.35f)); g.strokePath(shape, juce::PathStrokeType(1.0f));

    for (int d = 0; d < 12; ++d) {
        const auto p = dotPos(d);
        const bool on = (mask >> d) & 1;
        const int pc = (root + d) % 12;
        const float rr = d == 0 ? 15.0f : 13.0f;
        if (on) { g.setColour(d == 0 ? Palette::warm() : Palette::accent()); g.fillEllipse(p.x - rr, p.y - rr, rr * 2, rr * 2); }
        else { g.setColour(Palette::panel()); g.fillEllipse(p.x - rr, p.y - rr, rr * 2, rr * 2);
               g.setColour(Palette::line()); g.drawEllipse(p.x - rr, p.y - rr, rr * 2, rr * 2, 1.0f); }
        if (pc == ref) { g.setColour(Palette::text()); g.drawEllipse(p.x - rr - 4, p.y - rr - 4, rr * 2 + 8, rr * 2 + 8, 1.4f); }
        g.setColour(on ? Palette::bg() : Palette::dim());
        g.setFont(uiFont(12.0f, on));
        g.drawText(kNoteNames[pc], juce::Rectangle<float>(p.x - rr, p.y - rr, rr * 2, rr * 2), juce::Justification::centred);
    }
    g.setColour(Palette::text()); g.setFont(uiFont(22.0f, true));
    g.drawText(kNoteNames[root], getLocalBounds().withSizeKeepingCentre(120, 28).translated(0, -12), juce::Justification::centred);
    g.setColour(Palette::dim()); g.setFont(uiFont(12.5f));
    const juce::String scName = sc < OstrovProcessor::kCustomScale ? juce::String(ostrov::scalePresets()[(size_t)sc].name) : juce::String("Custom");
    g.drawText(scName, getLocalBounds().withSizeKeepingCentre(140, 20).translated(0, 12), juce::Justification::centred);
}

void ScaleRing::mouseDown(const juce::MouseEvent& e) {
    for (int d = 0; d < 12; ++d) {
        if (dotPos(d).getDistanceFrom(e.position) < 16.0f) {
            if (e.mods.isRightButtonDown() || e.mods.isShiftDown()) {
                // make this note the new root
                auto* rp = proc.apvts.getParameter("root");
                const int root = (int)proc.apvts.getRawParameterValue("root")->load();
                rp->beginChangeGesture();
                rp->setValueNotifyingHost(rp->convertTo0to1((float)((root + d) % 12)));
                rp->endChangeGesture();
            } else {
                proc.toggleNote(d);
            }
            repaint();
            return;
        }
    }
}

// ------------------------------------------------------------------ Editor
OstrovEditor::OstrovEditor(OstrovProcessor& p) : AudioProcessorEditor(&p), proc(p), ring(p) {
    setLookAndFeel(&lnf);
    auto& s = proc.apvts;
    for (int e = 0; e < ostrov::E_Count; ++e) {
        auto* b = engineButtons.add(new juce::TextButton(ostrov::engineName(e)));
        b->setClickingTogglesState(false);
        b->onClick = [this, e] {
            auto* ep = proc.apvts.getParameter("engine");
            ep->beginChangeGesture(); ep->setValueNotifyingHost(ep->convertTo0to1((float)e)); ep->endChangeGesture();
        };
        addAndMakeVisible(b);
    }
    timbre = std::make_unique<Knob>(s, "timbre", "Timbre", true);
    color  = std::make_unique<Knob>(s, "color", "Color", true);
    motion = std::make_unique<Knob>(s, "motion", "Motion", true);
    body   = std::make_unique<Knob>(s, "body", "Body", true);
    attack = std::make_unique<Knob>(s, "attack", "Attack", false);
    release= std::make_unique<Knob>(s, "release", "Release", false);
    drift  = std::make_unique<Knob>(s, "drift", "Drift", false);
    mix    = std::make_unique<Knob>(s, "spaceMix", "Mix", false);
    size   = std::make_unique<Knob>(s, "spaceSize", "Size", false);
    volume = std::make_unique<Knob>(s, "volume", "Volume", false);
    for (auto* k : {timbre.get(), color.get(), motion.get(), body.get(), attack.get(), release.get(),
                    drift.get(), mix.get(), size.get(), volume.get()})
        addAndMakeVisible(k);

    auto fill = [](juce::ComboBox& cb, juce::RangedAudioParameter* prm) {
        if (auto* ch = dynamic_cast<juce::AudioParameterChoice*>(prm)) cb.addItemList(ch->choices, 1);
    };
    fill(spaceType, s.getParameter("spaceType")); fill(root, s.getParameter("root"));
    fill(scale, s.getParameter("scale")); fill(tuning, s.getParameter("tuning"));
    for (auto* cb : {&spaceType, &root, &scale, &tuning}) addAndMakeVisible(cb);
    spaceAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(s, "spaceType", spaceType);
    rootAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(s, "root", root);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(s, "scale", scale);
    tuningAtt= std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(s, "tuning", tuning);
    addAndMakeVisible(lock);
    lockAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(s, "lock", lock);
    addAndMakeVisible(ring);

    setSize(860, 500);
    startTimerHz(20);
}

OstrovEditor::~OstrovEditor() { stopTimer(); setLookAndFeel(nullptr); }

void OstrovEditor::timerCallback() {
    const int e = (int)proc.apvts.getRawParameterValue("engine")->load();
    for (int i = 0; i < engineButtons.size(); ++i) engineButtons[i]->setToggleState(i == e, juce::dontSendNotification);
    ring.repaint();
}

void OstrovEditor::paint(juce::Graphics& g) {
    g.fillAll(Palette::bg());
    auto b = getLocalBounds();
    // wordmark
    g.setColour(Palette::text()); g.setFont(uiFont(26.0f, true));
    g.drawText("OSTROV", 28, 18, 200, 32, juce::Justification::centredLeft);
    g.setColour(Palette::dim()); g.setFont(uiFont(11.5f));
    g.drawText("soft waves / just intervals / drifting space", 28, 48, 320, 16, juce::Justification::centredLeft);
    // panels
    g.setColour(Palette::panel());
    g.fillRoundedRectangle(juce::Rectangle<float>(20, 84, 520, 396), 12.0f);
    g.fillRoundedRectangle(juce::Rectangle<float>(556, 84, 284, 396), 12.0f);
    g.setColour(Palette::line());
    g.drawLine(40, 300, 520, 300, 1.0f);
    g.setColour(Palette::dim()); g.setFont(uiFont(11.0f, true));
    g.drawText("VOICE", 40, 312, 120, 14, juce::Justification::centredLeft);
    g.drawText("SPACE", 300, 312, 120, 14, juce::Justification::centredLeft);
    g.drawText("SCALE", 576, 96, 120, 14, juce::Justification::centredLeft);
    juce::ignoreUnused(b);
}

void OstrovEditor::resized() {
    // engine selector
    int x = 360;
    for (auto* btn : engineButtons) { btn->setBounds(x, 26, 76, 30); x += 80; }
    // big macros
    const int bigW = 116, top = 110;
    int bx = 40;
    for (auto* k : {timbre.get(), color.get(), motion.get(), body.get()}) { k->setBounds(bx, top, bigW, 150); bx += 122; }
    // voice row
    const int sy = 334, sw = 70, sh = 86;
    attack->setBounds(40, sy, sw, sh); release->setBounds(112, sy, sw, sh); drift->setBounds(184, sy, sw, sh);
    // space row
    spaceType.setBounds(300, sy + 4, 112, 26);
    mix->setBounds(300, sy + 36, 60, 74); size->setBounds(362, sy + 36, 60, 74); volume->setBounds(440, sy, sw, sh);
    // scale panel
    ring.setBounds(578, 112, 240, 240);
    root.setBounds(576, 364, 76, 26); scale.setBounds(658, 364, 162, 26);
    tuning.setBounds(576, 398, 160, 26); lock.setBounds(740, 398, 100, 26);
}
