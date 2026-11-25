#include "ui/RhythmboxPanel.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <utility>

namespace ke310::ui
{

namespace
{

namespace rb_palette
{
const juce::Colour panelTop{0xff282422};
const juce::Colour panelBot{0xff181614};
const juce::Colour cardBg{0xff221f1c};
const juce::Colour cardBorder{0xff4a423a};
const juce::Colour trayBg{0xff11100e};
const juce::Colour silverTrim{0xffc6c2b8};
const juce::Colour creamText{0xffeae4d7};
const juce::Colour mutedText{0xffb0a798};
const juce::Colour amberLed{0xffffb432};
const juce::Colour pinkRedCore{0xffff345f};
const juce::Colour pinkRedHi{0xffff9db5};
const juce::Colour pinkRedHalo{0xffff2856};
const juce::Colour runLed{0xffff5268};
const juce::Colour tealAccent{0xff148f77};
} // namespace rb_palette

inline constexpr std::array<const char*, ke310::rhythmbox::kNumPatterns> kHardwareSilkscreenNames = {{
    "WALTZ", "BALLAD", "MARCH", "TANGO", "ROCK I", "ROCK II", "ROCK III", "BOSSANOVA",
    "SWING I", "SWING II", "SWING III", "SAMBA", "MAMBO", "BEGUINE", "CHA-CHA", "RHUMBA"
}};

void drawSectionBracket(juce::Graphics& g,
                        juce::Rectangle<int> area,
                        const juce::String& title,
                        float fontSize = 10.5f)
{
    if (area.getWidth() < 30 || area.getHeight() < 12)
    {
        return;
    }

    const juce::Font font(juce::FontOptions(fontSize, juce::Font::bold));
    g.setFont(font);
    const int textW = static_cast<int>(std::ceil(juce::TextLayout::getStringWidth(font, title))) + 12;
    const int cy = area.getCentreY();
    const int x0 = area.getX() + 4;
    const int x1 = area.getRight() - 4;
    const int cx = area.getCentreX();
    const int leftLineEnd = std::max(x0, cx - textW / 2);
    const int rightLineStart = std::min(x1, cx + textW / 2);

    g.setColour(rb_palette::creamText.withAlpha(0.65f));
    if (leftLineEnd > x0 + 2)
    {
        g.drawHorizontalLine(cy, static_cast<float>(x0), static_cast<float>(leftLineEnd));
        g.drawVerticalLine(x0, static_cast<float>(cy), static_cast<float>(area.getBottom() - 1));
    }
    if (rightLineStart < x1 - 2)
    {
        g.drawHorizontalLine(cy, static_cast<float>(rightLineStart), static_cast<float>(x1));
        g.drawVerticalLine(x1 - 1, static_cast<float>(cy), static_cast<float>(area.getBottom() - 1));
    }

    g.setColour(rb_palette::creamText);
    g.drawFittedText(title, area, juce::Justification::centred, 1, 0.75f);
}

struct CapPalette
{
    juce::Colour fwTop;
    juce::Colour fwBot;
    juce::Colour fwStroke;
    juce::Colour bevelCol;
    juce::Colour topHi;
    juce::Colour topLo;
    juce::Colour topStroke;
    juce::Colour bowlLo;
    juce::Colour bowlHi;
    juce::Colour arcStroke;
    bool drawGlossyArc = false;
};

CapPalette getCapPalette(PatternPushButton::CapColor color, bool latched, float hoverBoost) noexcept
{
    switch (color)
    {
        case PatternPushButton::CapColor::Black:
            return {
                latched ? juce::Colour(0xff131110) : juce::Colour(0xff1f1d1b),
                juce::Colour(0xff070606),
                juce::Colour(0xff383330).withAlpha(latched ? 0.35f : 0.75f),
                juce::Colour(latched ? 0xff36322f : 0xff5a544e),
                (latched ? juce::Colour(0xff151312) : juce::Colour(0xff232120)).brighter(hoverBoost),
                latched ? juce::Colour(0xff0a0909) : juce::Colour(0xff11100f),
                juce::Colour(latched ? 0xff3c3733 : 0xff4b4641),
                juce::Colour(0xff040404),
                juce::Colour(latched ? 0xff1e1c1b : 0xff2d2a28),
                juce::Colour(0xff6b655f).withAlpha(latched ? 0.38f : 0.60f),
                true
            };
        case PatternPushButton::CapColor::Red:
            return {
                latched ? juce::Colour(0xffa31616) : juce::Colour(0xffc61c1c),
                latched ? juce::Colour(0xff620808) : juce::Colour(0xff750b0b),
                juce::Colour(0xff520909).withAlpha(0.75f),
                juce::Colour(latched ? 0xffe25353 : 0xffff8383),
                (latched ? juce::Colour(0xffbe1b1b) : juce::Colour(0xffe22727)).brighter(hoverBoost),
                latched ? juce::Colour(0xff8f1010) : juce::Colour(0xffb11515),
                juce::Colour(latched ? 0xff7f1313 : 0xfff25c5c),
                juce::Colour(0xff680909),
                juce::Colour(latched ? 0xffd63030 : 0xfff44e4e),
                juce::Colour(0xffffbdbd).withAlpha(latched ? 0.38f : 0.62f),
                true
            };
        case PatternPushButton::CapColor::White:
        default:
            return {
                latched ? juce::Colour(0xffb4aa95) : juce::Colour(0xffd5ccb6),
                latched ? juce::Colour(0xff8c8370) : juce::Colour(0xffa69c86),
                juce::Colour(0xff7d7462).withAlpha(0.65f),
                juce::Colour(latched ? 0xffebe4d3 : 0xfffffcf2),
                (latched ? juce::Colour(0xffdfd6c1) : juce::Colour(0xfff7f2e5)).brighter(hoverBoost),
                latched ? juce::Colour(0xffcac0aa) : juce::Colour(0xffe7dec9),
                juce::Colour(latched ? 0xffa69c87 : 0xfffffef8),
                juce::Colour(latched ? 0xffb3a993 : 0xffccc2ab),
                juce::Colour(latched ? 0xffe8e0cd : 0xfffffdf6),
                juce::Colour(0xff9c927d).withAlpha(0.18f),
                false
            };
    }
}

} // namespace

//==============================================================================
// RhythmboxLookAndFeel
//==============================================================================
RhythmboxLookAndFeel::RhythmboxLookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1b1816));
    setColour(juce::ComboBox::textColourId, rb_palette::creamText);
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff6b6054));
    setColour(juce::ComboBox::arrowColourId, rb_palette::amberLed);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1e1b18));
    setColour(juce::PopupMenu::textColourId, rb_palette::creamText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, rb_palette::amberLed.withAlpha(0.28f));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::Label::textColourId, rb_palette::creamText);
    setColour(juce::Slider::textBoxTextColourId, rb_palette::creamText);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff141210));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff494139));
}

void RhythmboxLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                            int x, int y, int width, int height,
                                            float sliderPosProportional,
                                            float rotaryStartAngle,
                                            float rotaryEndAngle,
                                            juce::Slider& slider)
{
    juce::ignoreUnused(slider);

    const float radius = std::max(10.0f, static_cast<float>(std::min(width, height)) * 0.5f - 7.0f);
    const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Hardware white C-arc + center notch tick at 12 o'clock
    const float arcRadius = radius + 3.0f;
    juce::Path scaleArc;
    scaleArc.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(rb_palette::creamText.withAlpha(0.82f));
    g.strokePath(scaleArc, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (float endAng : {rotaryStartAngle, 0.5f * (rotaryStartAngle + rotaryEndAngle), rotaryEndAngle})
    {
        const float ta = endAng - juce::MathConstants<float>::halfPi;
        g.drawLine(cx + std::cos(ta) * (arcRadius - 3.5f), cy + std::sin(ta) * (arcRadius - 3.5f),
                   cx + std::cos(ta) * (arcRadius + 0.8f), cy + std::sin(ta) * (arcRadius + 0.8f), 1.2f);
    }

    // Outer molded skirt with soft drop shadow
    const float bodyR = radius * 0.86f;
    g.setColour(juce::Colours::black.withAlpha(0.62f));
    g.fillEllipse(cx - bodyR + 1.0f, cy - bodyR + 2.5f, bodyR * 2.0f, bodyR * 2.0f);

    juce::ColourGradient skirtGrad(juce::Colour(0xff282522), cx, cy - bodyR,
                                   juce::Colour(0xff0c0b0a), cx, cy + bodyR, false);
    g.setGradientFill(skirtGrad);
    g.fillEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour(juce::Colour(0xff47413c));
    g.drawEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // Black center cap with white aluminum trim ring (authentic KE-310 knob)
    const float capR = bodyR * 0.68f;
    juce::ColourGradient capGrad(juce::Colour(0xff201e1c), cx, cy - capR,
                                 juce::Colour(0xff090808), cx, cy + capR, false);
    g.setGradientFill(capGrad);
    g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
    g.setColour(juce::Colour(0xffeae6dc));
    g.drawEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.3f);

    const float pa = angle - juce::MathConstants<float>::halfPi;
    g.setColour(juce::Colour(0xfff7f4ec));
    g.drawLine(cx + std::cos(pa) * (capR * 0.28f), cy + std::sin(pa) * (capR * 0.28f),
               cx + std::cos(pa) * (capR * 0.94f), cy + std::sin(pa) * (capR * 0.94f), 2.1f);
}

void RhythmboxLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                            int x, int y, int width, int height,
                                            float sliderPos,
                                            float minSliderPos,
                                            float maxSliderPos,
                                            juce::Slider::SliderStyle style,
                                            juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const float trackY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float xStart = static_cast<float>(x) + 4.0f;
    const float xEnd = static_cast<float>(x + width) - 4.0f;
    g.setColour(juce::Colour(0xff100f0e));
    g.fillRoundedRectangle(xStart, trackY - 2.5f, std::max(2.0f, xEnd - xStart), 5.0f, 2.5f);

    const float fillW = juce::jlimit(0.0f, std::max(0.0f, xEnd - xStart), sliderPos - xStart);
    g.setColour(rb_palette::amberLed.withAlpha(0.82f));
    g.fillRoundedRectangle(xStart, trackY - 2.5f, fillW, 5.0f, 2.5f);

    const float thumbX = juce::jlimit(xStart, xEnd, sliderPos);
    g.setColour(rb_palette::creamText);
    g.fillEllipse(thumbX - 5.5f, trackY - 5.5f, 11.0f, 11.0f);
    g.setColour(juce::Colour(0xff1a1714));
    g.drawEllipse(thumbX - 5.5f, trackY - 5.5f, 11.0f, 11.0f, 1.0f);
}

void RhythmboxLookAndFeel::drawComboBox(juce::Graphics& g,
                                        int width, int height,
                                        bool isButtonDown,
                                        int buttonX, int buttonY, int buttonW, int buttonH,
                                        juce::ComboBox& box)
{
    juce::ignoreUnused(isButtonDown, box);
    const auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
    g.setColour(juce::Colour(0xff181512));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xff5b5147));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    const float cx = static_cast<float>(buttonX + buttonW / 2);
    const float cy = static_cast<float>(buttonY + buttonH / 2);
    juce::Path arrow;
    arrow.startNewSubPath(cx - 3.4f, cy - 1.6f);
    arrow.lineTo(cx + 3.4f, cy - 1.6f);
    arrow.lineTo(cx, cy + 2.4f);
    arrow.closeSubPath();
    g.setColour(rb_palette::amberLed);
    g.fillPath(arrow);
}

juce::Font RhythmboxLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    juce::ignoreUnused(box);
    return juce::Font(juce::FontOptions(11.5f, juce::Font::plain));
}

void RhythmboxLookAndFeel::drawToggleButton(juce::Graphics& g,
                                            juce::ToggleButton& button,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsDown);
    const auto bounds = button.getLocalBounds().toFloat();
    const bool checked = button.getToggleState();

    // Custom pill switch styling for START / STOP and VARIATION, clean LED checkbox for Multi-Pattern
    const juce::String text = button.getButtonText();
    const bool isPillButton = text.startsWithIgnoreCase("START") || text.startsWithIgnoreCase("VARIATION");

    if (isPillButton)
    {
        const bool isStartStop = text.startsWithIgnoreCase("START");
        const juce::Colour litCol = isStartStop ? rb_palette::runLed : rb_palette::amberLed;
        const juce::Colour bg = checked
                                    ? litCol.withAlpha(shouldDrawButtonAsHighlighted ? 0.30f : 0.22f)
                                    : juce::Colour(shouldDrawButtonAsHighlighted ? 0xff302b27 : 0xff221f1c);
        const auto pill = bounds.reduced(1.0f);
        g.setColour(bg);
        g.fillRoundedRectangle(pill, 5.0f);
        g.setColour(checked ? litCol : juce::Colour(0xff5a5046));
        g.drawRoundedRectangle(pill, 5.0f, checked ? 1.5f : 1.0f);

        // LED dot on the left
        const float ledR = 4.2f;
        const float ledCx = pill.getX() + 12.0f;
        const float ledCy = pill.getCentreY();
        if (checked)
        {
            g.setColour(litCol.withAlpha(0.35f));
            g.fillEllipse(ledCx - ledR * 1.9f, ledCy - ledR * 1.9f, ledR * 3.8f, ledR * 3.8f);
        }
        g.setColour(checked ? litCol.brighter(0.25f) : juce::Colour(0xff4a433d));
        g.fillEllipse(ledCx - ledR, ledCy - ledR, ledR * 2.0f, ledR * 2.0f);

        g.setColour(button.isEnabled() ? (checked ? juce::Colours::white : rb_palette::creamText)
                                       : rb_palette::mutedText.withAlpha(0.55f));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawFittedText(text,
                         pill.withTrimmedLeft(20.0f).toNearestInt(),
                         juce::Justification::centred, 1, 0.78f);
        return;
    }

    const float boxSz = 14.0f;
    const auto boxRect = juce::Rectangle<float>(bounds.getX() + 3.0f,
                                                bounds.getCentreY() - boxSz * 0.5f,
                                                boxSz, boxSz);
    g.setColour(juce::Colour(0xff141210));
    g.fillRoundedRectangle(boxRect, 3.0f);
    g.setColour(checked ? rb_palette::amberLed : juce::Colour(0xff6b6054));
    g.drawRoundedRectangle(boxRect, 3.0f, 1.1f);
    if (checked)
    {
        g.setColour(rb_palette::amberLed);
        g.fillRoundedRectangle(boxRect.reduced(3.2f), 1.6f);
    }

    g.setColour(shouldDrawButtonAsHighlighted ? juce::Colours::white : rb_palette::creamText);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawFittedText(text,
                     bounds.withTrimmedLeft(22.0f).toNearestInt(),
                     juce::Justification::centredLeft, 1, 0.66f);
}

void RhythmboxLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(button, backgroundColour);
    const auto r = button.getLocalBounds().toFloat().reduced(0.5f);
    juce::Colour bg(0xff2c2723);
    if (shouldDrawButtonAsDown)
    {
        bg = juce::Colour(0xff473e36);
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        bg = juce::Colour(0xff37312c);
    }
    g.setColour(bg);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(juce::Colour(0xff685d52));
    g.drawRoundedRectangle(r, 4.0f, 1.0f);
}

void RhythmboxLookAndFeel::drawButtonText(juce::Graphics& g,
                                          juce::TextButton& button,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    g.setColour(rb_palette::creamText);
    g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(2), juce::Justification::centred, 1);
}

//==============================================================================
// PatternPushButton
//==============================================================================
PatternPushButton::PatternPushButton(CapColor capColor, PressStateCallback onPressStateChanged)
    : juce::ToggleButton(),
      capColor_(capColor),
      pressStateFn_(std::move(onPressStateChanged))
{
    setClickingTogglesState(true);
}

void PatternPushButton::mouseDown(const juce::MouseEvent& event)
{
    if (!event.mods.isPopupMenu() && pressStateFn_)
    {
        pressStateFn_(true);
    }
    juce::ToggleButton::mouseDown(event);
}

void PatternPushButton::mouseUp(const juce::MouseEvent& event)
{
    juce::ToggleButton::mouseUp(event);
    if (pressStateFn_)
    {
        pressStateFn_(false);
    }
}

juce::Rectangle<float> PatternPushButton::computeTopSquareRect(bool latched, bool heldDown) const noexcept
{
    const auto full = getLocalBounds().toFloat();
    const float w = std::max(12.0f, full.getWidth() - 2.0f);
    const float h = std::max(16.0f, full.getHeight());
    const float sq = std::min(w, h / 1.44f);
    const float extrude = std::max(6.0f, h - sq - 3.0f);
    const float travel = latched ? (heldDown ? 0.78f : 0.68f) : (heldDown ? 0.40f : 0.0f);
    const float yTop = 1.0f + extrude * travel;
    const float xTop = full.getCentreX() - sq * 0.5f;
    return {xTop, yTop, sq, sq};
}

bool PatternPushButton::hitTest(int x, int y)
{
    const auto full = getLocalBounds().toFloat();
    const float w = std::max(12.0f, full.getWidth() - 2.0f);
    const float h = std::max(16.0f, full.getHeight());
    const float sq = std::min(w, h / 1.44f);
    const float extrude = std::max(6.0f, h - sq - 3.0f);
    const float yFrontBot = std::min(h - 1.0f, 1.0f + sq + extrude * 0.90f);
    const auto topRect = computeTopSquareRect(getToggleState(), isDown());
    const auto visibleBounds = juce::Rectangle<float>(topRect.getX(), topRect.getY(), topRect.getWidth(),
                                                      std::max(sq * 0.5f, yFrontBot - topRect.getY()));
    return visibleBounds.contains(static_cast<float>(x), static_cast<float>(y));
}

