#include "dsp/KE310Engine.hpp"
#include "dsp/MidiRouter.hpp"
#include "dsp/RotarySpeaker.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <numbers>
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

struct RenderedBuses
{
    std::vector<double> mix;
    std::vector<double> rotor;
    std::vector<double> stationary;
};

RenderedBuses renderEvents(KE310Engine& eng, const std::vector<TimedEvent>& events,
                           double durationS, double fs = 48000.0, int blockSize = 128)
{
    const int totalSamples = static_cast<int>(std::llround(durationS * fs));
    auto evSorted = events;
    std::stable_sort(evSorted.begin(), evSorted.end(),
                     [](const TimedEvent& a, const TimedEvent& b) { return a.timeS < b.timeS; });

    RenderedBuses out;
    out.mix.resize(static_cast<std::size_t>(totalSamples));
    out.rotor.resize(static_cast<std::size_t>(totalSamples));
    out.stationary.resize(static_cast<std::size_t>(totalSamples));

    std::size_t evIdx = 0;
    int pos = 0;
    while (pos < totalSamples)
    {
        const double tCur = static_cast<double>(pos) / fs;
        while (evIdx < evSorted.size() && evSorted[evIdx].timeS <= tCur + 1e-9)
        {
            const auto& e = evSorted[evIdx++];
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
        if (evIdx < evSorted.size())
        {
            nextEvSample = std::min(totalSamples,
                                    std::max(pos + 1, static_cast<int>(std::llround(evSorted[evIdx].timeS * fs))));
        }
        const int nBlk = std::min({blockSize, nextEvSample - pos, totalSamples - pos});
        eng.processBlock(nBlk, out.mix.data() + pos, out.rotor.data() + pos, out.stationary.data() + pos);
        pos += nBlk;
    }
    return out;
}

double computeRms(const double* data, int n)
{
    double acc = 0.0;
    for (int i = 0; i < n; ++i)
    {
        acc += data[i] * data[i];
    }
    return std::sqrt(acc / static_cast<double>(std::max(1, n)) + 1e-24);
}

// Evaluate DFT peak frequency in [fMin, fMax] Hz using Hann window
double dominantFreqHz(const double* seg, int n, double fs, double fMin, double fMax)
{
    constexpr int kBins = 160;
    double bestFreq = fMin;
    double bestMag = -1.0;
    for (int b = 0; b <= kBins; ++b)
    {
        const double f = fMin + (fMax - fMin) * (static_cast<double>(b) / kBins);
        const double w = 2.0 * std::numbers::pi * f / fs;
        double re = 0.0, im = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double win = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * i / std::max(1, n - 1)));
            const double x = seg[i] * win;
            re += x * std::cos(w * i);
            im -= x * std::sin(w * i);
        }
        const double mag = re * re + im * im;
        if (mag > bestMag)
        {
            bestMag = mag;
            bestFreq = f;
        }
    }
    return bestFreq;
}

double goertzelDb(const double* seg, int n, double fs, double targetHz)
{
    double re = 0.0, im = 0.0, wSum = 0.0;
    const double w = 2.0 * std::numbers::pi * targetHz / fs;
    for (int i = 0; i < n; ++i)
    {
        const double win = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * i / std::max(1, n - 1)));
        wSum += win;
        const double x = seg[i] * win;
        re += x * std::cos(w * i);
        im -= x * std::sin(w * i);
    }
    const double amp = std::hypot(re, im) / std::max(1e-12, 0.5 * wSum);
    return 20.0 * std::log10(std::max(amp, 1e-12));
}

} // namespace

TEST_CASE("1. Multirate filter stability (44.1 / 48 / 88.2 / 96 kHz)", "[dsp][stability]")
{
    KE310Engine eng;
    for (double sr : {44100.0, 48000.0, 88200.0, 96000.0})
    {
        eng.prepare(sr);
        for (std::size_t v = 0; v < kNumVoices; ++v)
        {
            const double r = eng.voiceFilter(static_cast<VoiceId>(v)).maxPoleRadius();
            INFO("Voice " << kVoiceSpecs[v].name << " @ " << sr << " Hz pole_r=" << r);
            REQUIRE(r < 0.9999);
        }
        for (std::size_t p = 0; p < kNumFixedPresets; ++p)
        {
            const double r = eng.fixedPresetFilter(static_cast<FixedPresetId>(p)).maxPoleRadius();
            INFO("Preset " << kFixedPresetSpecs[p].name << " @ " << sr << " Hz pole_r=" << r);
            REQUIRE(r < 0.9999);
        }
        REQUIRE(eng.wowBaseFilter().maxPoleRadius() < 0.9999);
        REQUIRE(eng.marimbaBusFilter().maxPoleRadius() < 0.9999);
        for (int m : {48, 60, 72, 84, 96})
        {
            const auto pair = eng.makeMarimbaNoteFilters(eng.tos().noteFreq(m));
            REQUIRE(pair.first.maxPoleRadius() < 0.9999);
            REQUIRE(pair.second.maxPoleRadius() < 0.9999);
        }
    }
}

TEST_CASE("2. TOS integer dividers and Pedal VCO octave stretch", "[dsp][tuning]")
{
    KE310Engine eng;
    eng.prepare(48000.0);

    // Vintage TOS A4 = 440.91 Hz, C4 / A4 = 602 / 506 * 0.5
    REQUIRE(eng.tos().noteFreq(69) == Catch::Approx(440.91).epsilon(1e-6));
    REQUIRE(eng.tos().noteFreq(60) == Catch::Approx(440.91 * (602.0 / 506.0) * 0.5).epsilon(1e-6));

    // Pedal C2 (-7.77c) -> C3 (+0.77c) shows +8.54c octave stretch
    const double fC2 = eng.pedalVco().noteFreq(36);
    const double fC3 = eng.pedalVco().noteFreq(48);
    const double stretchCents = 1200.0 * std::log2(fC3 / (2.0 * fC2));
    REQUIRE(stretchCents == Catch::Approx(8.54).margin(0.02));

    // Disabling vintage modes falls back cleanly to exact 12-TET
    KE310State st;
    st.vintageTos = false;
    st.vintagePedalVco = false;
    st.a4Hz = 440.0;
    eng.applyState(st);
    REQUIRE(eng.tos().noteFreq(69) == Catch::Approx(440.0).epsilon(1e-9));
    REQUIRE(eng.tos().noteFreq(60) == Catch::Approx(261.625565).epsilon(1e-5));
    const double stretch12Tet = 1200.0 * std::log2(eng.pedalVco().noteFreq(48) / (2.0 * eng.pedalVco().noteFreq(36)));
    REQUIRE(stretch12Tet == Catch::Approx(0.0).margin(1e-6));
}

TEST_CASE("3. Pedal low-note priority and String Bass decay/retrigger", "[dsp][pedal]")
{
    KE310Engine eng;
    eng.prepare(48000.0);
    KE310State st;
    st.upperTibia8 = false;
    st.pedalMajorFlute8 = true;
    st.pedalVolume = 1.0;
    eng.applyState(st);

    const auto out = renderEvents(eng,
        {{0.05, true, Manual::Pedal, 40},
         {0.60, true, Manual::Pedal, 36},
         {1.20, false, Manual::Pedal, 36},
         {1.80, false, Manual::Pedal, 40}},
        2.0);

    const double f1 = dominantFreqHz(out.mix.data() + static_cast<int>(0.20 * 48000), static_cast<int>(0.35 * 48000), 48000.0, 55.0, 100.0);
    const double f2 = dominantFreqHz(out.mix.data() + static_cast<int>(0.75 * 48000), static_cast<int>(0.35 * 48000), 48000.0, 55.0, 100.0);
    const double f3 = dominantFreqHz(out.mix.data() + static_cast<int>(1.35 * 48000), static_cast<int>(0.35 * 48000), 48000.0, 55.0, 100.0);
    REQUIRE(f1 > 80.0);
    REQUIRE(f1 < 85.0);
    REQUIRE(f2 > 63.0);
    REQUIRE(f2 < 67.5);
    REQUIRE(f3 > 80.0);
    REQUIRE(f3 < 85.0);

    // String Bass natural decay & low-note retrigger
    eng.reset();
    KE310State sbSt;
    sbSt.upperTibia8 = false;
    sbSt.pedalStringBass = true;
    sbSt.pedalVolume = 1.0;
    eng.applyState(sbSt);
    const auto outSb = renderEvents(eng,
        {{0.05, true, Manual::Pedal, 43},
         {1.10, true, Manual::Pedal, 36},
         {2.00, false, Manual::Pedal, 36},
         {2.00, false, Manual::Pedal, 43}},
        2.2);
    const double rAtk = computeRms(outSb.mix.data() + static_cast<int>(0.07 * 48000), static_cast<int>(0.10 * 48000));
    const double rDec = computeRms(outSb.mix.data() + static_cast<int>(0.90 * 48000), static_cast<int>(0.15 * 48000));
    const double rRetrig = computeRms(outSb.mix.data() + static_cast<int>(1.12 * 48000), static_cast<int>(0.10 * 48000));
    REQUIRE(rDec < rAtk * 0.25);
    REQUIRE(rRetrig > rDec * 3.0);
}

