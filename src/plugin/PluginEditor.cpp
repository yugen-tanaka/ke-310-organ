#include "PluginEditor.hpp"
#include "FactoryPresets.hpp"

#include <algorithm>
#include <cmath>

namespace ke310::plugin
{

namespace
{

namespace palette
{
const juce::Colour ivoryTop{0xffe8dfce};
const juce::Colour ivoryHi{0xfff5efe2};
const juce::Colour ivoryShadow{0xffb5aa96};
const juce::Colour panelTop{0xff282422};
const juce::Colour panelBot{0xff191715};
const juce::Colour bayBg{0xff1c1917};
const juce::Colour cardBg{0xff23201d};
const juce::Colour cardBorder{0xff4a423a};
const juce::Colour tabTrayBg{0xff12100f};
const juce::Colour silverTrim{0xffc6c2b8};
const juce::Colour creamText{0xffeae4d7};
const juce::Colour mutedText{0xffb0a798};
const juce::Colour amberLed{0xffffb432};
const juce::Colour darkInk{0xff1a1714};
const juce::Colour tealBus{0xff148f77};
const juce::Colour blueEnsemble{0xff1d6fd6};
} // namespace palette

juce::Colour tabBaseColour(TabColor c) noexcept
{
    switch (c)
    {
        case TabColor::White:  return juce::Colour(0xffeee7db);
        case TabColor::Yellow: return juce::Colour(0xffeeb209);
        case TabColor::Red:    return juce::Colour(0xffd02022);
        case TabColor::Green:  return juce::Colour(0xff009255);
        case TabColor::Blue:   return juce::Colour(0xff0f5dd4);
        case TabColor::Black:  return juce::Colour(0xff17181a);
    }
    return juce::Colour(0xffeee7db);
}

juce::Colour tabTextColour(TabColor c) noexcept
{
    switch (c)
    {
        case TabColor::White:
        case TabColor::Yellow:
            return juce::Colour(0xff161412);
        case TabColor::Red:
        case TabColor::Green:
        case TabColor::Blue:
        case TabColor::Black:
            return juce::Colour(0xfff8f5ee);
    }
    return juce::Colour(0xff161412);
}

void drawSectionBracket(juce::Graphics& g,
                        juce::Rectangle<int> area,
                        const juce::String& title,
                        float fontSize = 10.5f)
{
    if (area.getWidth() < 24 || area.getHeight() < 12)
    {
        return;
    }

    const juce::Font font(juce::FontOptions(fontSize, juce::Font::bold));
    g.setFont(font);
    const int textW = static_cast<int>(std::ceil(juce::TextLayout::getStringWidth(font, title))) + 10;
    const int cy = area.getCentreY();
    const int x0 = area.getX() + 3;
    const int x1 = area.getRight() - 3;
    const int cx = area.getCentreX();
    const int leftLineEnd = std::max(x0, cx - textW / 2);
    const int rightLineStart = std::min(x1, cx + textW / 2);

    g.setColour(palette::creamText.withAlpha(0.75f));
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

    g.setColour(palette::creamText);
    g.drawFittedText(title, area, juce::Justification::centred, 1, 0.7f);
}

} // namespace

//==============================================================================
// RockerTabButton
//==============================================================================
RockerTabButton::RockerTabButton(const juce::String& upperText,
                                 const juce::String& footageText,
                                 TabColor color,
                                 const juce::String& tooltip)
    : juce::ToggleButton(),
      upperText_(upperText),
      footageText_(footageText),
      color_(color)
{
    setClickingTogglesState(true);
    setTooltip(tooltip);
}

void RockerTabButton::paintButton(juce::Graphics& g,
                                  bool shouldDrawButtonAsHighlighted,
                                  bool shouldDrawButtonAsDown)
{
    const auto full = getLocalBounds().toFloat();
    const float h = full.getHeight();
    const float y0 = full.getY();

    // Dark recessed molded tone-lever slot housing
    g.setColour(juce::Colour(0xff090807));
    g.fillRect(full);
    g.setColour(juce::Colour(0xff141210));
    g.fillRect(full.getX(), y0 + h * 0.16f, full.getWidth(), h * 0.62f);

    const bool leverOn = getToggleState();
    const bool isBlack = (color_ == TabColor::Black);
    const juce::Colour base = tabBaseColour(color_);
    const juce::Colour fg = tabTextColour(color_);

    // Authentic plastic shadow tint (avoids muddy greying on ivory/yellow)
    auto plasticShade = [&](float amount) -> juce::Colour
    {
        if (color_ == TabColor::Yellow)
        {
            return base.interpolatedWith(juce::Colour(0xff9c6500), amount);
        }
        if (color_ == TabColor::White)
        {
            return base.interpolatedWith(juce::Colour(0xff8f8679), amount);
        }
        return base.darker(amount);
    };

    const float padX = 2.6f;
    const float bodyX = full.getX() + padX;
    const float bodyW = std::max(6.0f, full.getWidth() - 2.0f * padX);
    const float cx = bodyX + bodyW * 0.5f;

    // Perspective trapezoidal rounded-rectangle lever with subtle 3D tilt:
    // OFF: rear tilted slightly away (narrower topW), raised brighter front with thick edge + deep drop shadow.
    // ON : snapped downward at front toward player, soft shadow vignette shading the lowered front half.
    const float pressOffset = shouldDrawButtonAsDown ? 1.5f : 0.0f;
    const float bladeH = std::max(24.0f, h * (leverOn ? 0.880f : 0.807f));
    const float yTop = y0 + h * (leverOn ? 0.062f : 0.028f) + pressOffset;
    const float yBot = std::min(full.getBottom() - 3.5f, yTop + bladeH);
    const float effectiveBladeH = std::max(16.0f, yBot - yTop);
    const float lipH = leverOn ? 1.8f : 2.8f;
    const float cornerR = juce::jlimit(4.5f, 8.2f, bodyW * 0.145f);

    const float topW = std::max(4.0f, bodyW - (leverOn ? 2.8f : 3.4f));
    const float botW = std::max(4.0f, bodyW - (leverOn ? 0.8f : 0.0f));

    auto makeLeverTrapPath = [](float centerX, float wTop, float topY, float wBot, float botY, float radius)
    {
        juce::Path poly;
        poly.startNewSubPath(centerX - wTop * 0.5f, topY);
        poly.lineTo(centerX + wTop * 0.5f, topY);
        poly.lineTo(centerX + wBot * 0.5f, botY);
        poly.lineTo(centerX - wBot * 0.5f, botY);
        poly.closeSubPath();
        return poly.createPathWithRoundedCorners(radius);
    };

    // Contoured downward cast shadow beneath front perspective edge
    const float shadowH = leverOn ? 3.0f : std::max(8.5f, h * 0.095f);
    const float shadowBottom = std::min(full.getBottom(), yBot + shadowH);
    {
        const auto dropPath = makeLeverTrapPath(cx, topW * 0.96f, std::max(yTop, yBot - 6.0f),
                                                botW + 1.0f, shadowBottom, cornerR);
        juce::ColourGradient dropGrad(juce::Colours::black.withAlpha(leverOn ? 0.65f : 0.85f), cx, yBot - 1.5f,
                                      juce::Colours::transparentBlack, cx, shadowBottom, false);
        g.setGradientFill(dropGrad);
        g.fillPath(dropPath);
    }

    const auto facePath = makeLeverTrapPath(cx, topW, yTop, botW, yBot, cornerR);
    const float hoverBoost = shouldDrawButtonAsHighlighted ? 0.06f : 0.0f;

    // Subtle 3D front edge thickness extruded directly underneath the rounded trapezoid
    {
        juce::Path edgePath = facePath;
        edgePath.applyTransform(juce::AffineTransform::translation(0.0f, lipH));
        g.setColour(isBlack ? juce::Colour(0xff070709) : plasticShade(0.34f));
        g.fillPath(edgePath);
    }

    // Continuous single-piece rounded-trapezoid top face with shading on the lowered side
    {
        juce::Graphics::ScopedSaveState clipState(g);
        g.reduceClipRegion(facePath);

        if (leverOn)
        {
            juce::ColourGradient bladeGrad(
                isBlack ? juce::Colour(0xff191a1d).brighter(hoverBoost) : plasticShade(0.06f).brighter(hoverBoost),
                cx, yTop,
                isBlack ? juce::Colour(0xff101113).brighter(hoverBoost) : plasticShade(0.18f).brighter(hoverBoost),
                cx, yBot, false);
            bladeGrad.addColour(0.24, isBlack ? juce::Colour(0xff232428).brighter(hoverBoost)
                                              : base.brighter(0.05f).brighter(hoverBoost));
            g.setGradientFill(bladeGrad);
            g.fillPath(facePath);

            // Soft lowered-side front tilt vignette over bottom third
            juce::ColourGradient lowerShadeGrad(juce::Colours::transparentBlack, cx, yTop + effectiveBladeH * 0.62f,
                                                juce::Colours::black.withAlpha(isBlack ? 0.14f : 0.08f), cx, yBot, false);
            g.setGradientFill(lowerShadeGrad);
            g.fillPath(facePath);
        }
        else
        {
            juce::ColourGradient bladeGrad(
                isBlack ? juce::Colour(0xff161719).brighter(hoverBoost) : plasticShade(0.07f).brighter(hoverBoost),
                cx, yTop,
                isBlack ? juce::Colour(0xff242529).brighter(hoverBoost) : base.brighter(0.07f).brighter(hoverBoost),
                cx, yBot, false);
            g.setGradientFill(bladeGrad);
            g.fillPath(facePath);
        }

        // Soft symmetrical inner bezel rim highlight along rounded perspective edge
        g.setColour(juce::Colours::white.withAlpha(isBlack ? 0.10f : 0.20f));
        g.strokePath(facePath, juce::PathStrokeType(1.2f));
    }

    // Crisp outer molded plastic silhouette
    g.setColour(isBlack ? juce::Colour(0xff3e4046) : juce::Colours::black.withAlpha(0.36f));
    g.strokePath(facePath, juce::PathStrokeType(0.85f));

    // Typography anchored below unlabeled upper ~34% push-off zone
    const float widthScale = juce::jlimit(0.72f, 1.25f, bodyW / 42.0f);
    const float heightScale = juce::jlimit(0.72f, 1.25f, effectiveBladeH / 124.0f);
    const float typScale = std::min(widthScale, heightScale);

    auto drawStackedLines = [&](const juce::String& labelText, juce::Rectangle<int> rect, float fontSize, bool bold)
    {
        const auto lines = juce::StringArray::fromLines(labelText);
        const int nLines = std::max(1, lines.size());
        const int lineH = std::max(1, rect.getHeight() / nLines);
        g.setFont(juce::Font(juce::FontOptions(fontSize, bold ? juce::Font::bold : juce::Font::plain)));
        for (int i = 0; i < lines.size(); ++i)
        {
            auto row = (i + 1 == lines.size()) ? rect : rect.removeFromTop(lineH);
            g.drawFittedText(lines[i], row, juce::Justification::centred, 1, 0.48f);
        }
    };

    auto drawDiagonalFraction = [&](juce::Rectangle<int> rect)
    {
        const float fx = static_cast<float>(rect.getCentreX());
        const float fy = static_cast<float>(rect.getCentreY());
        const float u = std::min(static_cast<float>(rect.getWidth()) / 32.0f,
                                 static_cast<float>(rect.getHeight()) / 24.0f);

        g.setFont(juce::Font(juce::FontOptions(16.0f * u, juce::Font::plain)));
        g.drawText("2", juce::Rectangle<float>(fx - 14.5f * u, fy - 10.0f * u, 11.0f * u, 20.0f * u).toNearestInt(),
                   juce::Justification::centred, false);

        g.setFont(juce::Font(juce::FontOptions(10.0f * u, juce::Font::plain)));
        g.drawText("2", juce::Rectangle<float>(fx - 2.3f * u, fy - 10.2f * u, 8.5f * u, 11.0f * u).toNearestInt(),
                   juce::Justification::centred, false);

        g.drawLine(fx - 0.3f * u, fy + 6.8f * u, fx + 8.6f * u, fy - 7.0f * u, 1.25f * u);

        g.drawText("3", juce::Rectangle<float>(fx + 4.8f * u, fy - 0.8f * u, 8.5f * u, 11.0f * u).toNearestInt(),
                   juce::Justification::centred, false);
    };

    // Voice label at [35% .. 57%] of lever blade (keeps upper ~34% clean unlabeled)
    const auto upperRect = juce::Rectangle<float>(
        bodyX + 1.5f, yTop + effectiveBladeH * 0.35f,
        std::max(1.0f, bodyW - 3.0f), std::max(12.0f, effectiveBladeH * 0.22f)).toNearestInt();

    g.setColour(fg.withAlpha(leverOn ? 1.0f : 0.92f));
    const bool isMultiLineUpper = upperText_.containsChar('\n');
    const float upperFontPx = (isMultiLineUpper ? 8.8f : 9.4f) * typScale;
    drawStackedLines(upperText_, upperRect, upperFontPx, true);

    // Lower silkscreen footage numeral at [61% .. 89%] (no prime mark, diagonal 2 2/3)
    if (footageText_.isNotEmpty())
    {
        const auto lowerRect = juce::Rectangle<float>(
            bodyX + 1.5f, yTop + effectiveBladeH * 0.61f,
            std::max(1.0f, bodyW - 3.0f), std::max(14.0f, effectiveBladeH * 0.28f)).toNearestInt();

        if (footageText_ == "2 2/3")
        {
            drawDiagonalFraction(lowerRect);
        }
        else
        {
            const bool isNumericFootage = (footageText_.length() <= 2);
            const float lowerFontPx = (isNumericFootage ? 17.5f : 9.2f) * typScale;
            drawStackedLines(footageText_, lowerRect, lowerFontPx, !isNumericFootage);
        }
    }
}

//==============================================================================
// KE310LookAndFeel
//==============================================================================
KE310LookAndFeel::KE310LookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1b1816));
    setColour(juce::ComboBox::textColourId, palette::creamText);
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff6b6054));
    setColour(juce::ComboBox::arrowColourId, palette::amberLed);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1e1b18));
    setColour(juce::PopupMenu::textColourId, palette::creamText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, palette::amberLed.withAlpha(0.28f));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::Label::textColourId, palette::creamText);
    setColour(juce::Slider::textBoxTextColourId, palette::creamText);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff141210));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff494139));
}

