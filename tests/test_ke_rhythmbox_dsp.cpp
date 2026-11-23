// Catch2 unit tests for pure C++20 ke_rhythmbox_dsp.
#include <catch2/catch_test_macros.hpp>

#include "dsp/rhythmbox/RhythmboxEngine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

namespace
{

using namespace ke310::rhythmbox;

double computeRms(const double* data, std::size_t n) noexcept
{
    double sumSq = 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        sumSq += data[i] * data[i];
    }
    return std::sqrt(sumSq / static_cast<double>(std::max<std::size_t>(1, n)) + 1e-24);
}

double computeMaxAbs(const double* data, std::size_t n) noexcept
{
    double m = 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        m = std::max(m, std::abs(data[i]));
    }
    return m;
}

bool allFinite(const double* data, std::size_t n) noexcept
{
    for (std::size_t i = 0; i < n; ++i)
    {
        if (!std::isfinite(data[i]))
        {
            return false;
        }
    }
    return true;
}

std::vector<double> renderPatternMono(double fs,
                                      std::uint16_t patternMask,
                                      bool multiPattern,
                                      double durSec,
                                      double bpm,
                                      double tone = 0.5,
                                      double analogVariation = 1.0,
                                      int blockSize = 128)
{
    RhythmboxEngine eng;
    eng.prepare(fs);
    RhythmboxState st;
    st.enabled = true;
    st.patternMask = patternMask;
    st.multiPatternEnabled = multiPattern;
    st.clockMode = ClockMode::Internal;
    st.bpm = bpm;
    st.volume = 1.0;
    st.tone = tone;
    st.analogVariation = analogVariation;
    eng.applyState(st);

    const std::size_t total = static_cast<std::size_t>(std::llround(durSec * fs));
    std::vector<double> out(total, 0.0);
    std::size_t pos = 0;
    while (pos < total)
    {
        const int blk = static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(blockSize), total - pos));
        eng.processBlock(out.data() + pos, blk);
        pos += static_cast<std::size_t>(blk);
    }
    return out;
}

std::vector<double> renderOneShotMono(double fs,
                                      VoiceId voice,
                                      double durSec,
                                      double analogVariation = 0.0,
                                      double velocity = 1.0)
{
    RhythmboxEngine eng;
    eng.prepare(fs);
    RhythmboxState st;
    st.enabled = false;
    st.clockMode = ClockMode::Internal;
    st.volume = 1.0;
    st.tone = 0.5;
    st.analogVariation = analogVariation;
    eng.applyState(st);
    eng.triggerVoice(voice, velocity);

    const std::size_t total = static_cast<std::size_t>(std::llround(durSec * fs));
    std::vector<double> out(total, 0.0);
    std::size_t pos = 0;
    while (pos < total)
    {
        const int blk = static_cast<int>(std::min<std::size_t>(128u, total - pos));
        eng.processBlock(out.data() + pos, blk);
        pos += static_cast<std::size_t>(blk);
    }
    return out;
}

double dftBinPower(const double* x, std::size_t n, double freqHz, double fs, double tukeyAlpha = 1.0) noexcept
{
    const double w = 2.0 * std::numbers::pi * freqHz / fs;
    double re = 0.0;
    double im = 0.0;
    const double halfNMinus1 = 0.5 * static_cast<double>(n - 1);
    const double edge = 0.5 * tukeyAlpha * static_cast<double>(n - 1);
    for (std::size_t i = 0; i < n; ++i)
    {
        double win = 1.0;
        if (tukeyAlpha > 0.0 && n > 1)
        {
            const double di = static_cast<double>(i);
            if (tukeyAlpha >= 1.0)
            {
                win = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * di / static_cast<double>(n - 1)));
            }
            else if (di < edge)
            {
                win = 0.5 * (1.0 + std::cos(std::numbers::pi * (di / edge - 1.0)));
            }
            else if (di > static_cast<double>(n - 1) - edge)
            {
                const double r = (di - (static_cast<double>(n - 1) - edge)) / edge;
                win = 0.5 * (1.0 + std::cos(std::numbers::pi * r));
            }
            (void)halfNMinus1;
        }
        const double s = x[i] * win;
        const double ph = w * static_cast<double>(i);
        re += s * std::cos(ph);
        im -= s * std::sin(ph);
    }
    return re * re + im * im;
}

