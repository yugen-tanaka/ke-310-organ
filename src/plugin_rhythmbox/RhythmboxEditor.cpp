#include "plugin_rhythmbox/RhythmboxEditor.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ke310::rhythmbox_plugin
{

namespace
{

juce::Path makeGearPath(float cx, float cy, float outerR, float innerR, float holeR, int numTeeth)
{
    juce::Path p;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const int steps = numTeeth * 4;
    for (int i = 0; i < steps; ++i)
    {
        const float angle = (static_cast<float>(i) / static_cast<float>(steps)) * twoPi;
        const int phase = i % 4;
        const float rad = (phase == 0 || phase == 1) ? outerR : innerR;
        const float x = cx + std::cos(angle) * rad;
        const float y = cy + std::sin(angle) * rad;
        if (i == 0)
        {
            p.startNewSubPath(x, y);
        }
        else
        {
            p.lineTo(x, y);
        }
    }
    p.closeSubPath();
    p.setUsingNonZeroWinding(false);
    p.addEllipse(cx - holeR, cy - holeR, holeR * 2.0f, holeR * 2.0f);
    return p;
}

} // namespace

//==============================================================================
// RhythmboxSettingsGearButton
//==============================================================================
RhythmboxSettingsGearButton::RhythmboxSettingsGearButton()
{
    setClickingTogglesState(true);
    setTooltip("Rhythmbox Settings (Sync, MIDI Channel, Drift, MIDI Key Start & Rhythm Stop CC)");
}

void RhythmboxSettingsGearButton::paintButton(juce::Graphics& g,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    const auto r = getLocalBounds().toFloat().reduced(1.0f);
    const bool open = getToggleState();
    const juce::Colour accent(0xffffb432);

    juce::Colour bg = open ? accent.withAlpha(0.24f) : juce::Colour(0xff221f1c);
    if (shouldDrawButtonAsDown)
    {
        bg = bg.darker(0.18f);
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        bg = bg.brighter(0.14f);
    }
    g.setColour(bg);
    g.fillRoundedRectangle(r, 4.0f);

    g.setColour(open ? accent : juce::Colour(shouldDrawButtonAsHighlighted ? 0xff7c7063 : 0xff5a5046));
    g.drawRoundedRectangle(r, 4.0f, open ? 1.4f : 1.0f);

    const float side = std::min(r.getWidth(), r.getHeight());
    const auto gear = makeGearPath(r.getCentreX(), r.getCentreY(), side * 0.35f, side * 0.25f, side * 0.11f, 8);
    g.setColour(open ? accent : juce::Colour(0xffeae4d7));
    g.fillPath(gear);
}

//==============================================================================
// RhythmboxSettingsOverlay
//==============================================================================
RhythmboxSettingsOverlay::RhythmboxSettingsOverlay(juce::AudioProcessorValueTreeState& apvts,
                                                   std::function<void()> onCloseRequested,
                                                   bool isStandaloneApp)
    : apvts_(apvts),
      closeFn_(std::move(onCloseRequested)),
      isStandaloneApp_(isStandaloneApp)
{
    setLookAndFeel(&laf_);
    setWantsKeyboardFocus(true);

    titleLabel_.setLookAndFeel(&laf_);
    titleLabel_.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffeae4d7));
    addAndMakeVisible(titleLabel_);

    closeBtn_.setLookAndFeel(&laf_);
    closeBtn_.onClick = [this]()
    {
        if (closeFn_)
        {
            closeFn_();
        }
    };
    addAndMakeVisible(closeBtn_);

    const juce::Colour accentAmber(0xffffb432);
    const juce::Colour hintCol(0xffb0a798);

    auto initLbl = [this](juce::Label& lbl, float size, juce::Colour col = juce::Colour(0xffeae4d7))
    {
        lbl.setLookAndFeel(&laf_);
        lbl.setFont(juce::Font(juce::FontOptions(size, juce::Font::bold)));
        lbl.setColour(juce::Label::textColourId, col);
        addAndMakeVisible(lbl);
    };

    // Section 1: Engine & MIDI
    initLbl(engineSectionLbl_, 10.8f, accentAmber);
    initLbl(syncModeLbl_, 10.5f);
    syncModeBox_.setLookAndFeel(&laf_);
    syncModeBox_.addItemList(makeSyncModeChoices(), 1);
    addAndMakeVisible(syncModeBox_);
    comboAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts_, param_ids::rbSyncMode, syncModeBox_));
    syncModeLbl_.setVisible(!isStandaloneApp_);
    syncModeBox_.setVisible(!isStandaloneApp_);

    initLbl(midiChLbl_, 10.5f);
    midiChBox_.setLookAndFeel(&laf_);
    midiChBox_.addItemList(makeMidiChannelChoices(), 1);
    addAndMakeVisible(midiChBox_);
    comboAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts_, param_ids::rbMidiCh, midiChBox_));

    initLbl(driftLbl_, 10.5f);
    driftSlider_.setLookAndFeel(&laf_);
    driftSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
    driftSlider_.setNumDecimalPlacesToDisplay(2);
    driftSlider_.setDoubleClickReturnValue(true, 1.0);
    addAndMakeVisible(driftSlider_);
    sliderAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, param_ids::rbAnalogDrift, driftSlider_));

    // Section 2: MIDI Key Start
    initLbl(midiStartSectionLbl_, 10.8f, accentAmber);
    midiStartEnableBtn_.setLookAndFeel(&laf_);
    addAndMakeVisible(midiStartEnableBtn_);
    buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts_, param_ids::rbMidiStartEn, midiStartEnableBtn_));

    initLbl(midiStartChLbl_, 10.5f);
    midiStartChBox_.setLookAndFeel(&laf_);
    midiStartChBox_.addItemList(makeMidiTriggerChannelChoices(), 1);
    addAndMakeVisible(midiStartChBox_);
    comboAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts_, param_ids::rbMidiStartCh, midiStartChBox_));

    midiStartHintLbl_.setLookAndFeel(&laf_);
    midiStartHintLbl_.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
    midiStartHintLbl_.setColour(juce::Label::textColourId, hintCol);
    addAndMakeVisible(midiStartHintLbl_);

    // Section 3: Rhythm Stop CC
    initLbl(stopCcSectionLbl_, 10.8f, accentAmber);
    stopCcEnableBtn_.setLookAndFeel(&laf_);
    addAndMakeVisible(stopCcEnableBtn_);
    buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts_, param_ids::rbStopCcEn, stopCcEnableBtn_));

    initLbl(stopCcNumLbl_, 10.5f);
    stopCcNumSlider_.setLookAndFeel(&laf_);
    stopCcNumSlider_.setRange(0.0, 127.0, 1.0);
    stopCcNumSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
    stopCcNumSlider_.setDoubleClickReturnValue(true, 67.0);
    addAndMakeVisible(stopCcNumSlider_);
    sliderAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts_, param_ids::rbStopCcNum, stopCcNumSlider_));

    initLbl(stopCcChLbl_, 10.5f);
    stopCcChBox_.setLookAndFeel(&laf_);
    stopCcChBox_.addItemList(makeMidiTriggerChannelChoices(), 1);
    addAndMakeVisible(stopCcChBox_);
    comboAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts_, param_ids::rbStopCcCh, stopCcChBox_));

    stopCcHintLbl_.setLookAndFeel(&laf_);
    stopCcHintLbl_.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
    stopCcHintLbl_.setColour(juce::Label::textColourId, hintCol);
    addAndMakeVisible(stopCcHintLbl_);
}