void KE310LookAndFeel::drawRotarySlider(juce::Graphics& g,
                                        int x, int y, int width, int height,
                                        float sliderPosProportional,
                                        float rotaryStartAngle,
                                        float rotaryEndAngle,
                                        juce::Slider& slider)
{
    juce::ignoreUnused(slider);

    const float radius = std::max(8.0f, static_cast<float>(std::min(width, height)) * 0.5f - 6.0f);
    const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Hardware white panel scale C-arc with end ticks
    const float arcRadius = radius + 2.5f;
    juce::Path scaleArc;
    scaleArc.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(palette::creamText.withAlpha(0.80f));
    g.strokePath(scaleArc, juce::PathStrokeType(1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (float endAng : {rotaryStartAngle, rotaryEndAngle})
    {
        const float ta = endAng - juce::MathConstants<float>::halfPi;
        g.drawLine(cx + std::cos(ta) * (arcRadius - 3.5f), cy + std::sin(ta) * (arcRadius - 3.5f),
                   cx + std::cos(ta) * arcRadius, cy + std::sin(ta) * arcRadius, 1.1f);
    }

    // Outer molded black skirt
    const float bodyR = radius * 0.86f;
    g.setColour(juce::Colours::black.withAlpha(0.60f));
    g.fillEllipse(cx - bodyR + 1.0f, cy - bodyR + 2.0f, bodyR * 2.0f, bodyR * 2.0f);

    juce::ColourGradient skirtGrad(juce::Colour(0xff262422), cx, cy - bodyR,
                                   juce::Colour(0xff0d0c0b), cx, cy + bodyR, false);
    g.setGradientFill(skirtGrad);
    g.fillEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour(juce::Colour(0xff423d38));
    g.drawEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 0.9f);

    // Glossy black center cap encircled by crisp white ring (authentic KE-310 knob)
    const float capR = bodyR * 0.68f;
    juce::ColourGradient capGrad(juce::Colour(0xff1e1c1b), cx, cy - capR,
                                 juce::Colour(0xff090808), cx, cy + capR, false);
    g.setGradientFill(capGrad);
    g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
    g.setColour(juce::Colour(0xffeae6dc));
    g.drawEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.3f);

    // Crisp white pointer line inside cap
    const float pa = angle - juce::MathConstants<float>::halfPi;
    g.setColour(juce::Colour(0xfff5f2ea));
    g.drawLine(cx + std::cos(pa) * (capR * 0.30f), cy + std::sin(pa) * (capR * 0.30f),
               cx + std::cos(pa) * (capR * 0.95f), cy + std::sin(pa) * (capR * 0.95f), 1.9f);
}

