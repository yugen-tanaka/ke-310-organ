#pragma once

#include "PluginParams.hpp"
#include "PluginProcessor.hpp"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
#include <vector>

namespace ke310::plugin
{

enum class TabColor
{
    White,
    Yellow,
    Red,
    Green,
    Blue,
    Black
};

class RockerTabButton final : public juce::ToggleButton
{
public:
    RockerTabButton(const juce::String& upperText,
                    const juce::String& footageText,
                    TabColor color,
                    const juce::String& tooltip = {});

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

private:
    juce::String upperText_;
    juce::String footageText_;
    TabColor color_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RockerTabButton)
};

class KE310LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    KE310LookAndFeel();

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

class TelemetryDisplay final : public juce::Component
{
public:
    TelemetryDisplay() = default;

    void updateTelemetry(bool ensembleOn,
                         int activeUpper,
                         int activeLower,
                         int activePedalMidi,
                         float mainRms,
                         float rotorRms,
                         float statRms);

    static juce::String formatVoiceText(int activeUpper, int activeLower, int activePedalMidi);

    void paint(juce::Graphics& g) override;

private:
    static float rmsToDb(float rms) noexcept;
    static float dbToNorm(float db) noexcept;
    static void drawMeter(juce::Graphics& g,
                          juce::Rectangle<int> bounds,
                          const juce::String& label,
                          float normLevel,
                          float dbValue,
                          juce::Colour barColour);

    bool ensembleOn_ = false;
    int activeUpper_ = 0;
    int activeLower_ = 0;
    int activePedalMidi_ = -1;
    float mainDb_ = -120.0f;
    float rotorDb_ = -120.0f;
    float statDb_ = -120.0f;
    float mainSmooth_ = 0.0f;
    float rotorSmooth_ = 0.0f;
    float statSmooth_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TelemetryDisplay)
};

class KE310AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit KE310AudioProcessorEditor(KE310AudioProcessor& owner);
    ~KE310AudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct KnobSlot
    {
        const char* paramId = nullptr;
        juce::String label;
        std::unique_ptr<juce::Slider> slider;
        juce::Rectangle<int> cellBounds;
    };

    struct RockerTabSpec
    {
        const char* paramId = nullptr;
        const char* upperText = "";
        const char* footageText = "";
        TabColor color = TabColor::White;
    };

    struct TabGroup
    {
        int tier = 0;
        juce::String title;
        std::vector<std::unique_ptr<RockerTabButton>> tabs;
        juce::Rectangle<int> groupBounds;
    };

    void timerCallback() override;
    void initHeaderControls();
    void initKnobs();
    void initRockerSections();
    void initMidiAndCalibrationBay();
    void clearAllTabs();

    KE310LookAndFeel lookAndFeel_;
    KE310AudioProcessor& processor_;

    // Header bar
    juce::TextButton prevPresetBtn_{"<"};
    juce::ComboBox presetBox_;
    juce::TextButton nextPresetBtn_{">"};
    juce::TextButton allOffBtn_{"ALL OFF"};
    TelemetryDisplay telemetry_;

    // Row 2: 10 hardware rotary knobs
    std::vector<KnobSlot> knobs_;

    // Row 3: 35 color-coded hardware rocker tabs in 5 sections
    std::vector<TabGroup> tabGroups_;
    std::vector<const char*> allTabParamIds_;

    // Row 4 Left: MIDI Routing & Manual Assignment (12 parameters)
    juce::Label midiModeLbl_{{}, "MIDI Mode"};
    juce::ComboBox midiModeBox_;
    juce::Label splitInputChLbl_{{}, "Split Input Ch"};
    juce::ComboBox splitInputChBox_;
    juce::Label upperChLbl_{{}, "Upper Ch"};
    juce::ComboBox upperChBox_;
    juce::Label lowerChLbl_{{}, "Lower Ch"};
    juce::ComboBox lowerChBox_;
    juce::Label pedalChLbl_{{}, "Pedal Ch"};
    juce::ComboBox pedalChBox_;

    juce::Label upperOctLbl_{{}, "Upper Oct"};
    juce::Slider upperOctSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Label lowerOctLbl_{{}, "Lower Oct"};
    juce::Slider lowerOctSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Label pedalOctLbl_{{}, "Pedal Oct"};
    juce::Slider pedalOctSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Label pedalSplitMaxLbl_{{}, "Pedal Max Key"};
    juce::Slider pedalSplitMaxSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Label lowerSplitMaxLbl_{{}, "Lower Max Key"};
    juce::Slider lowerSplitMaxSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};

    juce::ToggleButton foldPedalBtn_{"Fold Pedal to C2-C3"};
    juce::ToggleButton clampKeyRangeBtn_{"Clamp Physical Key Range (U:49 / L:49 / P:13)"};

    // Row 4 Right: Tuning, Expression & Rotary
    juce::ToggleButton exprLoudnessBtn_{"Expression Loudness Comp (450 Hz)"};

    juce::Label a4HzLbl_{{}, "Master A4 (Hz)"};
    juce::Slider a4HzSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};

    juce::Label rotaryHeaderLbl_{{}, "ROTARY DRUM"};
    juce::Label rotaryStatusLbl_{{}, "BRAKE  0.00 Hz"};
    juce::Label rotarySlowLbl_{{}, "Rotary Slow (Hz)"};
    juce::Slider rotarySlowSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Label rotaryFastLbl_{{}, "Rotary Fast (Hz)"};
    juce::Slider rotaryFastSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Rectangle<int> rotarySectionBounds_;
    juce::String lastRotaryStatusText_;
    juce::Colour lastRotaryStatusColour_{0xffb0a798};

    // Cached layout frames for clean paint() pass
    juce::Rectangle<int> headerBounds_;
    juce::Rectangle<int> knobRailBounds_;
    juce::Rectangle<int> tabConsoleBounds_;
    juce::Rectangle<int> tabTier0Bounds_;
    juce::Rectangle<int> tabTier1Bounds_;
    juce::Rectangle<int> lowerBayBounds_;
    juce::Rectangle<int> midiCardBounds_;
    juce::Rectangle<int> calCardBounds_;

    // Declared last so attachments unlink before widgets are destroyed
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KE310AudioProcessorEditor)
};

} // namespace ke310::plugin
