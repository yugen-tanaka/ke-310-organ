// Reusable KE-310 hardware-styled JUCE 8 UI panel for KE Rhythmbox.
// Designed for both standalone KERhythmbox and Stage 2 KE310Organ embedding.
#pragma once

#include "dsp/rhythmbox/RhythmboxEngine.hpp"
#include "plugin_rhythmbox/RhythmboxPluginParams.hpp"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace ke310::ui
{

class RhythmboxLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RhythmboxLookAndFeel();

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    void drawComboBox(juce::Graphics& g,
                      int width, int height,
                      bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    juce::Font getComboBoxFont(juce::ComboBox& box) override;

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;
};

class PatternPushButton final : public juce::ToggleButton
{
public:
    enum class CapColor
    {
        White,
        Black,
        Red
    };

    using PressStateCallback = std::function<void(bool isDown)>;

    PatternPushButton(CapColor capColor, PressStateCallback onPressStateChanged = {});
    PatternPushButton(bool isBlackCap, PressStateCallback onPressStateChanged)
        : PatternPushButton(isBlackCap ? CapColor::Black : CapColor::White, std::move(onPressStateChanged))
    {
    }

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    bool hitTest(int x, int y) override;

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

private:
    juce::Rectangle<float> computeTopSquareRect(bool latched, bool heldDown) const noexcept;
    CapColor capColor_ = CapColor::White;
    PressStateCallback pressStateFn_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternPushButton)
};

class RhythmTransportButton final : public juce::Button
{
public:
    enum class IconType
    {
        Play,
        Stop
    };

    using HoldCallback = std::function<void(bool isDown)>;

    RhythmTransportButton(const juce::String& name,
                          IconType icon,
                          HoldCallback onHoldChanged = {});

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void setActiveIndicator(bool active) noexcept;
    void triggerHoldForTest(bool isDown)
    {
        forcedHold_ = isDown;
        if (holdFn_)
        {
            holdFn_(isDown);
        }
        repaint();
    }

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

private:
    IconType icon_ = IconType::Play;
    bool forcedHold_ = false;
    bool activeIndicator_ = false;
    HoldCallback holdFn_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RhythmTransportButton)
};

class AuditionPadButton final : public juce::TextButton
{
public:
    AuditionPadButton(const juce::String& voiceLabel, int gmMidiNote);

    void triggerFlash() noexcept { flashLevel_ = 1.0f; repaint(); }
    bool decayFlash() noexcept;

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

private:
    juce::String voiceLabel_;
    int gmMidiNote_ = 36;
    float flashLevel_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuditionPadButton)
};

enum class RhythmboxPanelMode
{
    Standalone,
    OrganEmbedded
};