void KE310LookAndFeel::drawLinearSlider(juce::Graphics& g,
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
    const float trackH = 5.0f;
    const auto trackRect = juce::Rectangle<float>(static_cast<float>(x) + 4.0f,
                                                  trackY - trackH * 0.5f,
                                                  std::max(8.0f, static_cast<float>(width) - 8.0f),
                                                  trackH);

    g.setColour(juce::Colour(0xff12100e));
    g.fillRoundedRectangle(trackRect, 2.5f);
    g.setColour(juce::Colour(0xff463f37));
    g.drawRoundedRectangle(trackRect, 2.5f, 0.8f);

    const float clampedX = std::clamp(sliderPos, trackRect.getX(), trackRect.getRight());
    const auto fillRect = trackRect.withWidth(std::max(0.0f, clampedX - trackRect.getX()));
    g.setColour(palette::amberLed.withAlpha(0.85f));
    g.fillRoundedRectangle(fillRect, 2.5f);

    const auto thumbRect = juce::Rectangle<float>(clampedX - 5.5f, trackY - 7.5f, 11.0f, 15.0f);
    g.setColour(juce::Colour(0xffdcd6ca));
    g.fillRoundedRectangle(thumbRect, 2.5f);
    g.setColour(juce::Colour(0xff231e1a));
    g.drawRoundedRectangle(thumbRect, 2.5f, 1.0f);
    g.drawLine(clampedX, thumbRect.getY() + 3.0f, clampedX, thumbRect.getBottom() - 3.0f, 1.2f);
}

void KE310LookAndFeel::drawComboBox(juce::Graphics& g,
                                    int width, int height,
                                    bool isButtonDown,
                                    int buttonX, int buttonY, int buttonW, int buttonH,
                                    juce::ComboBox& box)
{
    juce::ignoreUnused(isButtonDown, buttonX, buttonY, buttonW, buttonH);
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

    const float arrowZoneX = static_cast<float>(width - 16);
    const float cy = static_cast<float>(height) * 0.5f;
    juce::Path p;
    p.addTriangle(arrowZoneX, cy - 2.5f, arrowZoneX + 8.0f, cy - 2.5f, arrowZoneX + 4.0f, cy + 3.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.fillPath(p);
}

void KE310LookAndFeel::drawToggleButton(juce::Graphics& g,
                                        juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsDown);

    const auto bounds = button.getLocalBounds().toFloat();
    const float boxSize = 14.0f;
    const auto boxRect = juce::Rectangle<float>(bounds.getX() + 2.0f,
                                                bounds.getCentreY() - boxSize * 0.5f,
                                                boxSize,
                                                boxSize);
    const bool toggled = button.getToggleState();

    g.setColour(juce::Colour(0xff13110f));
    g.fillRoundedRectangle(boxRect, 2.5f);
    g.setColour(toggled ? palette::amberLed : juce::Colour(0xff6b6155));
    g.drawRoundedRectangle(boxRect, 2.5f, 1.1f);

    if (toggled)
    {
        g.setColour(palette::amberLed);
        g.fillRoundedRectangle(boxRect.reduced(3.0f), 1.5f);
    }

    auto textRect = bounds.withTrimmedLeft(boxSize + 8.0f).toNearestInt();
    g.setColour(shouldDrawButtonAsHighlighted ? juce::Colours::white : palette::creamText);
    g.setFont(juce::Font(juce::FontOptions(12.0f, toggled ? juce::Font::bold : juce::Font::plain)));
    g.drawFittedText(button.getButtonText(), textRect, juce::Justification::centredLeft, 1, 0.72f);
}

void KE310LookAndFeel::drawButtonBackground(juce::Graphics& g,
                                            juce::Button& button,
                                            const juce::Colour& backgroundColour,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(backgroundColour);

    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    juce::Colour fill(0xff2b2622);
    if (button.getButtonText() == "ALL OFF")
    {
        fill = juce::Colour(0xff7d2323);
    }
    if (shouldDrawButtonAsHighlighted)
    {
        fill = fill.brighter(0.14f);
    }
    if (shouldDrawButtonAsDown)
    {
        fill = fill.darker(0.18f);
    }

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(juce::Colour(0xff685d50));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

void KE310LookAndFeel::drawButtonText(juce::Graphics& g,
                                      juce::TextButton& button,
                                      bool shouldDrawButtonAsHighlighted,
                                      bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.setColour(palette::creamText);
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(3, 1),
                     juce::Justification::centred, 1, 0.7f);
}

