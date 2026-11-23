// Pure C++20 zero-allocation DSP synthesis & sequencer engine for KE Rhythmbox.
#pragma once

#include "../Biquad.hpp"
#include "RhythmboxClock.hpp"
#include "RhythmboxParams.hpp"
#include "RhythmboxVoices.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ke310::rhythmbox
{

struct RhythmboxState
{
    bool enabled = false;
    std::uint16_t patternMask = static_cast<std::uint16_t>(1u << static_cast<std::size_t>(PatternId::Rock1));
    bool multiPatternEnabled = false;
    bool variation = false;
    ClockMode clockMode = ClockMode::TempoSync;
    double bpm = 106.8;
    double volume = 0.85;
    double tone = 0.5;
    double analogVariation = 1.0;
};

struct RhythmboxTelemetry
{
    double outputRms = 0.0;
    double outputPeak = 0.0;
    TempoLampState lamp{};
};

class RhythmboxEngine
{
public:
    static constexpr int kMaxChunk = 128;

    void prepare(double sampleRate) noexcept
    {
        fs_ = std::max(8000.0, sampleRate);

        bassDrum_.prepare(fs_, kBassDrumSpec);
        lowConga_.prepare(fs_, kLowCongaSpec);
        highConga_.prepare(fs_, kHighCongaSpec);
        snareBody_.prepare(fs_, kSnareBodySpec);
        lowBongo_.prepare(fs_, kLowBongoSpec);
        rimShot_.prepare(fs_, kRimShotSpec);
        claves_.prepare(fs_, kClavesSpec);
        cowBell_.prepare(fs_, kCowBellSpec);

        hhVca_.prepare(fs_, kHighHatVcaSpec);
        cymVca_.prepare(fs_, kCymbalVcaSpec);
        snareNoiseVca_.prepare(fs_, kSnareNoiseVcaSpec);
        tambVca_.prepare(fs_, kTambourineVcaSpec);

        metalFilter_.design(kMetalBusFilterSpec.sections, kMetalBusFilterSpec.numSections,
                            kMetalBusFilterSpec.filterGainDb, fs_);
        snareNoiseFilter_.design(kSnareNoiseFilterSpec.sections, kSnareNoiseFilterSpec.numSections,
                                 kSnareNoiseFilterSpec.filterGainDb, fs_);
        tambFilter_.design(kTambourineFilterSpec.sections, kTambourineFilterSpec.numSections,
                           kTambourineFilterSpec.filterGainDb, fs_);

        lastTone_ = 0.5;
        toneActive_ = false;
        reset();
    }

    void reset() noexcept
    {
        noiseGen_.reset(0x12345678u);
        driftGen_.reset(0x9E3779B9u);

        bassDrum_.reset();
        lowConga_.reset();
        highConga_.reset();
        snareBody_.reset();
        lowBongo_.reset();
        rimShot_.reset();
        claves_.reset();
        cowBell_.reset();

        hhVca_.reset();
        cymVca_.reset();
        snareNoiseVca_.reset();
        tambVca_.reset();

        metalFilter_.reset();
        snareNoiseFilter_.reset();
        tambFilter_.reset();
        toneLowFilter_.reset();
        toneHighFilter_.reset();

        railSagEnv_ = 0.0;
        beatPos_ = 0.0;
        idleBeatPhase_ = 0.0;
        prevIdleMask_ = 0u;
        lastStepIndices_.fill(-1);
        wasRunning_ = false;
        hasExpectedPpq_ = false;
        expectedNextPpq_ = 0.0;
        prevActiveMask_ = 0u;
        telemetry_ = RhythmboxTelemetry{};
    }

    void restartSequencer() noexcept
    {
        wasRunning_ = false;
        hasExpectedPpq_ = false;
        beatPos_ = 0.0;
        lastStepIndices_.fill(-1);
    }

    void applyState(const RhythmboxState& newState) noexcept
    {
        state_ = newState;
        if (!state_.multiPatternEnabled)
        {
            const std::uint16_t mask = static_cast<std::uint16_t>(state_.patternMask & 0xFFFFu);
            if (mask != 0u)
            {
                state_.patternMask = static_cast<std::uint16_t>(mask & static_cast<std::uint16_t>(~mask + 1u));
            }
        }

        const double t = std::clamp(state_.tone, 0.0, 1.0);
        if (std::abs(t - lastTone_) > 1e-6)
        {
            lastTone_ = t;
            if (std::abs(t - 0.5) <= 1e-6)
            {
                toneActive_ = false;
            }
            else
            {
                const double tiltDb = kToneCircuit.maxTiltDb * ((t - 0.5) * 2.0);
                // Literal q=0.707 matches Python BiquadCascade(tone_circuit) exact bypass/tilt parity
                const ke310::dsp::FilterSectionSpec loSpec{
                    ke310::dsp::FilterType::LowShelf, kToneCircuit.lowShelfHz, 0.707, -tiltDb};
                const ke310::dsp::FilterSectionSpec hiSpec{
                    ke310::dsp::FilterType::HighShelf, kToneCircuit.highShelfHz, 0.707, tiltDb};
                toneLowFilter_.design(&loSpec, 1u, 0.0, fs_);
                toneHighFilter_.design(&hiSpec, 1u, 0.0, fs_);
                if (!toneActive_)
                {
                    toneLowFilter_.reset();
                    toneHighFilter_.reset();
                }
                toneActive_ = true;
            }
        }
    }

    void triggerVoice(VoiceId voice, double velocity = 1.0) noexcept
    {
        const double vel = std::max(0.0, velocity);
        if (vel <= 0.0)
        {
            return;
        }

        const DriftTriple d = computeHitDrift(voice, vel);

        switch (voice)
        {
            case VoiceId::BassDrum:
                bassDrum_.trigger(d.effVel, d.pitchMult, d.decayMult);
                if (state_.analogVariation > 1e-6)
                {
                    railSagEnv_ = std::min(1.5, railSagEnv_ + 1.0);
                }
                break;
            case VoiceId::LowConga:
                lowConga_.trigger(d.effVel, d.pitchMult, d.decayMult);
                break;
            case VoiceId::HighConga:
                highConga_.trigger(d.effVel, d.pitchMult, d.decayMult);
                break;
            case VoiceId::SnareDrum:
                snareBody_.trigger(d.effVel, d.pitchMult, d.decayMult);
                snareNoiseVca_.trigger(d.effVel, d.decayMult);
                break;
            case VoiceId::LowBongo:
                lowBongo_.trigger(d.effVel, d.pitchMult, d.decayMult);
                break;
            case VoiceId::RimShot:
                rimShot_.trigger(d.effVel, d.pitchMult, d.decayMult);
                break;
            case VoiceId::Claves:
                claves_.trigger(d.effVel, d.pitchMult, d.decayMult);
                break;
            case VoiceId::CowBell:
                cowBell_.trigger(d.effVel, d.pitchMult, d.decayMult);
                break;
            case VoiceId::HighHat:
                hhVca_.trigger(d.effVel, d.decayMult);
                break;
            case VoiceId::Cymbal:
                cymVca_.trigger(d.effVel, d.decayMult);
                break;
            case VoiceId::Tambourine:
                tambVca_.trigger(d.effVel, d.decayMult);
                break;
        }
    }

    bool triggerMidiNote(int midiNote, int velocity = 100) noexcept
    {
        if (velocity <= 0)
        {
            return false;
        }
        const auto voice = mapGmMidiNoteToVoice(midiNote);
        if (!voice.has_value())
        {
            return false;
        }
        triggerVoice(*voice, static_cast<double>(velocity) / 127.0);
        return true;
    }

    void processBlock(double* outMono, int numSamples, const HostTransportInfo& host = {}) noexcept
    {
        if (outMono == nullptr || numSamples <= 0)
        {
            return;
        }

        std::uint16_t activeMask = resolveActiveMask();
        const bool hostHasBpm = host.hasBpm && host.bpm >= 10.0;
        const bool usingHostBpm = (state_.clockMode != ClockMode::Internal) && hostHasBpm;
        const bool useHostClock = (state_.clockMode == ClockMode::HostSync) && host.hasTransport;

        bool isRunning = false;
        double effectiveBpm = state_.bpm;
        bool useHostPpq = false;

        if (useHostClock)
        {
            isRunning = state_.enabled && host.isPlaying && (activeMask != 0u);
            effectiveBpm = std::max(10.0, hostHasBpm ? host.bpm : state_.bpm);
            useHostPpq = host.hasPpqPosition;
        }
        else if (state_.clockMode == ClockMode::TempoSync)
        {
            isRunning = state_.enabled && (activeMask != 0u);
            effectiveBpm = std::max(10.0, hostHasBpm ? host.bpm : state_.bpm);
        }
        else
        {
            isRunning = state_.enabled && (activeMask != 0u);
            effectiveBpm = std::max(10.0, state_.bpm);
        }

        if (!isRunning)
        {
            int pos = 0;
            while (pos < numSamples)
            {
                const int blk = std::min(kMaxChunk, numSamples - pos);
                processVoicesChunk(outMono + pos, blk);
                pos += blk;
            }

            double phaseForLamp = 0.0;
            if (activeMask == 0u)
            {
                idleBeatPhase_ = 0.0;
                prevIdleMask_ = 0u;
            }
            else
            {
                if (prevIdleMask_ == 0u)
                {
                    idleBeatPhase_ = 0.0;
                }
                phaseForLamp = idleBeatPhase_;
                const double idleBps = effectiveBpm / (60.0 * fs_);
                idleBeatPhase_ += static_cast<double>(numSamples) * idleBps;
                idleBeatPhase_ -= std::floor(idleBeatPhase_);
                prevIdleMask_ = activeMask;
            }

            wasRunning_ = false;
            hasExpectedPpq_ = false;
            prevActiveMask_ = 0u;
            updateTelemetry(outMono, numSamples, false, effectiveBpm, activeMask, phaseForLamp, usingHostBpm);
            return;
        }

        idleBeatPhase_ = 0.0;
        prevIdleMask_ = activeMask;

        if (useHostPpq)
        {
            const bool needResync = !wasRunning_ || !hasExpectedPpq_ ||
                                    std::abs(host.ppqPosition - expectedNextPpq_) > 0.05;
            if (needResync)
            {
                beatPos_ = host.ppqPosition;
                for (std::size_t p = 0; p < kNumPatterns; ++p)
                {
                    if ((activeMask & (1u << p)) != 0u)
                    {
                        lastStepIndices_[p] = computeInitialLastStepForPpq(beatPos_, kPatterns[p]);
                    }
                    else
                    {
                        lastStepIndices_[p] = -1;
                    }
                }
            }
            else if (activeMask != prevActiveMask_)
            {
                for (std::size_t p = 0; p < kNumPatterns; ++p)
                {
                    const std::uint16_t bit = static_cast<std::uint16_t>(1u << p);
                    if ((activeMask & bit) != 0u && (prevActiveMask_ & bit) == 0u)
                    {
                        lastStepIndices_[p] = computeInitialLastStepForPpq(beatPos_, kPatterns[p]);
                    }
                }
            }
        }
        else
        {
            if (!wasRunning_)
            {
                beatPos_ = 0.0;
                lastStepIndices_.fill(-1);
            }
            else if (activeMask != prevActiveMask_)
            {
                for (std::size_t p = 0; p < kNumPatterns; ++p)
                {
                    const std::uint16_t bit = static_cast<std::uint16_t>(1u << p);
                    if ((activeMask & bit) != 0u && (prevActiveMask_ & bit) == 0u)
                    {
                        lastStepIndices_[p] = computeInitialLastStepForPpq(beatPos_, kPatterns[p]);
                    }
                }
            }
        }

        const double beatsPerSample = effectiveBpm / (60.0 * fs_);
        int pos = 0;

        while (pos < numSamples)
        {
            const int rem = numSamples - pos;

            // 1. Trigger due steps across active patterns (Wired-OR deduplication in canonical VoiceId order)
            std::uint16_t voicesToFireMask = 0u;
            for (std::size_t p = 0; p < kNumPatterns; ++p)
            {
                if ((activeMask & (1u << p)) == 0u)
                {
                    continue;
                }
                if (beatPos_ < -1e-9)
                {
                    continue;
                }
                const PatternSpec& pat = kPatterns[p];
                const double stepsPerBeat = static_cast<double>(pat.stepsPerBar) / pat.beatsPerBar;
                const std::int64_t stepIdxAbs = static_cast<std::int64_t>(std::floor(beatPos_ * stepsPerBeat + 1e-9));
                if (stepIdxAbs > lastStepIndices_[p])
                {
                    lastStepIndices_[p] = stepIdxAbs;
                    const int stepMod = positiveMod(stepIdxAbs, pat.totalSteps);
                    const std::uint64_t stepBit = 1ULL << static_cast<unsigned>(stepMod);
                    for (std::size_t v = 0; v < kNumVoices; ++v)
                    {
                        if ((pat.voiceStepMasks[v] & stepBit) != 0ULL)
                        {
                            voicesToFireMask |= static_cast<std::uint16_t>(1u << v);
                        }
                    }
                }
            }

            if (voicesToFireMask != 0u)
            {
                for (std::size_t v = 0; v < kNumVoices; ++v)
                {
                    if ((voicesToFireMask & (1u << v)) != 0u)
                    {
                        triggerVoice(static_cast<VoiceId>(v), 1.0);
                    }
                }
            }

            // 2. Advance until the next earliest pattern step crossing (or kMaxChunk)
            int nextCrossSamples = rem;
            for (std::size_t p = 0; p < kNumPatterns; ++p)
            {
                if ((activeMask & (1u << p)) == 0u)
                {
                    continue;
                }
                const PatternSpec& pat = kPatterns[p];
                const double stepsPerBeat = static_cast<double>(pat.stepsPerBar) / pat.beatsPerBar;
                const double curStepFloat = beatPos_ * stepsPerBeat + 1e-9;
                const double nextStepInt = std::floor(curStepFloat) + 1.0;
                const int samplesToNext = static_cast<int>(
                    std::ceil((nextStepInt / stepsPerBeat - beatPos_) / beatsPerSample));
                nextCrossSamples = std::min(nextCrossSamples, std::max(1, samplesToNext));
            }

            const int chunk = std::min({kMaxChunk, rem, std::max(1, nextCrossSamples)});
            processVoicesChunk(outMono + pos, chunk);
            beatPos_ += static_cast<double>(chunk) * beatsPerSample;
            pos += chunk;
        }

        wasRunning_ = true;
        prevActiveMask_ = activeMask;
        if (useHostPpq)
        {
            expectedNextPpq_ = beatPos_;
            hasExpectedPpq_ = true;
        }
        else
        {
            hasExpectedPpq_ = false;
        }

        updateTelemetry(outMono, numSamples, true, effectiveBpm, activeMask, 0.0, usingHostBpm);
    }

    void processBlockStereo(float* outL, float* outR, int numSamples,
                            const HostTransportInfo& host = {},
                            double* monoScratch = nullptr) noexcept
    {
        if (numSamples <= 0)
        {
            return;
        }

        if (monoScratch != nullptr)
        {
            processBlock(monoScratch, numSamples, host);
            for (int i = 0; i < numSamples; ++i)
            {
                const float s = static_cast<float>(monoScratch[i]);
                if (outL != nullptr)
                {
                    outL[i] = s;
                }
                if (outR != nullptr)
                {
                    outR[i] = s;
                }
            }
            return;
        }

        // Fallback stack-chunked path requiring zero scratch allocation from caller
        std::array<double, kMaxChunk> localMono{};
        int pos = 0;
        HostTransportInfo subHost = host;
        const double hostBpm = (host.hasBpm && host.bpm >= 10.0) ? host.bpm : state_.bpm;
        const double beatsPerSample = std::max(10.0, hostBpm) / (60.0 * fs_);

        while (pos < numSamples)
        {
            const int blk = std::min(kMaxChunk, numSamples - pos);
            processBlock(localMono.data(), blk, subHost);
            for (int i = 0; i < blk; ++i)
            {
                const float s = static_cast<float>(localMono[static_cast<std::size_t>(i)]);
                if (outL != nullptr)
                {
                    outL[pos + i] = s;
                }
                if (outR != nullptr)
                {
                    outR[pos + i] = s;
                }
            }
            if (subHost.hasPpqPosition)
            {
                subHost.ppqPosition += static_cast<double>(blk) * beatsPerSample;
            }
            pos += blk;
        }
    }

    double maxPoleRadius() const noexcept
    {
        double maxR = 0.0;
        maxR = std::max(maxR, bassDrum_.maxPoleRadius());
        maxR = std::max(maxR, lowConga_.maxPoleRadius());
        maxR = std::max(maxR, highConga_.maxPoleRadius());
        maxR = std::max(maxR, snareBody_.maxPoleRadius());
        maxR = std::max(maxR, lowBongo_.maxPoleRadius());
        maxR = std::max(maxR, rimShot_.maxPoleRadius());
        maxR = std::max(maxR, claves_.maxPoleRadius());
        maxR = std::max(maxR, cowBell_.maxPoleRadius());
        maxR = std::max(maxR, metalFilter_.maxPoleRadius());
        maxR = std::max(maxR, snareNoiseFilter_.maxPoleRadius());
        maxR = std::max(maxR, tambFilter_.maxPoleRadius());
        if (toneActive_)
        {
            maxR = std::max(maxR, toneLowFilter_.maxPoleRadius());
            maxR = std::max(maxR, toneHighFilter_.maxPoleRadius());
        }
        return maxR;
    }

    const RhythmboxState& state() const noexcept { return state_; }
    const RhythmboxTelemetry& telemetry() const noexcept { return telemetry_; }
    double sampleRate() const noexcept { return fs_; }

private:
    struct DriftTriple
    {
        double effVel;
        double pitchMult;
        double decayMult;
    };

    static int positiveMod(std::int64_t val, int mod) noexcept
    {
        const std::int64_t r = val % static_cast<std::int64_t>(mod);
        return static_cast<int>(r < 0 ? (r + mod) : r);
    }

    static std::int64_t computeInitialLastStepForPpq(double ppq, const PatternSpec& pat) noexcept
    {
        if (ppq < -1e-9)
        {
            return -1;
        }
        const double stepsPerBeat = static_cast<double>(pat.stepsPerBar) / pat.beatsPerBar;
        const double stepFloat = ppq * stepsPerBeat + 1e-9;
        const double stepFloor = std::floor(stepFloat);
        const double frac = stepFloat - stepFloor;
        const std::int64_t floorIdx = static_cast<std::int64_t>(stepFloor);
        return (frac < 0.02) ? (floorIdx - 1) : floorIdx;
    }

    std::uint16_t resolveActiveMask() const noexcept
    {
        const std::uint16_t mask = static_cast<std::uint16_t>(state_.patternMask & 0xFFFFu);
        if (mask == 0u)
        {
            return 0u;
        }
        if (!state_.multiPatternEnabled)
        {
            return static_cast<std::uint16_t>(mask & static_cast<std::uint16_t>(~mask + 1u));
        }
        return mask;
    }

    DriftTriple computeHitDrift(VoiceId voice, double vel) noexcept
    {
        const double v = std::clamp(state_.analogVariation, 0.0, 2.0);
        if (v <= 1e-6)
        {
            return DriftTriple{vel, 1.0, 1.0};
        }

        const double uP = driftGen_.nextScalar();
        const double uA = driftGen_.nextScalar();
        const double uD = driftGen_.nextScalar();

        double pJit = 0.0020;
        double aJit = 0.018;
        double dJit = 0.020;

        if (voice == VoiceId::RimShot)
        {
            pJit = 0.0038;
            aJit = 0.048;
            dJit = 0.025;
        }
        else if (voice == VoiceId::LowBongo || voice == VoiceId::Claves)
        {
            pJit = 0.0035;
            aJit = 0.022;
            dJit = 0.020;
        }
        else if (voice == VoiceId::SnareDrum)
        {
            pJit = 0.0022;
            aJit = 0.022;
            dJit = 0.020;
        }
        else if (voice == VoiceId::HighHat || voice == VoiceId::Cymbal || voice == VoiceId::Tambourine)
        {
            pJit = 0.0000;
            aJit = 0.018;
            dJit = 0.020;
        }

        const double sagAtten = (voice == VoiceId::BassDrum) ? 1.0 : std::max(0.90, 1.0 - 0.028 * v * railSagEnv_);
        return DriftTriple{
            vel * (1.0 + v * aJit * uA) * sagAtten,
            1.0 + v * pJit * uP,
            1.0 + v * dJit * uD};
    }

    void processVoicesChunk(double* out, int n) noexcept
    {
        std::array<double, kMaxChunk> avalanche{};
        std::array<double, kMaxChunk> envA{};
        std::array<double, kMaxChunk> pingA{};
        std::array<double, kMaxChunk> clkA{};
        std::array<double, kMaxChunk> envB{};
        std::array<double, kMaxChunk> pingB{};
        std::array<double, kMaxChunk> clkB{};
        std::array<double, kMaxChunk> metalIn{};
        std::array<double, kMaxChunk> metalOut{};
        std::array<double, kMaxChunk> snareNoiseIn{};
        std::array<double, kMaxChunk> snareNoiseOut{};
        std::array<double, kMaxChunk> tambIn{};
        std::array<double, kMaxChunk> tambOut{};
        std::array<double, kMaxChunk> voiceTmp{};
        std::array<double, kMaxChunk> drumBus{};

        noiseGen_.nextAvalancheBlock(avalanche.data(), n, kAvalancheAlpha);

        hhVca_.renderEnvelopePingClick(envA.data(), pingA.data(), clkA.data(), n);
        cymVca_.renderEnvelopePingClick(envB.data(), pingB.data(), clkB.data(), n);
        for (int i = 0; i < n; ++i)
        {
            metalIn[static_cast<std::size_t>(i)] =
                avalanche[static_cast<std::size_t>(i)] *
                    (envA[static_cast<std::size_t>(i)] + envB[static_cast<std::size_t>(i)]) +
                (pingA[static_cast<std::size_t>(i)] + pingB[static_cast<std::size_t>(i)]);
        }
        metalFilter_.processBlock(metalIn.data(), metalOut.data(), n);
        for (int i = 0; i < n; ++i)
        {
            metalOut[static_cast<std::size_t>(i)] +=
                clkA[static_cast<std::size_t>(i)] + clkB[static_cast<std::size_t>(i)];
        }

        snareNoiseVca_.renderEnvelopePingClick(envA.data(), pingA.data(), nullptr, n);
        for (int i = 0; i < n; ++i)
        {
            snareNoiseIn[static_cast<std::size_t>(i)] =
                avalanche[static_cast<std::size_t>(i)] * envA[static_cast<std::size_t>(i)] +
                pingA[static_cast<std::size_t>(i)];
        }
        snareNoiseFilter_.processBlock(snareNoiseIn.data(), snareNoiseOut.data(), n);

        tambVca_.renderEnvelopePingClick(envA.data(), pingA.data(), nullptr, n);
        for (int i = 0; i < n; ++i)
        {
            tambIn[static_cast<std::size_t>(i)] =
                avalanche[static_cast<std::size_t>(i)] * envA[static_cast<std::size_t>(i)] +
                pingA[static_cast<std::size_t>(i)];
        }
        tambFilter_.processBlock(tambIn.data(), tambOut.data(), n);

        auto accumulateVoice = [&](auto& v) noexcept
        {
            v.processBlock(voiceTmp.data(), n);
            for (int i = 0; i < n; ++i)
            {
                drumBus[static_cast<std::size_t>(i)] += voiceTmp[static_cast<std::size_t>(i)];
            }
        };

        bassDrum_.processBlock(drumBus.data(), n);
        accumulateVoice(lowConga_);
        accumulateVoice(highConga_);
        accumulateVoice(snareBody_);
        accumulateVoice(lowBongo_);
        accumulateVoice(rimShot_);
        accumulateVoice(claves_);
        accumulateVoice(cowBell_);

        double gDrum = 1.0;
        double gCym = 1.0;
        const double t = std::clamp(state_.tone, 0.0, 1.0);
        if (std::abs(t - 0.5) > 1e-6)
        {
            const double busDb = kToneCircuit.maxBusBalanceDb * ((t - 0.5) * 2.0);
            gDrum = std::pow(10.0, -busDb / 20.0);
            gCym = std::pow(10.0, busDb / 20.0);
        }

        for (int i = 0; i < n; ++i)
        {
            const double cym = metalOut[static_cast<std::size_t>(i)] +
                               snareNoiseOut[static_cast<std::size_t>(i)] +
                               tambOut[static_cast<std::size_t>(i)];
            out[i] = drumBus[static_cast<std::size_t>(i)] * gDrum + cym * gCym;
        }

        if (toneActive_)
        {
            toneLowFilter_.processInPlace(out, n);
            toneHighFilter_.processInPlace(out, n);
        }

        if (railSagEnv_ > 1e-15)
        {
            railSagEnv_ *= std::exp(-static_cast<double>(n) / (0.018 * fs_));
        }
        else
        {
            railSagEnv_ = 0.0;
        }

        const double vol = state_.volume;
        const double drive = kOutputSoftClipDrive;
        if (drive > 1e-6)
        {
            const double invTanhDrive = 1.0 / std::tanh(drive);
            for (int i = 0; i < n; ++i)
            {
                out[i] = std::tanh(drive * (out[i] * vol)) * invTanhDrive;
            }
        }
        else
        {
            for (int i = 0; i < n; ++i)
            {
                out[i] *= vol;
            }
        }
    }

    void updateTelemetry(const double* outMono, int numSamples, bool isRunning,
                         double effectiveBpm, std::uint16_t activeMask, double idlePhaseForLamp,
                         bool usingHostBpm) noexcept
    {
        double sumSq = 0.0;
        double peak = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const double s = outMono[i];
            sumSq += s * s;
            peak = std::max(peak, std::abs(s));
        }
        telemetry_.outputRms = std::sqrt(sumSq / static_cast<double>(std::max(1, numSamples)));
        telemetry_.outputPeak = peak;

        telemetry_.lamp.effectiveBpm = effectiveBpm;
        telemetry_.lamp.usingHostBpm = usingHostBpm;
        if (activeMask == 0u)
        {
            telemetry_.lamp.isRunning = false;
            telemetry_.lamp.beatPhase = 0.0;
            telemetry_.lamp.beatFlash = false;
            telemetry_.lamp.isDownbeat = false;
            telemetry_.lamp.currentStep = 0;
            telemetry_.lamp.totalSteps = 0;
            return;
        }

        std::size_t primaryIdx = static_cast<std::size_t>(PatternId::Rock1);
        for (std::size_t p = 0; p < kNumPatterns; ++p)
        {
            if ((activeMask & (1u << p)) != 0u)
            {
                primaryIdx = p;
                break;
            }
        }
        const PatternSpec& pat = kPatterns[primaryIdx];

        if (!isRunning)
        {
            // KE-310 hardware behavior: when rhythm is selected but not playing, flash every quarter beat
            telemetry_.lamp.isRunning = false;
            telemetry_.lamp.beatPhase = idlePhaseForLamp;
            telemetry_.lamp.beatFlash = (idlePhaseForLamp < 0.22);
            telemetry_.lamp.isDownbeat = false;
            telemetry_.lamp.currentStep = 0;
            telemetry_.lamp.totalSteps = pat.totalSteps;
            return;
        }

        // KE-310 hardware behavior: while playing, lamp flashes ONLY on Beat 1 (downbeat) of each bar
        const double stepsPerBeat = static_cast<double>(pat.stepsPerBar) / pat.beatsPerBar;
        const double nonNegBeat = std::max(0.0, beatPos_);
        const double beatInBar = nonNegBeat - pat.beatsPerBar * std::floor(nonNegBeat / pat.beatsPerBar);
        const double beatPhase = nonNegBeat - std::floor(nonNegBeat);
        const std::int64_t stepAbs = static_cast<std::int64_t>(std::floor(nonNegBeat * stepsPerBeat + 1e-9));
        const bool downbeatFlash = (beatInBar < 0.22);

        telemetry_.lamp.isRunning = true;
        telemetry_.lamp.beatPhase = beatPhase;
        telemetry_.lamp.beatFlash = downbeatFlash;
        telemetry_.lamp.isDownbeat = downbeatFlash;
        telemetry_.lamp.currentStep = positiveMod(stepAbs, pat.totalSteps);
        telemetry_.lamp.totalSteps = pat.totalSteps;
    }

    double fs_ = 48000.0;
    RhythmboxState state_{};

    Xorshift32Noise noiseGen_{0x12345678u};
    Xorshift32Noise driftGen_{0x9E3779B9u};

    BridgedTDampedVoice bassDrum_{};
    BridgedTDampedVoice lowConga_{};
    BridgedTDampedVoice highConga_{};
    BridgedTDampedVoice snareBody_{};
    BridgedTDampedVoice lowBongo_{};
    BridgedTDampedVoice rimShot_{};
    BridgedTDampedVoice claves_{};
    DualResonatorVoice cowBell_{};

    GatedNoiseVCA hhVca_{};
    GatedNoiseVCA cymVca_{};
    GatedNoiseVCA snareNoiseVca_{};
    GatedNoiseVCA tambVca_{};

    ke310::dsp::BiquadCascade<kMaxFilterSections> metalFilter_{};
    ke310::dsp::BiquadCascade<kMaxFilterSections> snareNoiseFilter_{};
    ke310::dsp::BiquadCascade<kMaxFilterSections> tambFilter_{};

    double lastTone_ = 0.5;
    bool toneActive_ = false;
    ke310::dsp::BiquadCascade<kMaxFilterSections> toneLowFilter_{};
    ke310::dsp::BiquadCascade<kMaxFilterSections> toneHighFilter_{};

    double railSagEnv_ = 0.0;
    double beatPos_ = 0.0;
    double idleBeatPhase_ = 0.0;
    std::uint16_t prevIdleMask_ = 0u;
    std::array<std::int64_t, kNumPatterns> lastStepIndices_{};
    bool wasRunning_ = false;
    bool hasExpectedPpq_ = false;
    double expectedNextPpq_ = 0.0;
    std::uint16_t prevActiveMask_ = 0u;
    RhythmboxTelemetry telemetry_{};
};

} // namespace ke310::rhythmbox
