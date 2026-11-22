/**
 * ==============================================================================
 *  VintageConsoleChannelStripLookAndFeel.h
 *  Complete JUCE C++ LookAndFeel containing:
 *  1. Tactical Winged-Bar Rotary Knob + Center Phillips Screw + Bipolar/Unipolar Arc
 *  2. Console Linear Fader (with calibrated dB scale & 0 dB hairline)
 *  3. Illuminated Push-Button (amber / crimson diffused lens) & Tactical Segmented Buttons
 *  4. Dark Tactical ComboBox styling
 * ==============================================================================
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

#if __has_include("BinaryData.h")
 #include "BinaryData.h"
 #define MILITANT_HAS_BINARY_DATA 1
#else
 #define MILITANT_HAS_BINARY_DATA 0
#endif

class VintageConsoleChannelStripLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VintageConsoleChannelStripLookAndFeel()
    {
        setDefaultSansSerifTypefaceName("Barlow");

#if MILITANT_HAS_BINARY_DATA
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const char* resName = BinaryData::namedResourceList[i];
            const juce::String lowerName = juce::String(resName).toLowerCase();
            if (!lowerName.contains("barlow") || (!lowerName.contains("ttf") && !lowerName.contains("otf")))
                continue;

            int dataSize = 0;
            const char* rawData = BinaryData::getNamedResource(resName, dataSize);
            if (rawData == nullptr || dataSize <= 0)
                continue;

            auto tf = juce::Typeface::createSystemTypefaceFor(rawData, static_cast<size_t>(dataSize));
            if (tf == nullptr)
                continue;

            if (lowerName.contains("italic"))
                continue;

            if (lowerName.contains("bold") || lowerName.contains("700"))
            {
                // Prefer true Bold over SemiBold if both exist
                if (barlowBold == nullptr || !lowerName.contains("semibold"))
                    barlowBold = tf;
            }
            else if (lowerName.contains("regular") || lowerName.contains("400") || lowerName.contains("medium"))
            {
                if (barlowRegular == nullptr || lowerName.contains("regular"))
                    barlowRegular = tf;
            }
            else if (barlowRegular == nullptr)
            {
                barlowRegular = tf;
            }
        }

        if (barlowRegular == nullptr) barlowRegular = barlowBold;
        if (barlowBold == nullptr)    barlowBold = barlowRegular;
#endif

        setColour(juce::ComboBox::backgroundColourId,             juce::Colour(0xff0d0f0d));
        setColour(juce::ComboBox::textColourId,                   juce::Colour(0xffecece6));
        setColour(juce::ComboBox::outlineColourId,                juce::Colour(0xff343b32));
        setColour(juce::ComboBox::arrowColourId,                  juce::Colour(0xff9ba196));

        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff141714));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(0xffecece6));
        setColour(juce::PopupMenu::headerTextColourId,            juce::Colour(0xff9ba196));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffe59a2b));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xff121412));
    }

    ~VintageConsoleChannelStripLookAndFeel() override = default;

    juce::Font makeFont(float height, bool bold = false) const
    {
        if (bold && barlowBold != nullptr)
            return juce::Font(juce::FontOptions(barlowBold).withHeight(height));
        if (!bold && barlowRegular != nullptr)
            return juce::Font(juce::FontOptions(barlowRegular).withHeight(height));
        if (barlowRegular != nullptr)
            return juce::Font(juce::FontOptions(barlowRegular).withHeight(height));

        return juce::Font(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& font) override
    {
        if (font.isBold() && barlowBold != nullptr)
            return barlowBold;

        if (barlowRegular != nullptr)
            return barlowRegular;

        return juce::LookAndFeel_V4::getTypefaceForFont(font);
    }

    //==============================================================================
    // 1. ROTARY KNOB (Tactical Winged Bar Pointer Knob + Center Screw)
    //==============================================================================
    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();

        const float scaleRadius = radius - 6.0f;
        const float skirtRadius = radius * 0.64f;
        const float bossRadius  = skirtRadius * 0.52f;
        const float wingHalfW   = skirtRadius * 1.04f;
        const float wingHalfH   = skirtRadius * 0.22f;

        // Outer dial scale (#2b2d31 gray track + #E59A2B amber active arc)
        juce::Path outerArc;
        outerArc.addCentredArc(centreX, centreY, scaleRadius, scaleRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour::fromString("ff2b2d31"));
        g.strokePath(outerArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const bool isBipolar = static_cast<bool>(slider.getProperties().getWithDefault("bipolar", false));
        const float midAngle = 0.5f * (rotaryStartAngle + rotaryEndAngle);
        const float activeStartAngle = isBipolar ? midAngle : rotaryStartAngle;

        if (std::abs(toAngle - activeStartAngle) > 0.015f)
        {
            juce::Path activeArc;
            const float arcFrom = juce::jmin(activeStartAngle, toAngle);
            const float arcTo   = juce::jmax(activeStartAngle, toAngle);
            activeArc.addCentredArc(centreX, centreY, scaleRadius, scaleRadius, 0.0f, arcFrom, arcTo, true);
            g.setColour(juce::Colour::fromString("ffe59a2b"));
            g.strokePath(activeArc, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Tick notches at min, center (12 o'clock), max
        const float tickAngles[3] = { rotaryStartAngle, midAngle, rotaryEndAngle };
        g.setColour(juce::Colour::fromString("ff4a5247"));
        for (float angle : tickAngles)
        {
            const float r1 = scaleRadius + 3.2f;
            const float r2 = scaleRadius + 6.0f;
            const float sinA = std::sin(angle);
            const float cosA = -std::cos(angle);
            g.drawLine(centreX + r1 * sinA, centreY + r1 * cosA,
                       centreX + r2 * sinA, centreY + r2 * cosA, 1.2f);
        }

        auto transform = juce::AffineTransform::rotation(toAngle - juce::MathConstants<float>::halfPi).translated(centreX, centreY);

        // Round Flanged Skirt
        juce::Path skirtPath;
        skirtPath.addEllipse(-skirtRadius, -skirtRadius, skirtRadius * 2.0f, skirtRadius * 2.0f);
        auto shadowPath = skirtPath;
        shadowPath.applyTransform(transform);
        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.55f), 10, juce::Point<int>(1, 4));
        shadow.drawForPath(g, shadowPath);

        g.setColour(juce::Colour::fromString("ff2c302b"));
        g.fillPath(skirtPath, transform);
        g.setColour(juce::Colour::fromString("ff464b43"));
        g.strokePath(skirtPath, juce::PathStrokeType(1.0f), transform);

        // Symmetrical Winged Pointer Bar
        juce::Path wingPath;
        wingPath.addRoundedRectangle(-wingHalfW, -wingHalfH, wingHalfW * 2.0f, wingHalfH * 2.0f, 2.0f);
        g.setColour(juce::Colour::fromString("ff353933"));
        g.fillPath(wingPath, transform);
        g.setColour(juce::Colour::fromString("ff4d534a"));
        g.strokePath(wingPath, juce::PathStrokeType(0.9f), transform);

        // Tail Wing Groove (-X)
        juce::Path tailGroove;
        tailGroove.addRoundedRectangle(-wingHalfW + 2.0f, -wingHalfH * 0.28f, wingHalfW * 0.38f, wingHalfH * 0.56f, 1.0f);
        g.setColour(juce::Colour::fromString("ff222621"));
        g.fillPath(tailGroove, transform);

        // Pointer Wing White Painted Indicator (+X)
        juce::Path tipDot;
        tipDot.addRoundedRectangle(bossRadius + 2.5f, -wingHalfH * 0.38f, (wingHalfW - bossRadius) * 0.65f, wingHalfH * 0.76f, 1.8f);
        g.setColour(juce::Colour::fromString("ffecece6"));
        g.fillPath(tipDot, transform);

        // Central Cylindrical Boss
        auto bossBounds = juce::Rectangle<float>(-bossRadius, -bossRadius, bossRadius * 2.0f, bossRadius * 2.0f);
        juce::Path bossPath;
        bossPath.addEllipse(bossBounds);
        g.setColour(juce::Colour::fromString("ff383d36"));
        g.fillPath(bossPath, transform);
        g.setColour(juce::Colour::fromString("ff4d534a"));
        g.strokePath(bossPath, juce::PathStrokeType(1.0f), transform);

        // Recessed Screw Well + Metallic Phillips Screw
        const float wellR  = bossRadius * 0.54f;
        const float screwR = bossRadius * 0.38f;
        juce::Path wellPath, screwPath;
        wellPath.addEllipse(-wellR, -wellR, wellR * 2.0f, wellR * 2.0f);
        screwPath.addEllipse(-screwR, -screwR, screwR * 2.0f, screwR * 2.0f);

        g.setColour(juce::Colour::fromString("ff1b1e1a"));
        g.fillPath(wellPath, transform);
        g.setColour(juce::Colour::fromString("ff9ba196"));
        g.fillPath(screwPath, transform);

        juce::Path crossPath;
        crossPath.addRectangle(-screwR * 0.58f, -screwR * 0.14f, screwR * 1.16f, screwR * 0.28f);
        crossPath.addRectangle(-screwR * 0.14f, -screwR * 0.58f, screwR * 0.28f, screwR * 1.16f);
        g.setColour(juce::Colour::fromString("ff20231f"));
        g.fillPath(crossPath, transform);
    }

    //==============================================================================
    // 2. LINEAR FADER (Vintage Console Channel Fader + Calibrated dB Scale)
    //==============================================================================
    int getSliderThumbRadius(juce::Slider& slider) override
    {
        if (slider.getSliderStyle() == juce::Slider::LinearVertical)
            return 20;

        return juce::LookAndFeel_V4::getSliderThumbRadius(slider);
    }

    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        juce::ignoreUnused(minSliderPos, maxSliderPos, style);

        // For single-thumb LinearVertical sliders, JUCE passes the full track bounds in y and height
        // (minSliderPos/maxSliderPos are only populated for TwoValue/ThreeValue sliders).
        const float topY = static_cast<float>(y);
        const float bottomY = static_cast<float>(y + height);
        const float trackLength = juce::jmax(10.0f, bottomY - topY);

        const float trackX = static_cast<float>(x) + static_cast<float>(width) * 0.5f + 11.0f;
        const float slotWidth = 6.0f;
        const float capW = 46.0f;
        const float capH = 38.0f;

        // Calibrated dB Scale (+12 down to -24)
        const double minVal = slider.getMinimum();
        const double maxVal = slider.getMaximum();
        const double range = juce::jmax(1.0, maxVal - minVal);
        static constexpr int scaleMarks[7] = { 12, 6, 0, -6, -12, -18, -24 };

        g.setFont(makeFont(9.5f, false));
        for (int mark : scaleMarks)
        {
            const float norm = static_cast<float>((static_cast<double>(mark) - minVal) / range);
            const float markY = bottomY - norm * trackLength;
            const juce::String text = (mark > 0 ? "+" : "") + juce::String(mark);

            g.setColour(mark == 0 ? juce::Colour::fromString("ffe59a2b") : juce::Colour::fromString("ff9ba196"));
            g.drawText(text,
                       static_cast<int>(trackX - capW * 0.5f - 28.0f),
                       static_cast<int>(markY - 6.0f),
                       24, 12,
                       juce::Justification::right);

            if (mark == 0)
            {
                g.setColour(juce::Colour::fromString("66e59a2b"));
                g.drawLine(trackX - capW * 0.45f, markY, trackX + capW * 0.45f, markY, 1.0f);
            }
        }

        // Recessed Fader Slot (#0a0b0d)
        auto slotBounds = juce::Rectangle<float>(trackX - slotWidth * 0.5f, topY, slotWidth, trackLength);
        g.setColour(juce::Colour::fromString("ff0a0b0d"));
        g.fillRoundedRectangle(slotBounds, 1.5f);
        g.setColour(juce::Colour::fromString("ff2c312b"));
        g.drawRoundedRectangle(slotBounds, 1.5f, 1.0f);

        // Sculpted Console Fader Cap (#1c1e22)
        auto capBounds = juce::Rectangle<float>(trackX - capW * 0.5f, sliderPos - capH * 0.5f, capW, capH);
        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.65f), 10, juce::Point<int>(1, 4));
        shadow.drawForRectangle(g, capBounds.toNearestInt());

        g.setColour(juce::Colour::fromString("ff1c1e22"));
        g.fillRoundedRectangle(capBounds, 3.0f);
        g.setColour(juce::Colour::fromString("ff343840"));
        g.drawRoundedRectangle(capBounds, 3.0f, 1.0f);

        // Cap Bevel & Center White Pointer Bar
        g.setColour(juce::Colour::fromString("ff2b2f36"));
        g.drawLine(capBounds.getX() + 6.0f, capBounds.getY() + 6.5f, capBounds.getRight() - 6.0f, capBounds.getY() + 6.5f, 1.0f);
        g.setColour(juce::Colour::fromString("ff101214"));
        g.drawLine(capBounds.getX() + 6.0f, capBounds.getBottom() - 6.5f, capBounds.getRight() - 6.0f, capBounds.getBottom() - 6.5f, 1.0f);

        g.setColour(juce::Colour::fromString("ffecece6"));
        g.fillRect(capBounds.getX() + 4.0f, sliderPos - 1.0f, capW - 8.0f, 2.0f);
    }

    //==============================================================================
    // 3. ILLUMINATED PUSH-BUTTON & TACTICAL UI BUTTONS
    //==============================================================================
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(backgroundColour);
        const auto cid = button.getComponentID();

        if (cid == "illuminated" || cid == "illuminated_crimson")
        {
            auto bounds = button.getLocalBounds().toFloat().reduced(3.0f);
            const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const float centreX = bounds.getCentreX();
            const float centreY = bounds.getCentreY();
            const bool isLit = button.getToggleState();

            auto collarBounds = juce::Rectangle<float>(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
            g.setColour(juce::Colour(0xff1c1e22));
            g.fillEllipse(collarBounds);
            g.setColour(juce::Colour(0xff32373f));
            g.drawEllipse(collarBounds, 1.0f);

            const float lensRadius = radius * 0.72f;
            auto lensBounds = juce::Rectangle<float>(centreX - lensRadius, centreY - lensRadius, lensRadius * 2.0f, lensRadius * 2.0f);

            if (isLit)
            {
                const auto litCol = (cid == "illuminated_crimson")
                    ? juce::Colour::fromString("ffdc2626")
                    : juce::Colour::fromString("fff59e0b");

                g.setColour(litCol.withAlpha(0.28f));
                g.fillEllipse(lensBounds.expanded(3.5f));
                g.setColour(litCol);
                g.fillEllipse(lensBounds);
                g.setColour(juce::Colours::white.withAlpha(0.55f));
                g.drawEllipse(lensBounds, 1.2f);
            }
            else
            {
                g.setColour(juce::Colour::fromString("ffdfd9cc"));
                g.fillEllipse(lensBounds);
                g.setColour(juce::Colours::black.withAlpha(0.4f));
                g.drawEllipse(lensBounds, 1.0f);
            }
            return;
        }

        // Rectangular tactical buttons (OS 1x/2x/4x segmented & Reset)
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const bool isOn = button.getToggleState();

        if (cid == "os_segment")
        {
            if (isOn)
            {
                g.setColour(juce::Colour::fromString("ffe59a2b"));
                g.fillRoundedRectangle(bounds, 2.0f);
            }
            else if (shouldDrawButtonAsHighlighted)
            {
                g.setColour(juce::Colour::fromString("ff181b17"));
                g.fillRoundedRectangle(bounds, 2.0f);
            }
            return;
        }

        // Standard utility button (Reset)
        auto bg = shouldDrawButtonAsDown
            ? juce::Colour::fromString("ff1a1d19")
            : shouldDrawButtonAsHighlighted
                ? juce::Colour::fromString("ff2c3129")
                : juce::Colour::fromString("ff222620");
        g.setColour(bg);
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour::fromString("ff4a5247") : juce::Colour::fromString("ff343b32"));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(shouldDrawButtonAsDown);
        const auto cid = button.getComponentID();
        if (cid == "illuminated" || cid == "illuminated_crimson")
            return;

        const bool isOn = button.getToggleState();
        if (cid == "os_segment")
        {
            g.setColour(isOn
                ? juce::Colour::fromString("ff121412")
                : shouldDrawButtonAsHighlighted ? juce::Colour::fromString("ffecece6") : juce::Colour::fromString("ff9ba196"));
            g.setFont(makeFont(11.5f, isOn));
        }
        else
        {
            g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour::fromString("ffecece6") : juce::Colour::fromString("ff9ba196"));
            g.setFont(makeFont(11.5f, true));
        }

        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
    }

    //==============================================================================
    // 4. TACTICAL COMBOBOX STYLING
    //==============================================================================
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        juce::Path arrow;
        const float ax = static_cast<float>(width) - 16.0f;
        const float ay = static_cast<float>(height) * 0.5f - 2.0f;
        arrow.startNewSubPath(ax, ay);
        arrow.lineTo(ax + 4.5f, ay + 4.5f);
        arrow.lineTo(ax + 9.0f, ay);

        g.setColour(juce::Colour::fromString("ff9ba196"));
        g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return makeFont(12.0f, false);
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        return makeFont(label.getFont().getHeight(), label.getFont().isBold());
    }

    juce::Font getPopupMenuFont() override
    {
        return makeFont(12.5f, false);
    }

private:
    juce::Typeface::Ptr barlowRegular;
    juce::Typeface::Ptr barlowBold;
};