//==============================================================================
// TelemetryDisplay
//==============================================================================
void TelemetryDisplay::updateTelemetry(bool ensembleOn,
                                       int activeUpper,
                                       int activeLower,
                                       int activePedalMidi,
                                       float mainRms,
                                       float rotorRms,
                                       float statRms)
{
    ensembleOn_ = ensembleOn;
    activeUpper_ = activeUpper;
    activeLower_ = activeLower;
    activePedalMidi_ = activePedalMidi;

    mainDb_ = rmsToDb(mainRms);
    rotorDb_ = rmsToDb(rotorRms);
    statDb_ = rmsToDb(statRms);

    auto smooth = [](float current, float target) noexcept -> float
    {
        return (target >= current) ? target : (current * 0.72f + target * 0.28f);
    };
    mainSmooth_ = smooth(mainSmooth_, dbToNorm(mainDb_));
    rotorSmooth_ = smooth(rotorSmooth_, dbToNorm(rotorDb_));
    statSmooth_ = smooth(statSmooth_, dbToNorm(statDb_));
}

float TelemetryDisplay::rmsToDb(float rms) noexcept
{
    return (rms > 1.0e-6f) ? (20.0f * std::log10(rms)) : -120.0f;
}

float TelemetryDisplay::dbToNorm(float db) noexcept
{
    return std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
}

void TelemetryDisplay::drawMeter(juce::Graphics& g,
                                 juce::Rectangle<int> bounds,
                                 const juce::String& label,
                                 float normLevel,
                                 float dbValue,
                                 juce::Colour barColour)
{
    auto cell = bounds.reduced(4, 0);
    auto labelArea = cell.removeFromLeft(42);
    auto dbArea = cell.removeFromRight(38);
    auto slot = cell.reduced(3, 5).toFloat();

    g.setColour(palette::mutedText);
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.drawText(label, labelArea, juce::Justification::centredLeft, false);

    g.setColour(juce::Colour(0xff0b0a09));
    g.fillRoundedRectangle(slot, 2.0f);
    g.setColour(juce::Colour(0xff3a342e));
    g.drawRoundedRectangle(slot, 2.0f, 0.7f);

    if (normLevel > 0.005f)
    {
        const auto fill = slot.withWidth(slot.getWidth() * normLevel);
        g.setColour(barColour);
        g.fillRoundedRectangle(fill, 2.0f);
    }

    const juce::String dbStr = (dbValue <= -80.0f)
        ? juce::String("-inf")
        : ((dbValue >= 0.0f ? "+" : "") + juce::String(dbValue, 1));
    g.setColour(palette::creamText);
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::plain)));
    g.drawText(dbStr, dbArea, juce::Justification::centredRight, false);
}

juce::String TelemetryDisplay::formatVoiceText(int activeUpper, int activeLower, int activePedalMidi)
{
    const juce::String pedStr = (activePedalMidi >= 0)
        ? juce::MidiMessage::getMidiNoteName(activePedalMidi, true, true, 4)
        : juce::String("--");
    return "UP:" + juce::String(activeUpper)
         + "   LO:" + juce::String(activeLower)
         + "   PED:" + pedStr;
}

void TelemetryDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.setColour(juce::Colour(0xff161311));
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff7e7262));
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 4.0f, 1.0f);

    auto inner = bounds.reduced(6, 3);
    auto topRow = inner.removeFromTop(inner.getHeight() / 2);
    auto botRow = inner;

    // Top row: MAIN meter (left half) + voice counters and activity LED (right half)
    const int halfW = inner.getWidth() / 2;
    drawMeter(g, topRow.removeFromLeft(halfW), "MAIN", mainSmooth_, mainDb_, palette::amberLed);

    const bool anyVoice = (activeUpper_ > 0 || activeLower_ > 0 || activePedalMidi_ >= 0);

    auto ledArea = topRow.removeFromRight(14).reduced(3, 5);
    g.setColour(anyVoice ? palette::amberLed : juce::Colour(0xff3c352e));
    g.fillEllipse(ledArea.toFloat());

    g.setColour(palette::creamText);
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    const juce::String voiceText = formatVoiceText(activeUpper_, activeLower_, activePedalMidi_);
    g.drawText(voiceText, topRow.reduced(6, 0), juce::Justification::centredRight, false);

    // Bottom row: ROTOR & STAT meters side by side with full room
    drawMeter(g, botRow.removeFromLeft(halfW), "ROTOR", rotorSmooth_, rotorDb_,
              ensembleOn_ ? palette::blueEnsemble.brighter(0.25f) : palette::tealBus.brighter(0.20f));
    drawMeter(g, botRow, "STAT", statSmooth_, statDb_,
              ensembleOn_ ? juce::Colour(0xff55504a) : juce::Colour(0xffc9923b));
}

//==============================================================================
// KE310AudioProcessorEditor
//==============================================================================
KE310AudioProcessorEditor::KE310AudioProcessorEditor(KE310AudioProcessor& owner)
    : AudioProcessorEditor(&owner),
      processor_(owner)
{
    setLookAndFeel(&lookAndFeel_);

    initHeaderControls();
    initKnobs();
    initRockerSections();
    initMidiAndCalibrationBay();

    setResizable(true, true);
    setResizeLimits(1000, 640, 1600, 1020);
    setSize(1200, 760);

    timerCallback();
    startTimerHz(20);
}

KE310AudioProcessorEditor::~KE310AudioProcessorEditor()
{
    stopTimer();
    buttonAttachments_.clear();
    sliderAttachments_.clear();
    comboAttachments_.clear();
    setLookAndFeel(nullptr);
}

void KE310AudioProcessorEditor::initHeaderControls()
{
    for (int i = 0; i < processor_.getNumPrograms(); ++i)
    {
        presetBox_.addItem(processor_.getProgramName(i), i + 1);
    }
    presetBox_.setSelectedId(processor_.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox_.onChange = [this]
    {
        const int idx = presetBox_.getSelectedId() - 1;
        if (idx >= 0)
        {
            processor_.setCurrentProgram(idx);
        }
    };

    prevPresetBtn_.onClick = [this]
    {
        const int n = processor_.getNumPrograms();
        if (n > 0)
        {
            const int prev = (processor_.getCurrentProgram() + n - 1) % n;
            processor_.setCurrentProgram(prev);
            presetBox_.setSelectedId(prev + 1, juce::dontSendNotification);
        }
    };
    nextPresetBtn_.onClick = [this]
    {
        const int n = processor_.getNumPrograms();
        if (n > 0)
        {
            const int next = (processor_.getCurrentProgram() + 1) % n;
            processor_.setCurrentProgram(next);
            presetBox_.setSelectedId(next + 1, juce::dontSendNotification);
        }
    };
    allOffBtn_.onClick = [this] { clearAllTabs(); };

    addAndMakeVisible(prevPresetBtn_);
    addAndMakeVisible(presetBox_);
    addAndMakeVisible(nextPresetBtn_);
    addAndMakeVisible(allOffBtn_);
    addAndMakeVisible(telemetry_);
}

void KE310AudioProcessorEditor::initKnobs()
{
    struct KnobSpec
    {
        const char* paramId;
        const char* label;
    };

    static constexpr std::array<KnobSpec, 10> kKnobSpecs = {{
        {param_ids::pedalVolume,      "PEDAL VOL"},
        {param_ids::pedalSustainTime, "PEDAL SUSTAIN"},
        {param_ids::reverb,           "REVERB"},
        {param_ids::upperSustainTime, "UPPER SUSTAIN"},
        {param_ids::presetVolume,     "PRESET VOL"},
        {param_ids::percusDecay,      "PERC DECAY"},
        {param_ids::repeatRate,       "REPEAT RATE"},
        {param_ids::lowerVolume,      "LOWER VOL"},
        {param_ids::totalVolume,      "TOTAL VOL"},
        {param_ids::expression,       "EXPRESSION"},
    }};

    auto& apvts = processor_.apvts();
    knobs_.reserve(kKnobSpecs.size());

    for (const auto& spec : kKnobSpecs)
    {
        KnobSlot slot;
        slot.paramId = spec.paramId;
        slot.label = spec.label;
        slot.slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag,
                                                     juce::Slider::TextBoxBelow);
        slot.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 15);
        slot.slider->setNumDecimalPlacesToDisplay(2);
        addAndMakeVisible(*slot.slider);

        sliderAttachments_.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, spec.paramId, *slot.slider));
        knobs_.push_back(std::move(slot));
    }
}