double scanPeakFrequency(const double* x, std::size_t n, double fLo, double fHi, double fs,
                         double stepHz = 0.5, double tukeyAlpha = 1.0) noexcept
{
    double bestF = fLo;
    double bestP = -1.0;
    for (double f = fLo; f <= fHi + 1e-6; f += stepHz)
    {
        const double p = dftBinPower(x, n, f, fs, tukeyAlpha);
        if (p > bestP)
        {
            bestP = p;
            bestF = f;
        }
    }
    return bestF;
}

double lowHighRatioDb(const std::vector<double>& y, double fs) noexcept
{
    ke310::dsp::BiquadCascade<1> lp500;
    ke310::dsp::BiquadCascade<1> hp3000;
    const ke310::dsp::FilterSectionSpec lpSpec{ke310::dsp::FilterType::LP2, 500.0, 0.707, 0.0};
    const ke310::dsp::FilterSectionSpec hpSpec{ke310::dsp::FilterType::HP2, 3000.0, 0.707, 0.0};
    lp500.design(&lpSpec, 1u, 0.0, fs);
    hp3000.design(&hpSpec, 1u, 0.0, fs);

    double eLow = 0.0;
    double eHigh = 0.0;
    for (double s : y)
    {
        const double lo = lp500.processSample(s);
        const double hi = hp3000.processSample(s);
        eLow += lo * lo;
        eHigh += hi * hi;
    }
    return 10.0 * std::log10((eLow + 1e-24) / (eHigh + 1e-24));
}

} // namespace

TEST_CASE("Rhythmbox DSP 1: Multirate stability across 44.1 / 48 / 88.2 / 96 kHz", "[rhythmbox_dsp]")
{
    constexpr std::array<double, 4> kRates = {44100.0, 48000.0, 88200.0, 96000.0};
    double worstR = 0.0;

    for (double sr : kRates)
    {
        RhythmboxEngine eng;
        eng.prepare(sr);
        RhythmboxState st;
        st.tone = 0.0;
        eng.applyState(st);
        worstR = std::max(worstR, eng.maxPoleRadius());
        REQUIRE(eng.maxPoleRadius() < 0.9999);

        st.tone = 1.0;
        eng.applyState(st);
        worstR = std::max(worstR, eng.maxPoleRadius());
        REQUIRE(eng.maxPoleRadius() < 0.9999);

        for (std::size_t v = 0; v < kNumVoices; ++v)
        {
            const auto y = renderOneShotMono(sr, static_cast<VoiceId>(v), 0.25, 1.0);
            REQUIRE(allFinite(y.data(), y.size()));
            const double pk = computeMaxAbs(y.data(), y.size());
            REQUIRE(pk > 1e-4);
            REQUIRE(pk < 2.0);
        }

        for (PatternId pid : {PatternId::Waltz, PatternId::Rock1, PatternId::Bossanova, PatternId::Samba})
        {
            const auto idx = static_cast<std::size_t>(pid);
            const auto y = renderPatternMono(sr, static_cast<std::uint16_t>(1u << idx), false, 1.0,
                                             kPatterns[idx].measuredBpm);
            REQUIRE(allFinite(y.data(), y.size()));
            const double pk = computeMaxAbs(y.data(), y.size());
            REQUIRE(pk > 1e-4);
            REQUIRE(pk < 2.0);
        }
    }
    REQUIRE(worstR < 0.9999);
}

