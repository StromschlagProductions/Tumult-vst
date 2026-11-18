#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "TumultDSP.h"

class TumultProcessor : public juce::AudioProcessor
{
public:
    TumultProcessor();
    ~TumultProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tumult"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    // Presets (Version 1): sie setzen nur die Regler
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::StringArray sizeNames();
    // "Neue Leute": die Menge wird in der naechsten Sprechpause neu zusammengewuerfelt
    void rerollCrowd() { engine.requestReroll(); }
    const tumult::Engine& crowd() const { return engine; }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> uiTheme { 0 }, uiWidth { 680 }, uiHeight { 420 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    tumult::Engine engine;
    std::vector<float> scratchL, scratchR;   // fuer Mono-Spuren (vorab angelegt)
    std::atomic<int> currentProgram { 0 };
    std::atomic<uint32_t> seed { 1 };
    std::atomic<float>* pSize = nullptr; std::atomic<float>* pAmount = nullptr; std::atomic<float>* pChaos = nullptr;
    std::atomic<float>* pVariety = nullptr; std::atomic<float>* pRoom = nullptr; std::atomic<float>* pOwn = nullptr;
    std::atomic<float>* pOut = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TumultProcessor)
};