void KE310AudioProcessorEditor::initRockerSections()
{
    auto addGroup = [&](int tier, const juce::String& title, std::initializer_list<RockerTabSpec> specs)
    {
        TabGroup grp;
        grp.tier = tier;
        grp.title = title;
        grp.tabs.reserve(specs.size());

        auto& apvts = processor_.apvts();
        for (const auto& s : specs)
        {
            const juce::String tip = (juce::String(s.upperText).replace("-\n", "").replace("\n", " ")
                                      + " " + s.footageText).trim();
            auto btn = std::make_unique<RockerTabButton>(
                s.upperText, s.footageText, s.color, tip);
            addAndMakeVisible(*btn);
            buttonAttachments_.push_back(
                std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                    apvts, s.paramId, *btn));
            allTabParamIds_.push_back(s.paramId);
            grp.tabs.push_back(std::move(btn));
        }

        tabGroups_.push_back(std::move(grp));
    };

    // Ergonomic 2-tier console layout maintaining hardware colors, grouping, and left-to-right order:
    // Tier 0 (19 levers): UPPER MANUAL (11) + PRESET (8)
    // Tier 1 (16 levers): PEDAL (4) + EFFECT (6) + LOWER MANUAL (6)
    // (Note: Knee Sustainer remains controllable independently via MIDI CC#64)
    addGroup(0, "UPPER MANUAL", {
        {param_ids::upperSustain,   "SUSTAIN",  "",      TabColor::Black},
        {param_ids::upperTibia16,   "TIBIA",    "16",    TabColor::White},
        {param_ids::upperTibia8,    "TIBIA",    "8",     TabColor::White},
        {param_ids::upperTibia4,    "TIBIA",    "4",     TabColor::White},
        {param_ids::upperTibia2_23, "TIBIA",    "2 2/3", TabColor::White},
        {param_ids::upperString16,  "STRING",   "16",    TabColor::Yellow},
        {param_ids::upperDiapason8, "DIAPASON", "8",     TabColor::White},
        {param_ids::upperString8,   "STRING",   "8",     TabColor::Yellow},
        {param_ids::upperOboe8,     "OBOE",     "8",     TabColor::Red},
        {param_ids::upperClarinet8, "CLARINET", "8",     TabColor::Red},
        {param_ids::upperString4,   "STRING",   "4",     TabColor::Yellow},
    });

    addGroup(0, "PRESET", {
        {param_ids::organSilent,       "ORGAN\nSILENT",   "",      TabColor::Black},
        {param_ids::presetPerc2_23,    "PERCUS.",         "2 2/3", TabColor::Black},
        {param_ids::presetWowTrumpet,  "WOW\nTRUMPET",    "",      TabColor::Black},
        {param_ids::presetBanjo,       "BANJO",           "",      TabColor::Green},
        {param_ids::presetMandolin,    "MANDOLIN",        "",      TabColor::Green},
        {param_ids::presetMarimba,     "MARIMBA",         "",      TabColor::Green},
        {param_ids::presetPiano,       "PIANO",           "",      TabColor::Yellow},
        {param_ids::presetHarpsichord, "HARPSI-\nCHORD",  "",      TabColor::Yellow},
    });

    addGroup(1, "PEDAL", {
        {param_ids::pedalBourdon16,   "BOURDON",      "16", TabColor::White},
        {param_ids::pedalMajorFlute8, "MAJOR\nFLUTE", "8",  TabColor::White},
        {param_ids::pedalStringBass,  "STRING\nBASS", "",   TabColor::Yellow},
        {param_ids::pedalSustain,     "SUSTAIN",      "",   TabColor::Black},
    });

    addGroup(1, "EFFECT", {
        {param_ids::vibratoFast,   "VIBRATO",        "FAST", TabColor::Green},
        {param_ids::delayVibrato,  "DELAY\nVIBRATO", "",     TabColor::Green},
        {param_ids::vibrato,       "VIBRATO",        "",     TabColor::Green},
        {param_ids::ensemble,      "ENSEMBLE",       "",     TabColor::Blue},
        {param_ids::rotaryChorus,  "CHORUS",         "",     TabColor::Blue},
        {param_ids::rotaryTremolo, "TREMOLO",        "",     TabColor::Blue},
    });

    addGroup(1, "LOWER MANUAL", {
        {param_ids::lowerTibia8,    "TIBIA",    "8", TabColor::White},
        {param_ids::lowerTibia4,    "TIBIA",    "4", TabColor::White},
        {param_ids::lowerDiapason8, "DIAPASON", "8", TabColor::White},
        {param_ids::lowerString8,   "STRING",   "8", TabColor::Yellow},
        {param_ids::lowerHorn8,     "HORN",     "8", TabColor::Red},
        {param_ids::lowerString4,   "STRING",   "4", TabColor::Yellow},
    });
}