TEST_CASE("Rhythmbox DSP 2: Exact TONE center bypass and left/right tilt balance", "[rhythmbox_dsp]")
{
    constexpr double fs = 48000.0;
    constexpr std::uint16_t rock1Mask = static_cast<std::uint16_t>(1u << static_cast<std::size_t>(PatternId::Rock1));

    const auto yLeft = renderPatternMono(fs, rock1Mask, false, 2.0, 106.8, 0.0, 1.0);
    const auto yCenter = renderPatternMono(fs, rock1Mask, false, 2.0, 106.8, 0.5, 1.0);
    const auto yRight = renderPatternMono(fs, rock1Mask, false, 2.0, 106.8, 1.0, 1.0);

    // Also verify returning to 0.5 restores exact flat bypass
    RhythmboxEngine eng;
    eng.prepare(fs);
    RhythmboxState st;
    st.enabled = true;
    st.patternMask = rock1Mask;
    st.clockMode = ClockMode::Internal;
    st.volume = 1.0;
    st.tone = 0.0;
    eng.applyState(st);
    st.tone = 0.5;
    eng.applyState(st);
    eng.reset();
    std::vector<double> yReturned(yCenter.size(), 0.0);
    for (std::size_t pos = 0; pos < yReturned.size(); pos += 128u)
    {
        const int blk = static_cast<int>(std::min<std::size_t>(128u, yReturned.size() - pos));
        eng.processBlock(yReturned.data() + pos, blk);
    }
    double maxDiff = 0.0;
    for (std::size_t i = 0; i < yCenter.size(); ++i)
    {
        maxDiff = std::max(maxDiff, std::abs(yReturned[i] - yCenter[i]));
    }
    REQUIRE(maxDiff == 0.0);

    const double rLeft = lowHighRatioDb(yLeft, fs);
    const double rCenter = lowHighRatioDb(yCenter, fs);
    const double rRight = lowHighRatioDb(yRight, fs);

    REQUIRE(rLeft > rCenter + 4.0);
    REQUIRE(rRight < rCenter - 4.0);
}

TEST_CASE("Rhythmbox DSP 3: Single mode vs Wired-OR multi-pattern sequencing & deduplication", "[rhythmbox_dsp]")
{
    constexpr double fs = 48000.0;
    constexpr std::uint16_t waltzBit = static_cast<std::uint16_t>(1u << static_cast<std::size_t>(PatternId::Waltz));
    constexpr std::uint16_t rock1Bit = static_cast<std::uint16_t>(1u << static_cast<std::size_t>(PatternId::Rock1));
    constexpr std::uint16_t bothMask = static_cast<std::uint16_t>(waltzBit | rock1Bit);

    const auto ySingle = renderPatternMono(fs, bothMask, false, 2.0, 106.8);
    const auto yWaltz = renderPatternMono(fs, waltzBit, false, 2.0, 106.8);
    for (std::size_t i = 0; i < ySingle.size(); ++i)
    {
        REQUIRE(ySingle[i] == yWaltz[i]);
    }

    const auto yWaltz4 = renderPatternMono(fs, waltzBit, false, 4.0, 106.8);
    const auto yRock4 = renderPatternMono(fs, rock1Bit, false, 4.0, 106.8);
    const auto yOr4 = renderPatternMono(fs, bothMask, true, 4.0, 106.8);

    const double rmsOr = computeRms(yOr4.data(), yOr4.size());
    const double rmsW = computeRms(yWaltz4.data(), yWaltz4.size());
    const double rmsR = computeRms(yRock4.data(), yRock4.size());
    REQUIRE(rmsOr > std::max(rmsW, rmsR));

    // Beat 1 low-passed BASS_DRUM deduplication check (Wired-OR fires BassDrum once, not 2x)
    ke310::dsp::BiquadCascade<1> lpRock;
    ke310::dsp::BiquadCascade<1> lpOr;
    const ke310::dsp::FilterSectionSpec lpSpec{ke310::dsp::FilterType::LP2, 150.0, 0.707, 0.0};
    lpRock.design(&lpSpec, 1u, 0.0, fs);
    lpOr.design(&lpSpec, 1u, 0.0, fs);

    const std::size_t n30ms = static_cast<std::size_t>(0.030 * fs);
    double pkRock = 0.0;
    double pkOr = 0.0;
    for (std::size_t i = 0; i < n30ms; ++i)
    {
        pkRock = std::max(pkRock, std::abs(lpRock.processSample(yRock4[i])));
        pkOr = std::max(pkOr, std::abs(lpOr.processSample(yOr4[i])));
    }
    REQUIRE(pkOr / std::max(pkRock, 1e-12) <= 1.15);
}

