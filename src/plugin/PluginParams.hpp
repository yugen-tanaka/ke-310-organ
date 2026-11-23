// Shared APVTS parameter IDs, real-time atomic pointer cache, and layout definition
// for the KE-310 JUCE 8 VST3 / AU / Standalone plugin.
#pragma once

#include "dsp/KE310Engine.hpp"
#include "dsp/MidiRouter.hpp"
#include "dsp/RotarySpeaker.hpp"
#include "plugin_rhythmbox/RhythmboxPluginParams.hpp"

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace ke310::plugin
{

namespace param_ids
{
// Upper Manual Tone Levers (10)
inline constexpr const char* upperTibia16   = "upper_tibia_16";
inline constexpr const char* upperTibia8    = "upper_tibia_8";
inline constexpr const char* upperTibia4    = "upper_tibia_4";
inline constexpr const char* upperTibia2_23 = "upper_tibia_2_23";
inline constexpr const char* upperDiapason8 = "upper_diapason_8";
inline constexpr const char* upperString16  = "upper_string_16";
inline constexpr const char* upperString8   = "upper_string_8";
inline constexpr const char* upperString4   = "upper_string_4";
inline constexpr const char* upperOboe8     = "upper_oboe_8";
inline constexpr const char* upperClarinet8 = "upper_clarinet_8";

// Lower Manual Tone Levers (6)
inline constexpr const char* lowerTibia8    = "lower_tibia_8";
inline constexpr const char* lowerTibia4    = "lower_tibia_4";
inline constexpr const char* lowerDiapason8 = "lower_diapason_8";
inline constexpr const char* lowerString8   = "lower_string_8";
inline constexpr const char* lowerHorn8     = "lower_horn_8";
inline constexpr const char* lowerString4   = "lower_string_4";

// Pedal Tone Levers (3)
inline constexpr const char* pedalBourdon16   = "pedal_bourdon_16";
inline constexpr const char* pedalMajorFlute8 = "pedal_major_flute_8";
inline constexpr const char* pedalStringBass  = "pedal_string_bass";

// Upper Presets (7 + Organ Silent)
inline constexpr const char* organSilent       = "organ_silent";
inline constexpr const char* presetPerc2_23    = "preset_perc_2_23";
inline constexpr const char* presetWowTrumpet  = "preset_wow_trumpet";
inline constexpr const char* presetBanjo       = "preset_banjo";
inline constexpr const char* presetMandolin    = "preset_mandolin";
inline constexpr const char* presetMarimba     = "preset_marimba";
inline constexpr const char* presetPiano       = "preset_piano";
inline constexpr const char* presetHarpsichord = "preset_harpsichord";

// Effects & Sustain (7)
inline constexpr const char* upperSustain  = "upper_sustain";
inline constexpr const char* pedalSustain  = "pedal_sustain";
inline constexpr const char* kneeSustain   = "knee_sustain";
inline constexpr const char* vibrato       = "vibrato";
inline constexpr const char* vibratoFast   = "vibrato_fast";
inline constexpr const char* delayVibrato  = "delay_vibrato";
inline constexpr const char* ensemble      = "ensemble";

// Built-in Hornless Rotary Drum Speaker (4)
inline constexpr const char* rotaryChorus  = "rotary_chorus";
inline constexpr const char* rotaryTremolo = "rotary_tremolo";
inline constexpr const char* rotarySlowHz  = "rotary_slow_hz";
inline constexpr const char* rotaryFastHz  = "rotary_fast_hz";

// Continuous Knobs (10)
inline constexpr const char* totalVolume      = "total_volume";
inline constexpr const char* lowerVolume      = "lower_volume";
inline constexpr const char* pedalVolume      = "pedal_volume";
inline constexpr const char* presetVolume     = "preset_volume";
inline constexpr const char* reverb           = "reverb";
inline constexpr const char* upperSustainTime = "upper_sustain_time";
inline constexpr const char* pedalSustainTime = "pedal_sustain_time";
inline constexpr const char* percusDecay      = "percus_decay";
inline constexpr const char* repeatRate       = "repeat_rate";
inline constexpr const char* expression       = "expression";

// User-Selectable MIDI Routing (12)
inline constexpr const char* midiMode        = "midi_mode";
inline constexpr const char* upperMidiCh     = "upper_midi_ch";
inline constexpr const char* lowerMidiCh     = "lower_midi_ch";
inline constexpr const char* pedalMidiCh     = "pedal_midi_ch";
inline constexpr const char* splitInputCh    = "split_input_ch";
inline constexpr const char* upperOctave     = "upper_octave";
inline constexpr const char* lowerOctave     = "lower_octave";
inline constexpr const char* pedalOctave     = "pedal_octave";
inline constexpr const char* pedalSplitMax   = "pedal_split_max";
inline constexpr const char* lowerSplitMax   = "lower_split_max";
inline constexpr const char* foldPedalOctave = "fold_pedal_octave";
inline constexpr const char* clampKeyRange   = "clamp_key_range";

// Circuit Authenticity & Calibration (7)
inline constexpr const char* vintageTos             = "vintage_tos";
inline constexpr const char* vintagePedalVco        = "vintage_pedal_vco";
inline constexpr const char* tibiaBusLoadComp       = "tibia_bus_load_comp";
inline constexpr const char* enableSubOctBleed      = "enable_sub_oct_bleed";
inline constexpr const char* expressionLoudnessComp = "expression_loudness_comp";
inline constexpr const char* a4Hz                   = "a4_hz";
inline constexpr const char* analogNoise            = "analog_noise";

// Embedded KE Rhythmbox Organ Controls (2 organ-specific + shared 25 params)
inline constexpr const char* rbBreak                = "rb_break";
inline constexpr const char* rbOutputBus            = "rb_output_bus";
} // namespace param_ids

inline juce::StringArray makeRhythmOutputBusChoices()
{
    return {"Stationary (Bypass Ensemble)", "Rhythm Out (+ Main)", "Rhythm Out (Isolated)"};
}

inline int choiceIndexToMidiChannel(int idx) noexcept
{
    return (idx >= 17) ? dsp::kMidiChannelOff : std::clamp(idx, 0, 16);
}

inline int midiChannelToChoiceIndex(int ch) noexcept
{
    return (ch < 0) ? 17 : std::clamp(ch, 0, 16);
}

inline juce::StringArray makeManualMidiChannelChoices()
{
    juce::StringArray arr;
    arr.add("Omni (All)");
    for (int ch = 1; ch <= 16; ++ch)
    {
        arr.add("Ch " + juce::String(ch));
    }
    arr.add("Off (Disabled)");
    return arr;
}

inline juce::StringArray makeSplitInputChannelChoices()
{
    juce::StringArray arr;
    arr.add("Omni (All)");
    for (int ch = 1; ch <= 16; ++ch)
    {
        arr.add("Ch " + juce::String(ch));
    }
    return arr;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addBool = [&](const char* id, const char* name, bool def)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, 1}, name, def));
    };
    auto addFloat = [&](const char* id, const char* name, float minV, float maxV, float defV, float step = 0.001f)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>{minV, maxV, step}, defV));
    };
    auto addInt = [&](const char* id, const char* name, int minV, int maxV, int defV)
    {
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{id, 1}, name, minV, maxV, defV));
    };

    // Upper Manual (default: TIBIA 8' ON)
    addBool(param_ids::upperTibia16,   "Upper Tibia 16'",   false);
    addBool(param_ids::upperTibia8,    "Upper Tibia 8'",    true);
    addBool(param_ids::upperTibia4,    "Upper Tibia 4'",    false);
    addBool(param_ids::upperTibia2_23, "Upper Tibia 2 2/3'", false);
    addBool(param_ids::upperDiapason8, "Upper Diapason 8'", false);
    addBool(param_ids::upperString16,  "Upper String 16'",  false);
    addBool(param_ids::upperString8,   "Upper String 8'",   false);
    addBool(param_ids::upperString4,   "Upper String 4'",   false);
    addBool(param_ids::upperOboe8,     "Upper Oboe 8'",     false);
    addBool(param_ids::upperClarinet8, "Upper Clarinet 8'", false);

    // Lower Manual
    addBool(param_ids::lowerTibia8,    "Lower Tibia 8'",    false);
    addBool(param_ids::lowerTibia4,    "Lower Tibia 4'",    false);
    addBool(param_ids::lowerDiapason8, "Lower Diapason 8'", false);
    addBool(param_ids::lowerString8,   "Lower String 8'",   false);
    addBool(param_ids::lowerHorn8,     "Lower Horn 8'",     false);
    addBool(param_ids::lowerString4,   "Lower String 4'",   false);

    // Pedal
    addBool(param_ids::pedalBourdon16,   "Pedal Bourdon 16'",     false);
    addBool(param_ids::pedalMajorFlute8, "Pedal Major Flute 8'", false);
    addBool(param_ids::pedalStringBass,  "Pedal String Bass",     false);

    // Upper Presets
    addBool(param_ids::organSilent,       "Organ Silent",       false);
    addBool(param_ids::presetPerc2_23,    "Preset Percus 2 2/3'", false);
    addBool(param_ids::presetWowTrumpet,  "Preset Wow Trumpet", false);
    addBool(param_ids::presetBanjo,       "Preset Banjo",       false);
    addBool(param_ids::presetMandolin,    "Preset Mandolin",    false);
    addBool(param_ids::presetMarimba,     "Preset Marimba",     false);
    addBool(param_ids::presetPiano,       "Preset Piano",       false);
    addBool(param_ids::presetHarpsichord, "Preset Harpsichord", false);

    // Effects & Sustain
    addBool(param_ids::upperSustain, "Upper Sustain", false);
    addBool(param_ids::pedalSustain, "Pedal Sustain", false);
    addBool(param_ids::kneeSustain,  "Knee Sustainer", false);
    addBool(param_ids::vibrato,      "Vibrato", false);
    addBool(param_ids::vibratoFast,  "Vibrato Fast", false);
    addBool(param_ids::delayVibrato, "Delay Vibrato", false);
    addBool(param_ids::ensemble,     "Ensemble (Stationary -> Rotor)", false);

    // Built-in Hornless Rotary Drum Speaker
    addBool(param_ids::rotaryChorus,  "Rotary Chorus (Slow)",  false);
    addBool(param_ids::rotaryTremolo, "Rotary Tremolo (Fast)", false);
    addFloat(param_ids::rotarySlowHz, "Rotary Slow Speed (Hz)", 0.30f, 1.80f, 0.80f, 0.01f);
    addFloat(param_ids::rotaryFastHz, "Rotary Fast Speed (Hz)", 3.50f, 8.50f, 6.00f, 0.01f);

    // Knobs
    addFloat(param_ids::totalVolume,      "Total Volume",       0.0f, 1.0f, 0.85f);
    addFloat(param_ids::lowerVolume,      "Lower Volume",       0.0f, 1.0f, 0.80f);
    addFloat(param_ids::pedalVolume,      "Pedal Volume",       0.0f, 1.0f, 0.85f);
    addFloat(param_ids::presetVolume,     "Preset Volume",      0.0f, 1.0f, 0.85f);
    addFloat(param_ids::reverb,           "Spring Reverb",      0.0f, 1.0f, 0.0f);
    addFloat(param_ids::upperSustainTime, "Upper Sustain Time", 0.0f, 1.0f, 0.45f);
    addFloat(param_ids::pedalSustainTime, "Pedal Sustain Time", 0.0f, 1.0f, 0.45f);
    addFloat(param_ids::percusDecay,      "Percus Decay",       0.0f, 1.0f, 0.25f);
    addFloat(param_ids::repeatRate,       "Repeat / Wah Rate",  0.30f, 0.80f, 0.40f);
    addFloat(param_ids::expression,       "Expression Pedal",   0.0f, 1.0f, 1.0f);

    // MIDI Routing (User-selectable per manual!)
    const auto manualChChoices = makeManualMidiChannelChoices();
    const auto splitChChoices = makeSplitInputChannelChoices();
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{param_ids::midiMode, 1}, "MIDI Mode",
        juce::StringArray{"Multi-Channel", "Single-Keyboard Split"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{param_ids::upperMidiCh, 1}, "Upper MIDI Channel", manualChChoices, 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{param_ids::lowerMidiCh, 1}, "Lower MIDI Channel", manualChChoices, 2));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{param_ids::pedalMidiCh, 1}, "Pedal MIDI Channel", manualChChoices, 3));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{param_ids::splitInputCh, 1}, "Split Input Channel", splitChChoices, 0));
    addInt(param_ids::upperOctave,   "Upper Octave Shift",  -2, 2, 0);
    addInt(param_ids::lowerOctave,   "Lower Octave Shift",  -2, 2, 0);
    addInt(param_ids::pedalOctave,   "Pedal Octave Shift",  -2, 2, 0);
    addInt(param_ids::pedalSplitMax, "Split Pedal Max Note", 24, 60, 47);
    addInt(param_ids::lowerSplitMax, "Split Lower Max Note", 36, 84, 59);
    addBool(param_ids::foldPedalOctave, "Fold Pedal to C2-C3", true);
    addBool(param_ids::clampKeyRange,   "Clamp Physical Key Range", false);

    // Circuit Authenticity & Calibration
    addBool(param_ids::vintageTos,             "Vintage TOS Dividers",     true);
    addBool(param_ids::vintagePedalVco,        "Vintage Pedal VCO Stretch", true);
    addBool(param_ids::tibiaBusLoadComp,       "Tibia Bus Loading",        true);
    addBool(param_ids::enableSubOctBleed,      "Pedal Sub-Octave Bleed",   true);
    addBool(param_ids::expressionLoudnessComp, "Expression Loudness Comp", true);
    addFloat(param_ids::a4Hz,        "Master Tuning A4 (Hz)", 430.0f, 450.0f, 440.91f, 0.01f);
    addFloat(param_ids::analogNoise, "Main Amp Noise Floor",  0.0f,   1.0f,   0.0f,    0.01f);

    // Embedded KE Rhythmbox parameters (default GM drum trigger channel = Ch 10)
    ke310::rhythmbox_plugin::addRhythmboxParameters(params, 10);
    addBool(param_ids::rbBreak, "Rhythm Break", false);
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{param_ids::rbOutputBus, 1}, "Rhythm Output Bus",
        makeRhythmOutputBusChoices(), 0));

    return {params.begin(), params.end()};
}

