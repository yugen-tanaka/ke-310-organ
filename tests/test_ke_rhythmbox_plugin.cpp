// Catch2 unit, AudioPlayHead sync, MIDI, and GUI tests for standalone KE Rhythmbox plugin.
#include "dsp/Biquad.hpp"
#include "plugin_rhythmbox/RhythmboxEditor.hpp"
#include "plugin_rhythmbox/RhythmboxPluginParams.hpp"
#include "plugin_rhythmbox/RhythmboxProcessor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace
{

using namespace ke310::rhythmbox;
using namespace ke310::rhythmbox_plugin;

void setParamUnnorm(juce::AudioProcessorValueTreeState& apvts, const char* id, float unnormalizedValue)
{
    auto* p = apvts.getParameter(id);
    REQUIRE(p != nullptr);
    p->setValueNotifyingHost(p->convertTo0to1(unnormalizedValue));
}

float bufferMaxAbs(const juce::AudioBuffer<float>& buf, int startSample = 0, int numSamples = -1) noexcept
{
    const int n = (numSamples < 0) ? (buf.getNumSamples() - startSample) : numSamples;
    float m = 0.0f;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const float* ptr = buf.getReadPointer(ch);
        for (int i = 0; i < n; ++i)
        {
            m = std::max(m, std::abs(ptr[startSample + i]));
        }
    }
    return m;
}

bool bufferAllFinite(const juce::AudioBuffer<float>& buf) noexcept
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const float* ptr = buf.getReadPointer(ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            if (!std::isfinite(ptr[i]))
            {
                return false;
            }
        }
    }
    return true;
}

class MockPlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying(playing);
        info.setBpm(bpm);
        info.setPpqPosition(ppq);
        return info;
    }

    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;
};

} // namespace

TEST_CASE("Rhythmbox Plugin 1: Main Stereo bus, default silence, 16 ROM patterns & Wired-OR", "[rb_plugin][buses]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    RhythmboxAudioProcessor proc;

    REQUIRE(proc.getName() == "KE Rhythmbox");
    REQUIRE(proc.getBusCount(true) == 0);
    REQUIRE(proc.getBusCount(false) == 1);
    REQUIRE(proc.getChannelCountOfBus(false, 0) == 2);
    REQUIRE(proc.acceptsMidi());
    REQUIRE_FALSE(proc.producesMidi());

    proc.prepareToPlay(48000.0, 512);
    auto& apvts = proc.apvts();

    // Default state: rb_enabled = false -> polite silence on load
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    proc.processBlock(buf, midi);
    REQUIRE(bufferMaxAbs(buf) == 0.0f);

    // Every one of the 16 ROM patterns produces finite stereo output when enabled
    setParamUnnorm(apvts, param_ids::rbSyncMode, 0.0f); // Internal
    setParamUnnorm(apvts, param_ids::rbEnabled, 1.0f);
    for (std::size_t p = 0; p < kNumPatterns; ++p)
    {
        for (std::size_t j = 0; j < kNumPatterns; ++j)
        {
            setParamUnnorm(apvts, param_ids::kPatternParamIds[j], (j == p) ? 1.0f : 0.0f);
        }
        proc.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> patBuf(2, 4096);
        proc.processBlock(patBuf, midi);
        REQUIRE(bufferAllFinite(patBuf));
        const float rmsL = patBuf.getRMSLevel(0, 0, 4096);
        const float rmsR = patBuf.getRMSLevel(1, 0, 4096);
        REQUIRE(rmsL > 1e-3f);
        REQUIRE(rmsL == Catch::Approx(rmsR).margin(1e-6f));
    }

    // Single mode keeps only lowest bit when multiple pattern toggles are set
    setParamUnnorm(apvts, param_ids::rbMultiPattern, 0.0f);
    for (std::size_t j = 0; j < kNumPatterns; ++j)
    {
        setParamUnnorm(apvts, param_ids::kPatternParamIds[j], (j == 0 || j == 11) ? 1.0f : 0.0f); // WALTZ | SAMBA
    }
    CachedRhythmboxParams cache;
    cache.bind(apvts);
    REQUIRE(cache.readState().patternMask == ((1u << 0u) | (1u << 11u)));
    setParamUnnorm(apvts, param_ids::rbMultiPattern, 1.0f);
    REQUIRE(cache.readState().multiPatternEnabled);
}

