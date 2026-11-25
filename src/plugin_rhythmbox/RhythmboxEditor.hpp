#pragma once

#include "plugin_rhythmbox/RhythmboxProcessor.hpp"
#include "ui/RhythmboxPanel.hpp"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace ke310::rhythmbox_plugin
{

class RhythmboxSettingsGearButton final : public juce::ToggleButton
{
public:
    RhythmboxSettingsGearButton();

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RhythmboxSettingsGearButton)
};

class RhythmboxSettingsOverlay final : public juce::Component
{
public:
    RhythmboxSettingsOverlay(juce::AudioProcessorValueTreeState& apvts,
                             std::function<void()> onCloseRequested,
                             bool isStandaloneApp = false);
    ~RhythmboxSettingsOverlay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void setStandaloneAppMode(bool standalone) noexcept;
    bool isSyncModeVisible() const noexcept { return syncModeBox_.isVisible(); }

private:
    juce::AudioProcessorValueTreeState& apvts_;
    std::function<void()> closeFn_;
    bool isStandaloneApp_ = false;
    ke310::ui::RhythmboxLookAndFeel laf_;

    juce::Rectangle<int> cardBounds_;
    float div1Y_ = 0.0f;
    float div2Y_ = 0.0f;
    float div3Y_ = 0.0f;

    juce::Label titleLabel_{{}, "RHYTHMBOX SETTINGS"};
    juce::TextButton closeBtn_{"X"};

    // Section 1: Engine & MIDI
    juce::Label engineSectionLbl_{{}, "ENGINE & MIDI"};
    juce::Label syncModeLbl_{{}, "Sync Mode"};
    juce::ComboBox syncModeBox_;
    juce::Label midiChLbl_{{}, "GM Drum MIDI Ch"};
    juce::ComboBox midiChBox_;
    juce::Label driftLbl_{{}, "Analog Drift"};
    juce::Slider driftSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};

    // Section 2: MIDI Key Start (Synchro-Start)
    juce::Label midiStartSectionLbl_{{}, "MIDI KEY START (SYNCHRO-START)"};
    juce::ToggleButton midiStartEnableBtn_{"Enable MIDI Key Start (Note-On Auto-Start)"};
    juce::Label midiStartChLbl_{{}, "Trigger MIDI Ch"};
    juce::ComboBox midiStartChBox_;
    juce::Label midiStartHintLbl_{{}, "When RHYTHM is ON, waits for Note-On on the trigger channel to start Beat 1."};

    // Section 3: Rhythm Stop Switch (MIDI CC)
    juce::Label stopCcSectionLbl_{{}, "RHYTHM STOP SWITCH (MIDI CC)"};
    juce::ToggleButton stopCcEnableBtn_{"Enable Rhythm Stop Switch (MIDI CC)"};
    juce::Label stopCcNumLbl_{{}, "Stop CC #"};
    juce::Slider stopCcNumSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Label stopCcChLbl_{{}, "CC Channel"};
    juce::ComboBox stopCcChBox_;
    juce::Label stopCcHintLbl_{{}, "CC >= 64 stops a running rhythm and inhibits rhythm start while held."};

    // Declared LAST so attachments unlink before widgets are destroyed
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RhythmboxSettingsOverlay)
};

class RhythmboxAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit RhythmboxAudioProcessorEditor(RhythmboxAudioProcessor& owner);
    ~RhythmboxAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Accessors for automated GUI tests
    ke310::ui::RhythmboxPanel& rhythmboxPanel() noexcept { return panel_; }
    RhythmboxSettingsGearButton& settingsGearButton() noexcept { return settingsGearBtn_; }
    RhythmboxSettingsOverlay& settingsOverlay() noexcept { return settingsOverlay_; }
    bool isSettingsOpen() const noexcept { return settingsOverlay_.isVisible(); }
    void syncUiNow();

private:
    void timerCallback() override;

    RhythmboxAudioProcessor& processor_;
    ke310::ui::RhythmboxLookAndFeel presetBarLaf_;

    juce::Label presetLabel_{{}, "PROGRAM"};
    juce::TextButton prevProgramBtn_{"<"};
    juce::ComboBox programBox_;
    juce::TextButton nextProgramBtn_{">"};
    RhythmboxSettingsGearButton settingsGearBtn_;

    juce::Rectangle<int> presetBarBounds_;
    ke310::ui::RhythmboxPanel panel_;
    RhythmboxSettingsOverlay settingsOverlay_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RhythmboxAudioProcessorEditor)
};

} // namespace ke310::rhythmbox_plugin
