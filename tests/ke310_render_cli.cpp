// Offline C++20 KE310Engine renderer CLI for exact Python <-> C++20 parity verification.
#include "dsp/KE310Engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

using namespace ke310::dsp;

struct TimedEvent
{
    double timeS;
    bool isOn;
    Manual manual;
    int midi;
};

bool parseBool(const std::string& val)
{
    return (val == "1" || val == "true" || val == "True");
}

bool applyKeyValue(KE310State& st, const std::string& key, const std::string& val)
{
    if (key == "upper_tibia_16") { st.upperTibia16 = parseBool(val); return true; }
    if (key == "upper_tibia_8") { st.upperTibia8 = parseBool(val); return true; }
    if (key == "upper_tibia_4") { st.upperTibia4 = parseBool(val); return true; }
    if (key == "upper_tibia_2_23") { st.upperTibia2_23 = parseBool(val); return true; }
    if (key == "upper_diapason_8") { st.upperDiapason8 = parseBool(val); return true; }
    if (key == "upper_string_16") { st.upperString16 = parseBool(val); return true; }
    if (key == "upper_string_8") { st.upperString8 = parseBool(val); return true; }
    if (key == "upper_string_4") { st.upperString4 = parseBool(val); return true; }
    if (key == "upper_oboe_8") { st.upperOboe8 = parseBool(val); return true; }
    if (key == "upper_clarinet_8") { st.upperClarinet8 = parseBool(val); return true; }

    if (key == "lower_tibia_8") { st.lowerTibia8 = parseBool(val); return true; }
    if (key == "lower_tibia_4") { st.lowerTibia4 = parseBool(val); return true; }
    if (key == "lower_diapason_8") { st.lowerDiapason8 = parseBool(val); return true; }
    if (key == "lower_string_8") { st.lowerString8 = parseBool(val); return true; }
    if (key == "lower_horn_8") { st.lowerHorn8 = parseBool(val); return true; }
    if (key == "lower_string_4") { st.lowerString4 = parseBool(val); return true; }

    if (key == "pedal_bourdon_16") { st.pedalBourdon16 = parseBool(val); return true; }
    if (key == "pedal_major_flute_8") { st.pedalMajorFlute8 = parseBool(val); return true; }
    if (key == "pedal_string_bass") { st.pedalStringBass = parseBool(val); return true; }

    if (key == "organ_silent") { st.organSilent = parseBool(val); return true; }
    if (key == "preset_perc_2_23") { st.presetPerc2_23 = parseBool(val); return true; }
    if (key == "preset_wow_trumpet") { st.presetWowTrumpet = parseBool(val); return true; }
    if (key == "preset_banjo") { st.presetBanjo = parseBool(val); return true; }
    if (key == "preset_mandolin") { st.presetMandolin = parseBool(val); return true; }
    if (key == "preset_marimba") { st.presetMarimba = parseBool(val); return true; }
    if (key == "preset_piano") { st.presetPiano = parseBool(val); return true; }
    if (key == "preset_harpsichord") { st.presetHarpsichord = parseBool(val); return true; }

    if (key == "upper_sustain") { st.upperSustain = parseBool(val); return true; }
    if (key == "pedal_sustain") { st.pedalSustain = parseBool(val); return true; }
    if (key == "knee_sustain") { st.kneeSustain = parseBool(val); return true; }
    if (key == "vibrato") { st.vibrato = parseBool(val); return true; }
    if (key == "vibrato_fast") { st.vibratoFast = parseBool(val); return true; }
    if (key == "delay_vibrato") { st.delayVibrato = parseBool(val); return true; }
    if (key == "ensemble") { st.ensemble = parseBool(val); return true; }

    if (key == "total_volume") { st.totalVolume = std::stod(val); return true; }
    if (key == "lower_volume") { st.lowerVolume = std::stod(val); return true; }
    if (key == "pedal_volume") { st.pedalVolume = std::stod(val); return true; }
    if (key == "preset_volume") { st.presetVolume = std::stod(val); return true; }
    if (key == "reverb") { st.reverb = std::stod(val); return true; }
    if (key == "upper_sustain_time") { st.upperSustainTime = std::stod(val); return true; }
    if (key == "pedal_sustain_time") { st.pedalSustainTime = std::stod(val); return true; }
    if (key == "percus_decay") { st.percusDecay = std::stod(val); return true; }
    if (key == "repeat_rate") { st.repeatRate = std::stod(val); return true; }
    if (key == "expression") { st.expression = std::stod(val); return true; }

    if (key == "vintage_tos") { st.vintageTos = parseBool(val); return true; }
    if (key == "vintage_pedal_vco") { st.vintagePedalVco = parseBool(val); return true; }
    if (key == "a4_hz") { st.a4Hz = std::stod(val); return true; }
    if (key == "enable_sub_oct_bleed") { st.enableSubOctBleed = parseBool(val); return true; }
    if (key == "tibia_bus_load_comp") { st.tibiaBusLoadComp = parseBool(val); return true; }
    if (key == "expression_loudness_comp") { st.expressionLoudnessComp = parseBool(val); return true; }
    if (key == "analog_noise_level") { st.analogNoiseLevel = std::stod(val); return true; }
    if (key == "repeat_rate_hz_override")
    {
        if (val == "none" || val == "None")
        {
            st.hasRepeatRateHzOverride = false;
            st.repeatRateHzOverride = 0.0;
        }
        else
        {
            st.hasRepeatRateHzOverride = true;
            st.repeatRateHzOverride = std::stod(val);
        }
        return true;
    }
    return false;
}

