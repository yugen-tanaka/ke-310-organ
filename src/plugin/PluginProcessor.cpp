#include "PluginProcessor.hpp"
#include "PluginEditor.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace ke310::plugin
{

KE310AudioProcessor::KE310AudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Main Mix", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Rotor Out", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Stationary Out", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Rhythm Out", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE310Parameters", createParameterLayout())
{
    rawParams_.bind(apvts_);
    engine_.prepare(48000.0);
    rotarySpeaker_.prepare(48000.0);
    rhythmbox_.prepare(48000.0);
}

void KE310AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const double fs = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    sampleRate_ = fs;
    const int scratchSize = std::max(512, samplesPerBlock);
    mixScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    mixLScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    mixRScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    rotorScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    rotorLScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    rotorRScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    statScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    rhythmMonoScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0);
    statOutScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    rhythmOutScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);

    cc11Expression_ = 1.0;
    cc7Volume_ = 1.0;
    cc64UpperSustain_ = false;
    cc64PedalSustain_ = false;

    engine_.prepare(fs);
    rotarySpeaker_.prepare(fs);
    rotarySpeaker_.setConfig(rawParams_.readRotaryConfig());
    rotarySpeedHz_.store(0.0f, std::memory_order_relaxed);

    rhythmbox_.prepare(fs);
    uiRhythmStartHeld_.store(false, std::memory_order_release);
    uiRhythmStopHeld_.store(false, std::memory_order_release);
    prevUiRhythmStartHeld_ = false;
    prevUiRhythmStopHeld_ = false;
    rhythmLatchedRunning_ = false;
    prevRhythmTriggerHeld_ = false;
    prevHostPlaying_ = false;
    auto rbState = rawParams_.rhythmbox.readState();
    prevRhythmArmed_ = rbState.enabled;
    prevRhythmBreak_ = rawParams_.readRhythmBreak();
    prevRhythmClockMode_ = rbState.clockMode;
    rbState.enabled = false;
    rhythmbox_.applyState(rbState);

    midiRouter_.reset();
    midiRouter_.setConfig(rawParams_.readMidiConfig());
    syncEngineAndMidiState(rawParams_.readEngineState(), midiRouter_.config());
}

void KE310AudioProcessor::releaseResources()
{
    midiRouter_.handleAllNotesOff(engine_);
    uiRhythmStartHeld_.store(false, std::memory_order_release);
    uiRhythmStopHeld_.store(false, std::memory_order_release);
    prevUiRhythmStartHeld_ = false;
    prevUiRhythmStopHeld_ = false;
    rhythmLatchedRunning_ = false;
    prevRhythmTriggerHeld_ = false;
    prevHostPlaying_ = false;
    rhythmbox_.restartSequencer();
}

bool KE310AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (!layouts.inputBuses.isEmpty())
    {
        return false;
    }

    const auto& mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
    {
        return false;
    }

    for (int busIdx = 1; busIdx < layouts.outputBuses.size(); ++busIdx)
    {
        const auto& aux = layouts.outputBuses.getReference(busIdx);
        if (!aux.isDisabled()
            && aux != juce::AudioChannelSet::mono()
            && aux != juce::AudioChannelSet::stereo())
        {
            return false;
        }
    }
    return true;
}

void KE310AudioProcessor::syncEngineAndMidiState(const dsp::KE310State& baseState,
                                                 const dsp::MidiRouterConfig& midiCfg) noexcept
{
    midiRouter_.setConfig(midiCfg, engine_);

    dsp::KE310State eff = baseState;
    eff.expression = std::clamp(baseState.expression * cc11Expression_ * cc7Volume_, 0.0, 1.0);
    eff.kneeSustain = (baseState.kneeSustain || cc64UpperSustain_);
    eff.pedalSustain = (baseState.pedalSustain || cc64PedalSustain_);
    engine_.applyState(eff);
}

