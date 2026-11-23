#include "plugin_rhythmbox/RhythmboxProcessor.hpp"
#include "plugin_rhythmbox/RhythmboxEditor.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace ke310::rhythmbox_plugin
{

RhythmboxAudioProcessor::RhythmboxAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Main Out", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KERhythmboxParameters", createRhythmboxParameterLayout())
{
    rawParams_.bind(apvts_);
    engine_.prepare(48000.0);
    monoScratch_.assign(512u, 0.0);
}

void RhythmboxAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    const std::size_t cap = static_cast<std::size_t>(std::max(512, samplesPerBlock));
    monoScratch_.assign(cap, 0.0);

    engine_.prepare(sampleRate_);
    engine_.applyState(rawParams_.readState());
}

void RhythmboxAudioProcessor::releaseResources()
{
}

bool RhythmboxAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (!layouts.inputBuses.isEmpty() || layouts.outputBuses.size() != 1)
    {
        return false;
    }
    const auto& mainOut = layouts.getMainOutputChannelSet();
    return mainOut == juce::AudioChannelSet::stereo() || mainOut == juce::AudioChannelSet::mono();
}

void RhythmboxAudioProcessor::enqueueUiAuditionVoice(ke310::rhythmbox::VoiceId voice) noexcept
{
    const auto bit = 1u << static_cast<unsigned>(voice);
    pendingUiVoices_.fetch_or(bit, std::memory_order_release);
}

ke310::rhythmbox::RhythmboxTelemetry RhythmboxAudioProcessor::readTelemetry() const noexcept
{
    ke310::rhythmbox::RhythmboxTelemetry tel{};
    tel.outputRms = static_cast<double>(telRms_.load(std::memory_order_relaxed));
    tel.outputPeak = static_cast<double>(telPeak_.load(std::memory_order_relaxed));
    tel.lamp.effectiveBpm = static_cast<double>(telBpm_.load(std::memory_order_relaxed));

    const std::uint32_t packed = telPacked_.load(std::memory_order_relaxed);
    tel.lamp.isRunning = (packed & 0x01u) != 0u;
    tel.lamp.beatFlash = (packed & 0x02u) != 0u;
    tel.lamp.isDownbeat = (packed & 0x04u) != 0u;
    tel.lamp.usingHostBpm = (packed & 0x08u) != 0u;
    tel.lamp.currentStep = static_cast<int>((packed >> 8u) & 0xFFu);
    tel.lamp.totalSteps = static_cast<int>((packed >> 16u) & 0xFFu);
    return tel;
}

void RhythmboxAudioProcessor::renderSlice(int startSample,
                                          int numSamples,
                                          juce::AudioBuffer<float>& buffer,
                                          const ke310::rhythmbox::HostTransportInfo& host) noexcept
{
    if (numSamples <= 0)
    {
        return;
    }

    const int scratchCap = static_cast<int>(monoScratch_.size());
    int offset = 0;
    ke310::rhythmbox::HostTransportInfo subHost = host;
    const double effBpm = (host.hasBpm && host.bpm >= 10.0) ? host.bpm : engine_.state().bpm;
    const double beatsPerSample = std::max(10.0, effBpm) / (60.0 * sampleRate_);

    const int numCh = buffer.getNumChannels();
    while (offset < numSamples)
    {
        const int chunk = std::min(scratchCap, numSamples - offset);
        engine_.processBlock(monoScratch_.data(), chunk, subHost);

        for (int i = 0; i < chunk; ++i)
        {
            const float sampleF = static_cast<float>(monoScratch_[static_cast<std::size_t>(i)]);
            for (int ch = 0; ch < numCh; ++ch)
            {
                buffer.setSample(ch, startSample + offset + i, sampleF);
            }
        }

        if (subHost.hasPpqPosition)
        {
            subHost.ppqPosition += static_cast<double>(chunk) * beatsPerSample;
        }
        offset += chunk;
    }
}

void RhythmboxAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() <= 0)
    {
        return;
    }

    auto rbState = rawParams_.readState();
    if (std::popcount(rbState.patternMask) > 1)
    {
        rbState.multiPatternEnabled = true;
    }
    engine_.applyState(rbState);
    const int listenChannel = rawParams_.readMidiChannel();

    // Flush queued UI audition pad triggers lock-free at sample 0
    const std::uint32_t uiMask = pendingUiVoices_.exchange(0u, std::memory_order_acq_rel);
    if (uiMask != 0u)
    {
        for (std::size_t v = 0; v < ke310::rhythmbox::kNumVoices; ++v)
        {
            if ((uiMask & (1u << static_cast<unsigned>(v))) != 0u)
            {
                engine_.triggerVoice(static_cast<ke310::rhythmbox::VoiceId>(v), 1.0);
            }
        }
    }

    // Read DAW playhead if available
    ke310::rhythmbox::HostTransportInfo host{};
    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
        {
            host.hasTransport = true;
            host.isPlaying = pos->getIsPlaying();
            if (const auto bpmOpt = pos->getBpm())
            {
                host.hasBpm = true;
                host.bpm = *bpmOpt;
            }
            if (const auto ppqOpt = pos->getPpqPosition())
            {
                host.hasPpqPosition = true;
                host.ppqPosition = *ppqOpt;
            }
        }
    }

    const double sliceBpm = (host.hasBpm && host.bpm >= 10.0) ? host.bpm : rbState.bpm;
    const double beatsPerSample = std::max(10.0, sliceBpm) / (60.0 * sampleRate_);

    // Render sample-accurately around incoming MIDI Note-On events
    int cursor = 0;
    for (const auto meta : midiMessages)
    {
        const int eventSample = std::clamp(meta.samplePosition, 0, numSamples);
        if (eventSample > cursor)
        {
            ke310::rhythmbox::HostTransportInfo sliceHost = host;
            if (sliceHost.hasPpqPosition)
            {
                sliceHost.ppqPosition += static_cast<double>(cursor) * beatsPerSample;
            }
            renderSlice(cursor, eventSample - cursor, buffer, sliceHost);
            cursor = eventSample;
        }

        const auto msg = meta.getMessage();
        if (msg.isNoteOn() && listenChannel >= 0 && (listenChannel == 0 || msg.getChannel() == listenChannel))
        {
            engine_.triggerMidiNote(msg.getNoteNumber(), static_cast<int>(msg.getVelocity()));
        }
    }

    if (cursor < numSamples)
    {
        ke310::rhythmbox::HostTransportInfo tailHost = host;
        if (tailHost.hasPpqPosition)
        {
            tailHost.ppqPosition += static_cast<double>(cursor) * beatsPerSample;
        }
        renderSlice(cursor, numSamples - cursor, buffer, tailHost);
    }

    // Compute whole-block RMS / peak telemetry from rendered output
    const float* ch0 = buffer.getReadPointer(0);
    double sumSq = 0.0;
    double pk = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        const double s = static_cast<double>(ch0[i]);
        sumSq += s * s;
        pk = std::max(pk, std::abs(s));
    }
    const auto& tel = engine_.telemetry();
    telRms_.store(static_cast<float>(std::sqrt(sumSq / static_cast<double>(numSamples))), std::memory_order_relaxed);
    telPeak_.store(static_cast<float>(pk), std::memory_order_relaxed);
    telBpm_.store(static_cast<float>(tel.lamp.effectiveBpm), std::memory_order_relaxed);

    const std::uint32_t packed =
        (tel.lamp.isRunning ? 0x01u : 0u) |
        (tel.lamp.beatFlash ? 0x02u : 0u) |
        (tel.lamp.isDownbeat ? 0x04u : 0u) |
        (tel.lamp.usingHostBpm ? 0x08u : 0u) |
        ((static_cast<std::uint32_t>(std::clamp(tel.lamp.currentStep, 0, 255)) & 0xFFu) << 8u) |
        ((static_cast<std::uint32_t>(std::clamp(tel.lamp.totalSteps, 0, 255)) & 0xFFu) << 16u);
    telPacked_.store(packed, std::memory_order_relaxed);
}

int RhythmboxAudioProcessor::getNumPrograms()
{
    return static_cast<int>(kFactoryRhythmPrograms.size());
}

int RhythmboxAudioProcessor::getCurrentProgram()
{
    return currentProgram_;
}

void RhythmboxAudioProcessor::setCurrentProgram(int index)
{
    if (kFactoryRhythmPrograms.empty())
    {
        return;
    }
    const int idx = std::clamp(index, 0, static_cast<int>(kFactoryRhythmPrograms.size()) - 1);
    currentProgram_ = idx;
    const auto& prog = kFactoryRhythmPrograms[static_cast<std::size_t>(idx)];

    auto setParamNorm = [this](const char* id, float norm)
    {
        if (auto* p = apvts_.getParameter(id))
        {
            p->setValueNotifyingHost(norm);
        }
    };
    auto setParamPlain = [this](const char* id, float plain)
    {
        if (auto* p = apvts_.getParameter(id))
        {
            p->setValueNotifyingHost(p->convertTo0to1(plain));
        }
    };

    setParamNorm(param_ids::rbMultiPattern, 0.0f);
    for (std::size_t p = 0; p < ke310::rhythmbox::kNumPatterns; ++p)
    {
        const bool on = (p == static_cast<std::size_t>(prog.pattern));
        setParamNorm(param_ids::kPatternParamIds[p], on ? 1.0f : 0.0f);
    }
    setParamPlain(param_ids::rbBpm, static_cast<float>(prog.bpm));
}

const juce::String RhythmboxAudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < static_cast<int>(kFactoryRhythmPrograms.size()))
    {
        return kFactoryRhythmPrograms[static_cast<std::size_t>(index)].name;
    }
    return {};
}

void RhythmboxAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = apvts_.copyState();
    if (const auto xml = state.createXml())
    {
        xml->setAttribute("currentProgram", currentProgram_);
        copyXmlToBinary(*xml, destData);
    }
}

void RhythmboxAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (xmlState->hasTagName(apvts_.state.getType()))
        {
            currentProgram_ = std::clamp(
                xmlState->getIntAttribute("currentProgram", currentProgram_),
                0,
                static_cast<int>(kFactoryRhythmPrograms.size()) - 1);
            apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

juce::AudioProcessorEditor* RhythmboxAudioProcessor::createEditor()
{
    return new RhythmboxAudioProcessorEditor(*this);
}

} // namespace ke310::rhythmbox_plugin

#if !defined(KE310_RHYTHMBOX_OMIT_CREATE_PLUGIN_FILTER)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ke310::rhythmbox_plugin::RhythmboxAudioProcessor();
}
#endif