TEST_CASE("4. PERCUS 2 2/3' single-trigger (suppresses legato retrigger)", "[dsp][presets]")
{
    KE310Engine eng;
    eng.prepare(48000.0);
    KE310State st;
    st.upperTibia8 = false;
    st.organSilent = true;
    st.presetPerc2_23 = true;
    eng.applyState(st);

    const auto out = renderEvents(eng,
        {{0.05, true, Manual::Upper, 60},
         {0.45, true, Manual::Upper, 67},
         {0.95, false, Manual::Upper, 60},
         {0.95, false, Manual::Upper, 67}},
        1.1);

    const double rInit = computeRms(out.mix.data() + static_cast<int>(0.06 * 48000), static_cast<int>(0.19 * 48000));
    const double rLegato = computeRms(out.mix.data() + static_cast<int>(0.46 * 48000), static_cast<int>(0.24 * 48000));
    REQUIRE(rLegato < rInit * 0.08);
}

TEST_CASE("5. Piano takeover & 2-bus Ensemble routing", "[dsp][routing]")
{
    KE310Engine eng;
    eng.prepare(48000.0);

    // Piano takeover decays Upper 8'/4' (Tibia 8') while keeping Tibia 16' sustained
    KE310State st8;
    st8.upperTibia8 = true;
    st8.presetPiano = true;
    st8.presetVolume = 0.0;
    eng.applyState(st8);
    const auto out8 = renderEvents(eng, {{0.05, true, Manual::Upper, 60}, {2.20, false, Manual::Upper, 60}}, 2.3);
    const double rStart = computeRms(out8.mix.data() + static_cast<int>(0.08 * 48000), static_cast<int>(0.12 * 48000));
    const double rLate = computeRms(out8.mix.data() + static_cast<int>(1.60 * 48000), static_cast<int>(0.30 * 48000));
    REQUIRE(rLate < rStart * 0.15);

    eng.reset();
    KE310State st16;
    st16.upperTibia16 = true;
    st16.upperTibia8 = false;
    st16.presetPiano = true;
    st16.presetVolume = 0.0;
    eng.applyState(st16);
    const auto out16 = renderEvents(eng, {{0.05, true, Manual::Upper, 60}, {2.20, false, Manual::Upper, 60}}, 2.3);
    const double r16Start = computeRms(out16.mix.data() + static_cast<int>(0.08 * 48000), static_cast<int>(0.12 * 48000));
    const double r16Late = computeRms(out16.mix.data() + static_cast<int>(1.60 * 48000), static_cast<int>(0.30 * 48000));
    REQUIRE(r16Late > r16Start * 0.85);

    // 2-bus Ensemble routing
    eng.reset();
    KE310State stOff;
    stOff.upperTibia8 = false;
    stOff.upperClarinet8 = true;
    stOff.ensemble = false;
    eng.applyState(stOff);
    const auto busOff = renderEvents(eng, {{0.02, true, Manual::Upper, 60}, {0.30, false, Manual::Upper, 60}}, 0.35);
    REQUIRE(computeRms(busOff.rotor.data(), static_cast<int>(busOff.rotor.size())) < 1e-6);
    REQUIRE(computeRms(busOff.stationary.data(), static_cast<int>(busOff.stationary.size())) > 0.01);

    eng.reset();
    stOff.ensemble = true;
    eng.applyState(stOff);
    const auto busOn = renderEvents(eng, {{0.02, true, Manual::Upper, 60}, {0.30, false, Manual::Upper, 60}}, 0.35);
    REQUIRE(computeRms(busOn.stationary.data(), static_cast<int>(busOn.stationary.size())) < 1e-6);
    REQUIRE(computeRms(busOn.rotor.data(), static_cast<int>(busOn.rotor.size())) > 0.01);
}

TEST_CASE("6. MARIMBA key-tracking 3-pole overlapping RC-VCA & clean note-off tail", "[dsp][marimba]")
{
    KE310Engine eng;
    eng.prepare(48000.0);
    KE310State st;
    st.upperTibia8 = false;
    st.organSilent = true;
    st.presetMarimba = true;
    st.totalVolume = 1.0;
    st.presetVolume = 1.0;
    st.hasRepeatRateHzOverride = true;
    st.repeatRateHzOverride = 6.71;
    eng.applyState(st);

    const auto out = renderEvents(eng, {{0.05, true, Manual::Upper, 60}, {2.05, false, Manual::Upper, 60}}, 2.50);
    for (double x : out.mix)
    {
        REQUIRE(std::isfinite(x));
    }

    // Verify alternating 8' (262 Hz) and 4' (524 Hz) energy and clean 4' 1-bit H2 suppression
    const double f0 = eng.tos().noteFreq(60);
    const double h1Db = goertzelDb(out.mix.data() + static_cast<int>(0.40 * 48000), static_cast<int>(0.04 * 48000), 48000.0, f0);
    const double h4fDb = goertzelDb(out.mix.data() + static_cast<int>(0.47 * 48000), static_cast<int>(0.04 * 48000), 48000.0, 2.0 * f0);
    REQUIRE(h1Db > -45.0);
    REQUIRE(h4fDb > -45.0);

    const double tailRms = computeRms(out.mix.data() + static_cast<int>(2.35 * 48000), static_cast<int>(0.15 * 48000));
    REQUIRE(tailRms < 1e-4);
}

TEST_CASE("7. WOW TRUMPET monotonic ZDF-SVF wah, onset delay & legato carry-over", "[dsp][wow]")
{
    static_assert(kWowTrumpetParams.wahFClosedHz < kWowTrumpetParams.wahFOpenHz);
    static_assert(kWowTrumpetParams.wahQClosed < kWowTrumpetParams.wahQOpen);

    KE310State st;
    st.upperTibia8 = false;
    st.organSilent = true;
    st.presetWowTrumpet = true;
    st.totalVolume = 1.0;
    st.presetVolume = 1.0;
    st.hasRepeatRateHzOverride = true;
    st.repeatRateHzOverride = 4.08;

    for (double sr : {44100.0, 48000.0, 88200.0, 96000.0})
    {
        KE310Engine eng;
        eng.prepare(sr);
        eng.applyState(st);
        const auto out = renderEvents(eng, {{0.05, true, Manual::Upper, 48}, {1.55, false, Manual::Upper, 48}}, 1.65, sr);
        for (double x : out.mix)
        {
            REQUIRE(std::isfinite(x));
        }

        if (std::abs(sr - 48000.0) < 1.0)
        {
            const double f0 = eng.tos().noteFreq(48);
            // Straight-open hold (85..145 ms after 0.05s onset) vs first closed rest (275..335 ms)
            const double hold9 = goertzelDb(out.mix.data() + static_cast<int>(0.135 * sr), static_cast<int>(0.060 * sr), sr, 9.0 * f0);
            const double rest9 = goertzelDb(out.mix.data() + static_cast<int>(0.325 * sr), static_cast<int>(0.060 * sr), sr, 9.0 * f0);
            const double pk2_9 = goertzelDb(out.mix.data() + static_cast<int>(0.440 * sr), static_cast<int>(0.060 * sr), sr, 9.0 * f0);
            REQUIRE(hold9 - rest9 > 15.0);
            REQUIRE(pk2_9 - rest9 > 15.0);
        }
    }

    // Legato state carry-over: G3 (55) triggered overlapping C3 (48) at 0.48s lands in closed rest in 0.565..0.625s
    KE310Engine eng;
    eng.prepare(48000.0);
    eng.applyState(st);
    const auto legSig = renderEvents(eng,
        {{0.05, true, Manual::Upper, 48},
         {0.48, true, Manual::Upper, 55},
         {0.55, false, Manual::Upper, 48},
         {1.10, false, Manual::Upper, 55}},
        1.20);

    eng.reset();
    eng.applyState(st);
    const auto isoSig = renderEvents(eng,
        {{0.48, true, Manual::Upper, 55},
         {1.10, false, Manual::Upper, 55}},
        1.20);

    const double fG3 = eng.tos().noteFreq(55);
    const double legDb = goertzelDb(legSig.mix.data() + static_cast<int>(0.565 * 48000), static_cast<int>(0.060 * 48000), 48000.0, 6.0 * fG3);
    const double isoDb = goertzelDb(isoSig.mix.data() + static_cast<int>(0.565 * 48000), static_cast<int>(0.060 * 48000), 48000.0, 6.0 * fG3);
    REQUIRE(legDb < isoDb - 12.0);
}

