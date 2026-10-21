#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace ostrov;
using APVTS = juce::AudioProcessorValueTreeState;

static juce::StringArray noteNames() { return {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}; }

APVTS::ParameterLayout OstrovProcessor::createLayout() {
    using namespace juce;
    APVTS::ParameterLayout l;
    auto pct = [](const String& id, const String& name, float def) {
        return std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, name, NormalisableRange<float>(0.0f, 1.0f), def);
    };
    StringArray engines; for (int e = 0; e < E_Count; ++e) engines.add(engineName(e));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"engine", 1}, "Engine", engines, 0));
    l.add(pct("timbre", "Timbre", 0.5f));
    l.add(pct("color", "Color", 0.35f));
    l.add(pct("motion", "Motion", 0.4f));
    l.add(pct("body", "Body", 0.3f));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"attack", 1}, "Attack",
        NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.3f), 0.02f));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"release", 1}, "Release",
        NormalisableRange<float>(0.02f, 10.0f, 0.0f, 0.35f), 1.5f));
    l.add(pct("drift", "Drift", 0.3f));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"spaceType", 1}, "Space",
        StringArray{"Plate", "Tape", "Tape+Plate"}, 0));
    l.add(pct("spaceMix", "Space Mix", 0.35f));
    l.add(pct("spaceSize", "Space Size", 0.6f));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"volume", 1}, "Volume",
        NormalisableRange<float>(-36.0f, 6.0f), -6.0f));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"root", 1}, "Root", noteNames(), 0));
    StringArray scales; for (auto& s : scalePresets()) scales.add(s.name); scales.add("Custom");
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"scale", 1}, "Scale", scales, 1));
    for (int i = 0; i < 12; ++i)
        l.add(std::make_unique<AudioParameterBool>(ParameterID{"deg" + String(i), 1}, "Degree " + String(i), i == 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"tuning", 1}, "Tuning",
        StringArray{"Equal", "Just (root)", "Just (adaptive)"}, 2));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"lock", 1}, "Scale Lock", true));
    l.add(pct("aftertouch", "Aftertouch", 0.6f));
    return l;
}

OstrovProcessor::OstrovProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "OSTROV", createLayout()) {
    auto P = [&](const char* id) { return apvts.getRawParameterValue(id); };
    pEngine = P("engine"); pTimbre = P("timbre"); pColor = P("color"); pMotion = P("motion"); pBody = P("body");
    pAttack = P("attack"); pRelease = P("release"); pDrift = P("drift"); pSpaceType = P("spaceType");
    pSpaceMix = P("spaceMix"); pSpaceSize = P("spaceSize"); pVolume = P("volume"); pRoot = P("root");
    pScale = P("scale"); pTuning = P("tuning"); pLock = P("lock"); pAftertouch = P("aftertouch");
    for (int i = 0; i < 12; ++i) pDeg[i] = apvts.getRawParameterValue("deg" + juce::String(i));
}

bool OstrovProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void OstrovProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    synth.prepare((float)sampleRate, samplesPerBlock);
}

uint16_t OstrovProcessor::currentMask() const {
    const int sc = (int)pScale->load();
    if (sc < (int)scalePresets().size()) return scalePresets()[(size_t)sc].mask;
    uint16_t m = 0;
    for (int i = 0; i < 12; ++i)
        if (pDeg[i]->load() > 0.5f) m |= (uint16_t)(1u << i);
    return m ? m : 1;
}

void OstrovProcessor::toggleNote(int deg) {
    const uint16_t mask = currentMask();
    auto* scaleP = apvts.getParameter("scale");
    const int sc = (int)pScale->load();
    if (sc != kCustomScale) {
        for (int i = 0; i < 12; ++i) {
            auto* p = apvts.getParameter("deg" + juce::String(i));
            p->beginChangeGesture(); p->setValueNotifyingHost(((mask >> i) & 1) ? 1.0f : 0.0f); p->endChangeGesture();
        }
        scaleP->beginChangeGesture();
        scaleP->setValueNotifyingHost(scaleP->convertTo0to1((float)kCustomScale));
        scaleP->endChangeGesture();
    }
    if (deg == 0) return; // root always stays in
    auto* p = apvts.getParameter("deg" + juce::String(deg));
    p->beginChangeGesture(); p->setValueNotifyingHost(((mask >> deg) & 1) ? 0.0f : 1.0f); p->endChangeGesture();
}

void OstrovProcessor::readParams() {
    Params p;
    p.engine = (int)pEngine->load();
    p.macros.timbre = pTimbre->load(); p.macros.color = pColor->load();
    p.macros.motion = pMotion->load(); p.macros.body = pBody->load();
    p.attack = pAttack->load(); p.release = pRelease->load(); p.drift = pDrift->load();
    p.spaceType = (int)pSpaceType->load(); p.spaceMix = pSpaceMix->load(); p.spaceSize = pSpaceSize->load();
    p.volumeDb = pVolume->load();
    p.root = (int)pRoot->load();
    p.scaleMask = (uint16_t)(currentMask() | 1u);
    p.tuning = (TuningMode)(int)pTuning->load();
    p.scaleLock = pLock->load() > 0.5f;
    p.aftertouchAmt = pAftertouch->load();
    synth.setParams(p);
}

void OstrovProcessor::handleMidi(const juce::MidiMessage& m) {
    if (m.isNoteOn()) synth.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) synth.noteOff(m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff();
    else if (m.isAftertouch()) synth.polyPressure(m.getNoteNumber(), m.getAfterTouchValue() / 127.0f);
    else if (m.isChannelPressure()) synth.channelPressure(m.getChannelPressureValue() / 127.0f);
    else if (m.isPitchWheel()) synth.pitchBend((m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
}

void OstrovProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    readParams();
    const int n = buffer.getNumSamples();
    float* L = buffer.getWritePointer(0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : L;
    int pos = 0;
    for (const auto meta : midi) {
        const int at = juce::jlimit(0, n, meta.samplePosition);
        if (at > pos) { synth.render(L + pos, R + pos, at - pos); pos = at; }
        handleMidi(meta.getMessage());
    }
    if (pos < n) synth.render(L + pos, R + pos, n - pos);
    refPc.store(synth.referencePitchClass());
}

juce::AudioProcessorEditor* OstrovProcessor::createEditor() { return new OstrovEditor(*this); }

void OstrovProcessor::getStateInformation(juce::MemoryBlock& destData) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, destData);
}

void OstrovProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OstrovProcessor(); }
