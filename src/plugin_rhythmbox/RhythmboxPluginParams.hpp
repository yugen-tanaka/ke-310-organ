// Shared APVTS parameter IDs, factory preset table, and atomic parameter reader
// for KE Rhythmbox (reused by both standalone KERhythmbox and Stage 2 KE310Organ).
#pragma once

#include "dsp/rhythmbox/RhythmboxEngine.hpp"

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace ke310::rhythmbox_plugin
{

namespace param_ids
{
inline constexpr const char* rbEnabled      = "rb_enabled";
inline constexpr const char* rbBreak        = "rb_break";
inline constexpr const char* rbMultiPattern = "rb_multi_pattern";
inline constexpr const char* rbVariation    = "rb_variation";
inline constexpr const char* rbSyncMode     = "rb_sync_mode";
inline constexpr const char* rbBpm          = "rb_bpm";
inline constexpr const char* rbVolume       = "rb_volume";
inline constexpr const char* rbTone         = "rb_tone";
inline constexpr const char* rbAnalogDrift  = "rb_analog_drift";
inline constexpr const char* rbMidiCh       = "rb_midi_ch";
inline constexpr const char* rbMidiStartEn  = "rb_midi_start_enabled";
inline constexpr const char* rbMidiStartCh  = "rb_midi_start_ch";
inline constexpr const char* rbStopCcEn     = "rb_stop_cc_enabled";
inline constexpr const char* rbStopCcNum    = "rb_stop_cc_num";
inline constexpr const char* rbStopCcCh     = "rb_stop_cc_ch";

inline constexpr std::array<const char*, ke310::rhythmbox::kNumPatterns> kPatternParamIds = {{
    "rb_pat_waltz",
    "rb_pat_ballad",
    "rb_pat_march",
    "rb_pat_tango",
    "rb_pat_rock_1",
    "rb_pat_rock_2",
    "rb_pat_rock_3",
    "rb_pat_bossanova",
    "rb_pat_swing_1",
    "rb_pat_swing_2",
    "rb_pat_swing_3",
    "rb_pat_samba",
    "rb_pat_mambo",
    "rb_pat_beguine",
    "rb_pat_cha_cha",
    "rb_pat_rhumba"
}};
} // namespace param_ids

struct FactoryRhythmProgram
{
    const char* name;
    ke310::rhythmbox::PatternId pattern;
    double bpm;
};

inline constexpr std::array<FactoryRhythmProgram, ke310::rhythmbox::kNumPatterns> kFactoryRhythmPrograms = {{
    {"01 Waltz (3/4)",       ke310::rhythmbox::PatternId::Waltz,     106.8},
    {"02 Ballad (Triplet)",  ke310::rhythmbox::PatternId::Ballad,    106.8},
    {"03 March",             ke310::rhythmbox::PatternId::March,     106.8},
    {"04 Tango",             ke310::rhythmbox::PatternId::Tango,     106.8},
    {"05 Rock I",            ke310::rhythmbox::PatternId::Rock1,     106.8},
    {"06 Rock II",           ke310::rhythmbox::PatternId::Rock2,     106.8},
    {"07 Rock III",          ke310::rhythmbox::PatternId::Rock3,     106.8},
    {"08 Bossa Nova",        ke310::rhythmbox::PatternId::Bossanova, 106.8},
    {"09 Swing I",           ke310::rhythmbox::PatternId::Swing1,    106.8},
    {"10 Swing II",          ke310::rhythmbox::PatternId::Swing2,    106.8},
    {"11 Swing III",         ke310::rhythmbox::PatternId::Swing3,    106.8},
    {"12 Samba",             ke310::rhythmbox::PatternId::Samba,     106.9},
    {"13 Mambo",             ke310::rhythmbox::PatternId::Mambo,     106.8},
    {"14 Beguine",           ke310::rhythmbox::PatternId::Beguine,   106.8},
    {"15 Cha-Cha",           ke310::rhythmbox::PatternId::ChaCha,    106.8},
    {"16 Rhumba",            ke310::rhythmbox::PatternId::Rhumba,    106.8}
}};

inline juce::StringArray makeSyncModeChoices()
{
    return {"Internal", "Host Tempo", "Host Sync"};
}

inline juce::StringArray makeMidiChannelChoices()
{
    juce::StringArray arr;
    arr.add("Omni");
    for (int ch = 1; ch <= 16; ++ch)
    {
        arr.add("Ch " + juce::String(ch));
    }
    arr.add("Off (Disabled)");
    return arr;
}

inline juce::StringArray makeMidiTriggerChannelChoices()
{
    juce::StringArray arr;
    arr.add("Omni");
    for (int ch = 1; ch <= 16; ++ch)
    {
        arr.add("Ch " + juce::String(ch));
    }
    return arr;
}

inline void addRhythmboxParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
                                   int defaultMidiChannelIndex = 0)
{
    auto addBool = [&params](const char* id, const char* name, bool def)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(id, 1), name, def));
    };

    addBool(param_ids::rbEnabled, "Rhythm Enable", false);
    addBool(param_ids::rbBreak, "Rhythm Break", false);
    addBool(param_ids::rbMultiPattern, "Multi-Pattern (Wired-OR)", false);
    addBool(param_ids::rbVariation, "Variation", false);

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(param_ids::rbSyncMode, 1), "Sync Mode", makeSyncModeChoices(), 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param_ids::rbBpm, 1), "Tempo (BPM)",
        juce::NormalisableRange<float>(40.0f, 240.0f, 0.1f, 1.0f), 106.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param_ids::rbVolume, 1), "Rhythm Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.85f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param_ids::rbTone, 1), "Rhythm Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param_ids::rbAnalogDrift, 1), "Analog Drift",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f, 1.0f), 1.0f));

    const int defMidiChIdx = std::clamp(defaultMidiChannelIndex, 0, 17);
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(param_ids::rbMidiCh, 1), "MIDI Channel", makeMidiChannelChoices(), defMidiChIdx));

    for (std::size_t i = 0; i < ke310::rhythmbox::kNumPatterns; ++i)
    {
        const bool isDefault = (i == static_cast<std::size_t>(ke310::rhythmbox::PatternId::Rock1));
        addBool(param_ids::kPatternParamIds[i], ke310::rhythmbox::kPatternDisplayNames[i], isDefault);
    }
}

inline void addStandaloneRhythmboxTriggerParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params)
{
    auto addBool = [&params](const char* id, const char* name, bool def)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(id, 1), name, def));
    };

    addBool(param_ids::rbMidiStartEn, "MIDI Key Start", false);
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(param_ids::rbMidiStartCh, 1), "MIDI Start Channel", makeMidiTriggerChannelChoices(), 0));

    addBool(param_ids::rbStopCcEn, "Rhythm Stop CC Enable", true);
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID(param_ids::rbStopCcNum, 1), "Rhythm Stop CC", 0, 127, 67));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(param_ids::rbStopCcCh, 1), "Rhythm Stop CC Channel", makeMidiTriggerChannelChoices(), 0));
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createRhythmboxParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    addRhythmboxParameters(params);
    addStandaloneRhythmboxTriggerParameters(params);
    return {params.begin(), params.end()};
}

struct CachedRhythmboxParams
{
    std::atomic<float>* enabled = nullptr;
    std::atomic<float>* breakMode = nullptr;
    std::atomic<float>* multiPattern = nullptr;
    std::atomic<float>* variation = nullptr;
    std::atomic<float>* syncMode = nullptr;
    std::atomic<float>* bpm = nullptr;
    std::atomic<float>* volume = nullptr;
    std::atomic<float>* tone = nullptr;
    std::atomic<float>* analogDrift = nullptr;
    std::atomic<float>* midiCh = nullptr;
    std::atomic<float>* midiStartEn = nullptr;
    std::atomic<float>* midiStartCh = nullptr;
    std::atomic<float>* stopCcEn = nullptr;
    std::atomic<float>* stopCcNum = nullptr;
    std::atomic<float>* stopCcCh = nullptr;
    std::array<std::atomic<float>*, ke310::rhythmbox::kNumPatterns> patterns{};

