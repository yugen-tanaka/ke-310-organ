// Real-time-safe C++20 DSP core mirroring research/dsp_proto/ke310_synth.py 1-to-1.
// Zero heap allocations inside processBlock(), noteOn(), noteOff(), applyState(), or reset().
#pragma once

#include "Biquad.hpp"
#include "KE310Params.hpp"
#include "Oscillators.hpp"
#include "SpringReverb.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <utility>

namespace ke310::dsp
{

struct KE310State
{
    // Upper Manual Tone Levers (10)
    bool upperTibia16 = false;
    bool upperTibia8 = true;
    bool upperTibia4 = false;
    bool upperTibia2_23 = false;
    bool upperDiapason8 = false;
    bool upperString16 = false;
    bool upperString8 = false;
    bool upperString4 = false;
    bool upperOboe8 = false;
    bool upperClarinet8 = false;

    // Lower Manual Tone Levers (6)
    bool lowerTibia8 = false;
    bool lowerTibia4 = false;
    bool lowerDiapason8 = false;
    bool lowerString8 = false;
    bool lowerHorn8 = false;
    bool lowerString4 = false;

    // Pedal Tone Levers (3)
    bool pedalBourdon16 = false;
    bool pedalMajorFlute8 = false;
    bool pedalStringBass = false;

    // Upper Presets (7 + Organ Silent)
    bool organSilent = false;
    bool presetPerc2_23 = false;
    bool presetWowTrumpet = false;
    bool presetBanjo = false;
    bool presetMandolin = false;
    bool presetMarimba = false;
    bool presetPiano = false;
    bool presetHarpsichord = false;

    // Effect Levers
    bool upperSustain = false;
    bool pedalSustain = false;
    bool kneeSustain = false; // Manual P.21: right-knee sustainer lever (OR with upperSustain)
    bool vibrato = false;
    bool vibratoFast = false;
    bool delayVibrato = false;
    bool ensemble = false;

    // Continuous Knobs (0.0 .. 1.0)
    double totalVolume = 0.85;
    double lowerVolume = 0.80;
    double pedalVolume = 0.85;
    double presetVolume = 0.85;
    double reverb = 0.0;
    double upperSustainTime = 0.45;
    double pedalSustainTime = 0.45;
    double percusDecay = 0.25;
    double repeatRate = 0.40;
    double expression = 1.0;

    // Authenticity / Research Options
    bool vintageTos = true;
    bool vintagePedalVco = true;
    double a4Hz = kTuningParams.a4HzSession1;
    bool enableSubOctBleed = true;
    bool hasRepeatRateHzOverride = false;
    double repeatRateHzOverride = 0.0;
    bool tibiaBusLoadComp = true;
    bool expressionLoudnessComp = true;
    double analogNoiseLevel = 0.0;
};

class KE310Engine
{
public:
    static constexpr int kMaxSubBlock = 128;

    void prepare(double sampleRate)
    {
        fs_ = sampleRate;
        tos_.prepare(state_.a4Hz, state_.vintageTos);
        pedalVco_.setConfig(440.0, state_.vintagePedalVco);
        vibratoLfo_.prepare(fs_);
        reverbRotor_.prepare(fs_);
        reverbStat_.prepare(fs_);

        for (std::size_t i = 0; i < kNumVoices; ++i)
        {
            const auto& v = kVoiceSpecs[i];
            voiceFilters_[i].design(v.sections, v.numSections, v.filterGainDb, fs_);
        }
        for (std::size_t i = 0; i < kNumFixedPresets; ++i)
        {
            const auto& p = kFixedPresetSpecs[i];
            fixedPresetFilters_[i].design(p.sections, p.numSections, p.filterGainDb, fs_);
        }

        wowBaseFilter_.design(kWowTrumpetParams.baseSections, kWowTrumpetParams.baseSections.size(),
                              kWowTrumpetParams.filterGainDb, fs_);
        wowWahSvf_.prepare(fs_);
        marimbaBusFilter_.design(kMarimbaKTParams.busSections, kMarimbaKTParams.busSections.size(), 0.0, fs_);

        reset();
    }

    void reset() noexcept
    {
        for (auto& n : upperNotes_)
        {
            n.active = false;
            n.held = false;
            n.marimbaFiltersReady = false;
        }
        for (auto& n : lowerNotes_)
        {
            n.active = false;
            n.held = false;
        }
        pedalVco_.reset();
        pedalActiveMidi_ = -1;
        pedalPhase_ = 0.0;
        pedalPhaseSub_ = 0.0;
        pedalEnvSust_ = 0.0;
        pedalEnvSb_ = 0.0;
        pedalSbGateOpen_ = false;
        pedalHeld_ = false;

        masterTimeS_ = 0.0;
        repeatPhase_ = 0.0;
        repRcBanjo_ = 0.0;
        repRcMandolin_ = 0.0;
        marimbaRc8_ = 0.0;
        marimbaRc4_ = 0.0;
        wahPhase_ = kWowTrumpetParams.wahInitPhase;
        wowOnsetAgeS_ = 0.0;
        expSmooth_ = std::clamp(state_.expression, 0.0, 1.0);
        expLpRotor_ = 0.0;
        expLpStat_ = 0.0;
        noiseRng_ = 0x13579BDFu;
        noiseLp_ = 0.0;

        vibratoLfo_.reset();
        reverbRotor_.reset();
        reverbStat_.reset();
        for (auto& f : voiceFilters_)
        {
            f.reset();
        }
        for (auto& f : fixedPresetFilters_)
        {
            f.reset();
        }
        wowBaseFilter_.reset();
        wowWahSvf_.reset();
        marimbaBusFilter_.reset();
        snapTabGainsToState();
    }

    void applyState(const KE310State& newState) noexcept
    {
        if (newState.a4Hz != tos_.a4Hz() || newState.vintageTos != tos_.vintageTos())
        {
            tos_.prepare(newState.a4Hz, newState.vintageTos);
        }
        pedalVco_.setVintageStretch(newState.vintagePedalVco);
        if (!anyKeyHeld() && masterTimeS_ == 0.0 && std::abs(newState.expression - state_.expression) > 1e-9)
        {
            expSmooth_ = std::clamp(newState.expression, 0.0, 1.0);
        }
        state_ = newState;
        if (!anyKeyHeld() && masterTimeS_ == 0.0)
        {
            snapTabGainsToState();
        }
    }

    const KE310State& state() const noexcept { return state_; }

    int activeUpperNoteCount() const noexcept
    {
        int count = 0;
        for (const auto& u : upperNotes_)
        {
            if (u.active && u.held)
            {
                ++count;
            }
        }
        return count;
    }

    int activeLowerNoteCount() const noexcept
    {
        int count = 0;
        for (const auto& l : lowerNotes_)
        {
            if (l.active && l.held)
            {
                ++count;
            }
        }
        return count;
    }

    int activePedalMidiNote() const noexcept { return pedalActiveMidi_; }

    bool isPedalHeld() const noexcept { return pedalHeld_; }

    bool hasHeldLowerNoteInRange(int minMidi, int maxMidi) const noexcept
    {
        const int lo = std::clamp(minMidi, 0, 127);
        const int hi = std::clamp(maxMidi, lo, 127);
        for (int m = lo; m <= hi; ++m)
        {
            const auto& slot = lowerNotes_[static_cast<std::size_t>(m)];
            if (slot.active && slot.held)
            {
                return true;
            }
        }
        return false;
    }