void KE310AudioProcessor::handleMidiController(int channel, int ccNumber, int value,
                                               const dsp::MidiRouterConfig& midiCfg) noexcept
{
    if (ccNumber == 11) // Expression Pedal
    {
        cc11Expression_ = static_cast<double>(std::clamp(value, 0, 127)) / 127.0;
    }
    else if (ccNumber == 7) // Channel Volume
    {
        cc7Volume_ = static_cast<double>(std::clamp(value, 0, 127)) / 127.0;
    }
    else if (ccNumber == 64) // Sustain / Knee Sustainer
    {
        const bool down = (value >= 64);
        if (midiCfg.mode == dsp::MidiMode::SingleKeyboardSplit)
        {
            if (midiCfg.splitInputChannel == dsp::kMidiChannelOmni || midiCfg.splitInputChannel == channel)
            {
                cc64UpperSustain_ = down;
            }
        }
        else
        {
            const bool matchUpper = (midiCfg.upperChannel == dsp::kMidiChannelOmni || midiCfg.upperChannel == channel);
            const bool matchPedal = (midiCfg.pedalChannel == dsp::kMidiChannelOmni || midiCfg.pedalChannel == channel);
            if (matchUpper)
            {
                cc64UpperSustain_ = down;
            }
            if (matchPedal)
            {
                cc64PedalSustain_ = down;
            }
        }
    }
    else if (ccNumber == 120 || ccNumber == 123)
    {
        midiRouter_.handleAllNotesOff(engine_);
    }
}

void KE310AudioProcessor::renderChunk(int startSample,
                                      int numSamples,
                                      juce::AudioBuffer<float>& buffer,
                                      const ke310::rhythmbox::HostTransportInfo& host,
                                      int rhythmOutputMode) noexcept
{
    if (numSamples <= 0)
    {
        return;
    }

    int offset = 0;
    const int scratchCap = static_cast<int>(mixScratch_.size());
    ke310::rhythmbox::HostTransportInfo subHost = host;
    const double effBpm = (host.hasBpm && host.bpm >= 10.0) ? host.bpm : rhythmbox_.state().bpm;
    const double beatsPerSample = std::max(10.0, effBpm) / (60.0 * sampleRate_);

    while (offset < numSamples)
    {
        const int n = std::min(scratchCap, numSamples - offset);
        engine_.processBlock(n, mixScratch_.data(), rotorScratch_.data(), statScratch_.data());
        rotarySpeaker_.processBlock(n, rotorScratch_.data(), rotorLScratch_.data(), rotorRScratch_.data());
        rhythmbox_.processBlock(rhythmMonoScratch_.data(), n, subHost);

        if (subHost.hasPpqPosition)
        {
            subHost.ppqPosition += static_cast<double>(n) * beatsPerSample;
        }

        for (int i = 0; i < n; ++i)
        {
            const std::size_t si = static_cast<std::size_t>(i);
            const float statVal = statScratch_[si];
            const float rVal = static_cast<float>(rhythmMonoScratch_[si]);

            if (rhythmOutputMode == 0)
            {
                // Default hardware-safe mode: bypasses Rotor/ENSEMBLE and sums post-ensemble into Stationary + Main
                statOutScratch_[si] = statVal + rVal;
                rhythmOutScratch_[si] = 0.0f;
                mixLScratch_[si] = rotorLScratch_[si] + statOutScratch_[si];
                mixRScratch_[si] = rotorRScratch_[si] + statOutScratch_[si];
            }
            else if (rhythmOutputMode == 1)
            {
                statOutScratch_[si] = statVal;
                rhythmOutScratch_[si] = rVal;
                mixLScratch_[si] = rotorLScratch_[si] + statVal + rVal;
                mixRScratch_[si] = rotorRScratch_[si] + statVal + rVal;
            }
            else
            {
                // Mode 2: Dedicated Rhythm Out isolated from Main Mix & Stationary Out
                statOutScratch_[si] = statVal;
                rhythmOutScratch_[si] = rVal;
                mixLScratch_[si] = rotorLScratch_[si] + statVal;
                mixRScratch_[si] = rotorRScratch_[si] + statVal;
            }
        }

        auto writeStereoBus = [&](int busIdx, const float* srcL, const float* srcR) noexcept
        {
            if (busIdx >= getBusCount(false))
            {
                return;
            }
            auto busBuf = getBusBuffer(buffer, false, busIdx);
            const int nCh = busBuf.getNumChannels();
            if (nCh <= 0)
            {
                return;
            }
            if (nCh == 1)
            {
                float* dst = busBuf.getWritePointer(0, startSample + offset);
                for (int i = 0; i < n; ++i)
                {
                    dst[i] = 0.5f * (srcL[i] + srcR[i]);
                }
                return;
            }
            busBuf.copyFrom(0, startSample + offset, srcL, n);
            busBuf.copyFrom(1, startSample + offset, srcR, n);
        };

        writeStereoBus(0, mixLScratch_.data(), mixRScratch_.data());
        writeStereoBus(1, rotorLScratch_.data(), rotorRScratch_.data());
        writeStereoBus(2, statOutScratch_.data(), statOutScratch_.data());
        writeStereoBus(3, rhythmOutScratch_.data(), rhythmOutScratch_.data());

        offset += n;
    }
}

