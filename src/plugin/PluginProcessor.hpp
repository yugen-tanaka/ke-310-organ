#pragma once

#include "FactoryPresets.hpp"
#include "PluginParams.hpp"
#include "dsp/KE310Engine.hpp"
#include "dsp/MidiRouter.hpp"
#include "dsp/RotarySpeaker.hpp"
#include "dsp/rhythmbox/RhythmboxEngine.hpp"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cstdint>
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
    ke310::rhythmbox::RhythmboxTelemetry readRhythmTelemetry() const noexcept;

    // Lock-free UI bridges & Standalone mode helpers for embedded Rhythmbox START/STOP buttons
    void setRhythmStartHeldFromUi(bool held) noexcept;
    void setRhythmStopHeldFromUi(bool held) noexcept;
    bool isStandaloneMode() const noexcept;
    void setForceStandaloneForTest(bool force) noexcept { forceStandaloneForTest_ = force; }

private:
    void syncEngineAndMidiState(const dsp::KE310State& baseState, const dsp::MidiRouterConfig& midiCfg) noexcept;
    void handleMidiController(int channel, int ccNumber, int value, const dsp::MidiRouterConfig& midiCfg) noexcept;
    bool updateRhythmGateState(bool rhythmArmed,
                               bool hasPattern,
                               bool breakOn,
                               bool uiStartHeld,
                               bool uiStopHeld) noexcept;
    void renderChunk(int startSample,
                     int numSamples,
                     juce::AudioBuffer<float>& buffer,
                     const ke310::rhythmbox::HostTransportInfo& host,
                     int rhythmOutputMode) noexcept;

    juce::AudioProcessorValueTreeState apvts_;
    CachedParameterPointers rawParams_{};
    int currentProgram_ = 0;
    double sampleRate_ = 48000.0;

    dsp::KE310Engine engine_{};
    dsp::MidiRouter midiRouter_{};
    dsp::RotaryDrumSpeaker rotarySpeaker_{};
    ke310::rhythmbox::RhythmboxEngine rhythmbox_{};
    bool rhythmLatchedRunning_ = false;
    bool prevRhythmTriggerHeld_ = false;
    std::atomic<bool> uiRhythmStartHeld_{false};
    std::atomic<bool> uiRhythmStopHeld_{false};
    bool prevUiRhythmStartHeld_ = false;
    bool prevUiRhythmStopHeld_ = false;
    bool prevRhythmArmed_ = false;
    bool prevRhythmBreak_ = false;
    bool prevHostPlaying_ = false;
    ke310::rhythmbox::ClockMode prevRhythmClockMode_ = ke310::rhythmbox::ClockMode::TempoSync;
    bool forceStandaloneForTest_ = false;

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
    std::vector<double> rhythmMonoScratch_;
    std::vector<float> statOutScratch_;
    std::vector<float> rhythmOutScratch_;

    std::atomic<float> mainRms_{0.0f};
    std::atomic<float> rotorRms_{0.0f};
    std::atomic<float> statRms_{0.0f};
    std::atomic<float> rotarySpeedHz_{0.0f};
    std::atomic<int> upperActiveNotes_{0};
    std::atomic<int> lowerActiveNotes_{0};
    std::atomic<int> pedalActiveMidi_{-1};

    std::atomic<float> rbTelRms_{0.0f};
    std::atomic<float> rbTelPeak_{0.0f};
    std::atomic<float> rbTelBpm_{106.8f};
    std::atomic<std::uint32_t> rbTelPacked_{0u};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KE310AudioProcessor)
};

} // namespace ke310::plugin