    void noteOn(Manual manual, int midiNote) noexcept
    {
        if (midiNote < 0 || midiNote >= 128)
        {
            return;
        }

        const bool wasAnyHeld = anyKeyHeld();
        const bool pianoHarpsiTakeover = (state_.presetPiano || state_.presetHarpsichord);
        if (!wasAnyHeld && state_.delayVibrato && !state_.vibrato && !pianoHarpsiTakeover)
        {
            vibratoLfo_.triggerDelayRestart();
        }

        const std::size_t idx = static_cast<std::size_t>(midiNote);
        if (manual == Manual::Upper)
        {
            int heldBefore = 0;
            bool anyActiveBefore = false;
            for (const auto& u : upperNotes_)
            {
                if (u.active)
                {
                    anyActiveBefore = true;
                    if (u.held)
                    {
                        ++heldBefore;
                    }
                }
            }
            if (heldBefore == 0)
            {
                repeatPhase_ = 0.0;
                if (!anyActiveBefore)
                {
                    marimbaRc8_ = 0.0;
                    marimbaRc4_ = 0.0;
                }
                wahPhase_ = kWowTrumpetParams.wahInitPhase;
                wowOnsetAgeS_ = 0.0;
            }

            const double f0 = tos_.noteFreq(midiNote);
            const double f223 = tos_.noteFreq(midiNote + 19);
            auto& slot = upperNotes_[idx];
            const bool existed = slot.active;
            const double initPhase = existed ? slot.phase : wrap01(masterTimeS_ * 0.5 * f0);
            const double initPhase223 = wrap01(masterTimeS_ * f223);
            const bool percTrig = state_.presetPerc2_23 && (heldBefore == 0);

            if (existed && slot.marimbaFiltersReady)
            {
                // Preserve running key-tracking MARIMBA filter states for clickless re-strike
            }
            else if (state_.presetMarimba)
            {
                initMarimbaNoteFilters(slot, f0);
            }
            else
            {
                slot.marimbaFiltersReady = false;
            }

            slot.active = true;
            slot.held = true;
            slot.phase = initPhase;
            slot.phase223 = initPhase223;
            slot.env84 = existed ? slot.env84 : 0.0;
            slot.env84DecayPhase = false;
            slot.envDirect = existed ? slot.envDirect : 0.0;
            slot.envPerc = 0.0;
            slot.percActive = percTrig;
            slot.percGateOpen = true;
        }
        else if (manual == Manual::Lower)
        {
            const double f0 = tos_.noteFreq(midiNote);
            auto& slot = lowerNotes_[idx];
            const bool existed = slot.active;
            slot.active = true;
            slot.held = true;
            slot.phase = existed ? slot.phase : wrap01(masterTimeS_ * f0);
            slot.env = existed ? slot.env : 0.0;
        }
        else
        {
            const int newActive = pedalVco_.noteOn(midiNote);
            pedalHeld_ = (newActive >= 0);
            if (newActive != pedalActiveMidi_)
            {
                pedalActiveMidi_ = newActive;
                pedalEnvSb_ = 0.0;
                pedalSbGateOpen_ = true;
            }
        }
    }

    void noteOff(Manual manual, int midiNote) noexcept
    {
        if (midiNote < 0 || midiNote >= 128)
        {
            return;
        }

        const std::size_t idx = static_cast<std::size_t>(midiNote);
        if (manual == Manual::Upper)
        {
            if (upperNotes_[idx].active)
            {
                upperNotes_[idx].held = false;
            }
        }
        else if (manual == Manual::Lower)
        {
            if (lowerNotes_[idx].active)
            {
                lowerNotes_[idx].held = false;
            }
        }
        else
        {
            const int newActive = pedalVco_.noteOff(midiNote);
            pedalHeld_ = (newActive >= 0);
            if (newActive >= 0 && newActive != pedalActiveMidi_)
            {
                pedalActiveMidi_ = newActive;
                pedalEnvSb_ = 0.0;
                pedalSbGateOpen_ = true;
            }
        }
    }

    void manualNotesOff(Manual manual) noexcept
    {
        if (manual == Manual::Upper)
        {
            for (auto& u : upperNotes_)
            {
                u.held = false;
            }
        }
        else if (manual == Manual::Lower)
        {
            for (auto& l : lowerNotes_)
            {
                l.held = false;
            }
        }
        else
        {
            pedalVco_.reset();
            pedalHeld_ = false;
        }
    }

    void allNotesOff() noexcept
    {
        manualNotesOff(Manual::Upper);
        manualNotesOff(Manual::Lower);
        manualNotesOff(Manual::Pedal);
    }

    void processBlock(int numSamples, double* mixOut, double* rotorOut, double* statOut) noexcept
    {
        int offset = 0;
        while (offset < numSamples)
        {
            const int chunk = std::min(kMaxSubBlock, numSamples - offset);
            processSubBlock(chunk);
            for (int i = 0; i < chunk; ++i)
            {
                const std::size_t si = static_cast<std::size_t>(i);
                if (mixOut != nullptr)
                {
                    mixOut[offset + i] = mixSub_[si];
                }
                if (rotorOut != nullptr)
                {
                    rotorOut[offset + i] = rotorSub_[si];
                }
                if (statOut != nullptr)
                {
                    statOut[offset + i] = statSub_[si];
                }
            }
            offset += chunk;
        }
    }

    void processBlock(int numSamples, float* mixOut, float* rotorOut, float* statOut) noexcept
    {
        int offset = 0;
        while (offset < numSamples)
        {
            const int chunk = std::min(kMaxSubBlock, numSamples - offset);
            processSubBlock(chunk);
            for (int i = 0; i < chunk; ++i)
            {
                const std::size_t si = static_cast<std::size_t>(i);
                if (mixOut != nullptr)
                {
                    mixOut[offset + i] = static_cast<float>(mixSub_[si]);
                }
                if (rotorOut != nullptr)
                {
                    rotorOut[offset + i] = static_cast<float>(rotorSub_[si]);
                }
                if (statOut != nullptr)
                {
                    statOut[offset + i] = static_cast<float>(statSub_[si]);
                }
            }
            offset += chunk;
        }
    }

    // Inspection helpers for unit tests & stability verification
    std::pair<BiquadCascade<2>, BiquadCascade<2>> makeMarimbaNoteFilters(double fBase) const noexcept
    {
        std::pair<BiquadCascade<2>, BiquadCascade<2>> out{};
        configureMarimbaPair(fBase, out.first, out.second);
        return out;
    }

    const BiquadCascade<kMaxFilterSections>& voiceFilter(VoiceId id) const noexcept
    {
        return voiceFilters_[static_cast<std::size_t>(id)];
    }

    const BiquadCascade<kMaxFilterSections>& fixedPresetFilter(FixedPresetId id) const noexcept
    {
        return fixedPresetFilters_[static_cast<std::size_t>(id)];
    }

    const BiquadCascade<3>& wowBaseFilter() const noexcept { return wowBaseFilter_; }
    const BiquadCascade<2>& marimbaBusFilter() const noexcept { return marimbaBusFilter_; }
    const TOSTuningTable& tos() const noexcept { return tos_; }
    const PedalVCO& pedalVco() const noexcept { return pedalVco_; }

private:
    struct UpperNoteState
    {
        bool active = false;
        bool held = false;
        double phase = 0.0;
        double phase223 = 0.0;
        double env84 = 0.0;
        bool env84DecayPhase = false;
        double envDirect = 0.0;
        double envPerc = 0.0;
        bool percActive = false;
        bool percGateOpen = false;
        bool marimbaFiltersReady = false;
        BiquadCascade<2> marimbaFlt8{};
        BiquadCascade<2> marimbaFlt4{};
    };