struct CachedParameterPointers
{
    std::atomic<float>* upperTibia16 = nullptr;
    std::atomic<float>* upperTibia8 = nullptr;
    std::atomic<float>* upperTibia4 = nullptr;
    std::atomic<float>* upperTibia2_23 = nullptr;
    std::atomic<float>* upperDiapason8 = nullptr;
    std::atomic<float>* upperString16 = nullptr;
    std::atomic<float>* upperString8 = nullptr;
    std::atomic<float>* upperString4 = nullptr;
    std::atomic<float>* upperOboe8 = nullptr;
    std::atomic<float>* upperClarinet8 = nullptr;

    std::atomic<float>* lowerTibia8 = nullptr;
    std::atomic<float>* lowerTibia4 = nullptr;
    std::atomic<float>* lowerDiapason8 = nullptr;
    std::atomic<float>* lowerString8 = nullptr;
    std::atomic<float>* lowerHorn8 = nullptr;
    std::atomic<float>* lowerString4 = nullptr;

    std::atomic<float>* pedalBourdon16 = nullptr;
    std::atomic<float>* pedalMajorFlute8 = nullptr;
    std::atomic<float>* pedalStringBass = nullptr;

    std::atomic<float>* organSilent = nullptr;
    std::atomic<float>* presetPerc2_23 = nullptr;
    std::atomic<float>* presetWowTrumpet = nullptr;
    std::atomic<float>* presetBanjo = nullptr;
    std::atomic<float>* presetMandolin = nullptr;
    std::atomic<float>* presetMarimba = nullptr;
    std::atomic<float>* presetPiano = nullptr;
    std::atomic<float>* presetHarpsichord = nullptr;