void PatternPushButton::paintButton(juce::Graphics& g,
                                    bool shouldDrawButtonAsHighlighted,
                                    bool shouldDrawButtonAsDown)
{
    const auto full = getLocalBounds().toFloat();
    const bool latched = getToggleState();
    const float w = std::max(12.0f, full.getWidth() - 2.0f);
    const float h = std::max(16.0f, full.getHeight());
    const float sq = std::min(w, h / 1.44f);
    const float extrude = std::max(6.0f, h - sq - 3.0f);
    const float yFrontBot = std::min(h - 1.0f, 1.0f + sq + extrude * 0.90f);

    const auto topRect = computeTopSquareRect(latched, shouldDrawButtonAsDown);
    const float x0 = topRect.getX();
    const float y0 = topRect.getY();
    const float cornerR = std::clamp(sq * 0.16f, 4.0f, 8.0f);
    const float hoverBoost = (shouldDrawButtonAsHighlighted && !latched) ? 0.04f : 0.0f;

    // 1. Cast shadow beneath front face inside panel cutout slot
    if (!latched)
    {
        const float shH = std::max(2.0f, std::min(4.0f, full.getBottom() - yFrontBot));
        g.setColour(juce::Colours::black.withAlpha(0.70f));
        g.fillRoundedRectangle(x0 + 0.5f, yFrontBot - 1.5f, sq - 1.0f, shH + 1.5f, cornerR * 0.6f);
    }

    // 2. Extruded 3D vertical front wall (tall when OFF/raised, short lip when ON/latched)
    const float frontStartY = y0 + sq * 0.60f;
    const float frontBodyH = std::max(cornerR + 2.0f, yFrontBot - frontStartY);
    const juce::Rectangle<float> frontRect(x0, frontStartY, sq, frontBodyH);
    const CapPalette pal = getCapPalette(capColor_, latched, hoverBoost);

    {
        juce::ColourGradient fwGrad(pal.fwTop, frontRect.getCentreX(), frontRect.getY(),
                                    pal.fwBot, frontRect.getCentreX(), frontRect.getBottom(), false);
        g.setGradientFill(fwGrad);
        g.fillRoundedRectangle(frontRect, cornerR);

        g.setColour(pal.fwStroke);
        g.drawRoundedRectangle(frontRect.reduced(0.35f), cornerR, 0.8f);
    }

    // Crisp horizontal front bevel edge between top square face and vertical front wall
    g.setColour(pal.bevelCol);
    g.drawHorizontalLine(static_cast<int>(std::round(y0 + sq - 0.5f)), x0 + cornerR * 0.45f, x0 + sq - cornerR * 0.45f);

    // 3. Rounded square top face
    {
        juce::ColourGradient topGrad(pal.topHi, x0, y0, pal.topLo, x0 + sq, y0 + sq, false);
        g.setGradientFill(topGrad);
        g.fillRoundedRectangle(topRect, cornerR);

        g.setColour(pal.topStroke);
        g.drawRoundedRectangle(topRect.reduced(0.4f), cornerR, 0.9f);
    }

    // 4. Shallow circular concave finger dimple / bowl centered on square cap
    const float dishR = sq * 0.365f;
    const float cx = topRect.getCentreX();
    const float cy = topRect.getCentreY();
    const juce::Rectangle<float> dishRect(cx - dishR, cy - dishR, dishR * 2.0f, dishR * 2.0f);

    {
        juce::ColourGradient bowlGrad(pal.bowlLo, cx - dishR * 0.65f, cy - dishR * 0.65f,
                                      pal.bowlHi, cx + dishR * 0.72f, cy + dishR * 0.72f,
                                      false);
        g.setGradientFill(bowlGrad);
        g.fillEllipse(dishRect);

        if (pal.drawGlossyArc)
        {
            juce::Path specArc;
            specArc.addCentredArc(cx, cy, dishR * 0.86f, dishR * 0.86f, 0.0f,
                                  juce::MathConstants<float>::pi * 0.55f,
                                  juce::MathConstants<float>::pi * 1.15f, true);
            g.setColour(pal.arcStroke);
            g.strokePath(specArc, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            g.setColour(pal.arcStroke);
            g.drawEllipse(dishRect, 0.7f);
        }
    }

}

//==============================================================================
// RhythmTransportButton -- Cream Mini-Fill-In Play / Stop Buttons
//==============================================================================
RhythmTransportButton::RhythmTransportButton(const juce::String& name,
                                             IconType icon,
                                             HoldCallback onHoldChanged)
    : juce::Button(name),
      icon_(icon),
      holdFn_(std::move(onHoldChanged))
{
}

void RhythmTransportButton::mouseDown(const juce::MouseEvent& event)
{
    if (!event.mods.isPopupMenu())
    {
        if (holdFn_)
        {
            holdFn_(true);
        }
        repaint();
    }
    juce::Button::mouseDown(event);
}

void RhythmTransportButton::mouseUp(const juce::MouseEvent& event)
{
    juce::Button::mouseUp(event);
    if (holdFn_)
    {
        holdFn_(false);
    }
    repaint();
}

void RhythmTransportButton::setActiveIndicator(bool active) noexcept
{
    if (activeIndicator_ != active)
    {
        activeIndicator_ = active;
        repaint();
    }
}

void RhythmTransportButton::paintButton(juce::Graphics& g,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool shouldDrawButtonAsDown)
{
    const bool held = shouldDrawButtonAsDown || forcedHold_;
    const bool glow = held || (icon_ == IconType::Play && activeIndicator_);
    const auto outer = getLocalBounds().toFloat().reduced(1.0f);

    if (glow)
    {
        g.setColour((held ? juce::Colour(0xffffb95c) : rb_palette::runLed).withAlpha(held ? 0.30f : 0.22f));
        g.fillRoundedRectangle(outer.expanded(1.2f), 4.5f);
    }

    // Dark recessed bezel framing cream cap
    g.setColour(juce::Colour(0xff11100e));
    g.fillRoundedRectangle(outer, 3.8f);

    const float pad = held ? 3.0f : 2.1f;
    const auto cap = outer.reduced(pad);
    const float capR = 3.2f;

    if (held)
    {
        juce::ColourGradient activeGrad(juce::Colour(0xfffff6d6), cap.getCentreX(), cap.getCentreY(),
                                        juce::Colour(0xffe6ba6e), cap.getX(), cap.getBottom(), true);
        g.setGradientFill(activeGrad);
        g.fillRoundedRectangle(cap, capR);

        g.setColour(juce::Colour(0xfffff1ca));
        g.drawRoundedRectangle(cap, capR, 1.0f);
    }
    else
    {
        const float boost = shouldDrawButtonAsHighlighted ? 0.06f : 0.0f;
        juce::ColourGradient creamGrad(juce::Colour(0xfff4efe2).brighter(boost), cap.getX(), cap.getY(),
                                       juce::Colour(0xffd5cbb5).brighter(boost), cap.getX(), cap.getBottom(), false);
        g.setGradientFill(creamGrad);
        g.fillRoundedRectangle(cap, capR);

        g.setColour(activeIndicator_ && icon_ == IconType::Play
                        ? rb_palette::runLed.withAlpha(0.85f)
                        : juce::Colour(0xfffffcf4));
        g.drawRoundedRectangle(cap, capR, (activeIndicator_ && icon_ == IconType::Play) ? 1.2f : 0.9f);
        g.setColour(juce::Colour(0xff9c8f78).withAlpha(0.45f));
        g.drawRoundedRectangle(cap.reduced(1.0f), capR - 0.6f, 0.7f);
    }

    const juce::Colour iconCol = (activeIndicator_ && icon_ == IconType::Play && !held)
                                     ? juce::Colour(0xff8c1724)
                                     : juce::Colour(0xff231710);
    g.setColour(iconCol);
    const float s = std::min(cap.getWidth(), cap.getHeight());
    const float cx = cap.getCentreX();
    const float cy = cap.getCentreY();

    if (icon_ == IconType::Play)
    {
        const float r = s * 0.25f;
        const float shiftX = r * 0.14f;
        juce::Path tri;
        tri.addTriangle(cx - r * 0.80f + shiftX, cy - r * 0.96f,
                        cx + r * 1.00f + shiftX, cy,
                        cx - r * 0.80f + shiftX, cy + r * 0.96f);
        g.fillPath(tri);
    }
    else
    {
        const float side = s * 0.42f;
        g.fillRoundedRectangle(cx - side * 0.5f, cy - side * 0.5f, side, side, 1.5f);
    }
}

//==============================================================================
// AuditionPadButton
//==============================================================================
AuditionPadButton::AuditionPadButton(const juce::String& voiceLabel, int gmMidiNote)
    : juce::TextButton(voiceLabel),
      voiceLabel_(voiceLabel),
      gmMidiNote_(gmMidiNote)
{
    setTooltip(voiceLabel + " (GM MIDI Note " + juce::String(gmMidiNote) + ")");
}

bool AuditionPadButton::decayFlash() noexcept
{
    if (flashLevel_ <= 0.0f)
    {
        return false;
    }
    flashLevel_ = std::max(0.0f, flashLevel_ - 0.18f);
    repaint();
    return true;
}

void AuditionPadButton::paintButton(juce::Graphics& g,
                                    bool shouldDrawButtonAsHighlighted,
                                    bool shouldDrawButtonAsDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const float activeGlow = shouldDrawButtonAsDown ? 1.0f : flashLevel_;

    const juce::Colour baseBg(shouldDrawButtonAsHighlighted ? 0xff302b26 : 0xff24201d);
    const juce::Colour fill = baseBg.interpolatedWith(rb_palette::amberLed.withAlpha(0.65f), activeGlow * 0.55f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(activeGlow > 0.05f ? rb_palette::amberLed : juce::Colour(0xff554c44));
    g.drawRoundedRectangle(bounds, 4.0f, activeGlow > 0.05f ? 1.4f : 1.0f);

    // Top tiny LED indicator bar
    const auto strip = juce::Rectangle<float>(bounds.getX() + 6.0f, bounds.getY() + 4.0f,
                                              std::max(4.0f, bounds.getWidth() - 12.0f), 2.5f);
    g.setColour(activeGlow > 0.05f ? rb_palette::amberLed : juce::Colour(0xff3e3731));
    g.fillRoundedRectangle(strip, 1.2f);

    g.setColour(activeGlow > 0.3f ? juce::Colours::white : rb_palette::creamText);
    g.setFont(juce::Font(juce::FontOptions(std::clamp(bounds.getWidth() * 0.17f, 8.2f, 10.2f), juce::Font::bold)));
    g.drawFittedText(voiceLabel_,
                     bounds.withTrimmedTop(7.0f).withTrimmedBottom(12.0f).reduced(1.5f, 0.0f).toNearestInt(),
                     juce::Justification::centred, 1, 0.72f);

    g.setColour(rb_palette::mutedText.withAlpha(0.75f));
    g.setFont(juce::Font(juce::FontOptions(8.2f, juce::Font::plain)));
    g.drawText(juce::String(gmMidiNote_),
               bounds.withTop(bounds.getBottom() - 12.0f).toNearestInt(),
               juce::Justification::centred, false);
}

//==============================================================================
// RhythmboxPanel
//==============================================================================
RhythmboxPanel::RhythmboxPanel(juce::AudioProcessorValueTreeState& apvts,
                               AuditionCallback onAuditionVoice,
                               TelemetryCallback getTelemetry,
                               RhythmboxPanelMode mode,
                               HoldCallback onStartHeld,
                               HoldCallback onStopHeld)
    : apvts_(apvts),
      auditionFn_(std::move(onAuditionVoice)),
      telemetryFn_(std::move(getTelemetry)),
      mode_(mode),
      startHoldFn_(std::move(onStartHeld)),
      stopHoldFn_(std::move(onStopHeld))
{
    setLookAndFeel(&lookAndFeel_);

    // 16 Hardware Concave Square Push-Buttons (2 rows x 8 alternating W/B columns)
    for (std::size_t idx = 0; idx < ke310::rhythmbox::kNumPatterns; ++idx)
    {
        const bool isBlackCap = ((idx % 8u) % 2u) == 1u;
        auto btn = std::make_unique<PatternPushButton>(
            isBlackCap ? PatternPushButton::CapColor::Black : PatternPushButton::CapColor::White,
            [this, idx](bool isDown) { handlePatternPressState(idx, isDown); });
        btn->setTooltip(kHardwareSilkscreenNames[idx]);
        addAndMakeVisible(*btn);
        buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts_, ke310::rhythmbox_plugin::param_ids::kPatternParamIds[idx], *btn));
        btn->onClick = [this, idx]() { handlePatternButtonClick(idx); };
        patternButtons_[idx] = std::move(btn);
    }

    // 3 Large KE-310 Rotary Knobs: TONE, VOLUME, TEMPO
    auto initKnob = [this](juce::Slider& sl, const char* paramId)
    {
        sl.setLookAndFeel(&lookAndFeel_);
        sl.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f,
                               true);
        addAndMakeVisible(sl);
        sliderAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts_, paramId, sl));
    };

    const int textBoxH = (mode_ == RhythmboxPanelMode::OrganEmbedded) ? 16 : 17;
    const int textBoxW = (mode_ == RhythmboxPanelMode::OrganEmbedded) ? 54 : 58;

    toneSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
    toneSlider_.setNumDecimalPlacesToDisplay(2);
    initKnob(toneSlider_, ke310::rhythmbox_plugin::param_ids::rbTone);
    toneSlider_.textFromValueFunction = [](double v)
    {
        return (std::abs(v - 0.5) < 0.005) ? juce::String("FLAT") : juce::String(v, 2);
    };
    toneSlider_.valueFromTextFunction = [](const juce::String& s)
    {
        return s.trim().equalsIgnoreCase("FLAT") ? 0.5 : s.getDoubleValue();
    };
    toneSlider_.updateText();
    toneSlider_.setDoubleClickReturnValue(true, 0.5);
    toneSlider_.setTooltip("Tone: Left = Drums emphasis, Center = Flat hardware calibration, Right = Cymbals emphasis");

    volumeSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
    volumeSlider_.setNumDecimalPlacesToDisplay(2);
    initKnob(volumeSlider_, ke310::rhythmbox_plugin::param_ids::rbVolume);
    volumeSlider_.textFromValueFunction = [](double v)
    {
        return juce::String(static_cast<int>(std::lround(v * 100.0))) + "%";
    };
    volumeSlider_.valueFromTextFunction = [](const juce::String& s)
    {
        return s.containsChar('%') ? (s.getDoubleValue() * 0.01) : s.getDoubleValue();
    };
    volumeSlider_.updateText();
    volumeSlider_.setDoubleClickReturnValue(true, 0.85);
    volumeSlider_.setTooltip("Rhythm Master Output Volume");

    bpmSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
    bpmSlider_.setNumDecimalPlacesToDisplay(1);
    initKnob(bpmSlider_, ke310::rhythmbox_plugin::param_ids::rbBpm);
    bpmSlider_.textFromValueFunction = [](double v) { return juce::String(v, 1); };
    bpmSlider_.updateText();
    bpmSlider_.setDoubleClickReturnValue(true, 106.8);
    bpmSlider_.setTooltip("Internal / Fallback Tempo (40 - 240 BPM)");

    // Shared Multi-Pattern (Wired-OR) toggle
    multiPatternBtn_.setLookAndFeel(&lookAndFeel_);
    multiPatternBtn_.setButtonText(mode_ == RhythmboxPanelMode::OrganEmbedded ? "Multi-Pattern" : "Multi-Pattern (Wired-OR)");
    addAndMakeVisible(multiPatternBtn_);
    buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts_, ke310::rhythmbox_plugin::param_ids::rbMultiPattern, multiPatternBtn_));
    multiPatternBtn_.onClick = [this]() { handleMultiPatternToggle(); };
    multiPatternBtn_.setTooltip("Allow selecting multiple rhythms simultaneously (Wired-OR hardware combinator)");

    if (mode_ == RhythmboxPanelMode::Standalone)
    {
        startStopBtn_.setLookAndFeel(&lookAndFeel_);
        addAndMakeVisible(startStopBtn_);
        buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts_, ke310::rhythmbox_plugin::param_ids::rbEnabled, startStopBtn_));

        variationBtn_.setLookAndFeel(&lookAndFeel_);
        addAndMakeVisible(variationBtn_);
        buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts_, ke310::rhythmbox_plugin::param_ids::rbVariation, variationBtn_));
        variationBtn_.setTooltip("Rhythm Variation (Not available on Tango / Beguine / Rhumba per KE-310 manual)");

        syncLbl_.setJustificationType(juce::Justification::centredRight);
        syncLbl_.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        addAndMakeVisible(syncLbl_);

        syncModeBox_.setLookAndFeel(&lookAndFeel_);
        syncModeBox_.addItemList(ke310::rhythmbox_plugin::makeSyncModeChoices(), 1);
        addAndMakeVisible(syncModeBox_);
        comboAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts_, ke310::rhythmbox_plugin::param_ids::rbSyncMode, syncModeBox_));

        midiChLbl_.setText("MIDI", juce::dontSendNotification);
        midiChLbl_.setJustificationType(juce::Justification::centredRight);
        midiChLbl_.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        addAndMakeVisible(midiChLbl_);

        midiChBox_.setLookAndFeel(&lookAndFeel_);
        midiChBox_.addItemList(ke310::rhythmbox_plugin::makeMidiChannelChoices(), 1);
        addAndMakeVisible(midiChBox_);
        comboAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts_, ke310::rhythmbox_plugin::param_ids::rbMidiCh, midiChBox_));

        driftLbl_.setJustificationType(juce::Justification::centredRight);
        driftLbl_.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        addAndMakeVisible(driftLbl_);

        driftSlider_.setLookAndFeel(&lookAndFeel_);
        driftSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 34, 18);
        driftSlider_.setNumDecimalPlacesToDisplay(2);
        driftSlider_.setDoubleClickReturnValue(true, 1.0);
        addAndMakeVisible(driftSlider_);
        sliderAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts_, ke310::rhythmbox_plugin::param_ids::rbAnalogDrift, driftSlider_));

        // Bottom 11 GM Voice Audition Pads
        for (std::size_t v = 0; v < ke310::rhythmbox::kNumVoices; ++v)
        {
            auto pad = std::make_unique<AuditionPadButton>(ke310::rhythmbox::kVoiceDisplayNames[v],
                                                           ke310::rhythmbox::kPrimaryGmMidiNotes[v]);
            const auto voiceId = static_cast<ke310::rhythmbox::VoiceId>(v);
            AuditionPadButton* rawPad = pad.get();
            pad->onClick = [this, voiceId, rawPad]()
            {
                rawPad->triggerFlash();
                if (auditionFn_)
                {
                    auditionFn_(voiceId, 1.0f);
                }
            };
            addAndMakeVisible(*pad);
            padButtons_[v] = std::move(pad);
        }
    }
    else
    {
        // OrganEmbedded square concave action buttons: BREAK (White), VARIATION (Black), RHYTHM (Red)
        breakBtn_ = std::make_unique<PatternPushButton>(PatternPushButton::CapColor::White);
        breakBtn_->setTooltip("Break: When RHYTHM is ON, rhythm plays from Beat 1 only while Pedal or Lower (C2-C5) is held");
        addAndMakeVisible(*breakBtn_);
        if (apvts_.getParameter("rb_break") != nullptr)
        {
            buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                apvts_, "rb_break", *breakBtn_));
        }

        variationSquareBtn_ = std::make_unique<PatternPushButton>(PatternPushButton::CapColor::Black);
        variationSquareBtn_->setTooltip("Variation (Not available on Tango / Beguine / Rhumba per KE-310 manual)");
        addAndMakeVisible(*variationSquareBtn_);
        buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts_, ke310::rhythmbox_plugin::param_ids::rbVariation, *variationSquareBtn_));

        rhythmBtn_ = std::make_unique<PatternPushButton>(PatternPushButton::CapColor::Red);
        rhythmBtn_->setTooltip("Rhythm: Master ON/OFF for rhythm function (arms START / Lower / Pedal trigger; OFF stops rhythm)");
        addAndMakeVisible(*rhythmBtn_);
        buttonAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts_, ke310::rhythmbox_plugin::param_ids::rbEnabled, *rhythmBtn_));

        rhythmStartBtn_ = std::make_unique<RhythmTransportButton>(
            "START",
            RhythmTransportButton::IconType::Play,
            [this](bool held)
            {
                if (startHoldFn_)
                {
                    startHoldFn_(held);
                }
            });
        rhythmStartBtn_->setTooltip("Start Rhythm (Play): Equivalent to Lower (C2-C5) / Pedal trigger; latches when BREAK is OFF or holds Beat 1 when BREAK is ON");
        addAndMakeVisible(*rhythmStartBtn_);

        rhythmStopBtn_ = std::make_unique<RhythmTransportButton>(
            "STOP",
            RhythmTransportButton::IconType::Stop,
            [this](bool held)
            {
                if (stopHoldFn_)
                {
                    stopHoldFn_(held);
                }
            });
        rhythmStopBtn_->setTooltip("Stop Rhythm: Equivalent to expression-pedal Rhythm Stop switch; stops rhythm & inhibits start while held (keeps RHYTHM armed)");
        addAndMakeVisible(*rhythmStopBtn_);
    }

    if (telemetryFn_)
    {
        telemetry_ = telemetryFn_();
        const float rms = static_cast<float>(telemetry_.outputRms);
        meterSmooth_ = (rms > 1e-6f) ? juce::jlimit(0.0f, 1.0f, (20.0f * std::log10(rms) + 54.0f) / 54.0f) : 0.0f;
    }
    startTimerHz(30);
}