void KE310AudioProcessorEditor::initMidiAndCalibrationBay()
{
    auto& apvts = processor_.apvts();

    auto initCombo = [&](juce::Label& lbl, juce::ComboBox& box, const char* paramId)
    {
        lbl.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        lbl.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(lbl);

        if (const auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramId)))
        {
            box.addItemList(choiceParam->choices, 1);
        }
        addAndMakeVisible(box);
        comboAttachments_.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramId, box));
    };

    auto initHSlider = [&](juce::Label& lbl, juce::Slider& slider, const char* paramId, int decimals)
    {
        lbl.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        lbl.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(lbl);

        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 18);
        slider.setNumDecimalPlacesToDisplay(decimals);
        addAndMakeVisible(slider);
        sliderAttachments_.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider));
    };

    auto initToggle = [&](juce::ToggleButton& btn, const char* paramId)
    {
        addAndMakeVisible(btn);
        buttonAttachments_.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, paramId, btn));
    };

    // Row 4 Left: MIDI Routing
    initCombo(midiModeLbl_, midiModeBox_, param_ids::midiMode);
    initCombo(splitInputChLbl_, splitInputChBox_, param_ids::splitInputCh);
    initCombo(upperChLbl_, upperChBox_, param_ids::upperMidiCh);
    initCombo(lowerChLbl_, lowerChBox_, param_ids::lowerMidiCh);
    initCombo(pedalChLbl_, pedalChBox_, param_ids::pedalMidiCh);

    initHSlider(upperOctLbl_, upperOctSlider_, param_ids::upperOctave, 0);
    initHSlider(lowerOctLbl_, lowerOctSlider_, param_ids::lowerOctave, 0);
    initHSlider(pedalOctLbl_, pedalOctSlider_, param_ids::pedalOctave, 0);
    initHSlider(pedalSplitMaxLbl_, pedalSplitMaxSlider_, param_ids::pedalSplitMax, 0);
    initHSlider(lowerSplitMaxLbl_, lowerSplitMaxSlider_, param_ids::lowerSplitMax, 0);

    initToggle(foldPedalBtn_, param_ids::foldPedalOctave);
    initToggle(clampKeyRangeBtn_, param_ids::clampKeyRange);

    // Row 4 Right: Tuning, Expression & Rotary
    initToggle(exprLoudnessBtn_, param_ids::expressionLoudnessComp);
    initHSlider(a4HzLbl_, a4HzSlider_, param_ids::a4Hz, 2);

    rotaryHeaderLbl_.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    rotaryHeaderLbl_.setColour(juce::Label::textColourId, palette::amberLed);
    rotaryHeaderLbl_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(rotaryHeaderLbl_);

    rotaryStatusLbl_.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    rotaryStatusLbl_.setColour(juce::Label::textColourId, palette::mutedText);
    rotaryStatusLbl_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(rotaryStatusLbl_);

    initHSlider(rotarySlowLbl_, rotarySlowSlider_, param_ids::rotarySlowHz, 2);
    initHSlider(rotaryFastLbl_, rotaryFastSlider_, param_ids::rotaryFastHz, 2);
}

void KE310AudioProcessorEditor::clearAllTabs()
{
    auto& apvts = processor_.apvts();
    for (const char* id : allTabParamIds_)
    {
        if (auto* p = apvts.getParameter(id))
        {
            p->setValueNotifyingHost(0.0f);
        }
    }
}

void KE310AudioProcessorEditor::paint(juce::Graphics& g)
{
    // Base dark cabinet
    g.fillAll(palette::bayBg);

    // Row 1: Ivory top console bezel
    {
        juce::ColourGradient ivoryGrad(palette::ivoryHi, 0.0f, static_cast<float>(headerBounds_.getY()),
                                       palette::ivoryTop, 0.0f, static_cast<float>(headerBounds_.getBottom()), false);
        g.setGradientFill(ivoryGrad);
        g.fillRect(headerBounds_);

        g.setColour(palette::ivoryShadow);
        g.drawHorizontalLine(headerBounds_.getBottom() - 1, 0.0f, static_cast<float>(getWidth()));

        auto emblemArea = headerBounds_.withWidth(220).reduced(12, 6);
        g.setColour(palette::darkInk);
        g.setFont(juce::Font(juce::FontOptions(19.0f, juce::Font::bold)));
        g.drawText("KE-310", emblemArea.removeFromTop(23), juce::Justification::centredLeft, false);

        g.setColour(juce::Colour(0xff5b5044));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText("1979 TRANSISTOR ORGAN", emblemArea, juce::Justification::centredLeft, false);
    }

    // Row 2: Upper dark anodized rotary knob rail
    {
        juce::ColourGradient railGrad(palette::panelTop, 0.0f, static_cast<float>(knobRailBounds_.getY()),
                                      palette::panelBot, 0.0f, static_cast<float>(knobRailBounds_.getBottom()), false);
        g.setGradientFill(railGrad);
        g.fillRect(knobRailBounds_);

        // Silver trim strips matching KE-310 faceplate
        g.setColour(palette::silverTrim.withAlpha(0.75f));
        g.fillRect(knobRailBounds_.getX(), knobRailBounds_.getY(), knobRailBounds_.getWidth(), 2);
        g.fillRect(knobRailBounds_.getX(), knobRailBounds_.getBottom() - 2, knobRailBounds_.getWidth(), 2);

        g.setColour(palette::creamText);
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        for (const auto& k : knobs_)
        {
            auto titleArea = k.cellBounds.withHeight(16);
            g.drawFittedText(k.label, titleArea, juce::Justification::centred, 1, 0.68f);
        }
    }

    // Row 3: Two-tier tone-lever console + silkscreen section headers
    {
        g.setColour(palette::tabTrayBg);
        g.fillRect(tabConsoleBounds_);

        auto drawTierFasciaAndWells = [&](juce::Rectangle<int> tierArea, int tierIdx)
        {
            g.setColour(juce::Colour(0xff2c2622));
            g.fillRect(tabConsoleBounds_.getX(), tierArea.getY(), tabConsoleBounds_.getWidth(), 19);

            for (const auto& grp : tabGroups_)
            {
                if (grp.tier != tierIdx)
                {
                    continue;
                }

                auto hdr = grp.groupBounds.withY(tierArea.getY() + 1).withHeight(17);
                drawSectionBracket(g, hdr, grp.title);

                // Recessed molded bezel well behind each group's levers
                auto well = grp.groupBounds.withTrimmedTop(20).withTrimmedBottom(2).reduced(-1, -1);
                g.setColour(juce::Colour(0xff080706));
                g.fillRoundedRectangle(well.toFloat(), 3.0f);
            }
        };

        drawTierFasciaAndWells(tabTier0Bounds_, 0);
        drawTierFasciaAndWells(tabTier1Bounds_, 1);

        // Brushed silver divider bar between Tier 0 and Tier 1
        const int midDivY = (tabTier0Bounds_.getBottom() + tabTier1Bounds_.getY()) / 2;
        g.setColour(palette::silverTrim.withAlpha(0.45f));
        g.fillRect(tabConsoleBounds_.getX(), midDivY - 1, tabConsoleBounds_.getWidth(), 2);

        // Balanced molded cheek framing beside both centered tiers
        {
            auto drawCheekBlock = [&](juce::Rectangle<int> cheek)
            {
                if (cheek.getWidth() < 8)
                {
                    return;
                }
                g.setColour(juce::Colour(0xff141210));
                g.fillRoundedRectangle(cheek.toFloat(), 3.0f);
                g.setColour(juce::Colour(0xff29241f));
                g.drawRoundedRectangle(cheek.toFloat().reduced(0.5f), 3.0f, 1.0f);
            };

            auto drawTierCheeks = [&](juce::Rectangle<int> tierBounds, int tierIdx)
            {
                int tierLeft = tierBounds.getRight();
                int tierRight = tierBounds.getX();
                for (const auto& grp : tabGroups_)
                {
                    if (grp.tier == tierIdx)
                    {
                        tierLeft = std::min(tierLeft, grp.groupBounds.getX());
                        tierRight = std::max(tierRight, grp.groupBounds.getRight());
                    }
                }
                drawCheekBlock(tierBounds.withTrimmedTop(20).withTrimmedBottom(2).withRight(tierLeft - 12));
                drawCheekBlock(tierBounds.withTrimmedTop(20).withTrimmedBottom(2).withLeft(tierRight + 12));
            };

            drawTierCheeks(tabTier0Bounds_, 0);
            drawTierCheeks(tabTier1Bounds_, 1);
        }

        g.setColour(palette::silverTrim.withAlpha(0.55f));
        g.fillRect(tabConsoleBounds_.getX(), tabConsoleBounds_.getBottom() - 2, tabConsoleBounds_.getWidth(), 2);
    }

    // Row 4: MIDI Routing & Tuning/Expression Cards
    {
        auto drawCard = [&](juce::Rectangle<int> rect, const juce::String& title)
        {
            g.setColour(palette::cardBg);
            g.fillRoundedRectangle(rect.toFloat(), 6.0f);
            g.setColour(palette::cardBorder);
            g.drawRoundedRectangle(rect.toFloat().reduced(0.5f), 6.0f, 1.0f);

            auto hdr = rect.withHeight(24).reduced(10, 0);
            g.setColour(palette::amberLed);
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.drawText(title, hdr, juce::Justification::centredLeft, false);

            g.setColour(palette::cardBorder);
            g.drawHorizontalLine(rect.getY() + 25, static_cast<float>(rect.getX() + 8),
                                 static_cast<float>(rect.getRight() - 8));
        };

        drawCard(midiCardBounds_, "MIDI ROUTING & MANUAL ASSIGNMENT");
        drawCard(calCardBounds_, "TUNING, EXPRESSION & ROTARY");

        if (!rotarySectionBounds_.isEmpty())
        {
            g.setColour(palette::cardBorder);
            g.drawHorizontalLine(rotarySectionBounds_.getY() - 4,
                                 static_cast<float>(calCardBounds_.getX() + 10),
                                 static_cast<float>(calCardBounds_.getRight() - 10));
        }
    }
}

