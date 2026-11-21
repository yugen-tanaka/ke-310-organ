#include "plugin/FactoryPresets.hpp"
#include "plugin/PluginEditor.hpp"
#include "plugin/PluginParams.hpp"
#include "plugin/PluginProcessor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <memory>

namespace
{

using namespace ke310::plugin;

void setParamUnnorm(juce::AudioProcessorValueTreeState& apvts, const char* id, float unnormalizedValue)
{
    auto* p = apvts.getParameter(id);
    REQUIRE(p != nullptr);
    p->setValueNotifyingHost(p->convertTo0to1(unnormalizedValue));
}

float busRms(const juce::AudioProcessor& proc, const juce::AudioBuffer<float>& buf, int busIdx)
{
    const auto busBuf = proc.getBusBuffer(const_cast<juce::AudioBuffer<float>&>(buf), false, busIdx);
    return (busBuf.getNumChannels() > 0) ? busBuf.getRMSLevel(0, 0, buf.getNumSamples()) : 0.0f;
}

} // namespace

TEST_CASE("Plugin 1. 3-stereo-bus multi-out separation and ENSEMBLE routing", "[plugin][buses]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    KE310AudioProcessor proc;

    REQUIRE(proc.getBusCount(false) == 3);
    REQUIRE(proc.getChannelCountOfBus(false, 0) == 2);
    REQUIRE(proc.getChannelCountOfBus(false, 1) == 2);
    REQUIRE(proc.getChannelCountOfBus(false, 2) == 2);

    proc.prepareToPlay(48000.0, 512);
    auto& apvts = proc.apvts();

    // Activate Rotor voice (UPPER TIBIA 8') + Stationary voice (UPPER CLARINET 8'), ENSEMBLE OFF
    setParamUnnorm(apvts, param_ids::upperTibia8, 1.0f);
    setParamUnnorm(apvts, param_ids::upperClarinet8, 0.0f);
    setParamUnnorm(apvts, param_ids::ensemble, 0.0f);

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    juce::AudioBuffer<float> buf(proc.getTotalNumOutputChannels(), 512);
    proc.processBlock(buf, midi);
    midi.clear();
    proc.processBlock(buf, midi);

    // Pure TIBIA 8': present on Main (Bus 0) & Rotor (Bus 1), silent on Stationary (Bus 2)
    REQUIRE(busRms(proc, buf, 0) > 0.005f);
    REQUIRE(busRms(proc, buf, 1) > 0.005f);
    REQUIRE(busRms(proc, buf, 2) < 1e-6f);

    // Now switch to Stationary voice (CLARINET 8' only) with ENSEMBLE OFF
    setParamUnnorm(apvts, param_ids::upperTibia8, 0.0f);
    setParamUnnorm(apvts, param_ids::upperClarinet8, 1.0f);
    for (int i = 0; i < 4; ++i)
    {
        proc.processBlock(buf, midi);
    }
    REQUIRE(busRms(proc, buf, 1) < 1e-5f);
    REQUIRE(busRms(proc, buf, 2) > 0.002f);

    // Toggle ENSEMBLE ON: Stationary folds into Rotor, Bus 2 goes silent!
    setParamUnnorm(apvts, param_ids::ensemble, 1.0f);
    proc.processBlock(buf, midi);
    REQUIRE(busRms(proc, buf, 1) > 0.002f);
    REQUIRE(busRms(proc, buf, 2) < 1e-6f);
    REQUIRE(busRms(proc, buf, 0) == Catch::Approx(busRms(proc, buf, 1)).margin(0.01f));
}