RhythmboxSettingsOverlay::~RhythmboxSettingsOverlay()
{
    titleLabel_.setLookAndFeel(nullptr);
    closeBtn_.setLookAndFeel(nullptr);
    engineSectionLbl_.setLookAndFeel(nullptr);
    syncModeLbl_.setLookAndFeel(nullptr);
    syncModeBox_.setLookAndFeel(nullptr);
    midiChLbl_.setLookAndFeel(nullptr);
    midiChBox_.setLookAndFeel(nullptr);
    driftLbl_.setLookAndFeel(nullptr);
    driftSlider_.setLookAndFeel(nullptr);
    midiStartSectionLbl_.setLookAndFeel(nullptr);
    midiStartEnableBtn_.setLookAndFeel(nullptr);
    midiStartChLbl_.setLookAndFeel(nullptr);
    midiStartChBox_.setLookAndFeel(nullptr);
    midiStartHintLbl_.setLookAndFeel(nullptr);
    stopCcSectionLbl_.setLookAndFeel(nullptr);
    stopCcEnableBtn_.setLookAndFeel(nullptr);
    stopCcNumLbl_.setLookAndFeel(nullptr);
    stopCcNumSlider_.setLookAndFeel(nullptr);
    stopCcChLbl_.setLookAndFeel(nullptr);
    stopCcChBox_.setLookAndFeel(nullptr);
    stopCcHintLbl_.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void RhythmboxSettingsOverlay::setStandaloneAppMode(bool standalone) noexcept
{
    if (isStandaloneApp_ != standalone)
    {
        isStandaloneApp_ = standalone;
        syncModeLbl_.setVisible(!isStandaloneApp_);
        syncModeBox_.setVisible(!isStandaloneApp_);
        resized();
        repaint();
    }
}

void RhythmboxSettingsOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xaa0b0a09));

    const auto card = cardBounds_.toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.68f));
    g.fillRoundedRectangle(card.translated(0.0f, 4.0f), 8.0f);

    juce::ColourGradient bg(juce::Colour(0xff292522), card.getX(), card.getY(),
                            juce::Colour(0xff191614), card.getX(), card.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(card, 8.0f);

    g.setColour(juce::Colour(0xff5b5147));
    g.drawRoundedRectangle(card, 8.0f, 1.4f);

    g.setColour(juce::Colour(0xff3d362f));
    for (float y : {div1Y_, div2Y_, div3Y_})
    {
        if (y > card.getY() && y < card.getBottom())
        {
            g.drawHorizontalLine(static_cast<int>(std::round(y)), card.getX() + 14.0f, card.getRight() - 14.0f);
        }
    }
}