    struct LowerNoteState
    {
        bool active = false;
        bool held = false;
        double phase = 0.0;
        double env = 0.0;
    };

    bool anyKeyHeld() const noexcept
    {
        if (pedalHeld_)
        {
            return true;
        }
        for (const auto& u : upperNotes_)
        {
            if (u.active && u.held)
            {
                return true;
            }
        }
        for (const auto& l : lowerNotes_)
        {
            if (l.active && l.held)
            {
                return true;
            }
        }
        return false;
    }

    static double pianoHarpsiTau(int midiNote, bool isHarpsi) noexcept
    {
        const double nClamped = std::clamp(static_cast<double>(midiNote), 48.0, 96.0);
        const double tC3 = isHarpsi ? 0.530 : 0.560;
        const double tC6 = isHarpsi ? 0.258 : 0.260;
        const double frac = std::min(1.0, (nClamped - 48.0) / 36.0);
        return tC3 * std::pow(tC6 / tC3, frac);
    }

    double repeatHz() const noexcept
    {
        if (state_.hasRepeatRateHzOverride)
        {
            return state_.repeatRateHzOverride;
        }
        const double rate = std::clamp(state_.repeatRate, 0.30, 0.80);
        return 3.0 + rate * 11.0;
    }

    static double keySlopeGain(const SourceParams& src, double f0Hz) noexcept
    {
        if (std::abs(src.keySlopeDbPerOct) < 1e-6)
        {
            return 1.0;
        }
        const double oct = std::log2(std::max(20.0, f0Hz) / 261.625565);
        return std::pow(10.0, (src.keySlopeDbPerOct * oct) / 20.0);
    }

    double expStepEnv(double current, double target, double tauS, double* out, int n) const noexcept
    {
        const double coeff = std::exp(-1.0 / std::max(1e-4, tauS * fs_));
        double e = current;
        for (int i = 0; i < n; ++i)
        {
            e = target + coeff * (e - target);
            out[i] = e;
        }
        return e;
    }

    void configureMarimbaPair(double fBase, BiquadCascade<2>& flt8, BiquadCascade<2>& flt4) const noexcept
    {
        const double fRef = kMarimbaKTParams.fRefHz;
        const double fLim = std::max(20.0, fBase);
        const double octKey = std::log2(fLim / fRef);
        const double fc8 = std::min(0.45 * fs_,
            kMarimbaKTParams.fc0C4_8ftHz * std::pow(fLim / fRef, kMarimbaKTParams.keytrackAlpha));
        const double fc4 = std::min(0.45 * fs_, fc8 * kMarimbaKTParams.oct4CutoffRatio);
        const double qKt = kMarimbaKTParams.qKt;
        const double g8 = kMarimbaKTParams.filterGainDb + kMarimbaKTParams.source.keySlopeDbPerOct * octKey;
        const double g4 = g8 + kMarimbaKTParams.oct4GainDb;

        const std::array<FilterSectionSpec, 2> sec8 = {{
            FilterSectionSpec{FilterType::LP1, fc8, 0.70710678, 0.0},
            FilterSectionSpec{FilterType::LP2, fc8, qKt, 0.0},
        }};
        const std::array<FilterSectionSpec, 2> sec4 = {{
            FilterSectionSpec{FilterType::LP1, fc4, 0.70710678, 0.0},
            FilterSectionSpec{FilterType::LP2, fc4, qKt, 0.0},
        }};

        flt8.design(sec8, 2, g8, fs_);
        flt4.design(sec4, 2, g4, fs_);
    }

    void initMarimbaNoteFilters(UpperNoteState& slot, double fBase) noexcept
    {
        configureMarimbaPair(fBase, slot.marimbaFlt8, slot.marimbaFlt4);
        slot.marimbaFlt8.reset();
        slot.marimbaFlt4.reset();
        slot.marimbaFiltersReady = true;
    }

    static constexpr double kTabFadeS = 0.006;

    static double smoothTabCurve(double g) noexcept
    {
        return g * g * (3.0 - 2.0 * g);
    }

    static double stepLinearGain(double g, double targetG, double step) noexcept
    {
        if (g < targetG)
        {
            return std::min(1.0, g + step);
        }
        if (g > targetG)
        {
            return std::max(0.0, g - step);
        }
        return g;
    }

    static void addSmoothedPreset(double* dst, const double* src, int n,
                                  double presetVolume, bool targetOn,
                                  double& tabGain, double tabStep) noexcept
    {
        const double targetG = targetOn ? 1.0 : 0.0;
        double g = tabGain;
        for (int i = 0; i < n; ++i)
        {
            g = stepLinearGain(g, targetG, tabStep);
            dst[i] += src[i] * presetVolume * smoothTabCurve(g);
        }
        tabGain = g;
    }

    double computeTargetTibiaLoadGain() const noexcept
    {
        if (!state_.tibiaBusLoadComp)
        {
            return 1.0;
        }
        int nUpperTibias = 0;
        for (std::size_t v = static_cast<std::size_t>(VoiceId::UpperTibia16);
             v <= static_cast<std::size_t>(VoiceId::UpperTibia2_23); ++v)
        {
            if (isVoiceTabActive(static_cast<VoiceId>(v)))
            {
                ++nUpperTibias;
            }
        }
        return 1.0 / (1.0 + 0.55 * static_cast<double>(std::max(0, nUpperTibias - 1)));
    }

    void snapTabGainsToState() noexcept
    {
        for (std::size_t v = 0; v < kNumVoices; ++v)
        {
            voiceTabGain_[v] = isVoiceTabActive(static_cast<VoiceId>(v)) ? 1.0 : 0.0;
        }
        presetTabGain_[0] = state_.presetPerc2_23 ? 1.0 : 0.0;
        presetTabGain_[1] = state_.presetPiano ? 1.0 : 0.0;
        presetTabGain_[2] = state_.presetHarpsichord ? 1.0 : 0.0;
        presetTabGain_[3] = state_.presetBanjo ? 1.0 : 0.0;
        presetTabGain_[4] = state_.presetMandolin ? 1.0 : 0.0;
        presetTabGain_[5] = state_.presetMarimba ? 1.0 : 0.0;
        presetTabGain_[6] = state_.presetWowTrumpet ? 1.0 : 0.0;
        tibiaLoadGainSmooth_ = computeTargetTibiaLoadGain();
    }