TEST_CASE("Rhythmbox Plugin 2: Sample-accurate GM MIDI Note-On & channel filtering", "[rb_plugin][midi]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    RhythmboxAudioProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto& apvts = proc.apvts();

    // Sequencer OFF: GM drum triggers still play cleanly!
    setParamUnnorm(apvts, param_ids::rbEnabled, 0.0f);
    setParamUnnorm(apvts, param_ids::rbVolume, 1.0f);
    setParamUnnorm(apvts, param_ids::rbMidiCh, 0.0f); // Omni

    // All 11 voices via primary GM drum notes + sample-offset verification at sample 200
    for (int note : kPrimaryGmMidiNotes)
    {
        proc.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(10, note, 1.0f), 200);
        proc.processBlock(buf, midi);
        REQUIRE(bufferAllFinite(buf));
        REQUIRE(bufferMaxAbs(buf, 0, 200) == 0.0f);
        REQUIRE(bufferMaxAbs(buf, 200, 312) > 5e-4f);
    }

    // Channel filtering: Ch 10 ignores Ch 1 and accepts Ch 10
    setParamUnnorm(apvts, param_ids::rbMidiCh, 10.0f); // Index 10 = "Ch 10"
    proc.prepareToPlay(48000.0, 512);
    {
        juce::AudioBuffer<float> bufReject(2, 512);
        juce::MidiBuffer midiCh1;
        midiCh1.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
        proc.processBlock(bufReject, midiCh1);
        REQUIRE(bufferMaxAbs(bufReject) == 0.0f);

        juce::AudioBuffer<float> bufAccept(2, 512);
        juce::MidiBuffer midiCh10;
        midiCh10.addEvent(juce::MidiMessage::noteOn(10, 36, 1.0f), 0);
        proc.processBlock(bufAccept, midiCh10);
        REQUIRE(bufferMaxAbs(bufAccept) > 0.02f);
    }
}

TEST_CASE("Rhythmbox Plugin 3: AudioPlayHead HostSync (PPQ/BPM/Loop) and TempoSync modes", "[rb_plugin][sync]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    RhythmboxAudioProcessor proc;
    MockPlayHead playHead;
    proc.setPlayHead(&playHead);
    proc.prepareToPlay(48000.0, 512);
    auto& apvts = proc.apvts();

    // Select MARCH (16th notes) & HostSync mode
    for (std::size_t j = 0; j < kNumPatterns; ++j)
    {
        setParamUnnorm(apvts, param_ids::kPatternParamIds[j], (j == static_cast<std::size_t>(PatternId::March)) ? 1.0f : 0.0f);
    }
    setParamUnnorm(apvts, param_ids::rbEnabled, 1.0f);
    setParamUnnorm(apvts, param_ids::rbSyncMode, 2.0f); // Host Sync
    setParamUnnorm(apvts, param_ids::rbVolume, 1.0f);

    // 1) Host stopped -> silent
    playHead.playing = false;
    playHead.bpm = 120.0;
    playHead.ppq = 0.0;
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    proc.processBlock(buf, midi);
    REQUIRE(bufferMaxAbs(buf) == 0.0f);
    REQUIRE_FALSE(proc.readTelemetry().lamp.isRunning);

    // 2) Host playing at 150 BPM -> beats/sec = 2.5, step duration = 0.1s (4800 samples)
    // Starting from PPQ = 0.0 should trigger Step 0 immediately and Step 1 near sample 4800
    playHead.playing = true;
    playHead.bpm = 150.0;
    playHead.ppq = 0.0;
    proc.prepareToPlay(48000.0, 512);
    proc.processBlock(buf, midi);
    REQUIRE(proc.readTelemetry().lamp.isRunning);
    REQUIRE(proc.readTelemetry().lamp.effectiveBpm == Catch::Approx(150.0).margin(0.05));
    REQUIRE(bufferMaxAbs(buf, 0, 256) > 0.02f);

    // 3) DAW Loop-back to PPQ = 0.0 after playing ahead triggers Beat 1 cleanly at sample 0
    playHead.ppq = 3.5;
    proc.processBlock(buf, midi);
    playHead.ppq = 0.0;
    proc.processBlock(buf, midi);
    REQUIRE(bufferMaxAbs(buf, 0, 256) > 0.02f);

    // 4) TempoSync (index 1) follows DAW BPM (142.0) even while DAW transport is paused
    setParamUnnorm(apvts, param_ids::rbSyncMode, 1.0f); // Host Tempo
    playHead.playing = false;
    playHead.bpm = 142.0;
    proc.processBlock(buf, midi);
    REQUIRE(proc.readTelemetry().lamp.isRunning);
    REQUIRE(proc.readTelemetry().lamp.effectiveBpm == Catch::Approx(142.0).margin(0.05));
}

