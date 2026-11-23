// Offline C++20 KE Rhythmbox DSP renderer CLI for exact Python <-> C++20 parity verification.
#include "dsp/rhythmbox/RhythmboxEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace
{

using namespace ke310::rhythmbox;

std::optional<PatternId> parsePatternName(const std::string& name)
{
    for (std::size_t i = 0; i < kNumPatterns; ++i)
    {
        if (name == kPatternNames[i])
        {
            return static_cast<PatternId>(i);
        }
    }
    return std::nullopt;
}

std::optional<VoiceId> parseVoiceName(const std::string& name)
{
    for (std::size_t i = 0; i < kNumVoices; ++i)
    {
        if (name == kVoiceNames[i])
        {
            return static_cast<VoiceId>(i);
        }
    }
    return std::nullopt;
}

bool parseBool(const std::string& v)
{
    return (v == "1" || v == "true" || v == "True");
}

} // namespace

int main(int argc, char* argv[])
{
    std::string mode = "pattern";
    std::string patternName = "ROCK_1";
    std::optional<std::uint16_t> patternMaskOverride;
    std::string voiceName = "BASS_DRUM";
    int midiNote = 36;
    double velocity = 1.0;
    bool velocityExplicit = false;

    bool multiPattern = false;
    double bpm = 106.8;
    bool bpmExplicit = false;
    double tone = 0.5;
    double volume = 1.0;
    double analogVariation = 1.0;
    double fs = 48000.0;
    double durSec = 2.0;
    int explicitSamples = -1;
    int blockSize = 128;
    std::string outRawPath;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) { mode = argv[++i]; }
        else if (arg == "--pattern" && i + 1 < argc) { patternName = argv[++i]; }
        else if (arg == "--pattern-mask" && i + 1 < argc)
        {
            patternMaskOverride = static_cast<std::uint16_t>(std::stoul(argv[++i], nullptr, 0) & 0xFFFFu);
        }
        else if (arg == "--multi-pattern" && i + 1 < argc) { multiPattern = parseBool(argv[++i]); }
        else if (arg == "--voice" && i + 1 < argc) { voiceName = argv[++i]; }
        else if (arg == "--midi-note" && i + 1 < argc) { midiNote = std::stoi(argv[++i]); }
        else if (arg == "--velocity" && i + 1 < argc)
        {
            velocity = std::stod(argv[++i]);
            velocityExplicit = true;
        }
        else if (arg == "--bpm" && i + 1 < argc)
        {
            bpm = std::stod(argv[++i]);
            bpmExplicit = true;
        }
        else if (arg == "--tone" && i + 1 < argc) { tone = std::stod(argv[++i]); }
        else if (arg == "--volume" && i + 1 < argc) { volume = std::stod(argv[++i]); }
        else if (arg == "--analog-variation" && i + 1 < argc) { analogVariation = std::stod(argv[++i]); }
        else if (arg == "--fs" && i + 1 < argc) { fs = std::stod(argv[++i]); }
        else if (arg == "--dur" && i + 1 < argc) { durSec = std::stod(argv[++i]); }
        else if (arg == "--samples" && i + 1 < argc) { explicitSamples = std::stoi(argv[++i]); }
        else if (arg == "--block-size" && i + 1 < argc) { blockSize = std::max(1, std::stoi(argv[++i])); }
        else if (arg == "--out-raw" && i + 1 < argc) { outRawPath = argv[++i]; }
        else
        {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            return 2;
        }
    }

    if (outRawPath.empty())
    {
        std::cerr << "--out-raw <path.f64> is required\n";
        return 2;
    }

    const int totalSamples = (explicitSamples > 0)
                                 ? explicitSamples
                                 : static_cast<int>(std::llround(durSec * fs));

    RhythmboxEngine engine;
    engine.prepare(fs);

    RhythmboxState st;
    st.enabled = (mode == "pattern");
    st.multiPatternEnabled = multiPattern;
    st.clockMode = ClockMode::Internal;
    st.tone = tone;
    st.volume = volume;
    st.analogVariation = analogVariation;

    if (patternMaskOverride.has_value())
    {
        st.patternMask = *patternMaskOverride;
        st.bpm = bpm;
    }
    else
    {
        const auto pid = parsePatternName(patternName);
        if (!pid.has_value())
        {
            std::cerr << "Unknown pattern: " << patternName << "\n";
            return 2;
        }
        const std::size_t idx = static_cast<std::size_t>(*pid);
        st.patternMask = static_cast<std::uint16_t>(1u << idx);
        st.bpm = bpmExplicit ? bpm : kPatterns[idx].measuredBpm;
    }

    engine.applyState(st);

    if (mode == "oneshot")
    {
        const auto vid = parseVoiceName(voiceName);
        if (!vid.has_value())
        {
            std::cerr << "Unknown voice: " << voiceName << "\n";
            return 2;
        }
        engine.triggerVoice(*vid, velocityExplicit ? velocity : 1.0);
    }
    else if (mode == "midi")
    {
        const int velInt = velocityExplicit ? static_cast<int>(std::llround(velocity)) : 100;
        engine.triggerMidiNote(midiNote, velInt);
    }
    else if (mode != "pattern")
    {
        std::cerr << "Unknown --mode: " << mode << "\n";
        return 2;
    }

    std::vector<double> out(static_cast<std::size_t>(totalSamples), 0.0);
    int pos = 0;
    while (pos < totalSamples)
    {
        const int blk = std::min(blockSize, totalSamples - pos);
        engine.processBlock(out.data() + pos, blk);
        pos += blk;
    }

    std::ofstream ofs(outRawPath, std::ios::binary);
    if (!ofs)
    {
        std::cerr << "Failed to open output file: " << outRawPath << "\n";
        return 1;
    }
    ofs.write(reinterpret_cast<const char*>(out.data()),
              static_cast<std::streamsize>(out.size() * sizeof(double)));
    ofs.close();
    return 0;
}