    bool isVoiceTabActive(VoiceId id) const noexcept
    {
        if (kVoiceSpecs[static_cast<std::size_t>(id)].manual == Manual::Upper && state_.organSilent)
        {
            return false;
        }
        switch (id)
        {
            case VoiceId::UpperTibia16:     return state_.upperTibia16;
            case VoiceId::UpperTibia8:      return state_.upperTibia8;
            case VoiceId::UpperTibia4:      return state_.upperTibia4;
            case VoiceId::UpperTibia2_23:   return state_.upperTibia2_23;
            case VoiceId::UpperDiapason8:   return state_.upperDiapason8;
            case VoiceId::UpperString16:    return state_.upperString16;
            case VoiceId::UpperString8:     return state_.upperString8;
            case VoiceId::UpperString4:     return state_.upperString4;
            case VoiceId::UpperOboe8:       return state_.upperOboe8;
            case VoiceId::UpperClarinet8:   return state_.upperClarinet8;
            case VoiceId::LowerTibia8:      return state_.lowerTibia8;
            case VoiceId::LowerTibia4:      return state_.lowerTibia4;
            case VoiceId::LowerDiapason8:   return state_.lowerDiapason8;
            case VoiceId::LowerString8:     return state_.lowerString8;
            case VoiceId::LowerHorn8:       return state_.lowerHorn8;
            case VoiceId::LowerString4:     return state_.lowerString4;
            case VoiceId::PedalBourdon16:   return state_.pedalBourdon16;
            case VoiceId::PedalMajorFlute8: return state_.pedalMajorFlute8;
            case VoiceId::PedalStringBass:  return state_.pedalStringBass;
            case VoiceId::Count:            return false;
        }
        return false;
    }