TEST_CASE("Plugin 2. User-selectable Upper/Lower/Pedal MIDI channels & stuck-note protection", "[plugin][midi]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    KE310AudioProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto& apvts = proc.apvts();

    // Assign custom channels: Upper=Ch 4, Lower=Ch 9, Pedal=Ch 15
    setParamUnnorm(apvts, param_ids::upperMidiCh, 4.0f);
    setParamUnnorm(apvts, param_ids::lowerMidiCh, 9.0f);
    setParamUnnorm(apvts, param_ids::pedalMidiCh, 15.0f);
    REQUIRE(apvts.getParameter(param_ids::upperMidiCh)->getCurrentValueAsText() == "Ch 4");
    REQUIRE(apvts.getParameter(param_ids::lowerMidiCh)->getCurrentValueAsText() == "Ch 9");
    REQUIRE(apvts.getParameter(param_ids::pedalMidiCh)->getCurrentValueAsText() == "Ch 15");

    // Configure distinct audible voices on Lower (DIAPASON -> Stationary) and Pedal (BOURDON -> Stationary)
    setParamUnnorm(apvts, param_ids::upperTibia8, 1.0f);
    setParamUnnorm(apvts, param_ids::lowerDiapason8, 1.0f);
    setParamUnnorm(apvts, param_ids::pedalBourdon16, 1.0f);

    juce::AudioBuffer<float> buf(proc.getTotalNumOutputChannels(), 512);

    // 1) Unassigned channel (Ch 10) must be ignored cleanly
    {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(10, 60, 1.0f), 0);
        proc.processBlock(buf, midi);
        REQUIRE(busRms(proc, buf, 0) < 1e-6f);
    }

    // 2) Ch 9 triggers Lower Diapason (Stationary only)
    {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(9, 48, 1.0f), 0);
        proc.processBlock(buf, midi);
        REQUIRE(proc.activeLowerNotes() == 1);
        REQUIRE(proc.activeUpperNotes() == 0);
        REQUIRE(busRms(proc, buf, 1) < 1e-6f);
        REQUIRE(busRms(proc, buf, 2) > 0.002f);

        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOff(9, 48), 0);
        for (int b = 0; b < 10; ++b)
        {
            proc.processBlock(buf, midi);
            midi.clear();
        }
    }

    // 3) Ch 15 triggers Pedal Bourdon (low-note monophonic)
    {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(15, 36, 1.0f), 0);
        proc.processBlock(buf, midi);
        REQUIRE(proc.activePedalMidi() == 36);
        REQUIRE(busRms(proc, buf, 2) > 0.002f);

        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOff(15, 36), 0);
        for (int b = 0; b < 30; ++b)
        {
            proc.processBlock(buf, midi);
            midi.clear();
        }
    }

    // 4) Hold Upper C4 on Ch 4, then switch upper_midi_ch -> Ch 11 while key is still held: must NOT stick!
    {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(4, 60, 1.0f), 0);
        proc.processBlock(buf, midi);
        REQUIRE(proc.activeUpperNotes() == 1);
        REQUIRE(busRms(proc, buf, 1) > 0.005f);

        setParamUnnorm(apvts, param_ids::upperMidiCh, 11.0f);
        midi.clear();
        for (int b = 0; b < 25; ++b)
        {
            proc.processBlock(buf, midi);
        }
        REQUIRE(proc.activeUpperNotes() == 0);
        REQUIRE(busRms(proc, buf, 1) < 1e-4f);
    }
}

TEST_CASE("Plugin 3. Sample-accurate MIDI CC#11 Expression & CC#64 Knee Sustainer", "[plugin][midi][cc]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    KE310AudioProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto& apvts = proc.apvts();

    setParamUnnorm(apvts, param_ids::upperTibia8, 1.0f);
    setParamUnnorm(apvts, param_ids::upperSustainTime, 0.55f);

    juce::AudioBuffer<float> buf(proc.getTotalNumOutputChannels(), 512);

    // Trigger note + hold sustain pedal (CC#64 = 127) on Ch 1
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 10);
    proc.processBlock(buf, midi);

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    for (int b = 0; b < 20; ++b) // ~213 ms after Note-Off -> should still ring because CC#64 is down!
    {
        proc.processBlock(buf, midi);
        midi.clear();
    }
    const float sustainedRms = busRms(proc, buf, 0);
    REQUIRE(sustainedRms > 0.002f);

    // Apply Expression CC#11 = 32 (~0.25) -> output attenuates smoothly
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 11, 32), 0);
    for (int b = 0; b < 4; ++b)
    {
        proc.processBlock(buf, midi);
        midi.clear();
    }
    REQUIRE(busRms(proc, buf, 0) < sustainedRms * 0.45f);
}