TEST_CASE("8. SpringReverb dry bypass and bounded decaying wet tail", "[dsp][reverb]")
{
    KE310Engine eng;
    eng.prepare(48000.0);
    KE310State st;
    st.upperTibia8 = true;
    st.reverb = 0.0;
    eng.applyState(st);
    const auto dry = renderEvents(eng, {{0.05, true, Manual::Upper, 60}, {0.40, false, Manual::Upper, 60}}, 1.50);

    eng.reset();
    st.reverb = 0.55;
    eng.applyState(st);
    const auto wet = renderEvents(eng, {{0.05, true, Manual::Upper, 60}, {0.40, false, Manual::Upper, 60}}, 1.50);

    const double tailDry = computeRms(dry.mix.data() + static_cast<int>(0.70 * 48000), static_cast<int>(0.30 * 48000));
    const double tailWetEarly = computeRms(wet.mix.data() + static_cast<int>(0.65 * 48000), static_cast<int>(0.20 * 48000));
    const double tailWetLate = computeRms(wet.mix.data() + static_cast<int>(1.30 * 48000), static_cast<int>(0.20 * 48000));
    REQUIRE(tailDry < 1e-6);
    REQUIRE(tailWetEarly > 2e-4);
    REQUIRE(tailWetLate < tailWetEarly);
}

TEST_CASE("9. MidiRouter multi-channel, custom user channels, layering, split & stuck-note protection", "[dsp][midi]")
{
    MidiRouter router;

    // Default Multi-Channel (Ch1 Upper, Ch2 Lower, Ch3 Pedal) + unassigned channel filtering
    MidiRouterConfig mc;
    mc.mode = MidiMode::MultiChannel;
    router.setConfig(mc);
    REQUIRE(router.route(1, 60).valid);
    REQUIRE(router.route(1, 60).manual == Manual::Upper);
    REQUIRE(router.route(2, 48).valid);
    REQUIRE(router.route(2, 48).manual == Manual::Lower);
    const auto pedMc = router.route(3, 24);
    REQUIRE(pedMc.valid);
    REQUIRE(pedMc.manual == Manual::Pedal);
    REQUIRE(pedMc.midiNote == 36);
    // Unassigned channel (Ch 10) must NOT falsely trigger Upper
    REQUIRE_FALSE(router.route(10, 60).valid);
    REQUIRE(router.routeNote(10, 60).empty());

    // Custom user-selected channels (Upper=Ch 5, Lower=Ch 12, Pedal=Ch 16) + Octave shift
    MidiRouterConfig customCh;
    customCh.mode = MidiMode::MultiChannel;
    customCh.upperChannel = 5;
    customCh.lowerChannel = 12;
    customCh.pedalChannel = 16;
    customCh.lowerOctaveShift = -1;
    router.setConfig(customCh);
    REQUIRE_FALSE(router.route(1, 60).valid);
    REQUIRE(router.route(5, 60).manual == Manual::Upper);
    const auto lowCustom = router.route(12, 60);
    REQUIRE(lowCustom.valid);
    REQUIRE(lowCustom.manual == Manual::Lower);
    REQUIRE(lowCustom.midiNote == 48);
    REQUIRE(router.route(16, 38).manual == Manual::Pedal);

    // Same-channel layering (Upper=Ch 1, Lower=Ch 1) + Disabled Pedal (Off=-1)
    MidiRouterConfig layered;
    layered.upperChannel = 1;
    layered.lowerChannel = 1;
    layered.pedalChannel = kMidiChannelOff;
    router.setConfig(layered);
    const auto layeredList = router.routeNote(1, 60);
    REQUIRE(layeredList.count == 2);
    REQUIRE(layeredList.items[0].manual == Manual::Lower);
    REQUIRE(layeredList.items[1].manual == Manual::Upper);
    REQUIRE(router.routeNote(3, 36).empty());

    // Omni mode (Upper=Omni 0) responds to any channel 1..16
    MidiRouterConfig omniCfg;
    omniCfg.upperChannel = kMidiChannelOmni;
    omniCfg.lowerChannel = kMidiChannelOff;
    omniCfg.pedalChannel = kMidiChannelOff;
    router.setConfig(omniCfg);
    for (int ch : {1, 7, 16})
    {
        REQUIRE(router.route(ch, 64).valid);
        REQUIRE(router.route(ch, 64).manual == Manual::Upper);
    }

    // Stuck-note prevention when user changes MIDI channel while holding a key
    KE310Engine eng;
    eng.prepare(48000.0);
    KE310State st;
    st.upperTibia8 = true;
    eng.applyState(st);

    MidiRouterConfig liveCfg;
    liveCfg.upperChannel = 2;
    liveCfg.lowerChannel = 3;
    liveCfg.pedalChannel = 4;
    router.reset();
    router.setConfig(liveCfg, eng);
    router.handleNoteOn(2, 60, eng); // Hold C4 on Ch 2 (Upper)

    std::array<double, 256> mix{}, rot{}, stat{};
    eng.processBlock(256, mix.data(), rot.data(), stat.data());
    REQUIRE(computeRms(mix.data(), 256) > 0.01);

    // User reassigns Upper from Ch 2 -> Ch 9 while key is still held!
    liveCfg.upperChannel = 9;
    router.setConfig(liveCfg, eng);

    // Let release envelope decay (~200 ms, >11*tau_rel) and confirm no stuck note remains
    std::vector<double> decayBuf(9600), rBuf(9600), sBuf(9600);
    eng.processBlock(9600, decayBuf.data(), rBuf.data(), sBuf.data());
    REQUIRE(computeRms(decayBuf.data() + 8400, 1200) < 1e-5);

    // Single-Keyboard Split mode
    MidiRouterConfig sp;
    sp.mode = MidiMode::SingleKeyboardSplit;
    router.setConfig(sp);
    const auto pedSp = router.route(1, 35); // below C2 -> folds to B2 (47)
    REQUIRE(pedSp.manual == Manual::Pedal);
    REQUIRE(pedSp.midiNote == 47);
    REQUIRE(router.route(1, 52).manual == Manual::Lower);
    REQUIRE(router.route(1, 72).manual == Manual::Upper);
}