void KE310AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerBounds_ = bounds.removeFromTop(getHeight() < 680 ? 54 : 56);

    const int remH = bounds.getHeight();
    const int knobH = juce::jlimit(86, 108, static_cast<int>(static_cast<float>(remH) * 0.145f));
    const int reservedLowerH = std::max(230, (remH * 36) / 100);
    const int tabH = juce::jlimit(248, 420, remH - knobH - reservedLowerH);

    knobRailBounds_ = bounds.removeFromTop(knobH);
    tabConsoleBounds_ = bounds.removeFromTop(tabH);
    lowerBayBounds_ = bounds;

    // Header layout
    {
        auto row = headerBounds_.reduced(10, 6);
        row.removeFromLeft(210); // emblem space

        const int stripW = std::min(430, std::max(270, row.getWidth() / 2));
        auto controls = row.removeFromLeft(stripW).reduced(0, 7);
        prevPresetBtn_.setBounds(controls.removeFromLeft(26).reduced(1, 1));
        allOffBtn_.setBounds(controls.removeFromRight(62).reduced(2, 1));
        nextPresetBtn_.setBounds(controls.removeFromRight(26).reduced(1, 1));
        presetBox_.setBounds(controls.reduced(3, 1));

        telemetry_.setBounds(row.withTrimmedLeft(8));
    }

    // 10 Knobs across knobRailBounds_
    {
        auto rail = knobRailBounds_.reduced(14, 5);
        const int nKnobs = static_cast<int>(knobs_.size());
        const int slotW = rail.getWidth() / std::max(1, nKnobs);
        for (int i = 0; i < nKnobs; ++i)
        {
            auto cell = (i == nKnobs - 1) ? rail : rail.removeFromLeft(slotW);
            knobs_[static_cast<std::size_t>(i)].cellBounds = cell;
            auto sliderArea = cell.withTrimmedTop(16).reduced(2, 0);
            knobs_[static_cast<std::size_t>(i)].slider->setBounds(sliderArea);
        }
    }

    // 35 Tone levers in 2 ergonomic centered tiers constrained to slender ~1 : 2.9 hardware proportion
    {
        auto tray = tabConsoleBounds_.reduced(10, 4);
        constexpr int kGapPx = 18;
        constexpr int kTierSplitGap = 6;
        const int tierH = (tray.getHeight() - kTierSplitGap) / 2;
        tabTier0Bounds_ = tray.removeFromTop(tierH);
        tray.removeFromTop(kTierSplitGap);
        tabTier1Bounds_ = tray;

        int tier0Tabs = 0;
        int tier0Groups = 0;
        for (const auto& grp : tabGroups_)
        {
            if (grp.tier == 0)
            {
                tier0Tabs += static_cast<int>(grp.tabs.size());
                ++tier0Groups;
            }
        }
        const int tier0AvailW = tabTier0Bounds_.getWidth() - kGapPx * std::max(0, tier0Groups - 1);
        const int hTabsRef = std::max(24, tabTier0Bounds_.getHeight() - 24);
        const float maxSlotWf = (static_cast<float>(hTabsRef) * 0.86f) / 2.88f + 5.2f;
        const float rawSlotWf = static_cast<float>(tier0AvailW) / static_cast<float>(std::max(1, tier0Tabs));
        const float uniformSlotWf = std::min(rawSlotWf, maxSlotWf);

        auto layoutTier = [&](int tierIdx, juce::Rectangle<int> tierArea)
        {
            int nTierTabs = 0;
            int nTierGroups = 0;
            for (const auto& grp : tabGroups_)
            {
                if (grp.tier == tierIdx)
                {
                    nTierTabs += static_cast<int>(grp.tabs.size());
                    ++nTierGroups;
                }
            }
            const int spanW = static_cast<int>(std::round(uniformSlotWf * static_cast<float>(nTierTabs)))
                              + kGapPx * std::max(0, nTierGroups - 1);
            int xCursor = tierArea.getX() + std::max(0, (tierArea.getWidth() - spanW) / 2);
            const int yTabs = tierArea.getY() + 21;
            const int hTabs = std::max(24, tierArea.getBottom() - yTabs - 3);

            for (int i = 0; i < static_cast<int>(tabGroups_.size()); ++i)
            {
                auto& grp = tabGroups_[static_cast<std::size_t>(i)];
                if (grp.tier != tierIdx)
                {
                    continue;
                }

                const int nTabs = static_cast<int>(grp.tabs.size());
                const int grpW = static_cast<int>(std::round(uniformSlotWf * static_cast<float>(nTabs)));

                grp.groupBounds = {xCursor, tierArea.getY(), grpW, tierArea.getHeight()};
                const float tabWf = static_cast<float>(grpW) / static_cast<float>(std::max(1, nTabs));
                for (int t = 0; t < nTabs; ++t)
                {
                    const int x0 = xCursor + static_cast<int>(std::round(static_cast<float>(t) * tabWf));
                    const int x1 = (t + 1 == nTabs)
                        ? grp.groupBounds.getRight()
                        : xCursor + static_cast<int>(std::round(static_cast<float>(t + 1) * tabWf));
                    grp.tabs[static_cast<std::size_t>(t)]->setBounds(x0, yTabs, std::max(1, x1 - x0), hTabs);
                }

                xCursor = grp.groupBounds.getRight() + kGapPx;
            }
        };

        layoutTier(0, tabTier0Bounds_);
        layoutTier(1, tabTier1Bounds_);
    }

    // Row 4 Lower Bay: Expanded MIDI Card (left 70%) + Tuning & Expression Card (right 30%)
    {
        auto bay = lowerBayBounds_.reduced(10, 8);
        const int midiW = (bay.getWidth() * 70) / 100;
        midiCardBounds_ = bay.removeFromLeft(midiW);
        bay.removeFromLeft(10);
        calCardBounds_ = bay;

        // MIDI Card internals
        {
            auto m = midiCardBounds_.reduced(10, 6);
            m.removeFromTop(24); // title header
            const int stepH = std::max(24, m.getHeight() / 5);

            auto rowA = m.removeFromTop(stepH).reduced(0, 2);
            {
                auto halfL = rowA.removeFromLeft(rowA.getWidth() / 2).reduced(2, 0);
                auto halfR = rowA.reduced(2, 0);
                midiModeLbl_.setBounds(halfL.removeFromLeft(78));
                midiModeBox_.setBounds(halfL);
                splitInputChLbl_.setBounds(halfR.removeFromLeft(94));
                splitInputChBox_.setBounds(halfR);
            }

            auto rowB = m.removeFromTop(stepH).reduced(0, 2);
            {
                const int thirdW = rowB.getWidth() / 3;
                auto c1 = rowB.removeFromLeft(thirdW).reduced(2, 0);
                auto c2 = rowB.removeFromLeft(thirdW).reduced(2, 0);
                auto c3 = rowB.reduced(2, 0);
                upperChLbl_.setBounds(c1.removeFromLeft(60));
                upperChBox_.setBounds(c1);
                lowerChLbl_.setBounds(c2.removeFromLeft(60));
                lowerChBox_.setBounds(c2);
                pedalChLbl_.setBounds(c3.removeFromLeft(58));
                pedalChBox_.setBounds(c3);
            }

            auto rowC = m.removeFromTop(stepH).reduced(0, 2);
            {
                const int thirdW = rowC.getWidth() / 3;
                auto c1 = rowC.removeFromLeft(thirdW).reduced(2, 0);
                auto c2 = rowC.removeFromLeft(thirdW).reduced(2, 0);
                auto c3 = rowC.reduced(2, 0);
                upperOctLbl_.setBounds(c1.removeFromLeft(64));
                upperOctSlider_.setBounds(c1);
                lowerOctLbl_.setBounds(c2.removeFromLeft(64));
                lowerOctSlider_.setBounds(c2);
                pedalOctLbl_.setBounds(c3.removeFromLeft(60));
                pedalOctSlider_.setBounds(c3);
            }

            auto rowD = m.removeFromTop(stepH).reduced(0, 2);
            {
                auto halfL = rowD.removeFromLeft(rowD.getWidth() / 2).reduced(2, 0);
                auto halfR = rowD.reduced(2, 0);
                pedalSplitMaxLbl_.setBounds(halfL.removeFromLeft(94));
                pedalSplitMaxSlider_.setBounds(halfL);
                lowerSplitMaxLbl_.setBounds(halfR.removeFromLeft(96));
                lowerSplitMaxSlider_.setBounds(halfR);
            }

            auto rowE = m.reduced(0, 2);
            {
                auto halfL = rowE.removeFromLeft(rowE.getWidth() * 40 / 100);
                foldPedalBtn_.setBounds(halfL);
                clampKeyRangeBtn_.setBounds(rowE);
            }
        }

        // Tuning, Expression & Rotary Card internals
        {
            auto c = calCardBounds_.reduced(10, 6);
            c.removeFromTop(26); // title header
            exprLoudnessBtn_.setBounds(c.removeFromTop(24));
            c.removeFromTop(6);
            auto a4Row = c.removeFromTop(26).reduced(0, 2);
            a4HzLbl_.setBounds(a4Row.removeFromLeft(108));
            a4HzSlider_.setBounds(a4Row);

            c.removeFromTop(10);
            rotarySectionBounds_ = c.removeFromTop(20);
            auto rHdr = rotarySectionBounds_;
            rotaryHeaderLbl_.setBounds(rHdr.removeFromLeft((rHdr.getWidth() * 45) / 100));
            rotaryStatusLbl_.setBounds(rHdr);

            c.removeFromTop(4);
            auto slowRow = c.removeFromTop(26).reduced(0, 2);
            rotarySlowLbl_.setBounds(slowRow.removeFromLeft(108));
            rotarySlowSlider_.setBounds(slowRow);

            c.removeFromTop(4);
            auto fastRow = c.removeFromTop(26).reduced(0, 2);
            rotaryFastLbl_.setBounds(fastRow.removeFromLeft(108));
            rotaryFastSlider_.setBounds(fastRow);
        }
    }
}