TEST_CASE("Plugin 4. 28 Official Manual Registration Presets & XML state save/restore", "[plugin][state]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    KE310AudioProcessor proc;
    proc.prepareToPlay(48000.0, 256);
    auto& apvts = proc.apvts();

    REQUIRE(proc.getNumPrograms() == 28);
    REQUIRE(proc.getProgramName(0) == "01. Jazz No.1 (Slow Tempo)");
    REQUIRE(proc.getProgramName(27) == "28. Surf");

    // Verify REPEAT RATE parameter range is restricted to [0.30, 0.80]
    const auto* repParam = apvts.getParameter(param_ids::repeatRate);
    REQUIRE(repParam != nullptr);
    const auto& repRange = repParam->getNormalisableRange();
    REQUIRE(repRange.start == Catch::Approx(0.30f).margin(1e-5f));
    REQUIRE(repRange.end == Catch::Approx(0.80f).margin(1e-5f));

    // Customize user MIDI channels BEFORE cycling factory presets
    setParamUnnorm(apvts, param_ids::upperMidiCh, 6.0f);
    setParamUnnorm(apvts, param_ids::lowerMidiCh, 7.0f);
    setParamUnnorm(apvts, param_ids::pedalMidiCh, 8.0f);

    juce::AudioBuffer<float> buf(proc.getTotalNumOutputChannels(), 256);
    for (int p = 0; p < proc.getNumPrograms(); ++p)
    {
        proc.setCurrentProgram(p);
        REQUIRE(proc.getCurrentProgram() == p);
        REQUIRE(proc.getProgramName(p).isNotEmpty());
        // Verify MIDI channels were NOT clobbered by factory preset changes!
        REQUIRE(apvts.getParameter(param_ids::upperMidiCh)->getCurrentValueAsText() == "Ch 6");
        REQUIRE(apvts.getParameter(param_ids::lowerMidiCh)->getCurrentValueAsText() == "Ch 7");
        REQUIRE(apvts.getParameter(param_ids::pedalMidiCh)->getCurrentValueAsText() == "Ch 8");

        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(6, 60, 1.0f), 0);
        proc.processBlock(buf, midi);
        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOff(6, 60), 0);
        proc.processBlock(buf, midi);
    }

    // Save preset 27 ("28. Surf") + custom MIDI channels, mutate state, then restore!
    proc.setCurrentProgram(27);
    REQUIRE(apvts.getParameter(param_ids::presetMarimba)->getValue() > 0.5f);
    REQUIRE(apvts.getParameter(param_ids::upperSustain)->getValue() > 0.5f);

    juce::MemoryBlock stateBlob;
    proc.getStateInformation(stateBlob);
    REQUIRE(stateBlob.getSize() > 100);

    proc.setCurrentProgram(0);
    setParamUnnorm(apvts, param_ids::upperMidiCh, 1.0f);
    REQUIRE(apvts.getParameter(param_ids::presetMarimba)->getValue() < 0.5f);
    REQUIRE(apvts.getParameter(param_ids::upperSustain)->getValue() < 0.5f);

    proc.setStateInformation(stateBlob.getData(), static_cast<int>(stateBlob.getSize()));
    REQUIRE(proc.getCurrentProgram() == 27);
    REQUIRE(apvts.getParameter(param_ids::presetMarimba)->getValue() > 0.5f);
    REQUIRE(apvts.getParameter(param_ids::upperSustain)->getValue() > 0.5f);
    REQUIRE(apvts.getParameter(param_ids::upperMidiCh)->getCurrentValueAsText() == "Ch 6");
    REQUIRE(apvts.getParameter(param_ids::lowerMidiCh)->getCurrentValueAsText() == "Ch 7");
    REQUIRE(apvts.getParameter(param_ids::pedalMidiCh)->getCurrentValueAsText() == "Ch 8");
}