TEST_CASE("10. Block-size invariance and dense realtime CPU benchmark", "[dsp][perf]")
{
    auto runWithBlock = [](int blockSize) -> std::vector<double>
    {
        KE310Engine eng;
        eng.prepare(48000.0);
        KE310State st;
        st.upperTibia16 = true;
        st.upperTibia8 = true;
        st.upperTibia4 = true;
        st.upperString8 = true;
        st.lowerHorn8 = true;
        st.pedalBourdon16 = true;
        st.vibrato = true;
        st.reverb = 0.35;
        eng.applyState(st);

        constexpr int total = 24000; // 0.5 s
        eng.noteOn(Manual::Pedal, 36);
        eng.noteOn(Manual::Lower, 48);
        eng.noteOn(Manual::Lower, 55);
        eng.noteOn(Manual::Upper, 60);
        eng.noteOn(Manual::Upper, 64);
        eng.noteOn(Manual::Upper, 67);

        std::vector<double> mix(total), r(total), s(total);
        int pos = 0;
        while (pos < total)
        {
            const int n = std::min(blockSize, total - pos);
            eng.processBlock(n, mix.data() + pos, r.data() + pos, s.data() + pos);
            pos += n;
        }
        return mix;
    };

    const auto ref128 = runWithBlock(128);
    for (int bs : {37, 64, 512})
    {
        const auto cmp = runWithBlock(bs);
        double maxDiff = 0.0;
        for (std::size_t i = 0; i < ref128.size(); ++i)
        {
            maxDiff = std::max(maxDiff, std::abs(cmp[i] - ref128[i]));
        }
        INFO("Block size " << bs << " maxDiff vs 128 = " << maxDiff);
        REQUIRE(maxDiff < 1e-12);
    }

    // Benchmark 2.0 seconds of dense 10-voice organ + Marimba + Vibrato + Spring Reverb at 48 kHz
    KE310Engine benchEng;
    benchEng.prepare(48000.0);
    KE310State bst;
    bst.upperTibia16 = true;
    bst.upperTibia8 = true;
    bst.upperTibia4 = true;
    bst.upperString8 = true;
    bst.lowerDiapason8 = true;
    bst.lowerHorn8 = true;
    bst.pedalBourdon16 = true;
    bst.pedalStringBass = true;
    bst.presetMarimba = true;
    bst.vibrato = true;
    bst.reverb = 0.40;
    benchEng.applyState(bst);

    for (int m : {60, 64, 67, 71, 74})
    {
        benchEng.noteOn(Manual::Upper, m);
    }
    for (int m : {48, 52, 55, 59})
    {
        benchEng.noteOn(Manual::Lower, m);
    }
    benchEng.noteOn(Manual::Pedal, 36);

    std::array<float, 128> mOut{}, rOut{}, sOut{};
    for (int w = 0; w < 16; ++w)
    {
        benchEng.processBlock(128, mOut.data(), rOut.data(), sOut.data());
    }
    constexpr int kBlocks = (48000 * 2) / 128; // 750 blocks = 2.0s audio
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < kBlocks; ++b)
    {
        benchEng.processBlock(128, mOut.data(), rOut.data(), sOut.data());
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double elapsedS = std::chrono::duration<double>(t1 - t0).count();
    const double rtf = elapsedS / 2.0;
    const double speedup = 2.0 / std::max(elapsedS, 1e-9);
    std::printf("[PERF] 10-voice dense KE-310 + Marimba + Vibrato + Reverb (2.0s @ 48kHz): %.4fs elapsed (RTF=%.4f, %.1fx realtime)\n",
                elapsedS, rtf, speedup);
    REQUIRE(rtf < 0.25);
}

TEST_CASE("11. Expression pedal loudness compensation, smooth automation & Knee Sustainer", "[dsp][effects]")
{
    // 1) At expression = 1.0, loudness compensation is 100% bypassed (bit-exact parity with false)
    KE310Engine eTrue, eFalse;
    eTrue.prepare(48000.0);
    eFalse.prepare(48000.0);
    KE310State st;
    st.upperTibia8 = false;
    st.upperString8 = true;
    st.expression = 1.0;
    st.expressionLoudnessComp = true;
    eTrue.applyState(st);
    st.expressionLoudnessComp = false;
    eFalse.applyState(st);

    const auto outT = renderEvents(eTrue, {{0.02, true, Manual::Upper, 60}, {0.40, false, Manual::Upper, 60}}, 0.50);
    const auto outF = renderEvents(eFalse, {{0.02, true, Manual::Upper, 60}, {0.40, false, Manual::Upper, 60}}, 0.50);
    double maxDiff = 0.0;
    for (std::size_t i = 0; i < outT.mix.size(); ++i)
    {
        maxDiff = std::max(maxDiff, std::abs(outT.mix[i] - outF.mix[i]));
    }
    REQUIRE(maxDiff == 0.0);

    // 2) At expression = 0.25, loudness compensation preserves bass/warmth (> +1.5 dB H1-to-H5 relative lift)
    eTrue.reset();
    eFalse.reset();
    st.expression = 0.25;
    st.expressionLoudnessComp = true;
    eTrue.applyState(st);
    st.expressionLoudnessComp = false;
    eFalse.applyState(st);

    const auto quietT = renderEvents(eTrue, {{0.02, true, Manual::Upper, 60}, {0.50, false, Manual::Upper, 60}}, 0.55);
    const auto quietF = renderEvents(eFalse, {{0.02, true, Manual::Upper, 60}, {0.50, false, Manual::Upper, 60}}, 0.55);
    const double f0 = eTrue.tos().noteFreq(60);
    const double tiltT = goertzelDb(quietT.mix.data() + 9600, 9600, 48000.0, f0)
                       - goertzelDb(quietT.mix.data() + 9600, 9600, 48000.0, 5.0 * f0);
    const double tiltF = goertzelDb(quietF.mix.data() + 9600, 9600, 48000.0, f0)
                       - goertzelDb(quietF.mix.data() + 9600, 9600, 48000.0, 5.0 * f0);
    REQUIRE(tiltT - tiltF > 1.5);

    // 3) Knee Sustainer extends Upper 8'/4' release identically to Upper Sustain (Manual P.21)
    KE310Engine eKnee, eUpper;
    eKnee.prepare(48000.0);
    eUpper.prepare(48000.0);
    KE310State stSust;
    stSust.upperTibia8 = true;
    stSust.upperSustainTime = 0.50;
    stSust.kneeSustain = true;
    stSust.upperSustain = false;
    eKnee.applyState(stSust);
    stSust.kneeSustain = false;
    stSust.upperSustain = true;
    eUpper.applyState(stSust);

    const auto outKnee = renderEvents(eKnee, {{0.02, true, Manual::Upper, 60}, {0.15, false, Manual::Upper, 60}}, 0.60);
    const auto outUp = renderEvents(eUpper, {{0.02, true, Manual::Upper, 60}, {0.15, false, Manual::Upper, 60}}, 0.60);
    double maxSustDiff = 0.0;
    for (std::size_t i = 0; i < outKnee.mix.size(); ++i)
    {
        maxSustDiff = std::max(maxSustDiff, std::abs(outKnee.mix[i] - outUp.mix[i]));
    }
    REQUIRE(maxSustDiff == 0.0);
    REQUIRE(computeRms(outKnee.mix.data() + 20000, 4000) > 0.002);

    // 4) Dense multi-manual chord scales linearly with totalVolume and stays below 0 dBFS (< 1.0 peak)
    auto renderFullChord = [](double totalVol)
    {
        KE310Engine eng;
        eng.prepare(48000.0);
        KE310State chordSt;
        chordSt.upperTibia16 = true;
        chordSt.upperTibia8 = true;
        chordSt.upperTibia4 = true;
        chordSt.upperDiapason8 = true;
        chordSt.lowerTibia8 = true;
        chordSt.lowerTibia4 = true;
        chordSt.lowerDiapason8 = true;
        chordSt.pedalBourdon16 = true;
        chordSt.pedalMajorFlute8 = true;
        chordSt.reverb = 0.25;
        chordSt.totalVolume = totalVol;
        eng.applyState(chordSt);

        for (int n : {60, 64, 67, 72})
        {
            eng.noteOn(Manual::Upper, n);
        }
        for (int n : {48, 52, 55})
        {
            eng.noteOn(Manual::Lower, n);
        }
        eng.noteOn(Manual::Pedal, 36);

        RenderedBuses res;
        constexpr int kN = 12000;
        res.mix.resize(kN);
        res.rotor.resize(kN);
        res.stationary.resize(kN);
        eng.processBlock(kN, res.mix.data(), res.rotor.data(), res.stationary.data());
        return res;
    };

    const auto loudChord = renderFullChord(0.85);
    const auto quietChord = renderFullChord(0.20);
    double peakLoud = 0.0;
    double maxLinErr = 0.0;
    constexpr double kVolRatio = 0.85 / 0.20;
    for (std::size_t i = 0; i < loudChord.mix.size(); ++i)
    {
        peakLoud = std::max(peakLoud, std::abs(loudChord.mix[i]));
        maxLinErr = std::max(maxLinErr, std::abs(loudChord.mix[i] - quietChord.mix[i] * kVolRatio));
    }
    REQUIRE(peakLoud > 0.10);
    REQUIRE(peakLoud < 1.0);
    REQUIRE(maxLinErr < 1e-12);
}