    void processSubBlock(int n) noexcept
    {
        const KE310State& st = state_;
        const bool pPerc = st.presetPerc2_23;
        const bool pPiano = st.presetPiano;
        const bool pHarpsi = st.presetHarpsichord;
        const bool pBanjo = st.presetBanjo;
        const bool pMandolin = st.presetMandolin;
        const bool pMarimba = st.presetMarimba;
        const bool pWow = st.presetWowTrumpet;
        const bool pianoHarpsiTakeover = (pPiano || pHarpsi);

        // Real Vibrato modulates the master TOS clock, so Piano/Harpsichord disables Vibrato
        const bool effVibrato = st.vibrato && !pianoHarpsiTakeover;
        const bool effDelayVibrato = st.delayVibrato && !pianoHarpsiTakeover;
        vibratoLfo_.processBlock(n, effVibrato, st.vibratoFast, effDelayVibrato, anyKeyHeld(),
                                 pitchMult_.data(), vibAm_.data());

        // Active tabs & buffers (keep faded-out voices running until tab gain reaches 0.0)
        const double tabStep = 1.0 / std::max(1.0, kTabFadeS * fs_);
        std::array<bool, kNumVoices> voiceActive{};
        std::array<bool, kNumVoices> voiceRunning{};
        for (std::size_t v = 0; v < kNumVoices; ++v)
        {
            voiceActive[v] = isVoiceTabActive(static_cast<VoiceId>(v));
            voiceRunning[v] = voiceActive[v] || (voiceTabGain_[v] > 0.0);
            if (voiceRunning[v])
            {
                busBufs_[v].fill(0.0);
            }
        }

        std::array<bool, 7> pOn = {{pPerc, pPiano, pHarpsi, pBanjo, pMandolin, pMarimba, pWow}};
        std::array<bool, 7> presetRunning{};
        for (std::size_t p = 0; p < 7; ++p)
        {
            presetRunning[p] = pOn[p] || (presetTabGain_[p] > 0.0);
            if (presetRunning[p])
            {
                presetBufs_[p].fill(0.0);
            }
        }

        // Repeat & Marimba Dual-VCA clocks
        const double repHz = repeatHz();
        bool anyUpperHeld = false;
        bool anyUpperActive = false;
        const bool upperSustActive = (st.upperSustain || st.kneeSustain);
        for (const auto& u : upperNotes_)
        {
            if (u.active)
            {
                if (u.held)
                {
                    anyUpperHeld = true;
                    anyUpperActive = true;
                    break;
                }
                if (u.env84 > 1e-5)
                {
                    anyUpperActive = true;
                }
            }
        }
        const bool keepRepeatPulsing = anyUpperHeld || (upperSustActive && anyUpperActive);

        repPulseBanjo_.fill(0.0);
        repPulseMandolin_.fill(0.0);
        marimbaEnv8_.fill(0.0);
        marimbaEnv4_.fill(0.0);

        if ((presetRunning[3] || presetRunning[4]) && (keepRepeatPulsing || repRcBanjo_ > 1e-6 || repRcMandolin_ > 1e-6))
        {
            const double incRep = repHz / fs_;
            const double periodS = 1.0 / std::max(1.0, repHz);
            const double gateDur = std::min(0.0028, 0.22 * periodS);
            const double cAtk = std::exp(-1.0 / (0.0014 * fs_));
            const double cDecB = std::exp(-1.0 / (0.0393 * fs_));
            const double cDecM = std::exp(-1.0 / (0.0376 * fs_));
            double ph = repeatPhase_;
            double rcB = repRcBanjo_;
            double rcM = repRcMandolin_;
            for (int i = 0; i < n; ++i)
            {
                const std::size_t si = static_cast<std::size_t>(i);
                if (keepRepeatPulsing)
                {
                    const double tInP = ph * periodS;
                    const bool gateOn = (tInP < gateDur);
                    rcB = gateOn ? (1.0 + cAtk * (rcB - 1.0)) : (rcB * cDecB);
                    rcM = gateOn ? (1.0 + cAtk * (rcM - 1.0)) : (rcM * cDecM);
                    ph += incRep;
                    if (ph >= 1.0)
                    {
                        ph -= 1.0;
                    }
                }
                else
                {
                    rcB *= cDecB;
                    rcM *= cDecM;
                }
                repPulseBanjo_[si] = rcB;
                repPulseMandolin_[si] = rcM;
            }
            if (keepRepeatPulsing && !presetRunning[5])
            {
                repeatPhase_ = ph;
            }
            repRcBanjo_ = rcB;
            repRcMandolin_ = rcM;
        }

        if (presetRunning[5] && (keepRepeatPulsing || marimbaRc8_ > 1e-6 || marimbaRc4_ > 1e-6))
        {
            const double periodCyc = 1.0 / std::max(1.0, repHz);
            const double incM = repHz / fs_;
            const double durOn = std::min(kMarimbaKTParams.gateOnMs * 1e-3, 0.22 * periodCyc);
            const double off4 = kMarimbaKTParams.oct4PhaseOffset * periodCyc;
            const double cA = std::exp(-1.0 / (kMarimbaKTParams.tauAtkMs * 1e-3 * fs_));
            const double cR = std::exp(-1.0 / (kMarimbaKTParams.tauRelMs * 1e-3 * fs_));
            const double drive = kMarimbaKTParams.vcaTanhDrive;
            const double td = std::tanh(drive);
            const double flk = kMarimbaKTParams.vcaHoldFloor;
            double phM = repeatPhase_;
            double rc8 = marimbaRc8_;
            double rc4 = marimbaRc4_;

            for (int i = 0; i < n; ++i)
            {
                const std::size_t si = static_cast<std::size_t>(i);
                if (keepRepeatPulsing)
                {
                    const double tCyc = phM * periodCyc;
                    const bool g8 = (tCyc >= 0.0 && tCyc < durOn);
                    const bool g4 = (tCyc >= off4 && tCyc < off4 + durOn);
                    rc8 = g8 ? (1.0 + cA * (rc8 - 1.0)) : (cR * rc8);
                    rc4 = g4 ? (1.0 + cA * (rc4 - 1.0)) : (cR * rc4);
                    marimbaEnv8_[si] = flk + (1.0 - flk) * (std::tanh(drive * rc8) / td);
                    marimbaEnv4_[si] = flk + (1.0 - flk) * (std::tanh(drive * rc4) / td);
                    phM += incM;
                    if (phM >= 1.0)
                    {
                        phM -= 1.0;
                    }
                }
                else
                {
                    rc8 *= cR;
                    rc4 *= cR;
                    marimbaEnv8_[si] = std::tanh(drive * rc8) / td;
                    marimbaEnv4_[si] = std::tanh(drive * rc4) / td;
                }
            }
            if (keepRepeatPulsing)
            {
                repeatPhase_ = phM;
            }
            marimbaRc8_ = rc8;
            marimbaRc4_ = rc4;
        }

        const double tau84Atk = 0.0042;
        const double tau84Rel = upperSustActive
                                  ? (0.018 + std::pow(st.upperSustainTime, 1.6) * 1.78)
                                  : 0.018;
        const double tauDirAtk = 0.0020;
        const double tauDirRel = 0.0025;

        // --- Process Upper Manual Notes ---
        const double coeffPianoAtk = std::exp(-1.0 / (0.0045 * fs_));
        const double tauPerc = 0.025 + st.percusDecay * 0.1724;
        const double cPercAtk = std::exp(-1.0 / (0.0035 * fs_));
        const double cPercDec = std::exp(-1.0 / (tauPerc * fs_));

        for (int midi = 0; midi < 128; ++midi)
        {
            auto& nst = upperNotes_[static_cast<std::size_t>(midi)];
            if (!nst.active)
            {
                continue;
            }

            const double fBase = tos_.noteFreq(midi);
            const double invFs = 1.0 / fs_;

            // 1) Upper 8'/4' Sustain Envelope
            if (pianoHarpsiTakeover)
            {
                const double tauDec = pianoHarpsiTau(midi, pHarpsi);
                if (nst.held && !nst.env84DecayPhase)
                {
                    const double cDec = std::exp(-1.0 / (tauDec * fs_));
                    double e = nst.env84;
                    bool dec = nst.env84DecayPhase;
                    for (int i = 0; i < n; ++i)
                    {
                        if (!dec)
                        {
                            e = 1.0 + coeffPianoAtk * (e - 1.0);
                            if (e >= 0.96)
                            {
                                dec = true;
                            }
                        }
                        else
                        {
                            e *= cDec;
                        }
                        env84_[static_cast<std::size_t>(i)] = e;
                    }
                    nst.env84 = e;
                    nst.env84DecayPhase = dec;
                }
                else
                {
                    const double tauEff = nst.held
                                            ? tauDec
                                            : (upperSustActive ? std::max(0.035, tau84Rel)
                                                               : std::min(tauDec, 0.035));
                    nst.env84 = expStepEnv(nst.env84, 0.0, tauEff, env84_.data(), n);
                }
            }
            else
            {
                nst.env84 = expStepEnv(nst.env84, nst.held ? 1.0 : 0.0, nst.held ? tau84Atk : tau84Rel, env84_.data(), n);
            }

            // 2) Direct Keying Envelope
            nst.envDirect = expStepEnv(nst.envDirect, nst.held ? 1.0 : 0.0,
                                       nst.held ? tauDirAtk : tauDirRel, envDir_.data(), n);

            // Prepare PERC 2 2/3' envelope if active
            bool runPerc = (presetRunning[0] && nst.percActive);
            if (runPerc)
            {
                double e = nst.envPerc;
                bool gate = nst.percGateOpen;
                for (int i = 0; i < n; ++i)
                {
                    if (gate)
                    {
                        e = 1.0 + cPercAtk * (e - 1.0);
                        if (e >= 0.96)
                        {
                            gate = false;
                        }
                    }
                    else
                    {
                        e *= cPercDec;
                    }
                    envPerc_[static_cast<std::size_t>(i)] = e;
                }
                nst.envPerc = e;
                nst.percGateOpen = gate;
                if (!nst.held)
                {
                    for (int i = 0; i < n; ++i)
                    {
                        envPerc_[static_cast<std::size_t>(i)] *= std::exp(-static_cast<double>(i) / (0.020 * fs_));
                    }
                    nst.envPerc = envPerc_[static_cast<std::size_t>(n - 1)];
                }
                if (nst.envPerc < 1e-5 && !nst.percGateOpen)
                {
                    nst.percActive = false;
                }
            }

            // Check if any Upper tone tab is active
            bool anyUpperOrganActive = false;
            for (std::size_t v = 0; v <= static_cast<std::size_t>(VoiceId::UpperClarinet8); ++v)
            {
                if (voiceRunning[v])
                {
                    anyUpperOrganActive = true;
                    break;
                }
            }

            const double f223 = tos_.noteFreq(midi + 19);
            const bool need223Phase = voiceRunning[static_cast<std::size_t>(VoiceId::UpperTibia2_23)] || runPerc;
            if (presetRunning[5] && (!nst.marimbaFiltersReady))
            {
                initMarimbaNoteFilters(nst, fBase);
            }

            double ph16 = nst.phase;
            double ph223 = nst.phase223;
            for (int i = 0; i < n; ++i)
            {
                const std::size_t si = static_cast<std::size_t>(i);
                const double pm = pitchMult_[si];
                const double dt8 = (fBase * pm) * invFs;
                const double dt16 = 0.5 * dt8;
                ph16 = wrap01(ph16 + dt16);
                const double ph8 = wrap01(2.0 * ph16);
                const double ph4 = wrap01(4.0 * ph16);

                if (anyUpperOrganActive)
                {
                    for (std::size_t v = 0; v <= static_cast<std::size_t>(VoiceId::UpperClarinet8); ++v)
                    {
                        if (!voiceRunning[v])
                        {
                            continue;
                        }
                        const auto& spec = kVoiceSpecs[v];
                        double phUse = ph8;
                        double dtUse = dt8;
                        double fEff = fBase;
                        if (spec.footageSemitones == -12)
                        {
                            phUse = ph16;
                            dtUse = dt16;
                            fEff = 0.5 * fBase;
                        }
                        else if (spec.footageSemitones == 12)
                        {
                            phUse = ph4;
                            dtUse = 2.0 * dt8;
                            fEff = 2.0 * fBase;
                        }
                        else if (spec.footageSemitones == 19)
                        {
                            phUse = ph223;
                            dtUse = (f223 * pm) * invFs;
                            fEff = f223;
                        }
                        const double w = staircaseWave(phUse, dtUse, spec.source, fs_);
                        const double kg = keySlopeGain(spec.source, fEff);
                        const double envU = spec.sustainBus ? env84_[si] : envDir_[si];
                        busBufs_[v][si] += w * envU * kg;
                    }
                }

                if (runPerc)
                {
                    const auto& ps = kFixedPresetSpecs[static_cast<std::size_t>(FixedPresetId::Perc2_23)];
                    const double dtP = (f223 * pm) * invFs;
                    const double w = staircaseWave(ph223, dtP, ps.source, fs_);
                    presetBufs_[0][si] += w * envPerc_[si] * keySlopeGain(ps.source, f223);
                }

                if (presetRunning[1])
                {
                    const auto& ps = kFixedPresetSpecs[static_cast<std::size_t>(FixedPresetId::Piano)];
                    const double w = staircaseWave(ph8, dt8, ps.source, fs_);
                    presetBufs_[static_cast<std::size_t>(FixedPresetId::Piano)][si] +=
                        w * env84_[si] * keySlopeGain(ps.source, fBase);
                }

                if (presetRunning[2])
                {
                    const auto& ps = kFixedPresetSpecs[static_cast<std::size_t>(FixedPresetId::Harpsichord)];
                    const double w = staircaseWave(ph8, dt8, ps.source, fs_);
                    presetBufs_[static_cast<std::size_t>(FixedPresetId::Harpsichord)][si] +=
                        w * env84_[si] * keySlopeGain(ps.source, fBase);
                }

                if (presetRunning[3])
                {
                    const auto& ps = kFixedPresetSpecs[static_cast<std::size_t>(FixedPresetId::Banjo)];
                    const double w = staircaseWave(ph8, dt8, ps.source, fs_);
                    presetBufs_[3][si] += w * repPulseBanjo_[si] * env84_[si] * keySlopeGain(ps.source, fBase);
                }

                if (presetRunning[4])
                {
                    const auto& ps = kFixedPresetSpecs[static_cast<std::size_t>(FixedPresetId::Mandolin)];
                    const double w = staircaseWave(ph8, dt8, ps.source, fs_);
                    presetBufs_[4][si] += w * repPulseMandolin_[si] * env84_[si] * keySlopeGain(ps.source, fBase);
                }

                if (presetRunning[5])
                {
                    const double w8 = staircaseWave(ph8, dt8, kMarimbaKTParams.source, fs_);
                    const double w4 = staircaseWave(ph4, 2.0 * dt8, kMarimbaKTParams.source, fs_);
                    const double yM = nst.marimbaFlt8.processSample(w8 * marimbaEnv8_[si])
                                    + nst.marimbaFlt4.processSample(w4 * marimbaEnv4_[si]);
                    presetBufs_[5][si] += yM * env84_[si];
                }

                if (presetRunning[6])
                {
                    const double w = staircaseWave(ph8, dt8, kWowTrumpetParams.source, fs_);
                    presetBufs_[6][si] += w * env84_[si] * keySlopeGain(kWowTrumpetParams.source, fBase);
                }

                if (need223Phase)
                {
                    ph223 = wrap01(ph223 + (f223 * pm) * invFs);
                }
            }
            nst.phase = ph16;
            nst.phase223 = ph223;

            const double maxAct = std::max({nst.env84, nst.envDirect, nst.envPerc});
            if (!nst.held && maxAct < 1e-5)
            {
                nst.active = false;
                nst.marimbaFiltersReady = false;
            }
        }

        // --- Process Lower Manual Notes ---
        bool anyLowerOrganActive = false;
        for (std::size_t v = static_cast<std::size_t>(VoiceId::LowerTibia8);
             v <= static_cast<std::size_t>(VoiceId::LowerString4); ++v)
        {
            if (voiceRunning[v])
            {
                anyLowerOrganActive = true;
                break;
            }
        }

        for (int midi = 0; midi < 128; ++midi)
        {
            auto& nst = lowerNotes_[static_cast<std::size_t>(midi)];
            if (!nst.active)
            {
                continue;
            }
            nst.env = expStepEnv(nst.env, nst.held ? 1.0 : 0.0, nst.held ? tauDirAtk : tauDirRel, envDir_.data(), n);
            const double fBase = tos_.noteFreq(midi);
            double ph8 = nst.phase;
            const double invFs = 1.0 / fs_;

            for (int i = 0; i < n; ++i)
            {
                const std::size_t si = static_cast<std::size_t>(i);
                const double dt8 = (fBase * pitchMult_[si]) * invFs;
                ph8 = wrap01(ph8 + dt8);
                const double ph4 = wrap01(2.0 * ph8);

                if (anyLowerOrganActive)
                {
                    for (std::size_t v = static_cast<std::size_t>(VoiceId::LowerTibia8);
                         v <= static_cast<std::size_t>(VoiceId::LowerString4); ++v)
                    {
                        if (!voiceRunning[v])
                        {
                            continue;
                        }
                        const auto& spec = kVoiceSpecs[v];
                        const bool is4 = (spec.footageSemitones == 12);
                        const double w = staircaseWave(is4 ? ph4 : ph8, is4 ? 2.0 * dt8 : dt8, spec.source, fs_);
                        const double kg = keySlopeGain(spec.source, is4 ? 2.0 * fBase : fBase);
                        busBufs_[v][si] += w * envDir_[si] * st.lowerVolume * kg;
                    }
                }
            }
            nst.phase = ph8;
            if (!nst.held && nst.env < 1e-5)
            {
                nst.active = false;
            }
        }

        // --- Process Pedal (Monophonic Low-Note Priority) ---
        const bool runPedalBourdon = voiceRunning[static_cast<std::size_t>(VoiceId::PedalBourdon16)];
        const bool runPedalFlute = voiceRunning[static_cast<std::size_t>(VoiceId::PedalMajorFlute8)];
        const bool runPedalSb = voiceRunning[static_cast<std::size_t>(VoiceId::PedalStringBass)];
        const bool pedalRanThisBlock =
            (pedalActiveMidi_ >= 0 && (runPedalBourdon || runPedalFlute || runPedalSb));
        if (pedalRanThisBlock)
        {
            const double fPed = pedalVco_.noteFreq(pedalActiveMidi_);
            const double invFs = 1.0 / fs_;
            const double tauPedRel = st.pedalSustain
                                       ? (0.022 + std::pow(st.pedalSustainTime, 1.6) * 1.60)
                                       : 0.022;

            if (runPedalBourdon || runPedalFlute)
            {
                pedalEnvSust_ = expStepEnv(pedalEnvSust_, pedalHeld_ ? 1.0 : 0.0,
                                           pedalHeld_ ? 0.0028 : tauPedRel, envDir_.data(), n);
            }
            if (runPedalSb)
            {
                const double tauSb = 0.391 + (st.pedalSustain ? st.pedalSustainTime * 1.2 : 0.0);
                if (pedalHeld_)
                {
                    const double cA = std::exp(-1.0 / (0.0012 * fs_));
                    const double cD = std::exp(-1.0 / (tauSb * fs_));
                    double e = pedalEnvSb_;
                    bool gate = pedalSbGateOpen_;
                    for (int i = 0; i < n; ++i)
                    {
                        if (gate)
                        {
                            e = 1.0 + cA * (e - 1.0);
                            if (e >= 0.96)
                            {
                                gate = false;
                            }
                        }
                        else
                        {
                            e *= cD;
                        }
                        envPerc_[static_cast<std::size_t>(i)] = e;
                    }
                    pedalEnvSb_ = e;
                    pedalSbGateOpen_ = gate;
                }
                else
                {
                    pedalSbGateOpen_ = false;
                    pedalEnvSb_ = expStepEnv(pedalEnvSb_, 0.0, st.pedalSustain ? tauSb : 0.35, envPerc_.data(), n);
                }
            }

            double ph16 = pedalPhase_;
            double ph32 = pedalPhaseSub_;
            for (int i = 0; i < n; ++i)
            {
                const std::size_t si = static_cast<std::size_t>(i);
                const double dt8 = (fPed * pitchMult_[si]) * invFs;
                const double dt16 = 0.5 * dt8;
                ph16 = wrap01(ph16 + dt16);
                const double ph8 = wrap01(2.0 * ph16);
                ph32 = wrap01(ph32 + 0.5 * dt16);

                if (runPedalBourdon)
                {
                    constexpr std::size_t v = static_cast<std::size_t>(VoiceId::PedalBourdon16);
                    const auto& spec = kVoiceSpecs[v];
                    const double w = staircaseWave(ph16, dt16, spec.source, fs_);
                    busBufs_[v][si] = w * envDir_[si] * st.pedalVolume * keySlopeGain(spec.source, 0.5 * fPed);
                }
                if (runPedalFlute)
                {
                    constexpr std::size_t v = static_cast<std::size_t>(VoiceId::PedalMajorFlute8);
                    const auto& spec = kVoiceSpecs[v];
                    const double bleed = st.enableSubOctBleed ? spec.source.bleedSubOct : 0.0;
                    const double w = staircaseWave(ph8, dt8, spec.source, fs_, bleed, ph16);
                    busBufs_[v][si] = w * envDir_[si] * st.pedalVolume * keySlopeGain(spec.source, fPed);
                }
                if (runPedalSb)
                {
                    constexpr std::size_t v = static_cast<std::size_t>(VoiceId::PedalStringBass);
                    const auto& spec = kVoiceSpecs[v];
                    const double bleed = st.enableSubOctBleed ? spec.source.bleedSubOct : 0.0;
                    const double w = staircaseWave(ph8, dt8, spec.source, fs_, bleed, ph16);
                    busBufs_[v][si] = w * envPerc_[si] * st.pedalVolume * keySlopeGain(spec.source, fPed);
                }
            }
            pedalPhase_ = ph16;
            pedalPhaseSub_ = ph32;

            if (!pedalHeld_ && pedalEnvSust_ < 1e-5 && pedalEnvSb_ < 1e-5)
            {
                pedalActiveMidi_ = -1;
            }
        }

        // --- Shared-Bus Formant Filtering & Rotor / Stationary Routing ---
        rotorSub_.fill(0.0);
        statSub_.fill(0.0);

        const double targetTibiaLoad = computeTargetTibiaLoadGain();
        const double cTibia = std::exp(-1.0 / (0.005 * fs_));
        double tlG = tibiaLoadGainSmooth_;
        for (int i = 0; i < n; ++i)
        {
            if (std::abs(targetTibiaLoad - tlG) < 1e-12)
            {
                tlG = targetTibiaLoad;
            }
            else
            {
                tlG = targetTibiaLoad + cTibia * (tlG - targetTibiaLoad);
            }
            tibiaLoadBuf_[static_cast<std::size_t>(i)] = tlG;
        }
        tibiaLoadGainSmooth_ = tlG;

        for (std::size_t v = 0; v < kNumVoices; ++v)
        {
            if (!voiceRunning[v])
            {
                continue;
            }
            const double targetG = voiceActive[v] ? 1.0 : 0.0;
            double g = voiceTabGain_[v];
            if (kVoiceSpecs[v].manual == Manual::Pedal && !pedalRanThisBlock)
            {
                for (int i = 0; i < n; ++i)
                {
                    g = stepLinearGain(g, targetG, tabStep);
                }
                voiceTabGain_[v] = g;
                if (!voiceActive[v] && g == 0.0)
                {
                    voiceFilters_[v].reset();
                }
                continue;
            }
            const bool isUpperTibia = (v <= static_cast<std::size_t>(VoiceId::UpperTibia2_23));
            voiceFilters_[v].processInPlace(busBufs_[v].data(), n);
            double* dst = (kVoiceSpecs[v].bus == BusRoute::Rotor) ? rotorSub_.data() : statSub_.data();
            for (int i = 0; i < n; ++i)
            {
                const std::size_t si = static_cast<std::size_t>(i);
                g = stepLinearGain(g, targetG, tabStep);
                const double tabG = smoothTabCurve(g);
                const double busG = isUpperTibia ? tibiaLoadBuf_[si] : 1.0;
                dst[i] += busBufs_[v][si] * busG * tabG;
            }
            voiceTabGain_[v] = g;
            if (!voiceActive[v] && g == 0.0)
            {
                voiceFilters_[v].reset();
            }
        }

        // Fixed Presets 0..4 (Perc, Piano, Harpsichord, Banjo, Mandolin)
        for (std::size_t p = 0; p < kNumFixedPresets; ++p)
        {
            if (!presetRunning[p])
            {
                continue;
            }
            fixedPresetFilters_[p].processInPlace(presetBufs_[p].data(), n);
            double* dst = (kFixedPresetSpecs[p].bus == BusRoute::Rotor) ? rotorSub_.data() : statSub_.data();
            addSmoothedPreset(dst, presetBufs_[p].data(), n, st.presetVolume, pOn[p], presetTabGain_[p], tabStep);
            if (!pOn[p] && presetTabGain_[p] == 0.0)
            {
                fixedPresetFilters_[p].reset();
            }
        }

        // MARIMBA_KT Shared Bus Filter
        if (presetRunning[5])
        {
            marimbaBusFilter_.processInPlace(presetBufs_[5].data(), n);
            addSmoothedPreset(statSub_.data(), presetBufs_[5].data(), n, st.presetVolume, pMarimba, presetTabGain_[5], tabStep);
            if (!pMarimba && presetTabGain_[5] == 0.0)
            {
                marimbaBusFilter_.reset();
            }
        }

        // WOW TRUMPET Onset Delay + Legato Carry-Over + ZDF-SVF Wah
        if (presetRunning[6])
        {
            wowBaseFilter_.processBlock(presetBufs_[6].data(), wowBase_.data(), n);
            const double repRate = std::clamp(st.repeatRate, 0.30, 0.80);
            const double wahHz = st.hasRepeatRateHzOverride
                                   ? st.repeatRateHzOverride
                                   : (2.0 + repRate * 5.2);
            const double restFrac = kWowTrumpetParams.lfoClosedRestFrac;
            const double cTh = -std::cos(std::numbers::pi * restFrac);
            const double initPh = kWowTrumpetParams.wahInitPhase;
            const double tCl = kWowTrumpetParams.wahOnsetClosedMs * 1e-3;
            const double tRise = kWowTrumpetParams.wahOnsetRiseMs * 1e-3;
            const double tHold = kWowTrumpetParams.wahOnsetHoldOpenMs * 1e-3;
            const double tTotal = tCl + tRise + tHold;
            const double dt = 1.0 / fs_;
            const double phInc = wahHz * dt;
            const double denom = std::max(1e-6, 1.0 - cTh);
            const double invRise = 1.0 / std::max(1e-6, tRise);

            const double fCl = kWowTrumpetParams.wahFClosedHz;
            const double fOp = kWowTrumpetParams.wahFOpenHz;
            const double qCl = kWowTrumpetParams.wahQClosed;
            const double qOp = kWowTrumpetParams.wahQOpen;
            const double fcExp = kWowTrumpetParams.lfoFcExp;
            const double qDrive = kWowTrumpetParams.lfoQTanhDrive;
            const double tanhQDrive = std::tanh(qDrive);

            double ph = wahPhase_;
            double age = wowOnsetAgeS_;
            for (int i = 0; i < n; ++i)
            {
                double vi = 0.0;
                if (anyUpperHeld && age < tTotal)
                {
                    ph = initPh;
                    if (age < tCl)
                    {
                        vi = 0.0;
                    }
                    else if (age < tCl + tRise)
                    {
                        vi = 0.5 - 0.5 * std::cos(std::numbers::pi * (age - tCl) * invRise);
                    }
                    else
                    {
                        vi = 1.0;
                    }
                    age += dt;
                }
                else
                {
                    vi = std::clamp((-std::cos(2.0 * std::numbers::pi * ph) - cTh) / denom, 0.0, 1.0);
                    ph += phInc;
                    if (ph >= 1.0)
                    {
                        ph -= 1.0;
                    }
                }
                const std::size_t si = static_cast<std::size_t>(i);
                wowFc_[si] = fCl + (fOp - fCl) * std::pow(vi, fcExp);
                wowQ_[si] = qCl + (qOp - qCl) * (std::tanh(qDrive * vi) / tanhQDrive);
            }
            wahPhase_ = ph;
            wowOnsetAgeS_ = age;

            wowWahSvf_.processModulated(wowBase_.data(), wowFc_.data(), wowQ_.data(), wowBase_.data(), n, 16);
            addSmoothedPreset(statSub_.data(), wowBase_.data(), n, st.presetVolume, pWow, presetTabGain_[6], tabStep);
            if (!pWow && presetTabGain_[6] == 0.0)
            {
                wowBaseFilter_.reset();
                wowWahSvf_.reset();
            }
        }

        // Subtle AM from Vibrato LFO
        for (int i = 0; i < n; ++i)
        {
            const std::size_t si = static_cast<std::size_t>(i);
            rotorSub_[si] *= vibAm_[si];
            statSub_[si] *= vibAm_[si];
        }

        // Ensemble Routing
        if (st.ensemble)
        {
            for (int i = 0; i < n; ++i)
            {
                const std::size_t si = static_cast<std::size_t>(i);
                rotorSub_[si] += statSub_[si];
                statSub_[si] = 0.0;
            }
        }

        // Spring Reverb
        if (st.reverb > 1e-6)
        {
            reverbRotor_.processBlock(rotorSub_.data(), n, st.reverb);
            if (!st.ensemble)
            {
                reverbStat_.processBlock(statSub_.data(), n, st.reverb);
            }
        }

        // Expression Pedal (smoothed + bass-preserving loudness contour) + Total Volume + Linear Polyphonic Output
        static constexpr double kPolyHeadroom = 0.18;
        const double targetExp = std::clamp(st.expression, 0.0, 1.0);
        const double cExp = std::exp(-1.0 / (0.005 * fs_));
        const double cExpLp = std::exp(-2.0 * std::numbers::pi * 450.0 / fs_);
        const double cNoiseLp = std::exp(-2.0 * std::numbers::pi * 1800.0 / fs_);
        const double noiseGain = st.analogNoiseLevel * 1.5e-4;
        const bool addNoise = (st.analogNoiseLevel > 1e-6);

        for (int i = 0; i < n; ++i)
        {
            if (std::abs(targetExp - expSmooth_) < 1e-12)
            {
                expSmooth_ = targetExp;
            }
            else
            {
                expSmooth_ = targetExp + cExp * (expSmooth_ - targetExp);
            }

            const std::size_t si = static_cast<std::size_t>(i);
            const double expGain = std::pow(expSmooth_, 1.8) * st.totalVolume * kPolyHeadroom;
            double rIn = rotorSub_[si];
            double sIn = statSub_[si];

            if (st.expressionLoudnessComp && expSmooth_ < 0.9999)
            {
                expLpRotor_ = (1.0 - cExpLp) * rIn + cExpLp * expLpRotor_;
                expLpStat_ = (1.0 - cExpLp) * sIn + cExpLp * expLpStat_;
                const double bassBoost = 0.65 * (1.0 - expSmooth_);
                rIn += bassBoost * expLpRotor_;
                sIn += bassBoost * expLpStat_;
            }
            else
            {
                expLpRotor_ = rIn;
                expLpStat_ = sIn;
            }

            if (addNoise)
            {
                noiseRng_ = noiseRng_ * 1664525u + 1013904223u;
                const double white = (static_cast<double>(noiseRng_ >> 8) * (1.0 / 8388608.0)) - 1.0;
                noiseLp_ = (1.0 - cNoiseLp) * white + cNoiseLp * noiseLp_;
                const double nSig = noiseLp_ * noiseGain;
                rIn += nSig;
                sIn += nSig;
            }

            const double r = rIn * expGain;
            const double s = sIn * expGain;
            rotorSub_[si] = r;
            statSub_[si] = s;
            mixSub_[si] = r + s;
        }

        masterTimeS_ += static_cast<double>(n) / fs_;
    }