TEST_CASE("Plugin 5. Custom KE-310 Retro GUI Editor instantiation, APVTS sync & screenshot export", "[plugin][gui]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    KE310AudioProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    // Load 20. Theatre Organ and play a 3-manual chord so all buses & meters light up
    proc.setCurrentProgram(19);
    REQUIRE(proc.getProgramName(19) == "20. Theatre Organ");

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, 1.0f), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 67, 1.0f), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 72, 1.0f), 0);
    midi.addEvent(juce::MidiMessage::noteOn(2, 48, 1.0f), 0);
    midi.addEvent(juce::MidiMessage::noteOn(2, 52, 1.0f), 0);
    midi.addEvent(juce::MidiMessage::noteOn(3, 36, 1.0f), 0);

    juce::AudioBuffer<float> buf(proc.getTotalNumOutputChannels(), 512);
    for (int i = 0; i < 4; ++i)
    {
        proc.processBlock(buf, midi);
        midi.clear();
    }

    REQUIRE(proc.activeUpperNotes() == 4);
    REQUIRE(proc.activeLowerNotes() == 2);
    REQUIRE(proc.activePedalMidi() == 36);
    REQUIRE(proc.mainBusRms() > 0.002f);
    REQUIRE(proc.rotorBusRms() > 0.002f);
    REQUIRE(proc.stationaryBusRms() > 0.002f);

    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditorIfNeeded());
    REQUIRE(editor != nullptr);
    REQUIRE(editor->getWidth() == 1200);
    REQUIRE(editor->getHeight() == 760);
    REQUIRE(editor->getNumChildComponents() > 50);
    REQUIRE(TelemetryDisplay::formatVoiceText(0, 0, -1) == "UP:0   LO:0   PED:--");
    REQUIRE(TelemetryDisplay::formatVoiceText(4, 2, 36) == "UP:4   LO:2   PED:C2");

    // Render full GUI offscreen at default resolution
    juce::Image img(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        juce::Graphics g(img);
        editor->paintEntireComponent(g, true);
    }
    REQUIRE(!img.isNull());
    REQUIRE(img.getPixelAt(20, 20) != img.getPixelAt(20, 110));
    REQUIRE(img.getPixelAt(20, 110) != img.getPixelAt(200, 250));
    REQUIRE(img.getPixelAt(20, 110) != img.getPixelAt(200, 420));

    // Export screenshot to research/reports/figures/08_ke310_plugin_gui.png when running inside repo tree
    juce::File probeDir = juce::File::getCurrentWorkingDirectory();
    juce::File reportsDir;
    for (int depth = 0; depth < 4; ++depth)
    {
        auto candidate = probeDir.getChildFile("research/reports");
        if (candidate.isDirectory())
        {
            reportsDir = candidate;
            break;
        }
        probeDir = probeDir.getParentDirectory();
    }
    if (reportsDir.isDirectory())
    {
        auto figuresDir = reportsDir.getChildFile("figures");
        figuresDir.createDirectory();
        auto outPng = figuresDir.getChildFile("08_ke310_plugin_gui.png");
        outPng.deleteFile();
        juce::FileOutputStream stream(outPng);
        REQUIRE(stream.openedOk());
        juce::PNGImageFormat pngFormat;
        REQUIRE(pngFormat.writeImageToStream(img, stream));
        stream.flush();
        REQUIRE(outPng.existsAsFile());
        REQUIRE(outPng.getSize() > 20000);
    }

    // Verify dynamic resizing and re-render cleanly
    editor->setSize(1024, 640);
    REQUIRE(editor->getWidth() == 1024);
    REQUIRE(editor->getHeight() == 640);
    juce::Image resizedImg(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        juce::Graphics rg(resizedImg);
        editor->paintEntireComponent(rg, true);
    }
    REQUIRE(!resizedImg.isNull());

    editor.reset();
}

