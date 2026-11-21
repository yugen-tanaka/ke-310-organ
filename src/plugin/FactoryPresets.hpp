// 28 Official KE-310 Owner's Manual Registration Guide Presets (Manual P.22).
// Selecting a factory preset resets registration tabs, presets, and panel effects
// while preserving the user's MIDI channel assignments, expression, and circuit calibration.
// Parenthesized stops in Manual P.22 ((2'), (BASSOON 8')) are KE-510/KE-510DX only and excluded here.
#pragma once

#include "PluginParams.hpp"

#include <array>
#include <cstddef>

namespace ke310::plugin
{

struct FactoryPreset
{
    const char* name;

    // Upper Levers
    bool upperTibia16 = false, upperTibia8 = false, upperTibia4 = false, upperTibia2_23 = false;
    bool upperDiapason8 = false, upperString16 = false, upperString8 = false, upperString4 = false;
    bool upperOboe8 = false, upperClarinet8 = false;

    // Lower Levers
    bool lowerTibia8 = false, lowerTibia4 = false, lowerDiapason8 = false;
    bool lowerString8 = false, lowerHorn8 = false, lowerString4 = false;

    // Pedal Levers
    bool pedalBourdon16 = false, pedalMajorFlute8 = false, pedalStringBass = false;

    // Upper Presets
    bool organSilent = false, presetPerc2_23 = false, presetWowTrumpet = false;
    bool presetBanjo = false, presetMandolin = false, presetMarimba = false;
    bool presetPiano = false, presetHarpsichord = false;

    // Effect Levers
    bool upperSustain = false, pedalSustain = false;
    bool vibrato = false, vibratoFast = false, delayVibrato = false, ensemble = false;
    bool rotaryChorus = false, rotaryTremolo = false;