Manual parseManual(const std::string& m)
{
    if (m == "lower") return Manual::Lower;
    if (m == "pedal") return Manual::Pedal;
    return Manual::Upper;
}

std::vector<TimedEvent> parseEvents(const std::string& spec)
{
    std::vector<TimedEvent> events;
    std::stringstream ss(spec);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        if (item.empty())
        {
            continue;
        }
        std::stringstream is(item);
        std::string tStr, kindStr, manStr, midiStr;
        if (!std::getline(is, tStr, ':') || !std::getline(is, kindStr, ':')
            || !std::getline(is, manStr, ':') || !std::getline(is, midiStr, ':'))
        {
            std::cerr << "Invalid event item: " << item << "\n";
            std::exit(2);
        }
        events.push_back(TimedEvent{std::stod(tStr), kindStr == "on", parseManual(manStr), std::stoi(midiStr)});
    }
    std::stable_sort(events.begin(), events.end(),
                     [](const TimedEvent& a, const TimedEvent& b) { return a.timeS < b.timeS; });
    return events;
}

} // namespace

int main(int argc, char* argv[])
{
    double sr = 48000.0;
    double dur = 2.0;
    int blockSize = 128;
    std::string outPath;
    std::string eventsStr;
    KE310State st;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--sr" && i + 1 < argc) { sr = std::stod(argv[++i]); }
        else if (arg == "--dur" && i + 1 < argc) { dur = std::stod(argv[++i]); }
        else if (arg == "--block" && i + 1 < argc) { blockSize = std::stoi(argv[++i]); }
        else if (arg == "--out" && i + 1 < argc) { outPath = argv[++i]; }
        else if (arg == "--events" && i + 1 < argc) { eventsStr = argv[++i]; }
        else if (arg == "--set" && i + 1 < argc)
        {
            const std::string kv = argv[++i];
            const auto eq = kv.find('=');
            if (eq == std::string::npos)
            {
                std::cerr << "Expected key=value for --set: " << kv << "\n";
                return 2;
            }
            const std::string k = kv.substr(0, eq);
            const std::string v = kv.substr(eq + 1);
            if (!applyKeyValue(st, k, v))
            {
                std::cerr << "Unknown KE310State field: " << k << "\n";
                return 2;
            }
        }
        else if (arg == "--voice" && i + 2 < argc)
        {
            // Convenience alias: `--voice UPPER_TIBIA_8 --midi 60` etc. -> mapped via --set in scripts
            std::cerr << "Use --set <tab_field>=true instead of --voice\n";
            return 2;
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << "\n";
            return 2;
        }
    }

    if (outPath.empty())
    {
        std::cerr << "--out <path.f64> is required\n";
        return 2;
    }

    const auto events = parseEvents(eventsStr);
    const int totalSamples = static_cast<int>(std::llround(dur * sr));

    KE310Engine eng;
    eng.prepare(sr);
    eng.applyState(st);

    std::vector<double> mixOut(static_cast<std::size_t>(totalSamples));
    std::vector<double> rotorOut(static_cast<std::size_t>(totalSamples));
    std::vector<double> statOut(static_cast<std::size_t>(totalSamples));

    std::size_t evIdx = 0;
    int pos = 0;
    while (pos < totalSamples)
    {
        const double tCur = static_cast<double>(pos) / sr;
        while (evIdx < events.size() && events[evIdx].timeS <= tCur + 1e-9)
        {
            const auto& e = events[evIdx++];
            if (e.isOn)
            {
                eng.noteOn(e.manual, e.midi);
            }
            else
            {
                eng.noteOff(e.manual, e.midi);
            }
        }
        int nextEvSample = totalSamples;
        if (evIdx < events.size())
        {
            nextEvSample = std::min(totalSamples,
                                    std::max(pos + 1, static_cast<int>(std::llround(events[evIdx].timeS * sr))));
        }
        const int nBlk = std::min({blockSize, nextEvSample - pos, totalSamples - pos});
        eng.processBlock(nBlk, mixOut.data() + pos, rotorOut.data() + pos, statOut.data() + pos);
        pos += nBlk;
    }

    std::ofstream ofs(outPath, std::ios::binary);
    if (!ofs)
    {
        std::cerr << "Failed to open output file: " << outPath << "\n";
        return 1;
    }
    ofs.write(reinterpret_cast<const char*>(mixOut.data()), static_cast<std::streamsize>(mixOut.size() * sizeof(double)));
    ofs.write(reinterpret_cast<const char*>(rotorOut.data()), static_cast<std::streamsize>(rotorOut.size() * sizeof(double)));
    ofs.write(reinterpret_cast<const char*>(statOut.data()), static_cast<std::streamsize>(statOut.size() * sizeof(double)));
    ofs.close();
    return 0;
}