RhythmboxPanel::~RhythmboxPanel()
{
    stopTimer();
    if (startHoldFn_)
    {
        startHoldFn_(false);
    }
    if (stopHoldFn_)
    {
        stopHoldFn_(false);
    }
    startStopBtn_.setLookAndFeel(nullptr);
    toneSlider_.setLookAndFeel(nullptr);
    volumeSlider_.setLookAndFeel(nullptr);
    bpmSlider_.setLookAndFeel(nullptr);
    variationBtn_.setLookAndFeel(nullptr);
    multiPatternBtn_.setLookAndFeel(nullptr);
    syncModeBox_.setLookAndFeel(nullptr);
    midiChBox_.setLookAndFeel(nullptr);
    driftSlider_.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void RhythmboxPanel::setParameterBoolNotifyingHost(const char* paramId, bool value)
{
    if (auto* param = apvts_.getParameter(paramId))
    {
        const float target = value ? 1.0f : 0.0f;
        if (std::abs(param->getValue() - target) > 0.01f)
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(target);
            param->endChangeGesture();
        }
    }
}

void RhythmboxPanel::applyPatternMaskToHost(std::uint16_t mask)
{
    for (std::size_t j = 0; j < ke310::rhythmbox::kNumPatterns; ++j)
    {
        const bool on = (mask & static_cast<std::uint16_t>(1u << j)) != 0u;
        setParameterBoolNotifyingHost(ke310::rhythmbox_plugin::param_ids::kPatternParamIds[j], on);
        patternButtons_[j]->setToggleState(on, juce::dontSendNotification);
    }
}