TEST_CASE("Rhythmbox DSP 4: Coherent METAL_BUS +6 dB & Phase R2.6 circuit physics", "[rhythmbox_dsp]")
{
    constexpr double fs = 48000.0;

    // 1) Coherent CYMBAL + HIGH_HAT +6 dB doubling on shared METAL_BUS
    {
        const int n40 = static_cast<int>(0.040 * fs);
        auto renderPair = [&](bool addCym)
        {
            RhythmboxEngine eng;
            eng.prepare(fs);
            RhythmboxState st;
            st.enabled = false;
            st.analogVariation = 0.0;
            st.volume = 1.0;
            eng.applyState(st);
            eng.triggerVoice(VoiceId::HighHat, 1.0);
            if (addCym)
            {
                eng.triggerVoice(VoiceId::Cymbal, 1.0);
            }
            std::vector<double> buf(static_cast<std::size_t>(n40), 0.0);
            eng.processBlock(buf.data(), n40);
            return computeRms(buf.data(), buf.size());
        };
        const double rHH = renderPair(false);
        const double rBoth = renderPair(true);
        const double gainDb = 20.0 * std::log10(rBoth / rHH);
        REQUIRE(gainDb >= 4.5);
        REQUIRE(gainDb <= 6.8);
    }

    // 2) BOSSANOVA Steps 28 & 60 (0-idx 27 & 59) CYMBAL hot-retrigger & VCA CV switch click
    {
        const auto idx = static_cast<std::size_t>(PatternId::Bossanova);
        const std::uint64_t cymMask = kPatterns[idx].voiceStepMasks[static_cast<std::size_t>(VoiceId::Cymbal)];
        REQUIRE((cymMask & (1ULL << 27u)) != 0ULL);
        REQUIRE((cymMask & (1ULL << 59u)) != 0ULL);

        const double bpm = kPatterns[idx].measuredBpm;
        const auto yBossa = renderPatternMono(fs, static_cast<std::uint16_t>(1u << idx), false, 9.0, bpm, 0.5, 1.0);

        ke310::dsp::BiquadCascade<1> hp2500;
        ke310::dsp::BiquadCascade<2> bpMid;
        const ke310::dsp::FilterSectionSpec hpSpec{ke310::dsp::FilterType::HP2, 2500.0, 0.707, 0.0};
        const std::array<ke310::dsp::FilterSectionSpec, 2> bpSpecs = {{
            {ke310::dsp::FilterType::HP2, 220.0, 0.707, 0.0},
            {ke310::dsp::FilterType::LP2, 2500.0, 0.707, 0.0},
        }};
        hp2500.design(&hpSpec, 1u, 0.0, fs);
        bpMid.design(bpSpecs, 2u, 0.0, fs);

        const double stepS = 60.0 / bpm / 4.0;
        const double t28 = 27.0 * stepS;
        const std::size_t a28 = static_cast<std::size_t>((t28 - 0.004) * fs);
        const std::size_t b28 = static_cast<std::size_t>((t28 + 0.012) * fs);
        double pk28Hi = 0.0;
        double pk28Mid = 0.0;
        for (std::size_t i = 0; i < b28 && i < yBossa.size(); ++i)
        {
            const double hi = hp2500.processSample(yBossa[i]);
            const double mid = bpMid.processSample(yBossa[i]);
            if (i >= a28)
            {
                pk28Hi = std::max(pk28Hi, std::abs(hi));
                pk28Mid = std::max(pk28Mid, std::abs(mid));
            }
        }
        REQUIRE(pk28Hi >= 0.30);
        REQUIRE(pk28Mid >= 0.006);
    }

    // 3) COW_BELL spectral centroid (350..3000 Hz) >= 1005 Hz & <= 1080 Hz
    {
        const auto yCb = renderOneShotMono(fs, VoiceId::CowBell, 0.05, 0.0);
        const std::size_t n40 = static_cast<std::size_t>(0.040 * fs);
        double num = 0.0;
        double den = 0.0;
        for (double f = 350.0; f <= 3000.0; f += 25.0)
        {
            const double p = dftBinPower(yCb.data(), n40, f, fs);
            num += f * p;
            den += p;
        }
        const double centroid = num / std::max(den, 1e-24);
        REQUIRE(centroid >= 1005.0);
        REQUIRE(centroid <= 1080.0);
    }

    // 4) HIGH_CONGA early attack chirp (>= 192 Hz) -> tail ([182, 186] Hz)
    {
        const auto yHc = renderOneShotMono(fs, VoiceId::HighConga, 0.15, 0.0);
        const std::size_t n10 = static_cast<std::size_t>(0.010 * fs);
        const std::size_t i30 = static_cast<std::size_t>(0.030 * fs);
        const std::size_t n60 = static_cast<std::size_t>(0.060 * fs);
        const double fEarly = scanPeakFrequency(yHc.data(), n10, 150.0, 260.0, fs, 0.5, 0.15);
        const double fTail = scanPeakFrequency(yHc.data() + i30, n60, 150.0, 220.0, fs, 0.25, 1.0);
        REQUIRE(fEarly >= 192.0);
        REQUIRE(fTail >= 182.0);
        REQUIRE(fTail <= 186.0);
    }

    // 5) Continuous ZDF Bridged-T roll carry-over & Analog micro-drift bounds
    {
        RhythmboxEngine engRoll;
        engRoll.prepare(fs);
        RhythmboxState st;
        st.enabled = false;
        st.analogVariation = 0.0;
        st.volume = 1.0;
        engRoll.applyState(st);

        const int nStep = static_cast<int>(0.140 * fs);
        std::vector<double> h1(static_cast<std::size_t>(nStep), 0.0);
        std::vector<double> h2(static_cast<std::size_t>(nStep), 0.0);
        engRoll.triggerVoice(VoiceId::HighConga, 1.0);
        engRoll.processBlock(h1.data(), nStep);
        engRoll.triggerVoice(VoiceId::HighConga, 1.0);
        engRoll.processBlock(h2.data(), nStep);

        double wfDiff = 0.0;
        for (int i = 0; i < nStep; ++i)
        {
            wfDiff = std::max(wfDiff, std::abs(h2[static_cast<std::size_t>(i)] - h1[static_cast<std::size_t>(i)]));
        }
        REQUIRE(wfDiff > 1e-3);

        // Consecutive Snare hits with analogVariation = 1.0 vary within hardware bounds
        engRoll.reset();
        st.analogVariation = 1.0;
        engRoll.applyState(st);
        const int n250 = static_cast<int>(0.250 * fs);
        const std::size_t n100 = static_cast<std::size_t>(0.100 * fs);
        std::vector<double> hitBuf(static_cast<std::size_t>(n250), 0.0);
        std::array<double, 8> rmsVals{};
        for (std::size_t k = 0; k < 8; ++k)
        {
            engRoll.triggerVoice(VoiceId::SnareDrum, 1.0);
            engRoll.processBlock(hitBuf.data(), n250);
            rmsVals[k] = computeRms(hitBuf.data(), n100);
        }
        double meanRms = 0.0;
        for (double r : rmsVals) meanRms += r / 8.0;
        double varRms = 0.0;
        for (double r : rmsVals) varRms += (r - meanRms) * (r - meanRms) / 8.0;
        const double cv = std::sqrt(varRms) / meanRms;
        REQUIRE(cv > 0.0005);
        REQUIRE(cv < 0.08);
    }
}