TEST_CASE("Rhythmbox Plugin 4: TONE tilt, 16 factory programs & XML state save/restore", "[rb_plugin][state]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    RhythmboxAudioProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto& apvts = proc.apvts();

    REQUIRE(proc.getNumPrograms() == 16);
    for (int i = 0; i < 16; ++i)
    {
        REQUIRE(proc.getProgramName(i).isNotEmpty());
    }

    auto measureToneRatioDb = [&](float toneVal)
    {
        proc.prepareToPlay(48000.0, 512);
        proc.setCurrentProgram(static_cast<int>(PatternId::Rock1));
        setParamUnnorm(apvts, param_ids::rbSyncMode, 0.0f);
        setParamUnnorm(apvts, param_ids::rbEnabled, 1.0f);
        setParamUnnorm(apvts, param_ids::rbTone, toneVal);
        setParamUnnorm(apvts, param_ids::rbVolume, 1.0f);

        ke310::dsp::BiquadCascade<1> lp500;
        ke310::dsp::BiquadCascade<1> hp3000;
        const ke310::dsp::FilterSectionSpec lpS{ke310::dsp::FilterType::LP2, 500.0, 0.707, 0.0};
        const ke310::dsp::FilterSectionSpec hpS{ke310::dsp::FilterType::HP2, 3000.0, 0.707, 0.0};
        lp500.design(&lpS, 1u, 0.0, 48000.0);
        hp3000.design(&hpS, 1u, 0.0, 48000.0);

        double eLow = 0.0;
        double eHigh = 0.0;
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 94; ++b) // ~1.0s
        {
            proc.processBlock(buf, midi);
            const float* p = buf.getReadPointer(0);
            for (int i = 0; i < 512; ++i)
            {
                const double lo = lp500.processSample(p[i]);
                const double hi = hp3000.processSample(p[i]);
                eLow += lo * lo;
                eHigh += hi * hi;
            }
        }
        return 10.0 * std::log10((eLow + 1e-24) / (eHigh + 1e-24));
    };

    const double rDrum = measureToneRatioDb(0.0f);
    const double rFlat = measureToneRatioDb(0.5f);
    const double rCym = measureToneRatioDb(1.0f);
    REQUIRE(rDrum > rFlat + 4.0);
    REQUIRE(rCym < rFlat - 4.0);

    // Save custom Multi-Pattern state and restore after changing program
    proc.setCurrentProgram(static_cast<int>(PatternId::ChaCha));
    REQUIRE(proc.getCurrentProgram() == static_cast<int>(PatternId::ChaCha));
    setParamUnnorm(apvts, param_ids::rbMultiPattern, 1.0f);
    setParamUnnorm(apvts, param_ids::kPatternParamIds[static_cast<std::size_t>(PatternId::Rock1)], 1.0f);
    setParamUnnorm(apvts, param_ids::rbTone, 0.75f);
    setParamUnnorm(apvts, param_ids::rbBpm, 128.4f);

    juce::MemoryBlock stateBlob;
    proc.getStateInformation(stateBlob);
    REQUIRE(stateBlob.getSize() > 100);

    proc.setCurrentProgram(0);
    REQUIRE(apvts.getParameter(param_ids::rbMultiPattern)->getValue() < 0.5f);

    proc.setStateInformation(stateBlob.getData(), static_cast<int>(stateBlob.getSize()));
    REQUIRE(proc.getCurrentProgram() == static_cast<int>(PatternId::ChaCha));
    REQUIRE(apvts.getParameter(param_ids::rbMultiPattern)->getValue() > 0.5f);
    REQUIRE(apvts.getParameter(param_ids::kPatternParamIds[static_cast<std::size_t>(PatternId::ChaCha)])->getValue() > 0.5f);
    REQUIRE(apvts.getParameter(param_ids::kPatternParamIds[static_cast<std::size_t>(PatternId::Rock1)])->getValue() > 0.5f);
    const auto* bpmParam = apvts.getParameter(param_ids::rbBpm);
    REQUIRE(bpmParam->convertFrom0to1(bpmParam->getValue()) == Catch::Approx(128.4f).margin(0.15f));
}