    double fs_ = 48000.0;
    KE310State state_{};
    TOSTuningTable tos_{};
    PedalVCO pedalVco_{};
    VibratoLFO vibratoLfo_{};
    SpringReverb reverbRotor_{};
    SpringReverb reverbStat_{};

    std::array<BiquadCascade<kMaxFilterSections>, kNumVoices> voiceFilters_{};
    std::array<BiquadCascade<kMaxFilterSections>, kNumFixedPresets> fixedPresetFilters_{};
    BiquadCascade<3> wowBaseFilter_{};
    ZdfSvfLP2 wowWahSvf_{};
    BiquadCascade<2> marimbaBusFilter_{};

    std::array<UpperNoteState, 128> upperNotes_{};
    std::array<LowerNoteState, 128> lowerNotes_{};

    int pedalActiveMidi_ = -1;
    double pedalPhase_ = 0.0;
    double pedalPhaseSub_ = 0.0;
    double pedalEnvSust_ = 0.0;
    double pedalEnvSb_ = 0.0;
    bool pedalSbGateOpen_ = false;
    bool pedalHeld_ = false;

    double masterTimeS_ = 0.0;
    double repeatPhase_ = 0.0;
    double repRcBanjo_ = 0.0;
    double repRcMandolin_ = 0.0;
    double marimbaRc8_ = 0.0;
    double marimbaRc4_ = 0.0;
    double wahPhase_ = kWowTrumpetParams.wahInitPhase;
    double wowOnsetAgeS_ = 0.0;
    double expSmooth_ = 1.0;
    double expLpRotor_ = 0.0;
    double expLpStat_ = 0.0;
    std::uint32_t noiseRng_ = 0x13579BDFu;
    double noiseLp_ = 0.0;
    std::array<double, kNumVoices> voiceTabGain_{};
    std::array<double, 7> presetTabGain_{};
    double tibiaLoadGainSmooth_ = 1.0;