TEST_CASE("Rhythmbox DSP 5: GM Drum Map MIDI triggers across all 11 voices", "[rhythmbox_dsp]")
{
    constexpr double fs = 48000.0;
    RhythmboxEngine eng;
    eng.prepare(fs);
    RhythmboxState st;
    st.enabled = false; // MIDI one-shots work even when pattern sequencer is stopped
    st.volume = 1.0;
    eng.applyState(st);

    const int n200 = static_cast<int>(0.20 * fs);
    std::vector<double> buf(static_cast<std::size_t>(n200), 0.0);

    std::array<bool, kNumVoices> covered{};
    for (int note = 0; note < 128; ++note)
    {
        const auto vOpt = mapGmMidiNoteToVoice(note);
        if (vOpt.has_value())
        {
            covered[static_cast<std::size_t>(*vOpt)] = true;
            eng.reset();
            REQUIRE(eng.triggerMidiNote(note, 100));
            eng.processBlock(buf.data(), n200);
            REQUIRE(allFinite(buf.data(), buf.size()));
            REQUIRE(computeRms(buf.data(), buf.size()) > 1e-4);
        }
    }
    for (bool c : covered)
    {
        REQUIRE(c);
    }

    REQUIRE_FALSE(eng.triggerMidiNote(12, 100));
    REQUIRE_FALSE(eng.triggerMidiNote(36, 0));
}