TEST_CASE("Rhythmbox Plugin 5: Custom GUI Editor, audition pad triggers, resize & PNG export", "[rb_plugin][gui]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    RhythmboxAudioProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto& apvts = proc.apvts();

    // Activate BOSSA NOVA & START/STOP at beat 1 so Tempo LED, LED cap & meter are lit in screenshot
    proc.setCurrentProgram(static_cast<int>(PatternId::Bossanova));
    setParamUnnorm(apvts, param_ids::rbSyncMode, 0.0f); // Internal
    setParamUnnorm(apvts, param_ids::rbEnabled, 1.0f);
    setParamUnnorm(apvts, param_ids::rbBpm, 112.0f);
    setParamUnnorm(apvts, param_ids::rbVolume, 0.90f);

    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    proc.processBlock(buf, midi);
    REQUIRE(proc.readTelemetry().lamp.isRunning);
    REQUIRE(proc.readTelemetry().lamp.beatFlash);

    std::unique_ptr<juce::AudioProcessorEditor> rawEditor(proc.createEditorIfNeeded());
    REQUIRE(rawEditor != nullptr);
    auto* rbEditor = dynamic_cast<RhythmboxAudioProcessorEditor*>(rawEditor.get());
    REQUIRE(rbEditor != nullptr);
    REQUIRE(rbEditor->getWidth() == 780);
    REQUIRE(rbEditor->getHeight() == 460);

    // Audition pad callback triggers audio through lock-free queue on next processBlock
    setParamUnnorm(apvts, param_ids::rbEnabled, 0.0f);
    proc.prepareToPlay(48000.0, 512);
    proc.processBlock(buf, midi);
    REQUIRE(bufferMaxAbs(buf) == 0.0f);

    auto& bdPad = rbEditor->rhythmboxPanel().auditionPad(static_cast<std::size_t>(VoiceId::BassDrum));
    REQUIRE(bdPad.onClick != nullptr);
    bdPad.onClick();
    proc.processBlock(buf, midi);
    REQUIRE(bufferMaxAbs(buf) > 0.02f);

    // Re-enable running state on Beat 1 for clean visual snapshot
    setParamUnnorm(apvts, param_ids::rbEnabled, 1.0f);
    proc.processBlock(buf, midi);
    rawEditor.reset();
    rawEditor.reset(proc.createEditorIfNeeded());
    rbEditor = dynamic_cast<RhythmboxAudioProcessorEditor*>(rawEditor.get());
    REQUIRE(rbEditor != nullptr);

    juce::Image img(juce::Image::ARGB, rbEditor->getWidth(), rbEditor->getHeight(), true);
    {
        juce::Graphics g(img);
        rbEditor->paintEntireComponent(g, true);
    }
    REQUIRE(!img.isNull());
    REQUIRE(img.getPixelAt(30, 14) != img.getPixelAt(200, 180));

    // Export screenshot to research/rhythm/figures/04_ke_rhythmbox_gui.png when inside repo tree
    juce::File probeDir = juce::File::getCurrentWorkingDirectory();
    juce::File rhythmDir;
    for (int depth = 0; depth < 4; ++depth)
    {
        auto candidate = probeDir.getChildFile("research/rhythm");
        if (candidate.isDirectory())
        {
            rhythmDir = candidate;
            break;
        }
        probeDir = probeDir.getParentDirectory();
    }
    if (rhythmDir.isDirectory())
    {
        auto figuresDir = rhythmDir.getChildFile("figures");
        figuresDir.createDirectory();
        auto outPng = figuresDir.getChildFile("04_ke_rhythmbox_gui.png");
        outPng.deleteFile();
        juce::FileOutputStream stream(outPng);
        REQUIRE(stream.openedOk());
        juce::PNGImageFormat pngFormat;
        REQUIRE(pngFormat.writeImageToStream(img, stream));
        stream.flush();
        REQUIRE(outPng.existsAsFile());
        REQUIRE(outPng.getSize() > 15000);
    }

    // Verify dynamic resizing (660 x 400 and back to 780 x 460)
    rbEditor->setSize(660, 400);
    REQUIRE(rbEditor->getWidth() == 660);
    REQUIRE(rbEditor->getHeight() == 400);
    juce::Image compactImg(juce::Image::ARGB, 660, 400, true);
    {
        juce::Graphics cg(compactImg);
        rbEditor->paintEntireComponent(cg, true);
    }
    REQUIRE(!compactImg.isNull());

    rbEditor->setSize(780, 460);
    REQUIRE(rbEditor->getWidth() == 780);
    REQUIRE(rbEditor->getHeight() == 460);

    rawEditor.reset();
}