void KE310AudioProcessor::setRhythmStartHeldFromUi(bool held) noexcept
{
    uiRhythmStartHeld_.store(held, std::memory_order_release);
}

void KE310AudioProcessor::setRhythmStopHeldFromUi(bool held) noexcept
{
    uiRhythmStopHeld_.store(held, std::memory_order_release);
}

bool KE310AudioProcessor::isStandaloneMode() const noexcept
{
    return forceStandaloneForTest_ || (wrapperType == wrapperType_Standalone);
}

bool KE310AudioProcessor::updateRhythmGateState(bool rhythmArmed,
                                                bool hasPattern,
                                                bool breakOn,
                                                bool uiStartHeld,
                                                bool uiStopHeld) noexcept
{
    const bool triggerHeld = engine_.isPedalHeld()
                          || engine_.hasHeldLowerNoteInRange(36, 72)
                          || uiStartHeld;
    bool effectiveRunning = false;

    if (!rhythmArmed || !hasPattern || uiStopHeld)
    {
        rhythmLatchedRunning_ = false;
        if (prevRhythmTriggerHeld_ && breakOn)
        {
            rhythmbox_.restartSequencer();
        }
        effectiveRunning = false;
    }
    else if (breakOn)
    {
        rhythmLatchedRunning_ = false;
        if (!triggerHeld && prevRhythmTriggerHeld_)
        {
            rhythmbox_.restartSequencer();
        }
        effectiveRunning = triggerHeld;
    }
    else
    {
        if (triggerHeld)
        {
            rhythmLatchedRunning_ = true;
        }
        effectiveRunning = rhythmLatchedRunning_;
    }

    prevRhythmTriggerHeld_ = triggerHeld;
    return effectiveRunning;
}

ke310::rhythmbox::RhythmboxTelemetry KE310AudioProcessor::readRhythmTelemetry() const noexcept
{
    ke310::rhythmbox::RhythmboxTelemetry tel{};
    tel.outputRms = static_cast<double>(rbTelRms_.load(std::memory_order_relaxed));
    tel.outputPeak = static_cast<double>(rbTelPeak_.load(std::memory_order_relaxed));
    tel.lamp.effectiveBpm = static_cast<double>(rbTelBpm_.load(std::memory_order_relaxed));

    const std::uint32_t packed = rbTelPacked_.load(std::memory_order_relaxed);
    tel.lamp.isRunning = (packed & 0x01u) != 0u;
    tel.lamp.beatFlash = (packed & 0x02u) != 0u;
    tel.lamp.isDownbeat = (packed & 0x04u) != 0u;
    tel.lamp.usingHostBpm = (packed & 0x08u) != 0u;
    tel.lamp.currentStep = static_cast<int>((packed >> 8u) & 0xFFu);
    tel.lamp.totalSteps = static_cast<int>((packed >> 16u) & 0xFFu);
    return tel;
}

