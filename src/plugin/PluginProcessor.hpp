#pragma once

#include "FactoryPresets.hpp"
#include "PluginParams.hpp"
#include "dsp/KE310Engine.hpp"
#include "dsp/MidiRouter.hpp"
#include "dsp/RotarySpeaker.hpp"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

namespace ke310::plugin
{

class KE310AudioProcessor final : public juce::AudioProcessor
{
public:
    KE310AudioProcessor();
    ~KE310AudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& apvts() noexcept { return apvts_; }

    // Lock-free telemetry for UI meters & keyboard activity indicators
    float mainBusRms() const noexcept { return mainRms_.load(std::memory_order_relaxed); }
    float rotorBusRms() const noexcept { return rotorRms_.load(std::memory_order_relaxed); }
    float stationaryBusRms() const noexcept { return statRms_.load(std::memory_order_relaxed); }
    float rotarySpeedHz() const noexcept { return rotarySpeedHz_.load(std::memory_order_relaxed); }
    int activeUpperNotes() const noexcept { return upperActiveNotes_.load(std::memory_order_relaxed); }
    int activeLowerNotes() const noexcept { return lowerActiveNotes_.load(std::memory_order_relaxed); }
    int activePedalMidi() const noexcept { return pedalActiveMidi_.load(std::memory_order_relaxed); }

private:
    void syncEngineAndMidiState(const dsp::KE310State& baseState, const dsp::MidiRouterConfig& midiCfg) noexcept;
    void handleMidiController(int channel, int ccNumber, int value, const dsp::MidiRouterConfig& midiCfg) noexcept;
    void renderChunk(int startSample, int numSamples, juce::AudioBuffer<float>& buffer) noexcept;

    juce::AudioProcessorValueTreeState apvts_;
    CachedParameterPointers rawParams_{};
    int currentProgram_ = 0;

    dsp::KE310Engine engine_{};
    dsp::MidiRouter midiRouter_{};
    dsp::RotaryDrumSpeaker rotarySpeaker_{};

    // Real-time MIDI CC overrides combined seamlessly with APVTS state
    double cc11Expression_ = 1.0;
    double cc7Volume_ = 1.0;
    bool cc64UpperSustain_ = false;
    bool cc64PedalSustain_ = false;

    std::vector<float> mixScratch_;
    std::vector<float> mixLScratch_;
    std::vector<float> mixRScratch_;
    std::vector<float> rotorScratch_;
    std::vector<float> rotorLScratch_;
    std::vector<float> rotorRScratch_;
    std::vector<float> statScratch_;

    std::atomic<float> mainRms_{0.0f};
    std::atomic<float> rotorRms_{0.0f};
    std::atomic<float> statRms_{0.0f};
    std::atomic<float> rotarySpeedHz_{0.0f};
    std::atomic<int> upperActiveNotes_{0};
    std::atomic<int> lowerActiveNotes_{0};
    std::atomic<int> pedalActiveMidi_{-1};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KE310AudioProcessor)
};

} // namespace ke310::plugin