void RhythmboxSettingsOverlay::resized()
{
    const int cardW = std::min(560, std::max(320, getWidth() - 40));
    const int desiredH = isStandaloneApp_ ? 344 : 374;
    const int cardH = std::min(desiredH, std::max(280, getHeight() - 24));
    cardBounds_ = getLocalBounds().withSizeKeepingCentre(cardW, cardH);

    auto area = cardBounds_.reduced(18, 12);
    auto hdr = area.removeFromTop(26);
    closeBtn_.setBounds(hdr.removeFromRight(28));
    titleLabel_.setBounds(hdr);

    area.removeFromTop(6);
    div1Y_ = static_cast<float>(area.getY());
    area.removeFromTop(8);

    // Section 1: Engine & MIDI
    engineSectionLbl_.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);
    if (!isStandaloneApp_)
    {
        auto r0 = area.removeFromTop(22);
        syncModeLbl_.setBounds(r0.removeFromLeft(118));
        syncModeBox_.setBounds(r0.removeFromLeft(142));
        r0.removeFromLeft(16);
        midiChLbl_.setBounds(r0.removeFromLeft(116));
        midiChBox_.setBounds(r0.removeFromLeft(116));
    }
    else
    {
        auto r0 = area.removeFromTop(22);
        midiChLbl_.setBounds(r0.removeFromLeft(118));
        midiChBox_.setBounds(r0.removeFromLeft(142));
    }
    area.removeFromTop(6);
    {
        auto rDrift = area.removeFromTop(22);
        driftLbl_.setBounds(rDrift.removeFromLeft(118));
        driftSlider_.setBounds(rDrift.removeFromLeft(220));
    }

    area.removeFromTop(8);
    div2Y_ = static_cast<float>(area.getY());
    area.removeFromTop(8);

    // Section 2: MIDI Key Start
    midiStartSectionLbl_.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);
    midiStartEnableBtn_.setBounds(area.removeFromTop(22));
    area.removeFromTop(5);
    {
        auto r1 = area.removeFromTop(22);
        midiStartChLbl_.setBounds(r1.removeFromLeft(118));
        midiStartChBox_.setBounds(r1.removeFromLeft(116));
    }
    area.removeFromTop(4);
    midiStartHintLbl_.setBounds(area.removeFromTop(16));

    area.removeFromTop(8);
    div3Y_ = static_cast<float>(area.getY());
    area.removeFromTop(8);

    // Section 3: Rhythm Stop CC
    stopCcSectionLbl_.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);
    stopCcEnableBtn_.setBounds(area.removeFromTop(22));
    area.removeFromTop(5);
    {
        auto r2 = area.removeFromTop(22);
        stopCcNumLbl_.setBounds(r2.removeFromLeft(74));
        stopCcNumSlider_.setBounds(r2.removeFromLeft(198));
        r2.removeFromLeft(14);
        stopCcChLbl_.setBounds(r2.removeFromLeft(84));
        stopCcChBox_.setBounds(r2.removeFromLeft(112));
    }
    area.removeFromTop(4);
    stopCcHintLbl_.setBounds(area.removeFromTop(16));
}