void KE310AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    const dsp::KE310State baseState = rawParams_.readEngineState();
    const dsp::MidiRouterConfig midiCfg = rawParams_.readMidiConfig();
    rotarySpeaker_.setConfig(rawParams_.readRotaryConfig());
    syncEngineAndMidiState(baseState, midiCfg);

    auto rbBaseState = rawParams_.rhythmbox.readState();
    if (std::popcount(rbBaseState.patternMask) > 1)
    {
        rbBaseState.multiPatternEnabled = true;
    }
    const bool hasPattern = (rbBaseState.patternMask != 0u);
    const bool rbArmed = rbBaseState.enabled;
    const bool rbBreak = rawParams_.readRhythmBreak();
    const int rbOutMode = rawParams_.readRhythmOutputBus();
    const int rbListenCh = rawParams_.rhythmbox.readMidiChannel();
    const bool uiStartHeld = uiRhythmStartHeld_.load(std::memory_order_acquire);
    const bool uiStopHeld = uiRhythmStopHeld_.load(std::memory_order_acquire);

    ke310::rhythmbox::HostTransportInfo host{};
    if (!isStandaloneMode())
    {
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
    }

    if (uiStopHeld && !prevUiRhythmStopHeld_)
    {
        rhythmLatchedRunning_ = false;
        rhythmbox_.restartSequencer();
    }
    if (!rbArmed && prevRhythmArmed_)
    {
        rhythmLatchedRunning_ = false;
        rhythmbox_.restartSequencer();
    }
    if (rbBreak && !prevRhythmBreak_)
    {
        rhythmLatchedRunning_ = false;
        rhythmbox_.restartSequencer();
    }

    if (rbBaseState.clockMode == ke310::rhythmbox::ClockMode::HostSync && host.hasTransport)
    {
        if (!host.isPlaying && prevHostPlaying_)
        {
            rhythmLatchedRunning_ = false;
            rhythmbox_.restartSequencer();
        }
        else if (((host.isPlaying && !prevHostPlaying_) ||
                  (rbArmed && !prevRhythmArmed_ && host.isPlaying) ||
                  (prevRhythmClockMode_ != ke310::rhythmbox::ClockMode::HostSync && host.isPlaying)) &&
                 rbArmed && hasPattern && !rbBreak && !uiStopHeld)
        {
            rhythmLatchedRunning_ = true;
        }
    }

    prevUiRhythmStartHeld_ = uiStartHeld;
    prevUiRhythmStopHeld_ = uiStopHeld;
    prevRhythmArmed_ = rbArmed;
    prevRhythmBreak_ = rbBreak;
    prevHostPlaying_ = host.hasTransport && host.isPlaying;
    prevRhythmClockMode_ = rbBaseState.clockMode;

    auto effectiveRbState = rbBaseState;
    auto applyEffectiveRb = [&]() noexcept
    {
        effectiveRbState = rbBaseState;
        // In HostSync: lock to DAW PPQ when DAW is playing and BREAK is OFF.
        // When DAW is stopped (manual START / Lower / Pedal preview) or BREAK is ON (Beat 1 restart),
        // use TempoSync so rhythm follows DAW BPM without being blocked by !host.isPlaying or jumping mid-bar.
        if (effectiveRbState.clockMode == ke310::rhythmbox::ClockMode::HostSync &&
            (!host.hasTransport || !host.isPlaying || rbBreak))
        {
            effectiveRbState.clockMode = ke310::rhythmbox::ClockMode::TempoSync;
        }
        effectiveRbState.enabled = updateRhythmGateState(rbArmed, hasPattern, rbBreak, uiStartHeld, uiStopHeld);
        rhythmbox_.applyState(effectiveRbState);
    };
    applyEffectiveRb();

    const double sliceBpm = (host.hasBpm && host.bpm >= 10.0) ? host.bpm : rbBaseState.bpm;
    const double beatsPerSample = std::max(10.0, sliceBpm) / (60.0 * sampleRate_);

    int cursor = 0;
    for (const auto metadata : midiMessages)
    {
        const int evSample = std::clamp(metadata.samplePosition, 0, numSamples);
        if (evSample > cursor)
        {
            ke310::rhythmbox::HostTransportInfo sliceHost = host;
            if (sliceHost.hasPpqPosition)
            {
                sliceHost.ppqPosition += static_cast<double>(cursor) * beatsPerSample;
            }
            renderChunk(cursor, evSample - cursor, buffer, sliceHost, rbOutMode);
            cursor = evSample;
        }

        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            midiRouter_.handleNoteOn(msg.getChannel(), msg.getNoteNumber(), engine_);
            if (rbListenCh >= 0 && (rbListenCh == 0 || msg.getChannel() == rbListenCh))
            {
                rhythmbox_.triggerMidiNote(msg.getNoteNumber(), static_cast<int>(msg.getVelocity()));
            }
            applyEffectiveRb();
        }
        else if (msg.isNoteOff())
        {
            midiRouter_.handleNoteOff(msg.getChannel(), msg.getNoteNumber(), engine_);
            applyEffectiveRb();
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            midiRouter_.handleAllNotesOff(engine_);
            rhythmLatchedRunning_ = false;
            prevRhythmTriggerHeld_ = false;
            rhythmbox_.restartSequencer();
            effectiveRbState.enabled = false;
            rhythmbox_.applyState(effectiveRbState);
        }
        else if (msg.isController())
        {
            handleMidiController(msg.getChannel(), msg.getControllerNumber(), msg.getControllerValue(), midiCfg);
            syncEngineAndMidiState(baseState, midiCfg);
            applyEffectiveRb();
        }
    }

    if (numSamples > cursor)
    {
        ke310::rhythmbox::HostTransportInfo tailHost = host;
        if (tailHost.hasPpqPosition)
        {
            tailHost.ppqPosition += static_cast<double>(cursor) * beatsPerSample;
        }
        renderChunk(cursor, numSamples - cursor, buffer, tailHost, rbOutMode);
    }

    auto calcBusRms = [&](int busIdx) noexcept -> float
    {
        if (busIdx >= getBusCount(false) || numSamples <= 0)
        {
            return 0.0f;
        }
        const auto busBuf = getBusBuffer(buffer, false, busIdx);
        const int nCh = busBuf.getNumChannels();
        if (nCh <= 0)
        {
            return 0.0f;
        }
        if (nCh == 1)
        {
            return busBuf.getRMSLevel(0, 0, numSamples);
        }
        const float rL = busBuf.getRMSLevel(0, 0, numSamples);
        const float rR = busBuf.getRMSLevel(1, 0, numSamples);
        return std::sqrt(0.5f * (rL * rL + rR * rR));
    };

    mainRms_.store(calcBusRms(0), std::memory_order_relaxed);
    rotorRms_.store(calcBusRms(1), std::memory_order_relaxed);
    statRms_.store(calcBusRms(2), std::memory_order_relaxed);
    rotarySpeedHz_.store(static_cast<float>(rotarySpeaker_.currentSpeedHz()), std::memory_order_relaxed);
    upperActiveNotes_.store(engine_.activeUpperNoteCount(), std::memory_order_relaxed);
    lowerActiveNotes_.store(engine_.activeLowerNoteCount(), std::memory_order_relaxed);
    pedalActiveMidi_.store(engine_.activePedalMidiNote(), std::memory_order_relaxed);

    const auto& rbTel = rhythmbox_.telemetry();
    rbTelRms_.store(static_cast<float>(rbTel.outputRms), std::memory_order_relaxed);
    rbTelPeak_.store(static_cast<float>(rbTel.outputPeak), std::memory_order_relaxed);
    rbTelBpm_.store(static_cast<float>(rbTel.lamp.effectiveBpm), std::memory_order_relaxed);

    const std::uint32_t packed =
        (rbTel.lamp.isRunning ? 0x01u : 0u) |
        (rbTel.lamp.beatFlash ? 0x02u : 0u) |
        (rbTel.lamp.isDownbeat ? 0x04u : 0u) |
        (rbTel.lamp.usingHostBpm ? 0x08u : 0u) |
        ((static_cast<std::uint32_t>(std::clamp(rbTel.lamp.currentStep, 0, 255)) & 0xFFu) << 8u) |
        ((static_cast<std::uint32_t>(std::clamp(rbTel.lamp.totalSteps, 0, 255)) & 0xFFu) << 16u);
    rbTelPacked_.store(packed, std::memory_order_relaxed);
}

int KE310AudioProcessor::getNumPrograms()
{
    return static_cast<int>(kNumFactoryPresets);
}

int KE310AudioProcessor::getCurrentProgram()
{
    return currentProgram_;
}

void KE310AudioProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= getNumPrograms())
    {
        return;
    }
    currentProgram_ = index;
    applyFactoryPreset(apvts_, index);
}

const juce::String KE310AudioProcessor::getProgramName(int index)
{
    if (index >= 0 && static_cast<std::size_t>(index) < kNumFactoryPresets)
    {
        return kFactoryPresets[static_cast<std::size_t>(index)].name;
    }
    return {};
}

void KE310AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    state.setProperty("currentProgram", currentProgram_, nullptr);
    if (auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void KE310AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts_.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml(*xml);
            currentProgram_ = std::clamp(static_cast<int>(vt.getProperty("currentProgram", 0)),
                                         0, std::max(0, getNumPrograms() - 1));
            apvts_.replaceState(vt);
        }
    }
}

juce::AudioProcessorEditor* KE310AudioProcessor::createEditor()
{
    return new KE310AudioProcessorEditor(*this);
}

} // namespace ke310::plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ke310::plugin::KE310AudioProcessor();
}