TEST_CASE("Rhythmbox Plugin 6: Pattern button clicks select/unselect patterns, multi-touch chords & zero-selection",
          "[rb_plugin][gui]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    RhythmboxAudioProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto& apvts = proc.apvts();

    std::unique_ptr<juce::AudioProcessorEditor> rawEditor(proc.createEditorIfNeeded());
    REQUIRE(rawEditor != nullptr);
    auto* rbEditor = dynamic_cast<RhythmboxAudioProcessorEditor*>(rawEditor.get());
    REQUIRE(rbEditor != nullptr);
    auto& panel = rbEditor->rhythmboxPanel();

    auto countActivePatterns = [&]()
    {
        int count = 0;
        for (std::size_t j = 0; j < kNumPatterns; ++j)
        {
            if (apvts.getParameter(param_ids::kPatternParamIds[j])->getValue() > 0.5f)
            {
                ++count;
            }
        }
        return count;
    };
    auto isPatternOn = [&](PatternId pid)
    {
        return apvts.getParameter(param_ids::kPatternParamIds[static_cast<std::size_t>(pid)])->getValue() > 0.5f;
    };
    auto clickPattern = [&](PatternId pid)
    {
        const auto idx = static_cast<std::size_t>(pid);
        auto& btn = panel.patternButton(idx);
        panel.handlePatternPressState(idx, true);
        btn.setToggleState(!btn.getToggleState(), juce::sendNotificationSync);
        panel.handlePatternPressState(idx, false);
    };

    // Single mode: switching patterns must not recurse and leaves exactly one active
    clickPattern(PatternId::Samba);
    REQUIRE(countActivePatterns() == 1);
    REQUIRE(isPatternOn(PatternId::Samba));

    clickPattern(PatternId::Bossanova);
    REQUIRE(countActivePatterns() == 1);
    REQUIRE(isPatternOn(PatternId::Bossanova));

    // Clicking an already-latched button in Single mode unlatches it -> 0 selected (no rhythm) + lamp OFF
    clickPattern(PatternId::Bossanova);
    REQUIRE(countActivePatterns() == 0);
    {
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        proc.processBlock(buf, midi);
        REQUIRE_FALSE(proc.readTelemetry().lamp.beatFlash);
        REQUIRE(proc.readTelemetry().lamp.totalSteps == 0);
    }

    // Multi-touch simultaneous press (even with Multi-Pattern checkbox OFF) latches chord & plays Wired-OR!
    const auto idxWaltz = static_cast<std::size_t>(PatternId::Waltz);
    const auto idxCha = static_cast<std::size_t>(PatternId::ChaCha);
    panel.handlePatternPressState(idxWaltz, true);
    panel.handlePatternPressState(idxCha, true); // 2 fingers held concurrently!
    // Lift both fingers (triggers ToggleButton::setToggleState + onClick before releasing touch bit)
    panel.patternButton(idxWaltz).setToggleState(!panel.patternButton(idxWaltz).getToggleState(), juce::sendNotificationSync);
    panel.handlePatternPressState(idxWaltz, false);
    panel.patternButton(idxCha).setToggleState(!panel.patternButton(idxCha).getToggleState(), juce::sendNotificationSync);
    panel.handlePatternPressState(idxCha, false);

    REQUIRE(countActivePatterns() == 2);
    REQUIRE(isPatternOn(PatternId::Waltz));
    REQUIRE(isPatternOn(PatternId::ChaCha));
    REQUIRE_FALSE(panel.multiPatternButton().getToggleState());

    // Subsequent single click clears the chord cleanly to 1 selected pattern
    clickPattern(PatternId::March);
    REQUIRE(countActivePatterns() == 1);
    REQUIRE(isPatternOn(PatternId::March));

    // Cursor Multi-Pattern checkbox: allows single-click multi-selection & cleanly collapses on uncheck
    panel.multiPatternButton().setToggleState(true, juce::sendNotificationSync);
    clickPattern(PatternId::Bossanova);
    REQUIRE(countActivePatterns() == 2);
    REQUIRE(isPatternOn(PatternId::March));
    REQUIRE(isPatternOn(PatternId::Bossanova));

    // Collapsing back to Single keeps lowest active bit (March index 2 < Bossanova 7)
    panel.multiPatternButton().setToggleState(false, juce::sendNotificationSync);
    REQUIRE(countActivePatterns() == 1);
    REQUIRE(isPatternOn(PatternId::March));

    // If 0 selected and Multi-Pattern unchecks, stay at 0 selected
    clickPattern(PatternId::March);
    REQUIRE(countActivePatterns() == 0);
    panel.multiPatternButton().setToggleState(true, juce::sendNotificationSync);
    panel.multiPatternButton().setToggleState(false, juce::sendNotificationSync);
    REQUIRE(countActivePatterns() == 0);

    // Cycle every single pattern button in Single mode (each click turns on that pattern cleanly)
    for (std::size_t i = 0; i < kNumPatterns; ++i)
    {
        const auto pid = static_cast<PatternId>(i);
        clickPattern(pid);
        REQUIRE(countActivePatterns() == 1);
        REQUIRE(isPatternOn(pid));
    }

    rawEditor.reset();
}