void RhythmboxSettingsOverlay::mouseDown(const juce::MouseEvent& event)
{
    if (!cardBounds_.contains(event.getPosition()) && closeFn_)
    {
        closeFn_();
    }
}

bool RhythmboxSettingsOverlay::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && closeFn_)
    {
        closeFn_();
        return true;
    }
    return false;
}

//==============================================================================
// RhythmboxAudioProcessorEditor
//==============================================================================
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
          },
          ke310::ui::RhythmboxPanelMode::Standalone,
          [this](bool held)
          {
              processor_.setRhythmStartHeldFromUi(held);
          },
          [this](bool held)
          {
              processor_.setRhythmStopHeldFromUi(held);
          },
          [this]()
          {
              return processor_.consumeMidiFlashMask();
          },
          owner.isStandaloneMode()),
      settingsOverlay_(
          owner.apvts(),
          [this]()
          {
              settingsGearBtn_.setToggleState(false, juce::dontSendNotification);
              settingsOverlay_.setVisible(false);
          },
          owner.isStandaloneMode())
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

    settingsGearBtn_.onClick = [this]()
    {
        const bool open = settingsGearBtn_.getToggleState();
        settingsOverlay_.setVisible(open);
        if (open)
        {
            settingsOverlay_.toFront(true);
        }
    };
    addAndMakeVisible(settingsGearBtn_);

    addAndMakeVisible(panel_);
    addChildComponent(settingsOverlay_);

    setResizable(true, true);
    setResizeLimits(760, 400, 1320, 760);
    setSize(860, 460);

    syncUiNow();
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

void RhythmboxAudioProcessorEditor::syncUiNow()
{
    timerCallback();
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

    const bool standalone = processor_.isStandaloneMode();
    panel_.setStandaloneAppMode(standalone);
    settingsOverlay_.setStandaloneAppMode(standalone);
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
    presetBarBounds_ = area.removeFromTop(30);

    auto bar = presetBarBounds_.reduced(8, 3);
    presetLabel_.setBounds(bar.removeFromLeft(58));
    prevProgramBtn_.setBounds(bar.removeFromLeft(24));
    bar.removeFromLeft(3);
    programBox_.setBounds(bar.removeFromLeft(std::min(210, bar.getWidth() / 2)));
    bar.removeFromLeft(3);
    nextProgramBtn_.setBounds(bar.removeFromLeft(24));
    settingsGearBtn_.setBounds(bar.removeFromRight(26));

    panel_.setBounds(area);
    settingsOverlay_.setBounds(area);
}

} // namespace ke310::rhythmbox_plugin
