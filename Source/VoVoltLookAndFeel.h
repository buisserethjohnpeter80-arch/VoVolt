#pragma once
#include <JuceHeader.h>
#include <cmath>

class VoVoltLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    static constexpr juce::uint32 ink = 0xff142b47;
    static constexpr juce::uint32 accent = 0xff1589f8;
    static constexpr juce::uint32 track = 0xffb7cbe2;

    VoVoltLookAndFeel()
    {
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(ink));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle, juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                    static_cast<float>(width), static_cast<float>(height));
        const float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const float radius = size * 0.43f;
        const float thickness = juce::jmax(5.0f, size * 0.052f);
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const auto ring = juce::Rectangle<float>(centre.x - radius, centre.y - radius,
                                                  radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xffaec7e3).withAlpha(0.7f));
        juce::Path base;
        base.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        g.strokePath(base, juce::PathStrokeType(thickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
        juce::Path active;
        active.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour(juce::Colour(0xff18bde9));
        g.strokePath(active, juce::PathStrokeType(thickness, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        const float inner = radius - thickness * 1.7f;
        g.setColour(juce::Colour(0xffc7dcf1).withAlpha(0.45f));
        g.fillEllipse(centre.x - inner + 1.5f, centre.y - inner + 4.0f,
                      inner * 2.0f, inner * 2.0f);
        g.setColour(juce::Colour(0xfff3f9ff));
        g.fillEllipse(centre.x - inner, centre.y - inner, inner * 2.0f, inner * 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawEllipse(centre.x - inner + 1.0f, centre.y - inner + 1.0f,
                      inner * 2.0f - 2.0f, inner * 2.0f - 2.0f, 1.3f);
        const auto point = centre.getPointOnCircumference(radius * 0.72f, angle);
        g.setColour(juce::Colour(ink));
        g.drawLine(centre.x, centre.y, point.x, point.y, juce::jmax(2.0f, size * 0.014f));
        juce::ignoreUnused(ring);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float, float, juce::Slider::SliderStyle style,
                          juce::Slider&) override
    {
        if (style != juce::Slider::LinearHorizontal) return;
        const float midY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float left = static_cast<float>(x) + 10.0f;
        const float right = static_cast<float>(x + width) - 10.0f;
        const float thumb = juce::jlimit(left, right, sliderPos);
        g.setColour(juce::Colour(track));
        g.fillRoundedRectangle(left, midY - 5.0f, right - left, 10.0f, 5.0f);
        g.setColour(juce::Colour(accent));
        g.fillRoundedRectangle(left, midY - 5.0f, juce::jmax(0.0f, thumb - left), 10.0f, 5.0f);
        g.setColour(juce::Colour(0xffc0d6ee).withAlpha(0.55f));
        g.fillEllipse(thumb - 13.0f, midY - 11.0f, 29.0f, 29.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(thumb - 13.0f, midY - 14.0f, 26.0f, 26.0f);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&, bool hover, bool down) override
    {
        auto r = button.getLocalBounds().toFloat().reduced(1.0f);
        const bool active = button.getToggleState();
        g.setColour(active ? juce::Colour(accent).brighter(down ? 0.05f : 0.0f)
                           : (hover ? juce::Colour(0xffe3f0ff) : juce::Colour(0xfff2f8ff)));
        g.fillRoundedRectangle(r, r.getHeight() * 0.43f);
        if (!active)
        {
            g.setColour(juce::Colour(0xffd4e5f7));
            g.drawRoundedRectangle(r, r.getHeight() * 0.43f, 1.0f);
        }
    }
};