    // Continuous Knobs
    float totalVolume = 0.85f;
    float lowerVolume = 0.80f;
    float pedalVolume = 0.85f;
    float presetVolume = 0.85f;
    float reverb = 0.25f;
    float upperSustainTime = 0.45f;
    float pedalSustainTime = 0.45f;
    float percusDecay = 0.25f;
    float repeatRate = 0.40f;
};

inline constexpr std::size_t kNumFactoryPresets = 28;

inline constexpr std::array<FactoryPreset, kNumFactoryPresets> kFactoryPresets = {{
    // No. 1 Jazz No.1 (Slow Tempo)
    [] {
        FactoryPreset p{"01. Jazz No.1 (Slow Tempo)"};
        p.upperTibia8 = p.upperTibia4 = true;
        p.lowerTibia8 = true;
        p.pedalMajorFlute8 = true;
        p.pedalSustain = true;
        p.rotaryTremolo = true;
        return p;
    }(),
    // No. 2 Jazz No.2 (Medium Tempo)
    [] {
        FactoryPreset p{"02. Jazz No.2 (Medium Tempo)"};
        p.upperTibia8 = p.upperTibia4 = true;
        p.lowerTibia8 = p.lowerTibia4 = true;
        p.pedalMajorFlute8 = true;
        p.pedalSustain = true;
        p.rotaryChorus = true;
        return p;
    }(),
    // No. 3 Jazz No.3 (Fast Tempo)
    [] {
        FactoryPreset p{"03. Jazz No.3 (Fast Tempo)"};
        p.upperTibia16 = p.upperTibia4 = true;
        p.lowerTibia8 = p.lowerTibia4 = true;
        p.pedalMajorFlute8 = true;
        p.pedalSustain = true;
        p.rotaryTremolo = true;
        return p;
    }(),
    // No. 4 Jazz No.4 (Ad-lib)
    [] {
        FactoryPreset p{"04. Jazz No.4 (Ad-lib)"};
        p.upperTibia16 = p.upperTibia2_23 = true;
        p.lowerTibia8 = p.lowerTibia4 = true;
        p.pedalMajorFlute8 = true;
        p.pedalSustain = true;
        p.rotaryTremolo = true;
        return p;
    }(),
    // No. 5 Latin No.1 (Slow Tempo)
    [] {
        FactoryPreset p{"05. Latin No.1 (Slow Tempo)"};
        p.upperTibia8 = p.upperClarinet8 = true;
        p.lowerTibia8 = p.lowerDiapason8 = true;
        p.pedalMajorFlute8 = p.pedalStringBass = true;
        p.pedalSustain = true;
        p.rotaryTremolo = true;
        return p;
    }(),
    // No. 6 Latin No.2 (Medium Tempo)
    [] {
        FactoryPreset p{"06. Latin No.2 (Medium Tempo)"};
        p.upperTibia16 = p.upperTibia8 = p.upperOboe8 = true;
        p.lowerTibia8 = p.lowerTibia4 = p.lowerDiapason8 = true;
        p.pedalMajorFlute8 = p.pedalStringBass = true;
        p.pedalSustain = true;
        p.rotaryTremolo = true;
        return p;
    }(),
    // No. 7 Latin No.3 (Fast Tempo)
    [] {
        FactoryPreset p{"07. Latin No.3 (Fast Tempo)"};
        p.upperTibia16 = p.upperTibia8 = p.upperTibia4 = p.upperOboe8 = true;
        p.lowerTibia8 = p.lowerTibia4 = p.lowerDiapason8 = true;
        p.pedalMajorFlute8 = p.pedalStringBass = true;
        p.pedalSustain = true;
        p.rotaryTremolo = true;
        return p;
    }(),
    // No. 8 Mambo
    [] {
        FactoryPreset p{"08. Mambo"};
        p.upperTibia16 = p.upperTibia4 = p.upperClarinet8 = true;
        p.lowerTibia8 = p.lowerDiapason8 = true;
        p.pedalMajorFlute8 = p.pedalStringBass = true;
        p.pedalSustain = true;
        return p;
    }(),
    // No. 9 Samba
    [] {
        FactoryPreset p{"09. Samba"};
        p.upperTibia16 = p.upperTibia4 = p.upperTibia2_23 = p.upperOboe8 = true;
        p.lowerTibia8 = p.lowerHorn8 = true;
        p.pedalMajorFlute8 = p.pedalStringBass = true;
        p.pedalSustain = true;
        return p;
    }(),
    // No. 10 Bossa Nova
    [] {
        FactoryPreset p{"10. Bossa Nova"};
        p.upperTibia16 = p.upperTibia4 = true;
        p.lowerTibia8 = p.lowerTibia4 = p.lowerDiapason8 = true;
        p.pedalMajorFlute8 = p.pedalStringBass = true;
        p.pedalSustain = true;
        return p;
    }(),
    // No. 11 Rock No.1 (Slow Tempo)
    [] {
        FactoryPreset p{"11. Rock No.1 (Slow Tempo)"};
        p.upperTibia8 = p.upperString8 = true;
        p.lowerTibia8 = p.lowerString8 = true;
        p.pedalMajorFlute8 = p.pedalStringBass = true;
        p.pedalSustain = true;
        p.rotaryTremolo = true;
        return p;
    }(),
    // No. 12 Rock No.2 (Medium Tempo)
    [] {
        FactoryPreset p{"12. Rock No.2 (Medium Tempo)"};
        p.upperTibia8 = p.upperTibia4 = p.upperString8 = true;
        p.lowerTibia8 = p.lowerTibia4 = p.lowerString8 = true;
        p.pedalMajorFlute8 = p.pedalStringBass = true;
        p.pedalSustain = true;
        p.rotaryChorus = true;
        return p;
    }(),
    // No. 13 Rock No.3 (Fast Tempo)
    [] {
        FactoryPreset p{"13. Rock No.3 (Fast Tempo)"};
        p.upperTibia16 = p.upperTibia8 = p.upperString16 = p.upperString8 = p.upperString4 = true;
        p.lowerTibia8 = p.lowerTibia4 = p.lowerString8 = true;
        p.pedalMajorFlute8 = p.pedalStringBass = true;
        p.pedalSustain = true;
        p.rotaryTremolo = true;
        return p;
    }(),
    // No. 14 Soul
    [] {
        FactoryPreset p{"14. Soul"};
        p.upperTibia16 = p.upperTibia4 = p.upperString4 = true;
        p.lowerTibia8 = p.lowerTibia4 = p.lowerString8 = true;
        p.pedalMajorFlute8 = true;
        p.pedalSustain = true;
        p.rotaryTremolo = true;
        return p;
    }(),
    // No. 15 Tango
    [] {
        FactoryPreset p{"15. Tango"};
        p.upperDiapason8 = p.upperString8 = p.upperString4 = true;
        p.lowerTibia8 = p.lowerDiapason8 = p.lowerString8 = true;
        p.pedalMajorFlute8 = true;
        p.vibrato = true;
        p.pedalSustain = true;
        return p;
    }(),
    // No. 16 Chanson
    [] {
        FactoryPreset p{"16. Chanson"};
        p.upperTibia2_23 = p.upperString8 = p.upperString4 = p.upperOboe8 = true;
        p.lowerDiapason8 = p.lowerHorn8 = p.lowerString8 = true;
        p.pedalMajorFlute8 = true;
        p.ensemble = true;
        p.pedalSustain = true;
        return p;
    }(),
    // No. 17 Canzonet
    [] {
        FactoryPreset p{"17. Canzonet"};
        p.upperTibia8 = p.upperTibia4 = p.upperString8 = p.upperClarinet8 = true;
        p.lowerTibia8 = p.lowerHorn8 = p.lowerString8 = true;
        p.pedalMajorFlute8 = true;
        p.pedalSustain = true;
        return p;
    }(),
    // No. 18 String Ensemble
    [] {
        FactoryPreset p{"18. String Ensemble"};
        p.upperString16 = p.upperString8 = p.upperString4 = true;
        p.lowerTibia8 = p.lowerDiapason8 = p.lowerString8 = p.lowerString4 = true;
        p.pedalMajorFlute8 = true;
        p.delayVibrato = true;
        p.pedalSustain = true;
        return p;
    }(),
    // No. 19 Full Organ
    [] {
        FactoryPreset p{"19. Full Organ"};
        p.upperTibia16 = p.upperTibia8 = p.upperTibia4 = p.upperDiapason8 = true;
        p.lowerTibia8 = p.lowerTibia4 = p.lowerDiapason8 = true;
        p.pedalBourdon16 = p.pedalMajorFlute8 = true;
        p.pedalSustain = true;
        p.rotaryChorus = true;
        return p;
    }(),
    // No. 20 Theatre Organ
    [] {
        FactoryPreset p{"20. Theatre Organ"};
        p.upperTibia16 = p.upperTibia8 = p.upperTibia4 = p.upperString8 = p.upperString4 = true;
        p.lowerTibia8 = p.lowerDiapason8 = p.lowerHorn8 = true;
        p.pedalBourdon16 = p.pedalMajorFlute8 = true;
        p.pedalSustain = true;
        p.rotaryTremolo = true;
        return p;
    }(),
    // No. 21 Guitar
    [] {
        FactoryPreset p{"21. Guitar"};
        p.upperString8 = p.upperString4 = true;
        p.upperSustain = true;
        return p;
    }(),
    // No. 22 Harp
    [] {
        FactoryPreset p{"22. Harp"};
        p.upperTibia8 = true;
        p.upperSustain = true;
        return p;
    }(),
    // No. 23 Music Box (Orgel)
    [] {
        FactoryPreset p{"23. Music Box (Orgel)"};
        p.upperTibia4 = true;
        p.upperSustain = true;
        return p;
    }(),
    // No. 24 Koto
    [] {
        FactoryPreset p{"24. Koto"};
        p.upperString8 = p.upperOboe8 = true;
        p.upperSustain = true;
        return p;
    }(),
    // No. 25 Accordion
    [] {
        FactoryPreset p{"25. Accordion"};
        p.upperDiapason8 = p.upperString8 = p.upperString4 = p.upperClarinet8 = p.upperOboe8 = true;
        p.ensemble = true;
        return p;
    }(),
    // No. 26 Jingle Bell
    [] {
        FactoryPreset p{"26. Jingle Bell"};
        p.upperTibia2_23 = p.upperString4 = true;
        p.upperSustain = true;
        return p;
    }(),
    // No. 27 Chime
    [] {
        FactoryPreset p{"27. Chime"};
        p.upperTibia2_23 = p.upperDiapason8 = p.upperString4 = true;
        p.upperSustain = true;
        return p;
    }(),
    // No. 28 Surf
    [] {
        FactoryPreset p{"28. Surf"};
        p.upperTibia8 = true;
        p.presetMarimba = true;
        p.upperSustain = true;
        return p;
    }(),
}};

inline void applyFactoryPreset(juce::AudioProcessorValueTreeState& apvts, int presetIndex)
{
    if (presetIndex < 0 || static_cast<std::size_t>(presetIndex) >= kNumFactoryPresets)
    {
        return;
    }

    const auto& fp = kFactoryPresets[static_cast<std::size_t>(presetIndex)];
    auto setBool = [&](const char* id, bool val)
    {
        if (auto* p = apvts.getParameter(id))
        {
            p->setValueNotifyingHost(val ? 1.0f : 0.0f);
        }
    };
    auto setFloat = [&](const char* id, float val)
    {
        if (auto* p = apvts.getParameter(id))
        {
            p->setValueNotifyingHost(p->convertTo0to1(val));
        }
    };

    setBool(param_ids::upperTibia16,   fp.upperTibia16);
    setBool(param_ids::upperTibia8,    fp.upperTibia8);
    setBool(param_ids::upperTibia4,    fp.upperTibia4);
    setBool(param_ids::upperTibia2_23, fp.upperTibia2_23);
    setBool(param_ids::upperDiapason8, fp.upperDiapason8);
    setBool(param_ids::upperString16,  fp.upperString16);
    setBool(param_ids::upperString8,   fp.upperString8);
    setBool(param_ids::upperString4,   fp.upperString4);
    setBool(param_ids::upperOboe8,     fp.upperOboe8);
    setBool(param_ids::upperClarinet8, fp.upperClarinet8);

    setBool(param_ids::lowerTibia8,    fp.lowerTibia8);
    setBool(param_ids::lowerTibia4,    fp.lowerTibia4);
    setBool(param_ids::lowerDiapason8, fp.lowerDiapason8);
    setBool(param_ids::lowerString8,   fp.lowerString8);
    setBool(param_ids::lowerHorn8,     fp.lowerHorn8);
    setBool(param_ids::lowerString4,   fp.lowerString4);

    setBool(param_ids::pedalBourdon16,   fp.pedalBourdon16);
    setBool(param_ids::pedalMajorFlute8, fp.pedalMajorFlute8);
    setBool(param_ids::pedalStringBass,  fp.pedalStringBass);

    setBool(param_ids::organSilent,       fp.organSilent);
    setBool(param_ids::presetPerc2_23,    fp.presetPerc2_23);
    setBool(param_ids::presetWowTrumpet,  fp.presetWowTrumpet);
    setBool(param_ids::presetBanjo,       fp.presetBanjo);
    setBool(param_ids::presetMandolin,    fp.presetMandolin);
    setBool(param_ids::presetMarimba,     fp.presetMarimba);
    setBool(param_ids::presetPiano,       fp.presetPiano);
    setBool(param_ids::presetHarpsichord, fp.presetHarpsichord);

    setBool(param_ids::upperSustain, fp.upperSustain);
    setBool(param_ids::pedalSustain, fp.pedalSustain);
    setBool(param_ids::kneeSustain,  false);
    setBool(param_ids::vibrato,      fp.vibrato);
    setBool(param_ids::vibratoFast,  fp.vibratoFast);
    setBool(param_ids::delayVibrato,  fp.delayVibrato);
    setBool(param_ids::ensemble,      fp.ensemble);
    setBool(param_ids::rotaryChorus,  fp.rotaryChorus);
    setBool(param_ids::rotaryTremolo, fp.rotaryTremolo);

    setFloat(param_ids::totalVolume,      fp.totalVolume);
    setFloat(param_ids::lowerVolume,      fp.lowerVolume);
    setFloat(param_ids::pedalVolume,      fp.pedalVolume);
    setFloat(param_ids::presetVolume,     fp.presetVolume);
    setFloat(param_ids::reverb,           fp.reverb);
    setFloat(param_ids::upperSustainTime, fp.upperSustainTime);
    setFloat(param_ids::pedalSustainTime, fp.pedalSustainTime);
    setFloat(param_ids::percusDecay,      fp.percusDecay);
    setFloat(param_ids::repeatRate,       fp.repeatRate);
}

} // namespace ke310::plugin