    std::atomic<float>* upperSustain = nullptr;
    std::atomic<float>* pedalSustain = nullptr;
    std::atomic<float>* kneeSustain = nullptr;
    std::atomic<float>* vibrato = nullptr;
    std::atomic<float>* vibratoFast = nullptr;
    std::atomic<float>* delayVibrato = nullptr;
    std::atomic<float>* ensemble = nullptr;

    std::atomic<float>* rotaryChorus = nullptr;
    std::atomic<float>* rotaryTremolo = nullptr;
    std::atomic<float>* rotarySlowHz = nullptr;
    std::atomic<float>* rotaryFastHz = nullptr;

    std::atomic<float>* totalVolume = nullptr;
    std::atomic<float>* lowerVolume = nullptr;
    std::atomic<float>* pedalVolume = nullptr;
    std::atomic<float>* presetVolume = nullptr;
    std::atomic<float>* reverb = nullptr;
    std::atomic<float>* upperSustainTime = nullptr;
    std::atomic<float>* pedalSustainTime = nullptr;
    std::atomic<float>* percusDecay = nullptr;
    std::atomic<float>* repeatRate = nullptr;
    std::atomic<float>* expression = nullptr;

    std::atomic<float>* midiMode = nullptr;
    std::atomic<float>* upperMidiCh = nullptr;
    std::atomic<float>* lowerMidiCh = nullptr;
    std::atomic<float>* pedalMidiCh = nullptr;
    std::atomic<float>* splitInputCh = nullptr;
    std::atomic<float>* upperOctave = nullptr;
    std::atomic<float>* lowerOctave = nullptr;
    std::atomic<float>* pedalOctave = nullptr;
    std::atomic<float>* pedalSplitMax = nullptr;
    std::atomic<float>* lowerSplitMax = nullptr;
    std::atomic<float>* foldPedalOctave = nullptr;
    std::atomic<float>* clampKeyRange = nullptr;

