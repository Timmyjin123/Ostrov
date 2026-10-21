#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Synth.h"

class OstrovProcessor : public juce::AudioProcessor {
public:
    OstrovProcessor();
    ~OstrovProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Ostrov"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // UI helpers
    uint16_t currentMask() const;
    int referencePitchClass() const { return refPc.load(); }
    void toggleNote(int semitoneFromRoot);   // switches the scale to Custom if needed
    static constexpr int kCustomScale = 14;
    void applyPreset(int index);
    int currentPreset = -1;

private:
    ostrov::Synth synth;
    std::atomic<int> refPc { 0 };
    std::atomic<float>* pEngine{}; std::atomic<float>* pTimbre{}; std::atomic<float>* pColor{};
    std::atomic<float>* pMotion{}; std::atomic<float>* pBody{}; std::atomic<float>* pAttack{};
    std::atomic<float>* pRelease{}; std::atomic<float>* pDrift{}; std::atomic<float>* pSpaceType{};
    std::atomic<float>* pSpaceMix{}; std::atomic<float>* pSpaceSize{}; std::atomic<float>* pVolume{};
    std::atomic<float>* pRoot{}; std::atomic<float>* pScale{}; std::atomic<float>* pTuning{};
    std::atomic<float>* pLock{}; std::atomic<float>* pAftertouch{};
    std::atomic<float>* pContour{}; std::atomic<float>* pModDecay{}; std::atomic<float>* pWidth{}; std::atomic<float>* pDrive{};
    std::atomic<float>* pDeg[12]{};
    void readParams();
    void handleMidi(const juce::MidiMessage& m);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OstrovProcessor)
};