    void bind(const juce::AudioProcessorValueTreeState& apvts) noexcept
    {
        auto p = [&apvts](const char* id) noexcept { return apvts.getRawParameterValue(id); };
        enabled = p(param_ids::rbEnabled);
        breakMode = p(param_ids::rbBreak);
        multiPattern = p(param_ids::rbMultiPattern);
        variation = p(param_ids::rbVariation);
        syncMode = p(param_ids::rbSyncMode);
        bpm = p(param_ids::rbBpm);
        volume = p(param_ids::rbVolume);
        tone = p(param_ids::rbTone);
        analogDrift = p(param_ids::rbAnalogDrift);
        midiCh = p(param_ids::rbMidiCh);
        midiStartEn = p(param_ids::rbMidiStartEn);
        midiStartCh = p(param_ids::rbMidiStartCh);
        stopCcEn = p(param_ids::rbStopCcEn);
        stopCcNum = p(param_ids::rbStopCcNum);
        stopCcCh = p(param_ids::rbStopCcCh);
        for (std::size_t i = 0; i < ke310::rhythmbox::kNumPatterns; ++i)
        {
            patterns[i] = p(param_ids::kPatternParamIds[i]);
        }
    }

    ke310::rhythmbox::RhythmboxState readState() const noexcept
    {
        auto b = [](const std::atomic<float>* ptr) noexcept
        {
            return (ptr != nullptr) && (ptr->load(std::memory_order_relaxed) >= 0.5f);
        };
        auto d = [](const std::atomic<float>* ptr, double fallback) noexcept
        {
            return (ptr != nullptr) ? static_cast<double>(ptr->load(std::memory_order_relaxed)) : fallback;
        };
        auto i = [](const std::atomic<float>* ptr, int fallback) noexcept
        {
            return (ptr != nullptr) ? static_cast<int>(std::lround(ptr->load(std::memory_order_relaxed))) : fallback;
        };

        ke310::rhythmbox::RhythmboxState st;
        st.enabled = b(enabled);
        st.multiPatternEnabled = b(multiPattern);
        st.variation = b(variation);
        const int modeIdx = std::clamp(i(syncMode, 1), 0, 2);
        st.clockMode = static_cast<ke310::rhythmbox::ClockMode>(modeIdx);
        st.bpm = std::clamp(d(bpm, 106.8), 40.0, 240.0);
        st.volume = std::clamp(d(volume, 0.85), 0.0, 1.0);
        st.tone = std::clamp(d(tone, 0.5), 0.0, 1.0);
        st.analogVariation = std::clamp(d(analogDrift, 1.0), 0.0, 1.5);

        std::uint16_t mask = 0u;
        for (std::size_t k = 0; k < ke310::rhythmbox::kNumPatterns; ++k)
        {
            if (b(patterns[k]))
            {
                mask |= static_cast<std::uint16_t>(1u << k);
            }
        }
        st.patternMask = mask;
        return st;
    }

    int readMidiChannel() const noexcept
    {
        if (midiCh == nullptr)
        {
            return 0;
        }
        const int idx = static_cast<int>(std::lround(midiCh->load(std::memory_order_relaxed)));
        return (idx >= 17) ? -1 : std::clamp(idx, 0, 16);
    }

    bool readBreakEnabled() const noexcept
    {
        return (breakMode != nullptr) && (breakMode->load(std::memory_order_relaxed) >= 0.5f);
    }

    bool readMidiStartEnabled() const noexcept
    {
        return (midiStartEn != nullptr) && (midiStartEn->load(std::memory_order_relaxed) >= 0.5f);
    }

    int readMidiStartChannel() const noexcept
    {
        if (midiStartCh == nullptr)
        {
            return 0;
        }
        return std::clamp(static_cast<int>(std::lround(midiStartCh->load(std::memory_order_relaxed))), 0, 16);
    }

    bool readStopCcEnabled() const noexcept
    {
        return (stopCcEn == nullptr) || (stopCcEn->load(std::memory_order_relaxed) >= 0.5f);
    }

    int readStopCcNumber() const noexcept
    {
        if (stopCcNum == nullptr)
        {
            return 67;
        }
        return std::clamp(static_cast<int>(std::lround(stopCcNum->load(std::memory_order_relaxed))), 0, 127);
    }

    int readStopCcChannel() const noexcept
    {
        if (stopCcCh == nullptr)
        {
            return 0;
        }
        return std::clamp(static_cast<int>(std::lround(stopCcCh->load(std::memory_order_relaxed))), 0, 16);
    }
};

} // namespace ke310::rhythmbox_plugin