    std::atomic<float>* vintageTos = nullptr;
    std::atomic<float>* vintagePedalVco = nullptr;
    std::atomic<float>* tibiaBusLoadComp = nullptr;
    std::atomic<float>* enableSubOctBleed = nullptr;
    std::atomic<float>* expressionLoudnessComp = nullptr;
    std::atomic<float>* a4Hz = nullptr;
    std::atomic<float>* analogNoise = nullptr;

    ke310::rhythmbox_plugin::CachedRhythmboxParams rhythmbox{};
    std::atomic<float>* rbBreak = nullptr;
    std::atomic<float>* rbOutputBus = nullptr;

    void bind(const juce::AudioProcessorValueTreeState& apvts) noexcept
    {
        auto g = [&](const char* id) { return apvts.getRawParameterValue(id); };
        upperTibia16   = g(param_ids::upperTibia16);
        upperTibia8    = g(param_ids::upperTibia8);
        upperTibia4    = g(param_ids::upperTibia4);
        upperTibia2_23 = g(param_ids::upperTibia2_23);
        upperDiapason8 = g(param_ids::upperDiapason8);
        upperString16  = g(param_ids::upperString16);
        upperString8   = g(param_ids::upperString8);
        upperString4   = g(param_ids::upperString4);
        upperOboe8     = g(param_ids::upperOboe8);
        upperClarinet8 = g(param_ids::upperClarinet8);

        lowerTibia8    = g(param_ids::lowerTibia8);
        lowerTibia4    = g(param_ids::lowerTibia4);
        lowerDiapason8 = g(param_ids::lowerDiapason8);
        lowerString8   = g(param_ids::lowerString8);
        lowerHorn8     = g(param_ids::lowerHorn8);
        lowerString4   = g(param_ids::lowerString4);

        pedalBourdon16   = g(param_ids::pedalBourdon16);
        pedalMajorFlute8 = g(param_ids::pedalMajorFlute8);
        pedalStringBass  = g(param_ids::pedalStringBass);

        organSilent       = g(param_ids::organSilent);
        presetPerc2_23    = g(param_ids::presetPerc2_23);
        presetWowTrumpet  = g(param_ids::presetWowTrumpet);
        presetBanjo       = g(param_ids::presetBanjo);
        presetMandolin    = g(param_ids::presetMandolin);
        presetMarimba     = g(param_ids::presetMarimba);
        presetPiano       = g(param_ids::presetPiano);
        presetHarpsichord = g(param_ids::presetHarpsichord);

        upperSustain = g(param_ids::upperSustain);
        pedalSustain = g(param_ids::pedalSustain);
        kneeSustain  = g(param_ids::kneeSustain);
        vibrato      = g(param_ids::vibrato);
        vibratoFast  = g(param_ids::vibratoFast);
        delayVibrato = g(param_ids::delayVibrato);
        ensemble     = g(param_ids::ensemble);

        rotaryChorus  = g(param_ids::rotaryChorus);
        rotaryTremolo = g(param_ids::rotaryTremolo);
        rotarySlowHz  = g(param_ids::rotarySlowHz);
        rotaryFastHz  = g(param_ids::rotaryFastHz);

        totalVolume      = g(param_ids::totalVolume);
        lowerVolume      = g(param_ids::lowerVolume);
        pedalVolume      = g(param_ids::pedalVolume);
        presetVolume     = g(param_ids::presetVolume);
        reverb           = g(param_ids::reverb);
        upperSustainTime = g(param_ids::upperSustainTime);
        pedalSustainTime = g(param_ids::pedalSustainTime);
        percusDecay      = g(param_ids::percusDecay);
        repeatRate       = g(param_ids::repeatRate);
        expression       = g(param_ids::expression);

        midiMode        = g(param_ids::midiMode);
        upperMidiCh     = g(param_ids::upperMidiCh);
        lowerMidiCh     = g(param_ids::lowerMidiCh);
        pedalMidiCh     = g(param_ids::pedalMidiCh);
        splitInputCh    = g(param_ids::splitInputCh);
        upperOctave     = g(param_ids::upperOctave);
        lowerOctave     = g(param_ids::lowerOctave);
        pedalOctave     = g(param_ids::pedalOctave);
        pedalSplitMax   = g(param_ids::pedalSplitMax);
        lowerSplitMax   = g(param_ids::lowerSplitMax);
        foldPedalOctave = g(param_ids::foldPedalOctave);
        clampKeyRange   = g(param_ids::clampKeyRange);

        vintageTos             = g(param_ids::vintageTos);
        vintagePedalVco        = g(param_ids::vintagePedalVco);
        tibiaBusLoadComp       = g(param_ids::tibiaBusLoadComp);
        enableSubOctBleed      = g(param_ids::enableSubOctBleed);
        expressionLoudnessComp = g(param_ids::expressionLoudnessComp);
        a4Hz                   = g(param_ids::a4Hz);
        analogNoise            = g(param_ids::analogNoise);

        rhythmbox.bind(apvts);
        rbBreak                = g(param_ids::rbBreak);
        rbOutputBus            = g(param_ids::rbOutputBus);
    }