class RhythmboxPanel final : public juce::Component,
                             private juce::Timer
{
public:
    using AuditionCallback = std::function<void(ke310::rhythmbox::VoiceId, float)>;
    using TelemetryCallback = std::function<ke310::rhythmbox::RhythmboxTelemetry()>;
    using HoldCallback = std::function<void(bool held)>;

    RhythmboxPanel(juce::AudioProcessorValueTreeState& apvts,
                   AuditionCallback onAuditionVoice,
                   TelemetryCallback getTelemetry,
                   RhythmboxPanelMode mode = RhythmboxPanelMode::Standalone,
                   HoldCallback onStartHeld = {},
                   HoldCallback onStopHeld = {});
    ~RhythmboxPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Accessors helpful for automated GUI tests
    RhythmboxPanelMode mode() const noexcept { return mode_; }
    juce::ToggleButton& startStopButton() noexcept { return startStopBtn_; }
    juce::ToggleButton& multiPatternButton() noexcept { return multiPatternBtn_; }
    juce::ToggleButton& patternButton(std::size_t index) noexcept { return *patternButtons_[index]; }
    juce::TextButton& auditionPad(std::size_t index) noexcept { return *padButtons_[index]; }
    PatternPushButton& breakButton() noexcept { return *breakBtn_; }
    PatternPushButton& variationSquareButton() noexcept { return *variationSquareBtn_; }
    PatternPushButton& rhythmButton() noexcept { return *rhythmBtn_; }
    RhythmTransportButton& rhythmStartButton() noexcept { return *rhythmStartBtn_; }
    RhythmTransportButton& rhythmStopButton() noexcept { return *rhythmStopBtn_; }
    void handlePatternPressState(std::size_t idx, bool isPressed);

private:
    void timerCallback() override;
    void paintStandalone(juce::Graphics& g);
    void paintOrganEmbedded(juce::Graphics& g);
    void drawTempoLamp(juce::Graphics& g, juce::Rectangle<int> bounds) const;
    void handlePatternButtonClick(std::size_t clickedIndex);
    void handleMultiPatternToggle();
    void applyPatternMaskToHost(std::uint16_t mask);
    void setParameterBoolNotifyingHost(const char* paramId, bool value);

    juce::AudioProcessorValueTreeState& apvts_;
    AuditionCallback auditionFn_;
    TelemetryCallback telemetryFn_;
    RhythmboxPanelMode mode_ = RhythmboxPanelMode::Standalone;
    HoldCallback startHoldFn_;
    HoldCallback stopHoldFn_;
    RhythmboxLookAndFeel lookAndFeel_;

    // Top bar controls (Standalone)
    juce::ToggleButton startStopBtn_{"START / STOP"};

    // Left 2x8 rhythm matrix
    std::array<std::unique_ptr<PatternPushButton>, ke310::rhythmbox::kNumPatterns> patternButtons_;

    // 3 Square action buttons (OrganEmbedded: BREAK, VARIATION, RHYTHM) + START / STOP transport buttons
    std::unique_ptr<PatternPushButton> breakBtn_;
    std::unique_ptr<PatternPushButton> variationSquareBtn_;
    std::unique_ptr<PatternPushButton> rhythmBtn_;
    std::unique_ptr<RhythmTransportButton> rhythmStartBtn_;
    std::unique_ptr<RhythmTransportButton> rhythmStopBtn_;

    // Right 3 large rotary knobs (TONE, VOLUME, TEMPO)
    juce::Slider toneSlider_{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox};
    juce::Slider volumeSlider_{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox};
    juce::Slider bpmSlider_{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};

    // Control & Sync strip
    juce::ToggleButton variationBtn_{"VARIATION"};
    juce::ToggleButton multiPatternBtn_{"Multi-Pattern (Wired-OR)"};
    juce::Label syncLbl_{{}, "SYNC"};
    juce::ComboBox syncModeBox_;
    juce::Label midiChLbl_{{}, "MIDI CH"};
    juce::ComboBox midiChBox_;
    juce::Label driftLbl_{{}, "DRIFT"};
    juce::Slider driftSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};

    // Bottom 11 GM drum audition pads (Standalone)
    std::array<std::unique_ptr<AuditionPadButton>, ke310::rhythmbox::kNumVoices> padButtons_;

    // Cached layout frames for clean paint()
    juce::Rectangle<int> headerBounds_;
    juce::Rectangle<int> tempoLampBounds_;
    juce::Rectangle<int> statusTextBounds_;
    juce::Rectangle<int> meterBounds_;
    juce::Rectangle<int> deckBounds_;
    juce::Rectangle<int> matrixCardBounds_;
    juce::Rectangle<int> wellBounds_;
    std::array<juce::Rectangle<int>, ke310::rhythmbox::kNumPatterns> patternLabelBounds_{};
    juce::Rectangle<int> lampMultiCardBounds_;
    juce::Rectangle<int> lampMultiHeaderBounds_;
    juce::Rectangle<int> knobCardBounds_;
    juce::Rectangle<int> knobBracketBounds_;
    std::array<juce::Rectangle<int>, 3> knobLabelBounds_{};
    juce::Rectangle<int> toneSubLabelBounds_;
    juce::Rectangle<int> tempoModeLabelBounds_;
    juce::Rectangle<int> actionCardBounds_;
    juce::Rectangle<int> actionWellBounds_;
    juce::Rectangle<int> actionBracketBounds_;
    std::array<juce::Rectangle<int>, 3> actionLabelBounds_{};
    juce::Rectangle<int> transportWellBounds_;
    std::array<juce::Rectangle<int>, 2> transportLabelBounds_{};
    juce::Rectangle<int> controlBarBounds_;
    juce::Rectangle<int> padBayBounds_;
    juce::Rectangle<int> padBracketBounds_;

    ke310::rhythmbox::RhythmboxTelemetry telemetry_{};
    float meterSmooth_ = 0.0f;
    float meterDb_ = -96.0f;
    bool variationSupported_ = true;
    bool updatingPatterns_ = false;
    std::uint16_t heldPatternMask_ = 0u;
    std::uint16_t chordPatternMask_ = 0u;
    bool chordGestureActive_ = false;

    // Declared LAST so attachments unlink before widgets are destroyed
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RhythmboxPanel)
};

} // namespace ke310::ui
