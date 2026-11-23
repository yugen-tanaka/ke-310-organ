#pragma once

#include "plugin_rhythmbox/RhythmboxProcessor.hpp"
#include "ui/RhythmboxPanel.hpp"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace ke310::rhythmbox_plugin
{

class RhythmboxAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit RhythmboxAudioProcessorEditor(RhythmboxAudioProcessor& owner);
    ~RhythmboxAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Helper accessor for GUI automated tests
    ke310::ui::RhythmboxPanel& rhythmboxPanel() noexcept { return panel_; }

private:
    void timerCallback() override;

    RhythmboxAudioProcessor& processor_;
    ke310::ui::RhythmboxLookAndFeel presetBarLaf_;

    juce::Label presetLabel_{{}, "PROGRAM"};
    juce::TextButton prevProgramBtn_{"<"};
    juce::ComboBox programBox_;
    juce::TextButton nextProgramBtn_{">"};
    juce::Label badgeLabel_{{}, "Standalone / VST3 / AU"};

    juce::Rectangle<int> presetBarBounds_;
    ke310::ui::RhythmboxPanel panel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RhythmboxAudioProcessorEditor)
};

} // namespace ke310::rhythmbox_plugin