    dsp::KE310State readEngineState() const noexcept
    {
        auto b = [](const std::atomic<float>* p) noexcept { return p->load(std::memory_order_relaxed) >= 0.5f; };
        auto d = [](const std::atomic<float>* p) noexcept { return static_cast<double>(p->load(std::memory_order_relaxed)); };

        dsp::KE310State st;
        st.upperTibia16   = b(upperTibia16);
        st.upperTibia8    = b(upperTibia8);
        st.upperTibia4    = b(upperTibia4);
        st.upperTibia2_23 = b(upperTibia2_23);
        st.upperDiapason8 = b(upperDiapason8);
        st.upperString16  = b(upperString16);
        st.upperString8   = b(upperString8);
        st.upperString4   = b(upperString4);
        st.upperOboe8     = b(upperOboe8);
        st.upperClarinet8 = b(upperClarinet8);

        st.lowerTibia8    = b(lowerTibia8);
        st.lowerTibia4    = b(lowerTibia4);
        st.lowerDiapason8 = b(lowerDiapason8);
        st.lowerString8   = b(lowerString8);
        st.lowerHorn8     = b(lowerHorn8);
        st.lowerString4   = b(lowerString4);

        st.pedalBourdon16   = b(pedalBourdon16);
        st.pedalMajorFlute8 = b(pedalMajorFlute8);
        st.pedalStringBass  = b(pedalStringBass);

        st.organSilent       = b(organSilent);
        st.presetPerc2_23    = b(presetPerc2_23);
        st.presetWowTrumpet  = b(presetWowTrumpet);
        st.presetBanjo       = b(presetBanjo);
        st.presetMandolin    = b(presetMandolin);
        st.presetMarimba     = b(presetMarimba);
        st.presetPiano       = b(presetPiano);
        st.presetHarpsichord = b(presetHarpsichord);

        st.upperSustain = b(upperSustain);
        st.pedalSustain = b(pedalSustain);
        st.kneeSustain  = b(kneeSustain);
        st.vibrato      = b(vibrato);
        st.vibratoFast  = b(vibratoFast);
        st.delayVibrato = b(delayVibrato);
        st.ensemble     = b(ensemble);

        st.totalVolume      = d(totalVolume);
        st.lowerVolume      = d(lowerVolume);
        st.pedalVolume      = d(pedalVolume);
        st.presetVolume     = d(presetVolume);
        st.reverb           = d(reverb);
        st.upperSustainTime = d(upperSustainTime);
        st.pedalSustainTime = d(pedalSustainTime);
        st.percusDecay      = d(percusDecay);
        st.repeatRate       = d(repeatRate);
        st.expression       = d(expression);

        st.vintageTos             = b(vintageTos);
        st.vintagePedalVco        = b(vintagePedalVco);
        st.tibiaBusLoadComp       = b(tibiaBusLoadComp);
        st.enableSubOctBleed      = b(enableSubOctBleed);
        st.expressionLoudnessComp = b(expressionLoudnessComp);
        st.a4Hz                   = d(a4Hz);
        st.analogNoiseLevel       = d(analogNoise);
        return st;
    }