TEST_CASE("12. Upper Preset circuit authenticity (clickless repeat, Vibrato cancel, combined decay, Upper Sustain)", "[dsp][presets]")
{
    constexpr double fs = 48000.0;

    // 1) Banjo & Mandolin rapid staccato / release clicklessness (continuous RC pulse)
    for (bool isMandolin : {false, true})
    {
        KE310Engine eng;
        eng.prepare(fs);
        KE310State st;
        st.upperTibia8 = false;
        st.organSilent = true;
        st.presetBanjo = !isMandolin;
        st.presetMandolin = isMandolin;
        st.repeatRate = 0.55; // ~9.05 Hz -> period ~110.5 ms
        st.presetVolume = 1.0;
        st.totalVolume = 0.85;
        eng.applyState(st);

        // Trigger note and release just after a pulse peak (~0.115s after noteOn)
        std::vector<TimedEvent> evs;
        for (int k = 0; k < 4; ++k)
        {
            const double t0 = 0.01 + 0.20 * static_cast<double>(k);
            evs.push_back({t0, true, Manual::Upper, 60 + k});
            evs.push_back({t0 + 0.115, false, Manual::Upper, 60 + k});
        }
        const auto res = renderEvents(eng, evs, 0.95);

        const std::size_t sOff = static_cast<std::size_t>((0.01 + 0.115) * fs);
        const std::size_t win2ms = static_cast<std::size_t>(0.002 * fs);
        const double preRms = computeRms(res.mix.data() + sOff - win2ms, win2ms);
        const double postRms = computeRms(res.mix.data() + sOff, win2ms);
        REQUIRE(preRms > 0.002);
        // Continuous RC decays smoothly instead of instantaneous zero-drop click
        REQUIRE(postRms > 0.60 * preRms);

        // Transition-local discontinuity right across noteOn/noteOff (+/- 3 samples) stays tiny (no step click)
        for (const auto& ev : evs)
        {
            const std::size_t idx = static_cast<std::size_t>(ev.timeS * fs);
            const std::size_t lo = (idx > 3) ? (idx - 3) : 1;
            const std::size_t hi = std::min(res.mix.size(), idx + 4);
            double boundaryJump = 0.0;
            for (std::size_t j = lo; j < hi; ++j)
            {
                boundaryJump = std::max(boundaryJump, std::abs(res.mix[j] - res.mix[j - 1]));
            }
            REQUIRE(boundaryJump < 0.035);
        }

        // Also compare single noteOff against continuous held note to verify zero cutoff impulse
        KE310Engine eHeld;
        eHeld.prepare(fs);
        eHeld.applyState(st);
        const auto resHeld = renderEvents(eHeld, {{0.01, true, Manual::Upper, 60}}, 0.20);
        double maxCutoffStep = 0.0;
        for (std::size_t j = sOff; j < sOff + 8; ++j)
        {
            const double dPrev = res.mix[j - 1] - resHeld.mix[j - 1];
            const double dCurr = res.mix[j] - resHeld.mix[j];
            maxCutoffStep = std::max(maxCutoffStep, std::abs(dCurr - dPrev));
        }
        REQUIRE(maxCutoffStep < 0.001);
    }

    // 2) PIANO / HARPSICHORD forcibly cancel VIBRATO and DELAY VIBRATO, and can layer together
    for (bool useHarpsi : {false, true})
    {
        KE310Engine eVib, eNoVib;
        eVib.prepare(fs);
        eNoVib.prepare(fs);
        KE310State st;
        st.upperTibia8 = true;
        st.presetPiano = !useHarpsi;
        st.presetHarpsichord = useHarpsi;
        st.vibrato = true;
        st.delayVibrato = true;
        eVib.applyState(st);
        st.vibrato = false;
        st.delayVibrato = false;
        eNoVib.applyState(st);

        const auto outVib = renderEvents(eVib, {{0.02, true, Manual::Upper, 60}, {0.95, false, Manual::Upper, 60}}, 1.05);
        const auto outNoVib = renderEvents(eNoVib, {{0.02, true, Manual::Upper, 60}, {0.95, false, Manual::Upper, 60}}, 1.05);
        double maxDiff = 0.0;
        for (std::size_t i = 0; i < outVib.mix.size(); ++i)
        {
            maxDiff = std::max(maxDiff, std::abs(outVib.mix[i] - outNoVib.mix[i]));
        }
        REQUIRE(maxDiff == 0.0);
    }

    {
        KE310Engine eBoth, eHarpsiOnly;
        eBoth.prepare(fs);
        eHarpsiOnly.prepare(fs);
        KE310State st;
        st.upperTibia8 = false;
        st.organSilent = true;
        st.presetPiano = true;
        st.presetHarpsichord = true;
        eBoth.applyState(st);
        st.presetPiano = false;
        eHarpsiOnly.applyState(st);
        const auto outBoth = renderEvents(eBoth, {{0.02, true, Manual::Upper, 60}, {0.50, false, Manual::Upper, 60}}, 0.55);
        const auto outH = renderEvents(eHarpsiOnly, {{0.02, true, Manual::Upper, 60}, {0.50, false, Manual::Upper, 60}}, 0.55);
        std::vector<double> diff(outBoth.mix.size());
        for (std::size_t i = 0; i < diff.size(); ++i)
        {
            diff[i] = outBoth.mix[i] - outH.mix[i];
        }
        REQUIRE(computeRms(diff.data() + 960, 4800) > 0.01);
    }

    // 3) Combining WOW TRUMPET / BANJO / MANDOLIN / MARIMBA with PIANO decays cleanly (zero stuck drone)
    for (int presetKind = 0; presetKind < 4; ++presetKind)
    {
        KE310Engine eng;
        eng.prepare(fs);
        KE310State st;
        st.upperTibia8 = false;
        st.organSilent = true;
        st.presetPiano = true;
        st.presetWowTrumpet = (presetKind == 0);
        st.presetBanjo = (presetKind == 1);
        st.presetMandolin = (presetKind == 2);
        st.presetMarimba = (presetKind == 3);
        eng.applyState(st);

        const auto res = renderEvents(eng, {{0.02, true, Manual::Upper, 60}}, 2.60);
        const double earlyRms = computeRms(res.mix.data() + static_cast<std::size_t>(0.02 * fs),
                                           static_cast<std::size_t>(0.25 * fs));
        const double lateRms = computeRms(res.mix.data() + static_cast<std::size_t>(2.15 * fs),
                                          static_cast<std::size_t>(0.40 * fs));
        REQUIRE(earlyRms > 0.01);
        REQUIRE(lateRms < 0.015 * earlyRms);
    }

    // 4) UPPER SUSTAIN on non-PERCUS Upper Presets, PERCUS immunity, and Piano early-vs-late release
    {
        // 4a) MARIMBA (alone + Reg #28 "Surf"), WOW TRUMPET, BANJO, MANDOLIN all receive UPPER SUSTAIN
        for (int presetKind = 0; presetKind < 4; ++presetKind)
        {
            auto renderSust = [&](bool sustainOn)
            {
                KE310Engine eng;
                eng.prepare(fs);
                KE310State st;
                st.upperTibia8 = false;
                st.organSilent = true;
                st.presetMarimba = (presetKind == 0);
                st.presetWowTrumpet = (presetKind == 1);
                st.presetBanjo = (presetKind == 2);
                st.presetMandolin = (presetKind == 3);
                st.upperSustain = sustainOn;
                st.upperSustainTime = 0.85;
                eng.applyState(st);
                return renderEvents(eng, {{0.02, true, Manual::Upper, 60}, {0.15, false, Manual::Upper, 60}}, 0.90);
            };
            const auto outSust = renderSust(true);
            const auto outDry = renderSust(false);
            const double tailSust = computeRms(outSust.mix.data() + static_cast<std::size_t>(0.35 * fs),
                                               static_cast<std::size_t>(0.40 * fs));
            const double tailDry = computeRms(outDry.mix.data() + static_cast<std::size_t>(0.35 * fs),
                                              static_cast<std::size_t>(0.40 * fs));
            REQUIRE(tailSust > 0.005);
            REQUIRE(tailSust > 20.0 * std::max(tailDry, 1e-9));

            if (presetKind == 0)
            {
                // Verify MARIMBA Dual-VCA continues pulsing during sustain tail (enabling Reg #28 "Surf")
                double minWin = 1e9;
                double maxWin = 0.0;
                const std::size_t win20ms = static_cast<std::size_t>(0.020 * fs);
                for (std::size_t s = static_cast<std::size_t>(0.22 * fs);
                     s + win20ms <= static_cast<std::size_t>(0.55 * fs);
                     s += static_cast<std::size_t>(0.010 * fs))
                {
                    const double r = computeRms(outSust.mix.data() + s, win20ms);
                    minWin = std::min(minWin, r);
                    maxWin = std::max(maxWin, r);
                }
                REQUIRE(maxWin > 1.35 * std::max(minWin, 1e-9));
            }
        }

        // Verify Reg #28 "Surf" (TIBIA 8' + MARIMBA + UPPER SUSTAIN) layers both voices in sustain tail
        {
            KE310Engine eSurf, eTibiaOnly;
            eSurf.prepare(fs);
            eTibiaOnly.prepare(fs);
            KE310State stSurf;
            stSurf.upperTibia8 = true;
            stSurf.presetMarimba = true;
            stSurf.upperSustain = true;
            stSurf.upperSustainTime = 0.85;
            eSurf.applyState(stSurf);
            stSurf.presetMarimba = false;
            eTibiaOnly.applyState(stSurf);
            const auto outSurf = renderEvents(eSurf, {{0.02, true, Manual::Upper, 60}, {0.15, false, Manual::Upper, 60}}, 0.90);
            const auto outTib = renderEvents(eTibiaOnly, {{0.02, true, Manual::Upper, 60}, {0.15, false, Manual::Upper, 60}}, 0.90);
            std::vector<double> marimbaDiff(outSurf.mix.size());
            for (std::size_t i = 0; i < marimbaDiff.size(); ++i)
            {
                marimbaDiff[i] = outSurf.mix[i] - outTib.mix[i];
            }
            REQUIRE(computeRms(marimbaDiff.data() + static_cast<std::size_t>(0.35 * fs),
                               static_cast<std::size_t>(0.40 * fs)) > 0.008);
        }

        // 4b) PERCUS 2 2/3' ignores UPPER SUSTAIN
        KE310Engine ePSust, ePDry;
        ePSust.prepare(fs);
        ePDry.prepare(fs);
        KE310State stP;
        stP.upperTibia8 = false;
        stP.organSilent = true;
        stP.presetPerc2_23 = true;
        stP.upperSustainTime = 0.90;
        stP.upperSustain = true;
        ePSust.applyState(stP);
        stP.upperSustain = false;
        ePDry.applyState(stP);
        const auto oPS = renderEvents(ePSust, {{0.02, true, Manual::Upper, 60}, {0.08, false, Manual::Upper, 60}}, 0.45);
        const auto oPD = renderEvents(ePDry, {{0.02, true, Manual::Upper, 60}, {0.08, false, Manual::Upper, 60}}, 0.45);
        double maxPercDiff = 0.0;
        for (std::size_t i = 0; i < oPS.mix.size(); ++i)
        {
            maxPercDiff = std::max(maxPercDiff, std::abs(oPS.mix[i] - oPD.mix[i]));
        }
        REQUIRE(maxPercDiff == 0.0);

        // 4c) PIANO with UPPER SUSTAIN: early release sustains strongly, late release has no sustain tail
        for (bool useHarpsi : {false, true})
        {
            KE310Engine eEarly, eEarlyDry, eLate;
            eEarly.prepare(fs);
            eEarlyDry.prepare(fs);
            eLate.prepare(fs);
            KE310State stPH;
            stPH.upperTibia8 = false;
            stPH.organSilent = true;
            stPH.presetPiano = !useHarpsi;
            stPH.presetHarpsichord = useHarpsi;
            stPH.upperSustainTime = 0.85;
            stPH.upperSustain = true;
            eEarly.applyState(stPH);
            eLate.applyState(stPH);
            stPH.upperSustain = false;
            eEarlyDry.applyState(stPH);

            const auto rEarly = renderEvents(eEarly, {{0.02, true, Manual::Upper, 60}, {0.07, false, Manual::Upper, 60}}, 1.00);
            const auto rEarlyDry = renderEvents(eEarlyDry, {{0.02, true, Manual::Upper, 60}, {0.07, false, Manual::Upper, 60}}, 1.00);
            const auto rLate = renderEvents(eLate, {{0.02, true, Manual::Upper, 60}, {2.80, false, Manual::Upper, 60}}, 3.30);

            const double atkRms = computeRms(rEarly.mix.data() + static_cast<std::size_t>(0.02 * fs),
                                             static_cast<std::size_t>(0.05 * fs));
            const double earlyTailSust = computeRms(rEarly.mix.data() + static_cast<std::size_t>(0.25 * fs),
                                                    static_cast<std::size_t>(0.40 * fs));
            const double earlyTailDry = computeRms(rEarlyDry.mix.data() + static_cast<std::size_t>(0.25 * fs),
                                                   static_cast<std::size_t>(0.40 * fs));
            const double lateTail = computeRms(rLate.mix.data() + static_cast<std::size_t>(2.85 * fs),
                                               static_cast<std::size_t>(0.40 * fs));
            REQUIRE(earlyTailSust > 0.25 * atkRms);
            REQUIRE(earlyTailSust > 10.0 * std::max(earlyTailDry, 1e-9));
            REQUIRE(lateTail < 0.005 * atkRms);
        }
    }
}