TEST_CASE("Plugin 6. Built-in Hornless Rotary Speaker switches, stereo bus routing & custom speed persistence", "[plugin][rotary]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    KE310AudioProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto& apvts = proc.apvts();

    const auto* chorusParam = apvts.getParameter(param_ids::rotaryChorus);
    const auto* tremParam = apvts.getParameter(param_ids::rotaryTremolo);
    const auto* slowParam = apvts.getParameter(param_ids::rotarySlowHz);
    const auto* fastParam = apvts.getParameter(param_ids::rotaryFastHz);
    REQUIRE(chorusParam != nullptr);
    REQUIRE(tremParam != nullptr);
    REQUIRE(slowParam != nullptr);
    REQUIRE(fastParam != nullptr);

    REQUIRE(chorusParam->getValue() < 0.5f);
    REQUIRE(tremParam->getValue() < 0.5f);
    REQUIRE(slowParam->convertFrom0to1(slowParam->getValue()) == Catch::Approx(0.80f).margin(1e-3f));
    REQUIRE(fastParam->convertFrom0to1(fastParam->getValue()) == Catch::Approx(6.00f).margin(1e-3f));
    REQUIRE(slowParam->getNormalisableRange().start == Catch::Approx(0.30f).margin(1e-5f));
    REQUIRE(slowParam->getNormalisableRange().end == Catch::Approx(1.80f).margin(1e-5f));
    REQUIRE(fastParam->getNormalisableRange().start == Catch::Approx(3.50f).margin(1e-5f));
    REQUIRE(fastParam->getNormalisableRange().end == Catch::Approx(8.50f).margin(1e-5f));

    auto busStereoDiff = [&](const juce::AudioBuffer<float>& buf, int busIdx) -> std::pair<float, float>
    {
        const auto b = proc.getBusBuffer(const_cast<juce::AudioBuffer<float>&>(buf), false, busIdx);
        const int n = buf.getNumSamples();
        const float* l = b.getReadPointer(0);
        const float* r = b.getReadPointer(1);
        double sq = 0.0;
        float maxDiff = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float d = std::abs(l[i] - r[i]);
            maxDiff = std::max(maxDiff, d);
            sq += static_cast<double>(d) * static_cast<double>(d);
        }
        return {maxDiff, static_cast<float>(std::sqrt(sq / static_cast<double>(n)))};
    };

    // 1) Play Rotor (TIBIA 8') + Stationary (CLARINET 8') with CHORUS/TREMOLO OFF -> dry mono L == R
    setParamUnnorm(apvts, param_ids::upperTibia8, 1.0f);
    setParamUnnorm(apvts, param_ids::upperClarinet8, 1.0f);
    setParamUnnorm(apvts, param_ids::ensemble, 0.0f);

    juce::AudioBuffer<float> buf(proc.getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    proc.processBlock(buf, midi);
    midi.clear();
    proc.processBlock(buf, midi);

    REQUIRE(busRms(proc, buf, 1) > 0.005f);
    REQUIRE(busRms(proc, buf, 2) > 0.002f);
    REQUIRE(busStereoDiff(buf, 1).first == 0.0f);
    REQUIRE(busStereoDiff(buf, 2).first == 0.0f);
    REQUIRE(proc.rotarySpeedHz() == 0.0f);

    // 2) Engage TREMOLO=ON for ~2.0 s -> speed climbs > 4.5 Hz, Main & Rotor become true stereo, Stationary stays dry
    setParamUnnorm(apvts, param_ids::rotaryTremolo, 1.0f);
    for (int b = 0; b < 188; ++b)
    {
        proc.processBlock(buf, midi);
    }
    REQUIRE(proc.rotarySpeedHz() > 4.5f);
    REQUIRE(busStereoDiff(buf, 0).second > 1.0e-3f);
    REQUIRE(busStereoDiff(buf, 1).second > 1.0e-3f);
    REQUIRE(busStereoDiff(buf, 2).first < 1.0e-6f);

    // 3) User Slow/Fast speed calibration persists across Official Manual P.22 preset switches
    setParamUnnorm(apvts, param_ids::rotarySlowHz, 1.25f);
    setParamUnnorm(apvts, param_ids::rotaryFastHz, 7.20f);

    proc.setCurrentProgram(0); // 01. Jazz No.1 -> TREMOLO
    REQUIRE(chorusParam->getValue() < 0.5f);
    REQUIRE(tremParam->getValue() > 0.5f);

    proc.setCurrentProgram(1); // 02. Jazz No.2 -> CHORUS
    REQUIRE(chorusParam->getValue() > 0.5f);
    REQUIRE(tremParam->getValue() < 0.5f);

    proc.setCurrentProgram(7); // 08. Mambo -> Brake (both OFF)
    REQUIRE(chorusParam->getValue() < 0.5f);
    REQUIRE(tremParam->getValue() < 0.5f);

    REQUIRE(slowParam->convertFrom0to1(slowParam->getValue()) == Catch::Approx(1.25f).margin(1e-2f));
    REQUIRE(fastParam->convertFrom0to1(fastParam->getValue()) == Catch::Approx(7.20f).margin(1e-2f));

    // 4) State save/restore preserves rotary switches and custom speeds
    setParamUnnorm(apvts, param_ids::rotaryChorus, 1.0f);
    setParamUnnorm(apvts, param_ids::rotaryTremolo, 1.0f);
    juce::MemoryBlock saved;
    proc.getStateInformation(saved);

    setParamUnnorm(apvts, param_ids::rotaryChorus, 0.0f);
    setParamUnnorm(apvts, param_ids::rotaryTremolo, 0.0f);
    setParamUnnorm(apvts, param_ids::rotarySlowHz, 0.55f);
    setParamUnnorm(apvts, param_ids::rotaryFastHz, 4.50f);

    proc.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(chorusParam->getValue() > 0.5f);
    REQUIRE(tremParam->getValue() > 0.5f);
    REQUIRE(slowParam->convertFrom0to1(slowParam->getValue()) == Catch::Approx(1.25f).margin(1e-2f));
    REQUIRE(fastParam->convertFrom0to1(fastParam->getValue()) == Catch::Approx(7.20f).margin(1e-2f));
}