void RhythmboxPanel::handlePatternPressState(std::size_t idx, bool isPressed)
{
    const std::uint16_t bit = static_cast<std::uint16_t>(1u << idx);
    if (isPressed)
    {
        heldPatternMask_ |= bit;
        if (std::popcount(heldPatternMask_) >= 2)
        {
            chordGestureActive_ = true;
            chordPatternMask_ |= heldPatternMask_;
            if (!updatingPatterns_)
            {
                const juce::ScopedValueSetter<bool> guard(updatingPatterns_, true);
                applyPatternMaskToHost(chordPatternMask_);
            }
        }
    }
    else
    {
        heldPatternMask_ = static_cast<std::uint16_t>(heldPatternMask_ & static_cast<std::uint16_t>(~bit));
        if (heldPatternMask_ == 0u)
        {
            chordGestureActive_ = false;
            chordPatternMask_ = 0u;
        }
    }
}

void RhythmboxPanel::handlePatternButtonClick(std::size_t clickedIndex)
{
    if (updatingPatterns_)
    {
        return;
    }
    const juce::ScopedValueSetter<bool> guard(updatingPatterns_, true);

    if (chordGestureActive_)
    {
        applyPatternMaskToHost(chordPatternMask_);
        return;
    }

    if (!multiPatternBtn_.getToggleState())
    {
        const bool nowOn = patternButtons_[clickedIndex]->getToggleState();
        const std::uint16_t targetMask = nowOn ? static_cast<std::uint16_t>(1u << clickedIndex) : 0u;
        applyPatternMaskToHost(targetMask);
    }
}

void RhythmboxPanel::handleMultiPatternToggle()
{
    if (updatingPatterns_)
    {
        return;
    }
    const juce::ScopedValueSetter<bool> guard(updatingPatterns_, true);

    if (!multiPatternBtn_.getToggleState())
    {
        std::uint16_t targetMask = 0u;
        for (std::size_t j = 0; j < ke310::rhythmbox::kNumPatterns; ++j)
        {
            if (patternButtons_[j]->getToggleState())
            {
                targetMask = static_cast<std::uint16_t>(1u << j);
                break;
            }
        }
        applyPatternMaskToHost(targetMask);
    }
}

void RhythmboxPanel::timerCallback()
{
    if (telemetryFn_)
    {
        telemetry_ = telemetryFn_();
    }

    const float rms = static_cast<float>(telemetry_.outputRms);
    const float norm = (rms > 1e-6f) ? juce::jlimit(0.0f, 1.0f, (20.0f * std::log10(rms) + 54.0f) / 54.0f) : 0.0f;
    meterSmooth_ = std::max(norm, meterSmooth_ * 0.80f);
    meterDb_ = (rms > 1e-6f) ? std::max(-96.0f, 20.0f * std::log10(rms)) : -96.0f;

    bool anyVarCapable = false;
    bool anyActive = false;
    for (std::size_t j = 0; j < ke310::rhythmbox::kNumPatterns; ++j)
    {
        if (patternButtons_[j]->getToggleState())
        {
            anyActive = true;
            if (ke310::rhythmbox::kPatterns[j].hasVariation)
            {
                anyVarCapable = true;
            }
        }
    }
    const bool newVarSupported = !anyActive || anyVarCapable;
    if (newVarSupported != variationSupported_)
    {
        variationSupported_ = newVarSupported;
        if (mode_ == RhythmboxPanelMode::Standalone)
        {
            variationBtn_.setButtonText(variationSupported_ ? "VARIATION" : "VAR (N/A)");
            variationBtn_.setAlpha(variationSupported_ ? 1.0f : 0.58f);
        }
        else if (variationSquareBtn_)
        {
            variationSquareBtn_->setAlpha(variationSupported_ ? 1.0f : 0.58f);
            repaint(actionCardBounds_);
        }
    }

    for (auto& pad : padButtons_)
    {
        if (pad)
        {
            pad->decayFlash();
        }
    }

    if (mode_ == RhythmboxPanelMode::Standalone)
    {
        repaint(headerBounds_);
        repaint(knobCardBounds_);
    }
    else
    {
        if (rhythmStartBtn_)
        {
            rhythmStartBtn_->setActiveIndicator(telemetry_.lamp.isRunning);
        }
        repaint(lampMultiCardBounds_);
        repaint(knobCardBounds_);
    }
}

void RhythmboxPanel::drawTempoLamp(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    const auto lamp = bounds.toFloat();
    const float lampR = std::min(lamp.getWidth(), lamp.getHeight()) * 0.42f;
    const float lx = lamp.getCentreX();
    const float ly = lamp.getCentreY();
    const bool hasPattern = (telemetry_.lamp.totalSteps > 0);
    const bool isFlash = hasPattern && telemetry_.lamp.beatFlash;

    if (isFlash)
    {
        g.setColour(rb_palette::pinkRedHalo.withAlpha(0.45f));
        g.fillEllipse(lx - lampR * 1.95f, ly - lampR * 1.95f, lampR * 3.9f, lampR * 3.9f);
    }
    g.setColour(juce::Colour(0xff0b0a09));
    g.fillEllipse(lx - lampR - 1.5f, ly - lampR - 1.5f, (lampR + 1.5f) * 2.0f, (lampR + 1.5f) * 2.0f);

    if (isFlash)
    {
        juce::ColourGradient ledGrad(rb_palette::pinkRedHi, lx - lampR * 0.25f, ly - lampR * 0.35f,
                                     rb_palette::pinkRedCore, lx + lampR * 0.6f, ly + lampR * 0.7f, true);
        g.setGradientFill(ledGrad);
        g.fillEllipse(lx - lampR, ly - lampR, lampR * 2.0f, lampR * 2.0f);
    }
    else
    {
        g.setColour(hasPattern ? juce::Colour(0xff42131d) : juce::Colour(0xff230d12));
        g.fillEllipse(lx - lampR, ly - lampR, lampR * 2.0f, lampR * 2.0f);
    }
    g.setColour(juce::Colours::white.withAlpha(isFlash ? 0.60f : 0.14f));
    g.fillEllipse(lx - lampR * 0.45f, ly - lampR * 0.55f, lampR * 0.45f, lampR * 0.35f);
}

void RhythmboxPanel::paint(juce::Graphics& g)
{
    if (mode_ == RhythmboxPanelMode::OrganEmbedded)
    {
        paintOrganEmbedded(g);
    }
    else
    {
        paintStandalone(g);
    }
}