TEST_CASE("13. Hornless RotaryDrumSpeaker inertia, Doppler/baffle stereo modulation, custom speeds & dry bypass", "[dsp][rotary]")
{
    static constexpr double kFs = 48000.0;
    static constexpr int kBlock = 128;

    auto renderSilentBlocks = [](RotaryDrumSpeaker& spk, double seconds, double fs = 48000.0)
    {
        const int total = static_cast<int>(std::llround(seconds * fs));
        std::array<double, kBlock> in{}, outL{}, outR{};
        int pos = 0;
        while (pos < total)
        {
            const int n = std::min(kBlock, total - pos);
            spk.processBlock(n, in.data(), outL.data(), outR.data());
            pos += n;
        }
    };

    // 1) Bit-exact dry bypass at rest (CHORUS OFF, TREMOLO OFF)
    {
        RotaryDrumSpeaker spk;
        spk.prepare(kFs);
        REQUIRE(spk.isBypassed());

        std::vector<double> in(1024), outL(1024), outR(1024);
        for (int i = 0; i < 1024; ++i)
        {
            in[static_cast<std::size_t>(i)] = 0.5 * std::sin(2.0 * std::numbers::pi * 440.0 * i / kFs);
        }
        spk.processBlock(1024, in.data(), outL.data(), outR.data());
        for (int i = 0; i < 1024; ++i)
        {
            REQUIRE(outL[static_cast<std::size_t>(i)] == in[static_cast<std::size_t>(i)]);
            REQUIRE(outR[static_cast<std::size_t>(i)] == in[static_cast<std::size_t>(i)]);
        }
        REQUIRE(spk.isBypassed());
    }

    // 2) Smooth acceleration (0 -> Slow -> Fast) and slower coast-down (Fast -> Slow -> Brake 0 Hz)
    {
        RotaryDrumSpeaker spk;
        spk.prepare(kFs);
        RotaryDrumConfig cfg;
        cfg.chorus = true;
        cfg.tremolo = false;
        spk.setConfig(cfg);

        // Engage CHORUS: at 0.5 s mid-ramp, at 4.0 s settled near 0.80 Hz
        renderSilentBlocks(spk, 0.5);
        REQUIRE(spk.currentSpeedHz() > 0.25);
        REQUIRE(spk.currentSpeedHz() < 0.65);
        renderSilentBlocks(spk, 3.5);
        REQUIRE(spk.currentSpeedHz() == Catch::Approx(0.80).margin(0.03));

        // Engage TREMOLO (with CHORUS still ON -> TREMOLO overrides): smooth spin-up to 6.00 Hz
        cfg.tremolo = true;
        spk.setConfig(cfg);
        renderSilentBlocks(spk, 0.5);
        REQUIRE(spk.currentSpeedHz() > 1.5);
        REQUIRE(spk.currentSpeedHz() < 4.5);
        renderSilentBlocks(spk, 4.5);
        const double fastSettled = spk.currentSpeedHz();
        REQUIRE(fastSettled == Catch::Approx(6.00).margin(0.05));

        // Compare acceleration rate (1.0 s from slow->fast) vs coast deceleration rate (1.0 s fast->slow)
        cfg.tremolo = false;
        spk.setConfig(cfg);
        renderSilentBlocks(spk, 1.0);
        const double speedAfter1sDecel = spk.currentSpeedHz();
        const double decelFraction = (fastSettled - speedAfter1sDecel) / (fastSettled - 0.80);
        REQUIRE(decelFraction > 0.30);
        REQUIRE(decelFraction < 0.55); // noticeably slower than accel (1 - exp(-1/0.9) ≈ 0.67)

        renderSilentBlocks(spk, 10.0);
        REQUIRE(spk.currentSpeedHz() == Catch::Approx(0.80).margin(0.05));

        // Brake to rest (both switches OFF): audible spin-down first, then bit-exact bypass
        cfg.chorus = false;
        spk.setConfig(cfg);
        renderSilentBlocks(spk, 0.5);
        REQUIRE(spk.currentSpeedHz() > 0.35);
        REQUIRE(spk.currentWetMix() > 0.99);
        renderSilentBlocks(spk, 8.0);
        REQUIRE(spk.currentSpeedHz() == 0.0);
        REQUIRE(spk.isBypassed());

        std::array<double, 256> dryIn{}, dryL{}, dryR{};
        for (int i = 0; i < 256; ++i)
        {
            dryIn[static_cast<std::size_t>(i)] = 0.4 * std::sin(2.0 * std::numbers::pi * 330.0 * i / kFs);
        }
        spk.processBlock(256, dryIn.data(), dryL.data(), dryR.data());
        for (int i = 0; i < 256; ++i)
        {
            REQUIRE(dryL[static_cast<std::size_t>(i)] == dryIn[static_cast<std::size_t>(i)]);
            REQUIRE(dryR[static_cast<std::size_t>(i)] == dryIn[static_cast<std::size_t>(i)]);
        }
    }

    // 3) User-adjustable Slow & Fast speeds
    {
        RotaryDrumSpeaker spk;
        spk.prepare(kFs);
        RotaryDrumConfig cfg;
        cfg.slowHz = 1.40;
        cfg.fastHz = 7.80;
        cfg.chorus = true;
        spk.setConfig(cfg);
        renderSilentBlocks(spk, 6.0);
        REQUIRE(spk.currentSpeedHz() == Catch::Approx(1.40).margin(0.05));

        cfg.tremolo = true;
        spk.setConfig(cfg);
        renderSilentBlocks(spk, 6.0);
        REQUIRE(spk.currentSpeedHz() == Catch::Approx(7.80).margin(0.08));
    }

    // 4) Stereo L/R decorrelation, AM/baffle modulation, linear headroom & multirate stability
    for (double fs : {44100.0, 48000.0, 88200.0, 96000.0})
    {
        RotaryDrumSpeaker spk;
        spk.prepare(fs);
        RotaryDrumConfig cfg;
        cfg.tremolo = true;
        cfg.fastHz = 6.00;
        spk.setConfig(cfg);

        // Warm up motor to steady-state fast rotation
        renderSilentBlocks(spk, 5.5, fs);

        const int total = static_cast<int>(std::llround(1.0 * fs));
        std::vector<double> in(static_cast<std::size_t>(total));
        std::vector<double> outL(static_cast<std::size_t>(total));
        std::vector<double> outR(static_cast<std::size_t>(total));
        for (int i = 0; i < total; ++i)
        {
            in[static_cast<std::size_t>(i)] = 0.5 * std::sin(2.0 * std::numbers::pi * 1000.0 * i / fs);
        }

        int pos = 0;
        while (pos < total)
        {
            const int n = std::min(kBlock, total - pos);
            spk.processBlock(n, in.data() + pos, outL.data() + pos, outR.data() + pos);
            pos += n;
        }

        double peak = 0.0;
        double diffSq = 0.0;
        for (int i = 0; i < total; ++i)
        {
            const double l = outL[static_cast<std::size_t>(i)];
            const double r = outR[static_cast<std::size_t>(i)];
            REQUIRE(std::isfinite(l));
            REQUIRE(std::isfinite(r));
            peak = std::max({peak, std::abs(l), std::abs(r)});
            const double d = l - r;
            diffSq += d * d;
        }
        const double rmsL = computeRms(outL.data(), total);
        const double rmsDiff = std::sqrt(diffSq / static_cast<double>(total));

        INFO("Sample rate = " << fs << " peak=" << peak << " rmsL=" << rmsL << " rmsDiff=" << rmsDiff);
        REQUIRE(peak < 0.70);
        REQUIRE(rmsL > 0.22);
        REQUIRE(rmsL < 0.45);
        REQUIRE(rmsDiff > 0.05 * rmsL);

        // Verify periodic amplitude/baffle modulation over one second of 20 ms windows
        const int win = static_cast<int>(0.020 * fs);
        const int hop = static_cast<int>(0.010 * fs);
        double minWin = 1e9;
        double maxWin = 0.0;
        for (int s = win; s + win <= total; s += hop)
        {
            const double wRms = computeRms(outL.data() + s, win);
            minWin = std::min(minWin, wRms);
            maxWin = std::max(maxWin, wRms);
        }
        REQUIRE(maxWin > 1.35 * std::max(minWin, 1e-9));
    }
}

