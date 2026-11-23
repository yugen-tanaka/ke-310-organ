#include "plugin_rhythmbox/RhythmboxEditor.hpp"

#include <algorithm>

namespace ke310::rhythmbox_plugin
{

RhythmboxAudioProcessorEditor::RhythmboxAudioProcessorEditor(RhythmboxAudioProcessor& owner)
    : AudioProcessorEditor(&owner),
      processor_(owner),
      panel_(
          owner.apvts(),
          [this](ke310::rhythmbox::VoiceId voice, float)
          {
              processor_.enqueueUiAuditionVoice(voice);
          },
          [this]()
          {
              return processor_.readTelemetry();
          })
{
    setLookAndFeel(&presetBarLaf_);

    presetLabel_.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    presetLabel_.setJustificationType(juce::Justification::centredLeft);
    presetLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffb0a798));
    addAndMakeVisible(presetLabel_);

    prevProgramBtn_.setLookAndFeel(&presetBarLaf_);
    prevProgramBtn_.onClick = [this]()
    {
        const int n = processor_.getNumPrograms();
        if (n > 0)
        {
            const int prevIdx = (processor_.getCurrentProgram() - 1 + n) % n;
            processor_.setCurrentProgram(prevIdx);
            programBox_.setSelectedId(prevIdx + 1, juce::dontSendNotification);
        }
    };
    addAndMakeVisible(prevProgramBtn_);

    programBox_.setLookAndFeel(&presetBarLaf_);
    for (int p = 0; p < processor_.getNumPrograms(); ++p)
    {
        programBox_.addItem(processor_.getProgramName(p), p + 1);
    }
    programBox_.setSelectedId(processor_.getCurrentProgram() + 1, juce::dontSendNotification);
    programBox_.onChange = [this]()
    {
        const int sel = programBox_.getSelectedId();
        if (sel > 0)
        {
            processor_.setCurrentProgram(sel - 1);
        }
    };
    addAndMakeVisible(programBox_);

    nextProgramBtn_.setLookAndFeel(&presetBarLaf_);
    nextProgramBtn_.onClick = [this]()
    {
        const int n = processor_.getNumPrograms();
        if (n > 0)
        {
            const int nextIdx = (processor_.getCurrentProgram() + 1) % n;
            processor_.setCurrentProgram(nextIdx);
            programBox_.setSelectedId(nextIdx + 1, juce::dontSendNotification);
        }
    };
    addAndMakeVisible(nextProgramBtn_);

    badgeLabel_.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    badgeLabel_.setJustificationType(juce::Justification::centredRight);
    badgeLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff9e9587));
    addAndMakeVisible(badgeLabel_);

    addAndMakeVisible(panel_);

    setResizable(true, true);
    setResizeLimits(660, 400, 1200, 720);
    setSize(780, 460);

    startTimerHz(15);
}

RhythmboxAudioProcessorEditor::~RhythmboxAudioProcessorEditor()
{
    stopTimer();
    prevProgramBtn_.setLookAndFeel(nullptr);
    programBox_.setLookAndFeel(nullptr);
    nextProgramBtn_.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void RhythmboxAudioProcessorEditor::timerCallback()
{
    if (!programBox_.isPopupActive())
    {
        const int wantId = processor_.getCurrentProgram() + 1;
        if (programBox_.getSelectedId() != wantId && wantId > 0)
        {
            programBox_.setSelectedId(wantId, juce::dontSendNotification);
        }
    }
}

void RhythmboxAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff141210));
    g.setColour(juce::Colour(0xff1b1815));
    g.fillRect(presetBarBounds_);
    g.setColour(juce::Colour(0xff3a332c));
    g.drawHorizontalLine(presetBarBounds_.getBottom() - 1, 0.0f, static_cast<float>(getWidth()));
}

void RhythmboxAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    presetBarBounds_ = area.removeFromTop(28);

    auto bar = presetBarBounds_.reduced(8, 3);
    presetLabel_.setBounds(bar.removeFromLeft(58));
    prevProgramBtn_.setBounds(bar.removeFromLeft(24));
    bar.removeFromLeft(3);
    programBox_.setBounds(bar.removeFromLeft(std::min(210, bar.getWidth() / 2)));
    bar.removeFromLeft(3);
    nextProgramBtn_.setBounds(bar.removeFromLeft(24));
    badgeLabel_.setBounds(bar);

    panel_.setBounds(area);
}

} // namespace ke310::rhythmbox_plugin