void RhythmboxPanel::paintStandalone(juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    juce::ColourGradient bgGrad(rb_palette::panelTop, 0.0f, full.getY(),
                                rb_palette::panelBot, 0.0f, full.getBottom(), false);
    g.setGradientFill(bgGrad);
    g.fillRect(full);

    // Subtle brushed aluminum trim line at top and bottom
    g.setColour(rb_palette::silverTrim.withAlpha(0.28f));
    g.fillRect(full.getX(), full.getY(), full.getWidth(), 1.5f);
    g.fillRect(full.getX(), full.getBottom() - 1.5f, full.getWidth(), 1.5f);

    // Top Header Bar
    {
        const auto hb = headerBounds_.toFloat();
        g.setColour(juce::Colour(0xff141210));
        g.fillRoundedRectangle(hb, 5.0f);
        g.setColour(rb_palette::cardBorder);
        g.drawRoundedRectangle(hb, 5.0f, 1.0f);

        // Title Badge on left
        auto titleBox = headerBounds_.withWidth(std::min(210, headerBounds_.getWidth() / 3)).reduced(10, 3);
        g.setColour(rb_palette::creamText);
        g.setFont(juce::Font(juce::FontOptions(14.5f, juce::Font::bold)));
        g.drawFittedText("KE RHYTHMBOX", titleBox.removeFromTop(18), juce::Justification::centredLeft, 1);
        g.setColour(rb_palette::mutedText);
        g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
        g.drawFittedText("ANALOG RHYTHM SYNTHESIZER", titleBox, juce::Justification::centredLeft, 1);

        drawTempoLamp(g, tempoLampBounds_);

        // Tempo & Step status readout
        const bool isRun = telemetry_.lamp.isRunning;
        const bool hasPattern = (telemetry_.lamp.totalSteps > 0);
        const juce::String bpmStr = juce::String(telemetry_.lamp.effectiveBpm, 1) + " BPM";
        const juce::String stepStr = !hasPattern
                                         ? "NO RHYTHM"
                                         : (isRun ? ("STEP " + juce::String(telemetry_.lamp.currentStep + 1).paddedLeft('0', 2) +
                                                     "/" + juce::String(telemetry_.lamp.totalSteps))
                                                  : "READY");
        const int syncIdx = syncModeBox_.getSelectedId() - 1;
        const juce::String syncTag = (syncIdx == 2) ? "HOST SYNC" : (syncIdx == 1 ? "HOST BPM" : "INT");
        g.setColour(isRun ? rb_palette::pinkRedHi : rb_palette::creamText);
        g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
        g.drawFittedText(bpmStr + "  |  " + stepStr + "  |  " + syncTag,
                         statusTextBounds_, juce::Justification::centredLeft, 1, 0.72f);

        // Stereo output bar meter
        const auto mb = meterBounds_.toFloat();
        g.setColour(juce::Colour(0xff0b0a09));
        g.fillRoundedRectangle(mb, 3.0f);
        g.setColour(juce::Colour(0xff423a32));
        g.drawRoundedRectangle(mb, 3.0f, 1.0f);
        const float innerW = std::max(0.0f, mb.getWidth() - 4.0f);
        const auto fillBar = juce::Rectangle<float>(mb.getX() + 2.0f, mb.getY() + 2.5f,
                                                    innerW * meterSmooth_, std::max(2.0f, mb.getHeight() - 5.0f));
        g.setColour(meterSmooth_ > 0.85f ? rb_palette::runLed : rb_palette::tealAccent);
        g.fillRoundedRectangle(fillBar, 2.0f);
    }

    // Main Deck Cards: Left 2x8 Matrix + Right 3 Knobs
    auto drawCard = [&g](const juce::Rectangle<int>& rect)
    {
        g.setColour(rb_palette::cardBg);
        g.fillRoundedRectangle(rect.toFloat(), 5.0f);
        g.setColour(rb_palette::cardBorder);
        g.drawRoundedRectangle(rect.toFloat(), 5.0f, 1.0f);
    };

    drawCard(matrixCardBounds_);

    // Recessed dark hardware bezel well behind the 16 push-buttons
    {
        const auto wb = wellBounds_.toFloat();
        g.setColour(juce::Colour(0xff0c0b0a));
        g.fillRoundedRectangle(wb, 4.5f);
        g.setColour(juce::Colour(0xff3b352f));
        g.drawRoundedRectangle(wb, 4.5f, 1.0f);
    }

    // Staggered hardware silkscreen labels above Row 0 and below Row 1
    {
        const float lblFont = std::clamp(static_cast<float>(wellBounds_.getWidth()) * 0.022f, 8.2f, 10.2f);
        g.setFont(juce::Font(juce::FontOptions(lblFont, juce::Font::bold)));
        g.setColour(rb_palette::creamText);
        for (std::size_t idx = 0; idx < ke310::rhythmbox::kNumPatterns; ++idx)
        {
            g.drawFittedText(kHardwareSilkscreenNames[idx], patternLabelBounds_[idx],
                             juce::Justification::centred, 1, 0.72f);
        }
    }

    drawCard(knobCardBounds_);
    drawSectionBracket(g, knobBracketBounds_, "TONE / LEVEL / TEMPO");

    const char* knobNames[3] = {"TONE", "VOLUME", "TEMPO"};
    for (std::size_t k = 0; k < 3; ++k)
    {
        g.setColour(rb_palette::creamText);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawFittedText(knobNames[k], knobLabelBounds_[k], juce::Justification::centred, 1);
    }

    // TONE scale captions: DRUM / CYM cleanly tucked at bottom of dial arc
    if (toneSubLabelBounds_.getWidth() > 24)
    {
        g.setFont(juce::Font(juce::FontOptions(8.2f, juce::Font::bold)));
        g.setColour(rb_palette::mutedText);
        g.drawText("DRUM", toneSubLabelBounds_, juce::Justification::bottomLeft, false);
        g.drawText("CYM", toneSubLabelBounds_, juce::Justification::bottomRight, false);
    }

    // Control & Sync Strip card
    drawCard(controlBarBounds_);

    // Bottom Voice Audition Pads Bay
    drawCard(padBayBounds_);
    drawSectionBracket(g, padBracketBounds_, "ANALOG VOICE TRIGGERS (GM DRUM MAP)");
}

void RhythmboxPanel::paintOrganEmbedded(juce::Graphics& g)
{
    auto drawCard = [&g](const juce::Rectangle<int>& rect)
    {
        g.setColour(rb_palette::cardBg);
        g.fillRoundedRectangle(rect.toFloat(), 6.0f);
        g.setColour(rb_palette::cardBorder);
        g.drawRoundedRectangle(rect.toFloat().reduced(0.5f), 6.0f, 1.0f);
    };

    // Section 1: 2x8 Rhythm Pattern Matrix
    drawCard(matrixCardBounds_);
    {
        const auto wb = wellBounds_.toFloat();
        g.setColour(juce::Colour(0xff0c0b0a));
        g.fillRoundedRectangle(wb, 4.5f);
        g.setColour(juce::Colour(0xff3b352f));
        g.drawRoundedRectangle(wb, 4.5f, 1.0f);

        const float lblFont = std::clamp(static_cast<float>(wellBounds_.getWidth()) * 0.022f, 8.2f, 10.2f);
        g.setFont(juce::Font(juce::FontOptions(lblFont, juce::Font::bold)));
        g.setColour(rb_palette::creamText);
        for (std::size_t idx = 0; idx < ke310::rhythmbox::kNumPatterns; ++idx)
        {
            g.drawFittedText(kHardwareSilkscreenNames[idx], patternLabelBounds_[idx],
                             juce::Justification::centred, 1, 0.72f);
        }
    }

    // Section 2: Vertical {Red Tempo Lamp, Multi-Pattern} column (no BPM/Step text per spec)
    drawCard(lampMultiCardBounds_);
    {
        g.setColour(rb_palette::mutedText);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawFittedText("TEMPO LAMP", lampMultiHeaderBounds_, juce::Justification::centred, 1, 0.75f);
        drawTempoLamp(g, tempoLampBounds_);
    }

    // Section 3: 3 Knobs (TONE, VOLUME, TEMPO) with labels BELOW + External Tempo indicator below TEMPO
    drawCard(knobCardBounds_);
    {
        const char* knobNames[3] = {"TONE", "VOLUME", "TEMPO"};
        for (std::size_t k = 0; k < 3; ++k)
        {
            g.setColour(rb_palette::creamText);
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.drawFittedText(knobNames[k], knobLabelBounds_[k], juce::Justification::centred, 1);
        }

        if (toneSubLabelBounds_.getWidth() > 24)
        {
            g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
            g.setColour(rb_palette::mutedText);
            g.drawText("DRUM", toneSubLabelBounds_, juce::Justification::bottomLeft, false);
            g.drawText("CYM", toneSubLabelBounds_, juce::Justification::bottomRight, false);
        }

        const auto* syncP = apvts_.getRawParameterValue(ke310::rhythmbox_plugin::param_ids::rbSyncMode);
        const int syncIdx = (syncP != nullptr) ? static_cast<int>(std::lround(syncP->load(std::memory_order_relaxed))) : 1;
        if (syncIdx != 0)
        {
            const juce::String extLabel = (syncIdx == 2) ? "HOST SYNC" : "HOST TEMPO";
            g.setColour(telemetry_.lamp.usingHostBpm ? rb_palette::amberLed : rb_palette::mutedText);
            g.setFont(juce::Font(juce::FontOptions(9.2f, juce::Font::bold)));
            g.drawFittedText(extLabel, tempoModeLabelBounds_, juce::Justification::centred, 1, 0.72f);
        }
    }

    // Section 4: Square Action Buttons (BREAK White, VARIATION Black, RHYTHM Red) + START / STOP transport buttons
    drawCard(actionCardBounds_);
    {
        drawSectionBracket(g, actionBracketBounds_, "RHYTHM", 9.5f);

        const auto aw = actionWellBounds_.toFloat();
        g.setColour(juce::Colour(0xff0c0b0a));
        g.fillRoundedRectangle(aw, 4.5f);
        g.setColour(juce::Colour(0xff3b352f));
        g.drawRoundedRectangle(aw, 4.5f, 1.0f);

        const char* actionNames[3] = {"BREAK", variationSupported_ ? "VARIATION" : "VAR (N/A)", "RHYTHM"};
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        for (std::size_t k = 0; k < 3; ++k)
        {
            const bool dimmed = (k == 1 && !variationSupported_);
            g.setColour(dimmed ? rb_palette::mutedText.withAlpha(0.6f) : rb_palette::creamText);
            g.drawFittedText(actionNames[k], actionLabelBounds_[k], juce::Justification::centred, 1, 0.70f);
        }

        if (!transportWellBounds_.isEmpty())
        {
            const auto tw = transportWellBounds_.toFloat();
            g.setColour(juce::Colour(0xff0c0b0a));
            g.fillRoundedRectangle(tw, 4.5f);
            g.setColour(juce::Colour(0xff3b352f));
            g.drawRoundedRectangle(tw, 4.5f, 1.0f);

            const char* transportNames[2] = {"START", "STOP"};
            g.setFont(juce::Font(juce::FontOptions(9.2f, juce::Font::bold)));
            g.setColour(rb_palette::creamText);
            for (std::size_t k = 0; k < 2; ++k)
            {
                g.drawFittedText(transportNames[k], transportLabelBounds_[k], juce::Justification::centred, 1, 0.72f);
            }
        }
    }
}

