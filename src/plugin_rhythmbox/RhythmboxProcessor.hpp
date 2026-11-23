#pragma once

#include "dsp/rhythmbox/RhythmboxEngine.hpp"
#include "plugin_rhythmbox/RhythmboxPluginParams.hpp"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace ke310::rhythmbox_plugin
{

class RhythmboxAudioProcessor final : public juce::AudioProcessor
{
public:
    RhythmboxAudioProcessor();
    ~RhythmboxAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "KE Rhythmbox"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& apvts() noexcept { return apvts_; }

    // Lock-free UI pad trigger & telemetry bridge
    void enqueueUiAuditionVoice(ke310::rhythmbox::VoiceId voice) noexcept;
    ke310::rhythmbox::RhythmboxTelemetry readTelemetry() const noexcept;

private:
    void renderSlice(int startSample,
                     int numSamples,
                     juce::AudioBuffer<float>& buffer,
                     const ke310::rhythmbox::HostTransportInfo& host) noexcept;

    juce::AudioProcessorValueTreeState apvts_;
    CachedRhythmboxParams rawParams_{};
    int currentProgram_ = static_cast<int>(ke310::rhythmbox::PatternId::Rock1);

    double sampleRate_ = 48000.0;
    ke310::rhythmbox::RhythmboxEngine engine_{};
    std::vector<double> monoScratch_;

    std::atomic<std::uint32_t> pendingUiVoices_{0u};
    std::atomic<float> telRms_{0.0f};
    std::atomic<float> telPeak_{0.0f};
    std::atomic<float> telBpm_{106.8f};
    std::atomic<std::uint32_t> telPacked_{0u};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RhythmboxAudioProcessor)
};

} // namespace ke310::rhythmbox_plugin