void KE310AudioProcessorEditor::timerCallback()
{
    const int currentProg = processor_.getCurrentProgram();
    if (presetBox_.getSelectedId() != currentProg + 1)
    {
        presetBox_.setSelectedId(currentProg + 1, juce::dontSendNotification);
    }

    const auto* ensParam = processor_.apvts().getRawParameterValue(param_ids::ensemble);
    const bool ensOn = (ensParam != nullptr) && (ensParam->load(std::memory_order_relaxed) >= 0.5f);

    const auto* chorusParam = processor_.apvts().getRawParameterValue(param_ids::rotaryChorus);
    const auto* tremParam = processor_.apvts().getRawParameterValue(param_ids::rotaryTremolo);
    const bool chorusOn = (chorusParam != nullptr) && (chorusParam->load(std::memory_order_relaxed) >= 0.5f);
    const bool tremOn = (tremParam != nullptr) && (tremParam->load(std::memory_order_relaxed) >= 0.5f);
    const float rotaryHz = processor_.rotarySpeedHz();

    juce::String rotaryText;
    juce::Colour rotaryColour = palette::mutedText;
    if (tremOn)
    {
        rotaryText = "TREMOLO  " + juce::String(rotaryHz, 2) + " Hz";
        rotaryColour = juce::Colour(0xff67b6ff);
    }
    else if (chorusOn)
    {
        rotaryText = "CHORUS  " + juce::String(rotaryHz, 2) + " Hz";
        rotaryColour = juce::Colour(0xff67b6ff);
    }
    else if (rotaryHz >= 0.01f)
    {
        rotaryText = "BRAKING  " + juce::String(rotaryHz, 2) + " Hz";
        rotaryColour = palette::amberLed;
    }
    else
    {
        rotaryText = "BRAKE  0.00 Hz";
    }

    if (rotaryText != lastRotaryStatusText_ || rotaryColour != lastRotaryStatusColour_)
    {
        lastRotaryStatusText_ = rotaryText;
        lastRotaryStatusColour_ = rotaryColour;
        rotaryStatusLbl_.setText(rotaryText, juce::dontSendNotification);
        rotaryStatusLbl_.setColour(juce::Label::textColourId, rotaryColour);
    }

    telemetry_.updateTelemetry(ensOn,
                               processor_.activeUpperNotes(),
                               processor_.activeLowerNotes(),
                               processor_.activePedalMidi(),
                               processor_.mainBusRms(),
                               processor_.rotorBusRms(),
                               processor_.stationaryBusRms());
    telemetry_.repaint();
}

} // namespace ke310::plugin