void RhythmboxPanel::resized()
{
    if (mode_ == RhythmboxPanelMode::OrganEmbedded)
    {
        auto area = getLocalBounds();
        deckBounds_ = area;
        const int gap = 8;
        const int availW = std::max(400, deckBounds_.getWidth() - gap * 3);

        const int matrixW = std::clamp((availW * 42) / 100, 385, 510);
        const int lampW = std::clamp((availW * 10) / 100, 98, 122);
        const int actionW = std::clamp((availW * 20) / 100, 180, 255);
        const int knobW = std::max(210, availW - matrixW - lampW - actionW);

        auto row = deckBounds_;
        matrixCardBounds_ = row.removeFromLeft(matrixW);
        row.removeFromLeft(gap);
        lampMultiCardBounds_ = row.removeFromLeft(lampW);
        row.removeFromLeft(gap);
        knobCardBounds_ = row.removeFromLeft(knobW);
        row.removeFromLeft(gap);
        actionCardBounds_ = row;

        // 1. 2x8 Rhythm Pattern Matrix (faceplate-aligned bezel cutout)
        int matrixSq = 46;
        {
            auto mInner = matrixCardBounds_.reduced(8, 5);
            const int topBandH = juce::jlimit(26, 30, mInner.getHeight() / 6);
            const int botBandH = topBandH;
            const int availH = std::max(60, mInner.getHeight() - topBandH - botBandH);
            const int mAvailW = std::max(180, mInner.getWidth() - 10);

            const int cols = 8;
            const int rows = 2;
            const int gapX = 2;
            const float maxSqW = static_cast<float>(mAvailW - gapX * (cols - 1)) / static_cast<float>(cols);
            const float maxSqH = static_cast<float>(availH - 6) / 2.60f;
            const int sq = std::clamp(static_cast<int>(std::floor(std::min(maxSqW, maxSqH))), 22, 54);
            matrixSq = sq;
            const int extrude = static_cast<int>(std::round(static_cast<float>(sq) * 0.42f));
            const int btnH = sq + extrude;
            const int rowPitch = static_cast<int>(std::round(static_cast<float>(sq) * 1.16f));

            const int totalButtonsH = rowPitch + btnH;
            const int row0Y = mInner.getY() + topBandH + std::max(0, (availH - totalButtonsH) / 2);
            const int baseOffsetY = static_cast<int>(std::round(static_cast<float>(extrude) * 0.90f));

            const int wellW = sq * cols + gapX * (cols - 1) + 8;
            const int wellX = mInner.getCentreX() - wellW / 2;
            const int wellTop = row0Y + baseOffsetY;
            const int wellBot = row0Y + rowPitch + sq + baseOffsetY + 3;
            wellBounds_ = {wellX, wellTop, wellW, wellBot - wellTop};

            const int cardLeft = matrixCardBounds_.getX() + 3;
            const int cardRight = matrixCardBounds_.getRight() - 3;
            const int lblW = sq + 24;
            const int lblH = 13;

            for (int r = 0; r < rows; ++r)
            {
                for (int c = 0; c < cols; ++c)
                {
                    const std::size_t idx = static_cast<std::size_t>(r * cols + c);
                    const int bx = wellX + 4 + c * (sq + gapX);
                    const int by = row0Y + r * rowPitch;
                    patternButtons_[idx]->setBounds(bx, by, sq, btnH);

                    const int cx = bx + sq / 2;
                    const int lx = std::clamp(cx - lblW / 2, cardLeft, cardRight - lblW);
                    const bool isOddCol = (c % 2) == 1;
                    int ly = 0;
                    if (r == 0)
                    {
                        ly = isOddCol ? (row0Y - 26) : (row0Y - 14);
                    }
                    else
                    {
                        ly = isOddCol ? (wellBounds_.getBottom() + 2) : (wellBounds_.getBottom() + 14);
                    }
                    patternLabelBounds_[idx] = {lx, ly, lblW, lblH};
                }
            }
        }

        // 2. Vertical {Red Tempo Lamp, Multi-Pattern} column
        {
            auto lInner = lampMultiCardBounds_.reduced(5, 8);
            lampMultiHeaderBounds_ = lInner.removeFromTop(16);
            auto multiArea = lInner.removeFromBottom(28);
            const int lampSz = std::min(36, std::min(lInner.getWidth() - 12, lInner.getHeight() - 12));
            tempoLampBounds_ = juce::Rectangle<int>(0, 0, lampSz, lampSz).withCentre(lInner.getCentre());
            multiPatternBtn_.setBounds(multiArea.reduced(2, 1));
        }

        // 3. 3 Knobs (TONE, VOLUME, TEMPO) with labels BELOW + External Tempo notice below TEMPO
        {
            auto kInner = knobCardBounds_.reduced(6, 6);
            const int colW = kInner.getWidth() / 3;
            const int textBoxH = 16;
            const int textGap = 2;
            const int labelH = 15;
            const int modeH = 14;
            const int dialSz = std::clamp(
                std::min(colW - 4, kInner.getHeight() - textBoxH - textGap - labelH - modeH - 6),
                48, 102);
            const int totalStackH = dialSz + textBoxH + textGap + labelH + modeH;
            const int topY = kInner.getY() + std::max(0, (kInner.getHeight() - totalStackH) / 2);

            juce::Slider* knobs[3] = {&toneSlider_, &volumeSlider_, &bpmSlider_};
            for (int k = 0; k < 3; ++k)
            {
                const int colX = kInner.getX() + k * colW + 2;
                const int cellW = std::max(24, colW - 4);
                knobs[k]->setBounds(colX, topY, cellW, dialSz + textBoxH);
                if (k == 0)
                {
                    toneSubLabelBounds_ = {colX + 2, topY + dialSz - 12, std::max(10, cellW - 4), 12};
                }
                const int lblY = topY + dialSz + textBoxH + textGap;
                knobLabelBounds_[static_cast<std::size_t>(k)] = {colX, lblY, cellW, labelH};
                if (k == 2)
                {
                    tempoModeLabelBounds_ = {colX - 4, lblY + labelH, cellW + 8, modeH};
                }
            }
        }

        // 4. 3 Square Action Buttons: BREAK (White), VARIATION (Black), RHYTHM (Red) + START / STOP transport buttons
        {
            auto aInner = actionCardBounds_.reduced(8, 6);
            const int gapX = 6;
            const int maxActSqW = (aInner.getWidth() - 10 - gapX * 2) / 3;
            const int maxActSqH = static_cast<int>(std::floor(static_cast<float>(std::max(60, aInner.getHeight() - 70)) / 2.25f));
            const int sq = std::clamp(std::min({matrixSq, maxActSqW, maxActSqH}), 26, 44);
            const int extrude = static_cast<int>(std::round(static_cast<float>(sq) * 0.42f));
            const int btnH = sq + extrude;
            const int baseOffsetY = static_cast<int>(std::round(static_cast<float>(extrude) * 0.90f));

            const int bracketH = 15;
            const int bracketGap = 3;
            const int lblGap = 4;
            const int lblH = 13;
            const int rowGap = 8;

            const int ssSide = std::clamp(sq - 6, 26, 38);
            const int ssPad = 4;
            const int ssGap = 6;
            const int trLblGap = 3;
            const int trLblH = 13;
            const int transportBezelW = ssSide * 2 + ssGap + ssPad * 2;
            const int transportBezelH = ssSide + ssPad * 2;

            const int topBlockH = bracketH + bracketGap + sq + baseOffsetY + 3 + lblGap + lblH;
            const int botBlockH = transportBezelH + trLblGap + trLblH;
            const int totalActH = topBlockH + rowGap + botBlockH;
            const int topY = aInner.getY() + std::max(0, (aInner.getHeight() - totalActH) / 2);

            const int rowY = topY + bracketH + bracketGap;
            const int wellW = sq * 3 + gapX * 2 + 10;
            const int wellX = aInner.getCentreX() - wellW / 2;
            const int wellTop = rowY + baseOffsetY;
            const int wellBot = rowY + sq + baseOffsetY + 3;
            actionWellBounds_ = {wellX, wellTop, wellW, wellBot - wellTop};

            PatternPushButton* actBtns[3] = {breakBtn_.get(), variationSquareBtn_.get(), rhythmBtn_.get()};
            const int cardLeft = actionCardBounds_.getX() + 3;
            const int cardRight = actionCardBounds_.getRight() - 3;
            const int lblW = sq + 12;

            int varX = wellX + 5;
            int rhyRight = wellX + wellW - 5;
            for (int k = 0; k < 3; ++k)
            {
                const int bx = wellX + 5 + k * (sq + gapX);
                if (actBtns[k] != nullptr)
                {
                    actBtns[k]->setBounds(bx, rowY, sq, btnH);
                }
                if (k == 1)
                {
                    varX = bx;
                }
                else if (k == 2)
                {
                    rhyRight = bx + sq;
                }
                const int lx = std::clamp(bx + sq / 2 - lblW / 2, cardLeft, cardRight - lblW);
                actionLabelBounds_[static_cast<std::size_t>(k)] = {lx, actionWellBounds_.getBottom() + lblGap, lblW, lblH};
            }

            actionBracketBounds_ = {varX - 4, topY, std::max(24, (rhyRight - varX) + 8), bracketH};

            const int trWellX = aInner.getCentreX() - transportBezelW / 2;
            const int trWellY = actionWellBounds_.getBottom() + lblGap + lblH + rowGap;
            transportWellBounds_ = {trWellX, trWellY, transportBezelW, transportBezelH};

            const int startX = trWellX + ssPad;
            const int stopX = startX + ssSide + ssGap;
            const int btnY = trWellY + ssPad;
            if (rhythmStartBtn_)
            {
                rhythmStartBtn_->setBounds(startX, btnY, ssSide, ssSide);
            }
            if (rhythmStopBtn_)
            {
                rhythmStopBtn_->setBounds(stopX, btnY, ssSide, ssSide);
            }

            const int trLblW = ssSide + 12;
            transportLabelBounds_[0] = {
                std::clamp(startX + ssSide / 2 - trLblW / 2, cardLeft, cardRight - trLblW),
                transportWellBounds_.getBottom() + trLblGap,
                trLblW,
                trLblH
            };
            transportLabelBounds_[1] = {
                std::clamp(stopX + ssSide / 2 - trLblW / 2, cardLeft, cardRight - trLblW),
                transportWellBounds_.getBottom() + trLblGap,
                trLblW,
                trLblH
            };
        }
        return;
    }

    auto area = getLocalBounds().reduced(8, 6);

    // 1. Header strip
    const int headerH = juce::jlimit(36, 46, area.getHeight() / 9);
    headerBounds_ = area.removeFromTop(headerH);
    {
        auto hRow = headerBounds_.reduced(6, 5);
        hRow.removeFromLeft(std::min(205, hRow.getWidth() / 3));
        startStopBtn_.setBounds(hRow.removeFromRight(std::min(126, hRow.getWidth() / 4)));
        hRow.removeFromRight(8);
        meterBounds_ = hRow.removeFromRight(std::min(104, hRow.getWidth() / 4)).reduced(0, 7);
        hRow.removeFromRight(8);
        tempoLampBounds_ = hRow.removeFromLeft(26);
        hRow.removeFromLeft(10);
        statusTextBounds_ = hRow;
    }

    area.removeFromTop(6);

    // Reserve bottom strips
    const int padBayH = juce::jlimit(66, 88, area.getHeight() / 5);
    padBayBounds_ = area.removeFromBottom(padBayH);
    area.removeFromBottom(6);

    const int ctrlBarH = juce::jlimit(38, 48, area.getHeight() / 7);
    controlBarBounds_ = area.removeFromBottom(ctrlBarH);
    area.removeFromBottom(6);

    // 2. Main Deck: 2x8 Matrix (Left ~66%) + 3 Large Knobs (Right ~34%)
    deckBounds_ = area;
    const int knobCardW = juce::jlimit(210, 265, (deckBounds_.getWidth() * 34) / 100);
    knobCardBounds_ = deckBounds_.removeFromRight(knobCardW);
    deckBounds_.removeFromRight(6);
    matrixCardBounds_ = deckBounds_;

    // Layout 2x8 square push-buttons inside dark recessed well + staggered silkscreen labels
    {
        auto mInner = matrixCardBounds_.reduced(8, 6);
        const int topBandH = juce::jlimit(26, 32, mInner.getHeight() / 6);
        const int botBandH = topBandH;
        const int availH = std::max(60, mInner.getHeight() - topBandH - botBandH);
        const int availW = std::max(180, mInner.getWidth() - 10);

        const int cols = 8;
        const int rows = 2;
        const int gapX = 2;
        const float maxSqW = static_cast<float>(availW - gapX * (cols - 1)) / static_cast<float>(cols);
        const float maxSqH = static_cast<float>(availH - 6) / 2.60f;
        const int sq = std::clamp(static_cast<int>(std::floor(std::min(maxSqW, maxSqH))), 22, 56);
        const int extrude = static_cast<int>(std::round(static_cast<float>(sq) * 0.42f));
        const int btnH = sq + extrude;
        const int rowPitch = static_cast<int>(std::round(static_cast<float>(sq) * 1.16f));

        const int totalButtonsH = rowPitch + btnH;
        const int row0Y = mInner.getY() + topBandH + std::max(0, (availH - totalButtonsH) / 2);
        const int baseOffsetY = static_cast<int>(std::round(static_cast<float>(extrude) * 0.90f));

        const int wellW = sq * cols + gapX * (cols - 1) + 8;
        const int wellX = mInner.getCentreX() - wellW / 2;
        const int wellTop = row0Y + baseOffsetY;
        const int wellBot = row0Y + rowPitch + sq + baseOffsetY + 3;
        wellBounds_ = {wellX, wellTop, wellW, wellBot - wellTop};

        const int cardLeft = matrixCardBounds_.getX() + 3;
        const int cardRight = matrixCardBounds_.getRight() - 3;
        const int lblW = sq + 24;
        const int lblH = 13;

        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                const std::size_t idx = static_cast<std::size_t>(r * cols + c);
                const int bx = wellX + 4 + c * (sq + gapX);
                const int by = row0Y + r * rowPitch;
                patternButtons_[idx]->setBounds(bx, by, sq, btnH);

                const int cx = bx + sq / 2;
                const int lx = std::clamp(cx - lblW / 2, cardLeft, cardRight - lblW);
                const bool isOddCol = (c % 2) == 1;
                int ly = 0;
                if (r == 0)
                {
                    ly = isOddCol ? (row0Y - 26) : (row0Y - 14);
                }
                else
                {
                    ly = isOddCol ? (wellBounds_.getBottom() + 2) : (wellBounds_.getBottom() + 14);
                }
                patternLabelBounds_[idx] = {lx, ly, lblW, lblH};
            }
        }
    }

    // Layout 3 Large Knobs: TONE, VOLUME, TEMPO vertically centered around circular dial
    {
        auto kInner = knobCardBounds_.reduced(8, 5);
        knobBracketBounds_ = kInner.removeFromTop(18);
        kInner.removeFromTop(4);

        const int colW = kInner.getWidth() / 3;
        const int textBoxH = 18;
        const int labelH = 16;
        const int labelGap = 4;
        const int dialSz = std::max(44, std::min(colW - 6, kInner.getHeight() - labelH - labelGap - textBoxH - 4));
        const int totalBlockH = labelH + labelGap + dialSz + textBoxH;
        const int blockY = kInner.getY() + std::max(0, (kInner.getHeight() - totalBlockH) / 2);

        juce::Slider* knobs[3] = {&toneSlider_, &volumeSlider_, &bpmSlider_};
        for (int k = 0; k < 3; ++k)
        {
            const int colX = kInner.getX() + k * colW + 2;
            const int cellW = std::max(24, colW - 4);
            knobLabelBounds_[static_cast<std::size_t>(k)] = {colX, blockY, cellW, labelH};
            const int sliderY = blockY + labelH + labelGap;
            knobs[k]->setBounds(colX, sliderY, cellW, dialSz + textBoxH);
            if (k == 0)
            {
                toneSubLabelBounds_ = {colX + 2, sliderY + dialSz - 12, std::max(10, cellW - 4), 12};
            }
        }
    }

    // 3. Control & Sync Strip (single clean row with generous ComboBox widths)
    {
        auto cInner = controlBarBounds_.reduced(8, 6);
        const int h = cInner.getHeight();
        variationBtn_.setBounds(cInner.removeFromLeft(std::min(96, cInner.getWidth() / 5)));
        cInner.removeFromLeft(6);
        multiPatternBtn_.setBounds(cInner.removeFromLeft(std::min(154, cInner.getWidth() / 3)));
        cInner.removeFromLeft(6);

        syncLbl_.setBounds(cInner.removeFromLeft(34).withHeight(h));
        syncModeBox_.setBounds(cInner.removeFromLeft(std::min(98, cInner.getWidth() / 3)).withHeight(h));
        cInner.removeFromLeft(6);

        midiChLbl_.setBounds(cInner.removeFromLeft(32).withHeight(h));
        midiChBox_.setBounds(cInner.removeFromLeft(std::min(74, cInner.getWidth() / 3)).withHeight(h));
        cInner.removeFromLeft(6);

        driftLbl_.setBounds(cInner.removeFromLeft(36).withHeight(h));
        driftSlider_.setBounds(cInner.withHeight(h));
    }

    // 4. Bottom 11 Voice Audition Pads
    {
        auto pInner = padBayBounds_.reduced(7, 4);
        padBracketBounds_ = pInner.removeFromTop(16);
        pInner.removeFromTop(3);

        const int count = static_cast<int>(ke310::rhythmbox::kNumVoices);
        const int gap = 4;
        const int padW = std::max(24, (pInner.getWidth() - gap * (count - 1)) / count);
        const int totalW = padW * count + gap * (count - 1);
        const int xOff = pInner.getX() + (pInner.getWidth() - totalW) / 2;

        for (int v = 0; v < count; ++v)
        {
            padButtons_[static_cast<std::size_t>(v)]->setBounds(
                xOff + v * (padW + gap), pInner.getY(), padW, pInner.getHeight());
        }
    }
}

} // namespace ke310::ui
