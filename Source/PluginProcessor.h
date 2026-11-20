#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "TumultDSP.h"
#include <limits>

class TumultProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    TumultProcessor();
    ~TumultProcessor() override { stopTimer(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tumult"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    // Presets (Version 1): sie setzen nur die Regler
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static constexpr int kUiWidth = 820, kUiHeight = 420;
    static juce::StringArray sizeNames();
    // "Neue Leute": die Menge wird in der naechsten Sprechpause neu zusammengewuerfelt
    void rerollCrowd();
    const tumult::Engine& crowd() const { return engine; }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> uiTheme { 0 }, uiWidth { 820 }, uiHeight { 420 };

private:
    void processAudio (juce::AudioBuffer<float>&, bool bypassed);
    // Latenz-Meldung an den Host ausserhalb des Audio-Threads (CHOR aus = keine Latenz)
    void timerCallback() override { const int d = reportedLatency.load(); if (getLatencySamples() != d) setLatencySamples (d); }
    std::atomic<int> reportedLatency { 0 };
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    tumult::Engine engine;
    std::vector<float> scratchL, scratchR;   // fuer Mono-Spuren (vorab angelegt)
    std::atomic<int> currentProgram { 8 };
    std::atomic<uint32_t> seed { 1 };
    std::atomic<float>* pSize = nullptr; std::atomic<float>* pAmount = nullptr; std::atomic<float>* pChaos = nullptr;
    std::atomic<float>* pVariety = nullptr; std::atomic<float>* pRoom = nullptr; std::atomic<float>* pOwn = nullptr;
    std::atomic<float>* pOut = nullptr; std::atomic<float>* pWalla = nullptr; std::atomic<float>* pWallaInt = nullptr; std::atomic<float>* pWallaEnabled = nullptr; std::atomic<float>* pChoirEnabled = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TumultProcessor)
};