    // Pre-allocated real-time scratch buffers
    std::array<double, kMaxSubBlock> pitchMult_{};
    std::array<double, kMaxSubBlock> vibAm_{};
    std::array<double, kMaxSubBlock> repPulseBanjo_{};
    std::array<double, kMaxSubBlock> repPulseMandolin_{};
    std::array<double, kMaxSubBlock> marimbaEnv8_{};
    std::array<double, kMaxSubBlock> marimbaEnv4_{};
    std::array<double, kMaxSubBlock> env84_{};
    std::array<double, kMaxSubBlock> envDir_{};
    std::array<double, kMaxSubBlock> envPerc_{};
    std::array<double, kMaxSubBlock> wowFc_{};
    std::array<double, kMaxSubBlock> wowQ_{};
    std::array<double, kMaxSubBlock> wowBase_{};
    std::array<double, kMaxSubBlock> tibiaLoadBuf_{};
    std::array<std::array<double, kMaxSubBlock>, kNumVoices> busBufs_{};
    std::array<std::array<double, kMaxSubBlock>, 7> presetBufs_{};
    std::array<double, kMaxSubBlock> rotorSub_{};
    std::array<double, kMaxSubBlock> statSub_{};
    std::array<double, kMaxSubBlock> mixSub_{};
};

} // namespace ke310::dsp