TEST_CASE("Rhythmbox DSP 6: Block-size invariance with analogVariation = 1.0", "[rhythmbox_dsp]")
{
    constexpr double fs = 48000.0;
    constexpr std::uint16_t rock1Bit = static_cast<std::uint16_t>(1u << static_cast<std::size_t>(PatternId::Rock1));
    const auto yRef = renderPatternMono(fs, rock1Bit, false, 2.0, 106.8, 0.35, 1.0, 96000);

    for (int blk : {16, 37, 64, 128, 256, 512})
    {
        const auto yBlk = renderPatternMono(fs, rock1Bit, false, 2.0, 106.8, 0.35, 1.0, blk);
        REQUIRE(yBlk.size() == yRef.size());
        double maxDiff = 0.0;
        for (std::size_t i = 0; i < yRef.size(); ++i)
        {
            maxDiff = std::max(maxDiff, std::abs(yBlk[i] - yRef[i]));
        }
        REQUIRE(maxDiff < 1e-12);
    }
}

TEST_CASE("Rhythmbox DSP 7: HostSync PPQ / TempoSync clock accuracy & Standalone fallback", "[rhythmbox_dsp]")
{
    constexpr double fs = 48000.0;
    constexpr std::uint16_t marchBit = static_cast<std::uint16_t>(1u << static_cast<std::size_t>(PatternId::March));

    // A) HostSync when DAW stopped -> sequencer idle (silent)
    {
        RhythmboxEngine eng;
        eng.prepare(fs);
        RhythmboxState st;
        st.enabled = true;
        st.patternMask = marchBit;
        st.clockMode = ClockMode::HostSync;
        st.analogVariation = 0.0;
        st.volume = 1.0;
        eng.applyState(st);

        HostTransportInfo hostStopped{true, false, true, 120.0, true, 0.0};
        std::vector<double> out(2048, 0.0);
        eng.processBlock(out.data(), 2048, hostStopped);
        REQUIRE(computeMaxAbs(out.data(), out.size()) == 0.0);

        // B) DAW starts playing at 120 BPM from PPQ = 0.0 across 256-sample buffers (1.0 sec)
        // Must match Internal 120 BPM rendering to < 1e-12!
        eng.reset();
        const int blk = 256;
        const int total = 48000;
        std::vector<double> hostPlayOut(static_cast<std::size_t>(total), 0.0);
        double ppq = 0.0;
        for (int pos = 0; pos < total; pos += blk)
        {
            const int n = std::min(blk, total - pos);
            HostTransportInfo hPlay{true, true, true, 120.0, true, ppq};
            eng.processBlock(hostPlayOut.data() + pos, n, hPlay);
            ppq += static_cast<double>(n) * (120.0 / (60.0 * fs));
        }
        const auto ref120 = renderPatternMono(fs, marchBit, false, 1.0, 120.0, 0.5, 0.0, 256);
        double maxDiff = 0.0;
        for (std::size_t i = 0; i < ref120.size(); ++i)
        {
            maxDiff = std::max(maxDiff, std::abs(hostPlayOut[i] - ref120[i]));
        }
        REQUIRE(maxDiff < 1e-12);

        // C) Loop-back to PPQ = 0.0 triggers Bar 1 Beat 1 cleanly at start of buffer
        eng.reset();
        HostTransportInfo hLoop{true, true, true, 120.0, true, 0.0};
        std::vector<double> loopBuf(1024, 0.0);
        eng.processBlock(loopBuf.data(), 1024, hLoop);
        REQUIRE(computeMaxAbs(loopBuf.data(), 256) > 0.05);

        // D) Mid-step seek to PPQ = 0.12 waits cleanly until PPQ = 0.25 (doesn't double-fire Step 0)
        eng.reset();
        HostTransportInfo hSeekMid{true, true, true, 120.0, true, 0.12};
        std::vector<double> midBuf(1024, 0.0);
        eng.processBlock(midBuf.data(), 1024, hSeekMid);
        REQUIRE(computeMaxAbs(midBuf.data(), 1024) == 0.0);
    }

    // D) HostSync with !hasTransport (Standalone) falls back to Internal mode & runs when enabled
    {
        RhythmboxEngine eng;
        eng.prepare(fs);
        RhythmboxState st;
        st.enabled = true;
        st.patternMask = marchBit;
        st.clockMode = ClockMode::HostSync;
        st.bpm = 135.0;
        st.volume = 1.0;
        eng.applyState(st);

        HostTransportInfo noHost{};
        std::vector<double> out(4096, 0.0);
        eng.processBlock(out.data(), 4096, noHost);
        REQUIRE(computeMaxAbs(out.data(), out.size()) > 0.05);
        REQUIRE(eng.telemetry().lamp.isRunning);
        REQUIRE(std::abs(eng.telemetry().lamp.effectiveBpm - 135.0) < 1e-6);
    }

    // E) TempoSync uses host BPM while obeying local START/STOP
    {
        RhythmboxEngine eng;
        eng.prepare(fs);
        RhythmboxState st;
        st.enabled = true;
        st.patternMask = marchBit;
        st.clockMode = ClockMode::TempoSync;
        st.bpm = 90.0;
        eng.applyState(st);

        HostTransportInfo hPaused{true, false, true, 148.0, true, 0.0};
        std::vector<double> out(1024, 0.0);
        eng.processBlock(out.data(), 1024, hPaused);
        REQUIRE(eng.telemetry().lamp.isRunning);
        REQUIRE(std::abs(eng.telemetry().lamp.effectiveBpm - 148.0) < 1e-6);
    }
}

