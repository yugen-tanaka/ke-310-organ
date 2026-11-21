#include "PluginProcessor.hpp"
#include "PluginEditor.hpp"

#include <algorithm>
#include <cmath>

namespace ke310::plugin
{

KE310AudioProcessor::KE310AudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Main Mix", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Rotor Out", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Stationary Out", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE310Parameters", createParameterLayout())
{
    rawParams_.bind(apvts_);
    engine_.prepare(48000.0);
    rotarySpeaker_.prepare(48000.0);
}

void KE310AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const double fs = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    const int scratchSize = std::max(512, samplesPerBlock);
    mixScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    mixLScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    mixRScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    rotorScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    rotorLScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    rotorRScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);
    statScratch_.assign(static_cast<std::size_t>(scratchSize), 0.0f);

    cc11Expression_ = 1.0;
    cc7Volume_ = 1.0;
    cc64UpperSustain_ = false;
    cc64PedalSustain_ = false;

    engine_.prepare(fs);
    rotarySpeaker_.prepare(fs);
    rotarySpeaker_.setConfig(rawParams_.readRotaryConfig());
    rotarySpeedHz_.store(0.0f, std::memory_order_relaxed);
    midiRouter_.reset();
    midiRouter_.setConfig(rawParams_.readMidiConfig());
    syncEngineAndMidiState(rawParams_.readEngineState(), midiRouter_.config());
}

void KE310AudioProcessor::releaseResources()
{
    midiRouter_.handleAllNotesOff(engine_);
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

void KE310AudioProcessor::renderChunk(int startSample, int numSamples, juce::AudioBuffer<float>& buffer) noexcept
{
    if (numSamples <= 0)
    {
        return;
    }

    int offset = 0;
    const int scratchCap = static_cast<int>(mixScratch_.size());
    while (offset < numSamples)
    {
        const int n = std::min(scratchCap, numSamples - offset);
        engine_.processBlock(n, mixScratch_.data(), rotorScratch_.data(), statScratch_.data());
        rotarySpeaker_.processBlock(n, rotorScratch_.data(), rotorLScratch_.data(), rotorRScratch_.data());

        for (int i = 0; i < n; ++i)
        {
            const std::size_t si = static_cast<std::size_t>(i);
            const float statVal = statScratch_[si];
            mixLScratch_[si] = rotorLScratch_[si] + statVal;
            mixRScratch_[si] = rotorRScratch_[si] + statVal;
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
        writeStereoBus(2, statScratch_.data(), statScratch_.data());

        offset += n;
    }
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

    int cursor = 0;
    for (const auto metadata : midiMessages)
    {
        const int evSample = std::clamp(metadata.samplePosition, 0, numSamples);
        if (evSample > cursor)
        {
            renderChunk(cursor, evSample - cursor, buffer);
            cursor = evSample;
        }

        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            midiRouter_.handleNoteOn(msg.getChannel(), msg.getNoteNumber(), engine_);
        }
        else if (msg.isNoteOff())
        {
            midiRouter_.handleNoteOff(msg.getChannel(), msg.getNoteNumber(), engine_);
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            midiRouter_.handleAllNotesOff(engine_);
        }
        else if (msg.isController())
        {
            handleMidiController(msg.getChannel(), msg.getControllerNumber(), msg.getControllerValue(), midiCfg);
            syncEngineAndMidiState(baseState, midiCfg);
        }
    }

    if (numSamples > cursor)
    {
        renderChunk(cursor, numSamples - cursor, buffer);
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