    dsp::MidiRouterConfig readMidiConfig() const noexcept
    {
        auto b = [](const std::atomic<float>* p) noexcept { return p->load(std::memory_order_relaxed) >= 0.5f; };
        auto i = [](const std::atomic<float>* p) noexcept { return static_cast<int>(std::lround(p->load(std::memory_order_relaxed))); };

        dsp::MidiRouterConfig cfg;
        cfg.mode = (i(midiMode) == 1) ? dsp::MidiMode::SingleKeyboardSplit : dsp::MidiMode::MultiChannel;
        cfg.upperChannel = choiceIndexToMidiChannel(i(upperMidiCh));
        cfg.lowerChannel = choiceIndexToMidiChannel(i(lowerMidiCh));
        cfg.pedalChannel = choiceIndexToMidiChannel(i(pedalMidiCh));
        cfg.splitInputChannel = std::clamp(i(splitInputCh), 0, 16);
        cfg.upperOctaveShift = i(upperOctave);
        cfg.lowerOctaveShift = i(lowerOctave);
        cfg.pedalOctaveShift = i(pedalOctave);
        cfg.pedalSplitMaxNote = i(pedalSplitMax);
        cfg.lowerSplitMaxNote = std::max(cfg.pedalSplitMaxNote + 1, i(lowerSplitMax));
        cfg.foldPedalOctave = b(foldPedalOctave);
        cfg.clampToPhysicalKeyRange = b(clampKeyRange);
        return cfg;
    }

    dsp::RotaryDrumConfig readRotaryConfig() const noexcept
    {
        auto b = [](const std::atomic<float>* p) noexcept { return p->load(std::memory_order_relaxed) >= 0.5f; };
        auto d = [](const std::atomic<float>* p) noexcept { return static_cast<double>(p->load(std::memory_order_relaxed)); };

        dsp::RotaryDrumConfig cfg;
        cfg.chorus  = b(rotaryChorus);
        cfg.tremolo = b(rotaryTremolo);
        cfg.slowHz  = d(rotarySlowHz);
        cfg.fastHz  = d(rotaryFastHz);
        return cfg;
    }

    bool readRhythmBreak() const noexcept
    {
        return (rbBreak != nullptr) && (rbBreak->load(std::memory_order_relaxed) >= 0.5f);
    }

    int readRhythmOutputBus() const noexcept
    {
        if (rbOutputBus == nullptr)
        {
            return 0;
        }
        return std::clamp(static_cast<int>(std::lround(rbOutputBus->load(std::memory_order_relaxed))), 0, 2);
    }
};

} // namespace ke310::plugin