TEST_CASE("14. Upper String 16' bypasses Upper Sustain & clickless live tone-tab switching", "[dsp][sustain][clickless]")
{
    constexpr double fs = 48000.0;

    // 1) UPPER STRING 16' ignores UPPER SUSTAIN & KNEE SUSTAIN (matches UPPER TIBIA 16')
    {
        KE310Engine eSust, eKnee, eDry, eStr8;
        eSust.prepare(fs);
        eKnee.prepare(fs);
        eDry.prepare(fs);
        eStr8.prepare(fs);

        KE310State st;
        st.upperTibia8 = false;
        st.upperString16 = true;
        st.upperSustainTime = 0.85;
        st.upperSustain = false;
        st.kneeSustain = false;
        eDry.applyState(st);

        st.upperSustain = true;
        eSust.applyState(st);

        st.upperSustain = false;
        st.kneeSustain = true;
        eKnee.applyState(st);

        KE310State st8;
        st8.upperTibia8 = false;
        st8.upperString8 = true;
        st8.upperSustain = true;
        st8.upperSustainTime = 0.85;
        eStr8.applyState(st8);

        const auto outDry = renderEvents(eDry, {{0.02, true, Manual::Upper, 60}, {0.15, false, Manual::Upper, 60}}, 0.60);
        const auto outSust = renderEvents(eSust, {{0.02, true, Manual::Upper, 60}, {0.15, false, Manual::Upper, 60}}, 0.60);
        const auto outKnee = renderEvents(eKnee, {{0.02, true, Manual::Upper, 60}, {0.15, false, Manual::Upper, 60}}, 0.60);
        const auto outStr8 = renderEvents(eStr8, {{0.02, true, Manual::Upper, 60}, {0.15, false, Manual::Upper, 60}}, 0.60);

        double maxSustDiff = 0.0;
        double maxKneeDiff = 0.0;
        for (std::size_t i = 0; i < outDry.mix.size(); ++i)
        {
            maxSustDiff = std::max(maxSustDiff, std::abs(outSust.mix[i] - outDry.mix[i]));
            maxKneeDiff = std::max(maxKneeDiff, std::abs(outKnee.mix[i] - outDry.mix[i]));
        }
        INFO("maxSustDiff=" << maxSustDiff << " maxKneeDiff=" << maxKneeDiff);
        REQUIRE(maxSustDiff < 1e-12);
        REQUIRE(maxKneeDiff < 1e-12);

        const double tail16 = computeRms(outSust.mix.data() + static_cast<std::size_t>(0.25 * fs),
                                         static_cast<std::size_t>(0.20 * fs));
        const double tail8 = computeRms(outStr8.mix.data() + static_cast<std::size_t>(0.25 * fs),
                                        static_cast<std::size_t>(0.20 * fs));
        REQUIRE(tail16 < 1e-5);
        REQUIRE(tail8 > 0.005);

        // Also stays sustained during key-hold under Piano takeover (matching UPPER TIBIA 16')
        KE310Engine ePiano16;
        ePiano16.prepare(fs);
        KE310State stP16;
        stP16.upperTibia8 = false;
        stP16.upperString16 = true;
        stP16.presetPiano = true;
        stP16.presetVolume = 0.0;
        ePiano16.applyState(stP16);
        const auto outP16 = renderEvents(ePiano16, {{0.05, true, Manual::Upper, 60}, {2.20, false, Manual::Upper, 60}}, 2.30);
        const double rStart = computeRms(outP16.mix.data() + static_cast<std::size_t>(0.08 * fs), static_cast<std::size_t>(0.12 * fs));
        const double rLate = computeRms(outP16.mix.data() + static_cast<std::size_t>(1.60 * fs), static_cast<std::size_t>(0.30 * fs));
        REQUIRE(rStart > 0.005);
        REQUIRE(rLate > rStart * 0.85);
    }

    // Helper: measure max sample-to-sample jump around a switch sample index vs steady-state
    auto maxAbsDelta = [](const std::vector<double>& sig, int startIdx, int endIdx) -> double
    {
        const int lo = std::max(1, startIdx);
        const int hi = std::min(static_cast<int>(sig.size()), endIdx);
        double mx = 0.0;
        for (int i = lo; i < hi; ++i)
        {
            mx = std::max(mx, std::abs(sig[static_cast<std::size_t>(i)] - sig[static_cast<std::size_t>(i - 1)]));
        }
        return mx;
    };

    auto renderLiveToggle = [&](const KE310State& initialState,
                                const KE310State& toggledOffState,
                                const KE310State& toggledOnState,
                                Manual manual,
                                int midiNote,
                                std::vector<double>& outMix,
                                int& offSample,
                                int& onSample)
    {
        KE310Engine eng;
        eng.prepare(fs);
        eng.applyState(initialState);
        eng.noteOn(manual, midiNote);

        constexpr int kWarmupSamples = 4800; // 100 ms steady hold
        constexpr int kOffSamples = 2400;    // 50 ms off (well past 6 ms ramp)
        constexpr int kOnSamples = 2400;     // 50 ms back on
        constexpr int kBlock = 64;
        const int total = kWarmupSamples + kOffSamples + kOnSamples;
        outMix.assign(static_cast<std::size_t>(total), 0.0);
        std::vector<double> r(static_cast<std::size_t>(kBlock)), s(static_cast<std::size_t>(kBlock));

        offSample = kWarmupSamples;
        onSample = kWarmupSamples + kOffSamples;

        int pos = 0;
        while (pos < total)
        {
            if (pos == offSample)
            {
                eng.applyState(toggledOffState);
            }
            else if (pos == onSample)
            {
                eng.applyState(toggledOnState);
            }
            const int nextStop = (pos < offSample) ? offSample : ((pos < onSample) ? onSample : total);
            const int n = std::min(kBlock, nextStop - pos);
            eng.processBlock(n, outMix.data() + pos, r.data(), s.data());
            pos += n;
        }
    };

    // 2) Live toggling of Upper String 8' while note is held: smooth fade-out, silence, clickless fade-in
    {
        KE310State sOn, sOff;
        sOn.upperTibia8 = false;
        sOn.upperString8 = true;
        sOff = sOn;
        sOff.upperString8 = false;

        std::vector<double> sig;
        int offSample = 0, onSample = 0;
        renderLiveToggle(sOn, sOff, sOn, Manual::Upper, 60, sig, offSample, onSample);

        const double steadyDelta = maxAbsDelta(sig, offSample - 1024, offSample - 64);
        const double offEdgeDelta = maxAbsDelta(sig, offSample - 16, offSample + 16);
        const double onEdgeDelta = maxAbsDelta(sig, onSample - 16, onSample + 16);
        const double silentRms = computeRms(sig.data() + offSample + static_cast<int>(0.025 * fs),
                                            static_cast<int>(0.020 * fs));

        REQUIRE(steadyDelta > 1e-4);
        REQUIRE(silentRms < 1e-6);
        REQUIRE(offEdgeDelta <= steadyDelta * 1.25);
        REQUIRE(onEdgeDelta <= steadyDelta * 1.25);
    }

    // 3) Live toggling of Upper Tibia 4' alongside Tibia 8' (also exercises tibiaLoadGain smoothing)
    {
        KE310State sBoth, sOnly8;
        sBoth.upperTibia8 = true;
        sBoth.upperTibia4 = true;
        sOnly8.upperTibia8 = true;
        sOnly8.upperTibia4 = false;

        std::vector<double> sig;
        int offSample = 0, onSample = 0;
        renderLiveToggle(sBoth, sOnly8, sBoth, Manual::Upper, 60, sig, offSample, onSample);

        const double steadyDelta = maxAbsDelta(sig, offSample - 1024, offSample - 64);
        const double offEdgeDelta = maxAbsDelta(sig, offSample - 16, offSample + 16);
        const double onEdgeDelta = maxAbsDelta(sig, onSample - 16, onSample + 16);
        REQUIRE(offEdgeDelta <= steadyDelta * 1.25);
        REQUIRE(onEdgeDelta <= steadyDelta * 1.25);
    }

    // 4) Live toggling of WOW TRUMPET & MARIMBA presets mid-note (clears filter state & fades smoothly)
    {
        KE310State sOn, sOff;
        sOn.upperTibia8 = false;
        sOn.organSilent = true;
        sOn.presetWowTrumpet = true;
        sOn.presetMarimba = true;
        sOn.presetVolume = 1.0;
        sOff = sOn;
        sOff.presetWowTrumpet = false;
        sOff.presetMarimba = false;

        std::vector<double> sig;
        int offSample = 0, onSample = 0;
        renderLiveToggle(sOn, sOff, sOn, Manual::Upper, 60, sig, offSample, onSample);

        const double steadyDelta = maxAbsDelta(sig, offSample - 1024, offSample - 64);
        const double offEdgeDelta = maxAbsDelta(sig, offSample - 16, offSample + 16);
        const double onEdgeDelta = maxAbsDelta(sig, onSample - 16, onSample + 16);
        const double silentRms = computeRms(sig.data() + offSample + static_cast<int>(0.025 * fs),
                                            static_cast<int>(0.020 * fs));

        REQUIRE(steadyDelta > 1e-4);
        REQUIRE(silentRms < 1e-6);
        REQUIRE(offEdgeDelta <= steadyDelta * 1.25);
        REQUIRE(onEdgeDelta <= steadyDelta * 1.25);
    }

    // 5) Live toggling of ORGAN SILENT & Pedal Bourdon 16' mid-note
    {
        KE310State sOn, sOff;
        sOn.upperTibia8 = false;
        sOn.upperString8 = true;
        sOff = sOn;
        sOff.organSilent = true;

        std::vector<double> sig;
        int offSample = 0, onSample = 0;
        renderLiveToggle(sOn, sOff, sOn, Manual::Upper, 60, sig, offSample, onSample);

        const double steadyDelta = maxAbsDelta(sig, offSample - 1024, offSample - 64);
        const double silentRms = computeRms(sig.data() + offSample + static_cast<int>(0.025 * fs),
                                            static_cast<int>(0.020 * fs));
        REQUIRE(silentRms < 1e-6);
        REQUIRE(maxAbsDelta(sig, offSample - 16, offSample + 16) <= steadyDelta * 1.25);
        REQUIRE(maxAbsDelta(sig, onSample - 16, onSample + 16) <= steadyDelta * 1.25);
    }

    {
        KE310State sOn, sOff;
        sOn.upperTibia8 = false;
        sOn.pedalBourdon16 = true;
        sOn.pedalVolume = 1.0;
        sOff = sOn;
        sOff.pedalBourdon16 = false;

        std::vector<double> sig;
        int offSample = 0, onSample = 0;
        renderLiveToggle(sOn, sOff, sOn, Manual::Pedal, 36, sig, offSample, onSample);

        const double steadyDelta = maxAbsDelta(sig, offSample - 1024, offSample - 64);
        const double silentRms = computeRms(sig.data() + offSample + static_cast<int>(0.025 * fs),
                                            static_cast<int>(0.020 * fs));
        REQUIRE(steadyDelta > 1e-4);
        REQUIRE(silentRms < 1e-6);
        REQUIRE(maxAbsDelta(sig, offSample - 16, offSample + 16) <= steadyDelta * 1.25);
        REQUIRE(maxAbsDelta(sig, onSample - 16, onSample + 16) <= steadyDelta * 1.25);
    }
}