TEST_CASE("Rhythmbox DSP 8: Tempo Lamp flashes per beat when idle, downbeat-only when playing, OFF when unselected",
          "[rhythmbox_dsp]")
{
    constexpr double fs = 48000.0;
    constexpr int samplesPerBeat120 = 24000; // 120 BPM -> 0.5 s / beat = 24000 samples
    constexpr std::uint16_t rock1Bit = static_cast<std::uint16_t>(1u << static_cast<std::size_t>(PatternId::Rock1));
    constexpr std::uint16_t waltzBit = static_cast<std::uint16_t>(1u << static_cast<std::size_t>(PatternId::Waltz));

    RhythmboxEngine eng;
    eng.prepare(fs);
    std::vector<double> scratch(24000, 0.0);

    // 1) No rhythm selected -> Lamp stays completely OFF
    {
        RhythmboxState st;
        st.enabled = true;
        st.patternMask = 0u;
        st.clockMode = ClockMode::Internal;
        st.bpm = 120.0;
        eng.applyState(st);
        for (int b = 0; b < 5; ++b)
        {
            eng.processBlock(scratch.data(), 512);
            REQUIRE_FALSE(eng.telemetry().lamp.beatFlash);
            REQUIRE_FALSE(eng.telemetry().lamp.isDownbeat);
            REQUIRE(eng.telemetry().lamp.totalSteps == 0);
        }
    }

    // 2) Rhythm selected but stopped -> flashes every quarter-note beat at 120 BPM
    {
        RhythmboxState st;
        st.enabled = false;
        st.patternMask = rock1Bit;
        st.clockMode = ClockMode::Internal;
        st.bpm = 120.0;
        eng.applyState(st);

        // Beat 0 start: flash ON
        eng.processBlock(scratch.data(), 512);
        REQUIRE_FALSE(eng.telemetry().lamp.isRunning);
        REQUIRE(eng.telemetry().lamp.beatFlash);
        REQUIRE(eng.telemetry().lamp.totalSteps == 64);

        // Advance to beat 0.5 -> flash OFF
        eng.processBlock(scratch.data(), 12000 - 512);
        eng.processBlock(scratch.data(), 512);
        REQUIRE_FALSE(eng.telemetry().lamp.beatFlash);

        // Advance to beat 1.0 -> flash ON on Beat 2 when stopped
        eng.processBlock(scratch.data(), samplesPerBeat120 - 12000 - 512);
        eng.processBlock(scratch.data(), 512);
        REQUIRE(eng.telemetry().lamp.beatFlash);
    }

    // 3) Rhythm playing (4/4 Rock I at 120 BPM) -> flashes ONLY on Beat 1 (downbeat)
    {
        eng.reset();
        RhythmboxState st;
        st.enabled = true;
        st.patternMask = rock1Bit;
        st.clockMode = ClockMode::Internal;
        st.bpm = 120.0;
        eng.applyState(st);

        // Beat 1 (beat 0.0): flash ON
        eng.processBlock(scratch.data(), 512);
        REQUIRE(eng.telemetry().lamp.isRunning);
        REQUIRE(eng.telemetry().lamp.beatFlash);
        REQUIRE(eng.telemetry().lamp.isDownbeat);

        // Beat 2 (beat 1.0): flash OFF
        eng.processBlock(scratch.data(), samplesPerBeat120 - 512);
        eng.processBlock(scratch.data(), 512);
        REQUIRE_FALSE(eng.telemetry().lamp.beatFlash);

        // Beat 3 (beat 2.0): flash OFF
        eng.processBlock(scratch.data(), samplesPerBeat120 - 512);
        eng.processBlock(scratch.data(), 512);
        REQUIRE_FALSE(eng.telemetry().lamp.beatFlash);

        // Beat 4 (beat 3.0): flash OFF
        eng.processBlock(scratch.data(), samplesPerBeat120 - 512);
        eng.processBlock(scratch.data(), 512);
        REQUIRE_FALSE(eng.telemetry().lamp.beatFlash);

        // Next bar Beat 1 (beat 4.0): flash ON again
        eng.processBlock(scratch.data(), samplesPerBeat120 - 512);
        eng.processBlock(scratch.data(), 512);
        REQUIRE(eng.telemetry().lamp.beatFlash);
        REQUIRE(eng.telemetry().lamp.isDownbeat);
    }

    // 4) Waltz 3/4 playing -> flashes on beat 0.0 and beat 3.0 (NOT beat 2.0)
    {
        eng.reset();
        RhythmboxState st;
        st.enabled = true;
        st.patternMask = waltzBit;
        st.clockMode = ClockMode::Internal;
        st.bpm = 120.0;
        eng.applyState(st);

        eng.processBlock(scratch.data(), 512);
        REQUIRE(eng.telemetry().lamp.beatFlash);
        eng.processBlock(scratch.data(), samplesPerBeat120 - 512);
        eng.processBlock(scratch.data(), 512);
        REQUIRE_FALSE(eng.telemetry().lamp.beatFlash);
        eng.processBlock(scratch.data(), samplesPerBeat120 - 512);
        eng.processBlock(scratch.data(), 512);
        REQUIRE_FALSE(eng.telemetry().lamp.beatFlash);
        eng.processBlock(scratch.data(), samplesPerBeat120 - 512);
        eng.processBlock(scratch.data(), 512);
        REQUIRE(eng.telemetry().lamp.beatFlash);
    }
}
