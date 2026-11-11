#include "PluginEditor.h"
#include <cmath>

namespace
{
const auto navy = juce::Colour(0xff172a40);
const auto ink = juce::Colour(0xff142b47);
const auto blue = juce::Colour(0xff1589f8);

void labelStyle(juce::Label& l, float size, bool bold, juce::Colour colour = ink)
{
    l.setFont(juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)));
    l.setColour(juce::Label::textColourId, colour);
    l.setJustificationType(juce::Justification::centred);
    l.setInterceptsMouseClicks(false, false);
}
}

VoVoltAudioProcessorEditor::VoVoltAudioProcessorEditor(VoVoltAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    setSize(1024, 560);
    setResizable(true, true);
    setResizeLimits(900, 500, 1536, 840);

    heading.setText("VoVolt", juce::dontSendNotification);
    labelStyle(heading, 32.0f, true, juce::Colours::white);
    heading.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(heading);

    subtitle.setText("Vocal Saturation", juce::dontSendNotification);
    labelStyle(subtitle, 16.0f, false, juce::Colour(0xffd3e4f6));
    addAndMakeVisible(subtitle);

    presetName.setText("DEFAULT", juce::dontSendNotification);
    labelStyle(presetName, 16.0f, true, juce::Colours::white);
    addAndMakeVisible(presetName);

    auto setup = [this](juce::Label& l, const char* title)
    {
        l.setText(title, juce::dontSendNotification);
        labelStyle(l, 18.0f, true);
        addAndMakeVisible(l);
    };
    setup(characterLabel, "Character");
    setup(driveLabel, "DRIVE");
    setup(toneLabel, "Tone");
    setup(mixLabel, "Mix");
    setup(outputLabel, "Output");
    setup(lowCutLabel, "LOW CUT");
    setup(thumpLabel, "THUMP");
    setup(highCutLabel, "HIGH CUT");

    driveValue.setText("25%", juce::dontSendNotification);
    labelStyle(driveValue, 29.0f, true);
    addAndMakeVisible(driveValue);

    const char* names[] = { "SOFT", "TUBE", "TAPE", "HARD" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = modes[static_cast<size_t>(i)];
        b.setButtonText(names[i]);
        b.setClickingTogglesState(false);
        b.onClick = [this, i] { setCharacter(i); };
        b.setLookAndFeel(&lookAndFeel);
        addAndMakeVisible(b);
    }

    bypass.setClickingTogglesState(true);
    bypass.setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(bypass);

    for (auto* s : { &drive, &tone, &mix, &output, &lowCut, &thump, &highCut })
    {
        s->setLookAndFeel(&lookAndFeel);
        addAndMakeVisible(*s);
        s->setColour(juce::Slider::textBoxTextColourId, ink);
        s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }
    drive.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    drive.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    drive.setDoubleClickReturnValue(true, 0.25);
    for (auto* s : { &tone, &mix, &output })
    {
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 83, 25);
    }
    tone.setTextValueSuffix(" %");
    mix.setTextValueSuffix(" %");
    output.setTextValueSuffix(" dB");
    lowCut.setSliderStyle(juce::Slider::LinearHorizontal);
    lowCut.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
    lowCut.setTextValueSuffix(" Hz");
    thump.setSliderStyle(juce::Slider::LinearHorizontal);
    thump.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 22);
    thump.setTextValueSuffix(" %");
    thump.textFromValueFunction = [](double value) { return juce::String(juce::roundToInt(value * 100.0)); };
    thump.valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(0.0, 1.0, text.getDoubleValue() / 100.0); };
    highCut.setSliderStyle(juce::Slider::LinearHorizontal);
    highCut.setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 22);
    highCut.setTextValueSuffix(" Hz");
    for (auto* b : { &punish, &steep, &autoGain })
    {
        b->setClickingTogglesState(true);
        b->setLookAndFeel(&lookAndFeel);
        addAndMakeVisible(*b);
    }
    tone.textFromValueFunction = [](double value) { return juce::String(juce::roundToInt((value - 0.5) * 200.0)) + "%"; };
    tone.valueFromTextFunction = [](const juce::String& s) { return juce::jlimit(0.0, 1.0, s.getDoubleValue() / 200.0 + 0.5); };
    driveAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "drive", drive);
    toneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "tone", tone);
    mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "mix", mix);
    outputAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "output", output);
    bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, "bypass", bypass);
    lowCutAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "lowCut", lowCut);
    thumpAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "thump", thump);
    highCutAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "highCut", highCut);
    punishAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, "punish", punish);
    steepAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, "steep", steep);
    autoGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, "autoGain", autoGain);
    startTimerHz(30);
    timerCallback();
}

VoVoltAudioProcessorEditor::~VoVoltAudioProcessorEditor()
{
    stopTimer();
    autoGainAttach.reset(); steepAttach.reset(); punishAttach.reset();
    highCutAttach.reset(); thumpAttach.reset(); lowCutAttach.reset();
    bypassAttach.reset();
    outputAttach.reset(); mixAttach.reset(); toneAttach.reset(); driveAttach.reset();
    for (auto& b : modes) b.setLookAndFeel(nullptr);
    for (auto* b : { &bypass, &punish, &steep, &autoGain }) b->setLookAndFeel(nullptr);
    for (auto* s : { &drive, &tone, &mix, &output, &lowCut, &thump, &highCut }) s->setLookAndFeel(nullptr);
}

void VoVoltAudioProcessorEditor::setCharacter(int index)
{
    if (auto* parameter = processor.parameters.getParameter("character"))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(static_cast<float>(index) / 3.0f);
        parameter->endChangeGesture();
    }
}

void VoVoltAudioProcessorEditor::timerCallback()
{
    inMeter = juce::jmax(processor.inputLevel.load(std::memory_order_relaxed), inMeter * 0.87f);
    outMeter = juce::jmax(processor.outputLevel.load(std::memory_order_relaxed), outMeter * 0.87f);
    // VU ballistics for the needle: fast attack, slow release, so it rides
    // the body of the vocal instead of its peaks (30 Hz timer).
    const float target = processor.inputLevel.load(std::memory_order_relaxed);
    vuLevel += (target - vuLevel) * (target > vuLevel ? 0.4f : 0.12f);
    if (auto* parameter = processor.parameters.getRawParameterValue("character"))
    {
        const int selected = juce::jlimit(0, 3, static_cast<int>(std::lround(parameter->load())));
        for (int i = 0; i < 4; ++i)
        {
            auto& b = modes[static_cast<size_t>(i)];
            b.setToggleState(i == selected, juce::dontSendNotification);
            b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            b.setColour(juce::TextButton::textColourOffId, ink);
        }
    }
    driveValue.setText(juce::String(juce::roundToInt(drive.getValue() * 100.0)) + "%", juce::dontSendNotification);
    for (auto* b : { &bypass, &punish, &steep, &autoGain })
    {
        b->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        b->setColour(juce::TextButton::textColourOffId, ink);
    }
    repaint();
}

void VoVoltAudioProcessorEditor::paintMeter(juce::Graphics& g, float x, float y, float level,
                                            const juce::String& label) const
{
    const float height = 190.0f;
    const float fraction = juce::jlimit(0.0f, 1.0f, level);
    g.setColour(ink);
    g.setFont(juce::FontOptions(16.0f));
    g.drawText(label, juce::Rectangle<float>(x - 10.0f, y - 37.0f, 56.0f, 30.0f), juce::Justification::centred);
    for (int i = 0; i < 2; ++i)
    {
        const float xx = x + static_cast<float>(i) * 16.0f;
        g.setColour(juce::Colour(0xffa8bdd5));
        g.fillRoundedRectangle(xx, y, 12.0f, height, 6.0f);
        if (fraction > 0.0f)
        {
            const float fillHeight = height * fraction;
            const auto green = juce::Colour(0xff0acb87);
            const auto yellow = juce::Colour(0xfff6d548);
            juce::ColourGradient grad(yellow, xx, y, green, xx, y + height, false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(xx, y + height - fillHeight, 12.0f, fillHeight, 5.0f);
        }
    }
    const float dB = level > 0.00001f ? juce::Decibels::gainToDecibels(level) : -100.0f;
    g.setColour(ink);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(dB <= -99.0f ? "-inf dB" : juce::String(dB, 1) + " dB",
               juce::Rectangle<float>(x - 18.0f, y + height + 9.0f, 74.0f, 26.0f),
               juce::Justification::centred);
}

void VoVoltAudioProcessorEditor::paintVuMeter(juce::Graphics& g, float x, float y, float w, float h, float level) const
{
    // Classic VU meter on the INPUT level: the needle shows how hard the
    // vocal hits the saturation circuit, which is what the Drive knob
    // reacts to. 0 VU = -18 dBFS (studio standard). Ballistics come from
    // the editor timer (fast attack, slow release), so the needle rides
    // the body of the vocal instead of its peaks. The numeric dB readout
    // sits under the face for exact staging.
    const float cx = x + w * 0.5f;
    const float pivotY = y + h - 4.0f;
    const float radius = h - 26.0f;

    g.setColour(juce::Colour(0xfff6ead0));
    g.fillRoundedRectangle(x, y, w, h, 8.0f);
    g.setColour(juce::Colour(0xff8a7a5a));
    g.drawRoundedRectangle(x, y, w, h, 8.0f, 1.2f);

    const auto toAngle = [](float vu)
    {
        return juce::degreesToRadians(-42.0f + (vu + 20.0f) * (84.0f / 23.0f));
    };
    const auto pointOnArc = [&](float vu, float r)
    {
        const float a = toAngle(vu);
        return juce::Point<float>(cx + std::sin(a) * r, pivotY - std::cos(a) * r);
    };

    // Scale arc (-20 to +3 VU).
    juce::Path arc;
    bool first = true;
    for (float vu = -20.0f; vu <= 3.0f; vu += 0.5f)
    {
        const auto p = pointOnArc(vu, radius);
        if (first) { arc.startNewSubPath(p); first = false; }
        else arc.lineTo(p);
    }
    g.setColour(ink);
    g.strokePath(arc, juce::PathStrokeType(1.4f));

    // Red zone (0 to +3 VU).
    juce::Path redArc;
    first = true;
    for (float vu = 0.0f; vu <= 3.0f; vu += 0.25f)
    {
        const auto p = pointOnArc(vu, radius);
        if (first) { redArc.startNewSubPath(p); first = false; }
        else redArc.lineTo(p);
    }
    g.setColour(juce::Colour(0xffc8102e));
    g.strokePath(redArc, juce::PathStrokeType(3.0f));

    // Ticks + numbers.
    const float ticks[] = { -20.0f, -10.0f, -7.0f, -3.0f, 0.0f, 3.0f };
    g.setFont(juce::FontOptions(11.0f));
    for (float vu : ticks)
    {
        const auto outer = pointOnArc(vu, radius);
        const auto inner = pointOnArc(vu, radius - 9.0f);
        g.setColour(vu >= 0.0f ? juce::Colour(0xffc8102e) : ink);
        g.drawLine(inner.x, inner.y, outer.x, outer.y, 1.4f);
        const auto labelPos = pointOnArc(vu, radius - 21.0f);
        const juce::String text = vu > 0.0f ? "+" + juce::String(static_cast<int>(vu))
                                             : juce::String(static_cast<int>(vu));
        g.drawText(text, juce::Rectangle<float>(labelPos.x - 14.0f, labelPos.y - 7.0f, 28.0f, 14.0f),
                   juce::Justification::centred);
    }

    g.setColour(ink);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("VU", juce::Rectangle<float>(x, y + 5.0f, w, 16.0f), juce::Justification::centred);

    // Needle.
    const float dB = level > 0.00001f ? juce::Decibels::gainToDecibels(level) : -100.0f;
    const float vu = juce::jlimit(-20.0f, 3.0f, dB + 18.0f);
    const float angle = toAngle(vu);
    const auto tip = juce::Point<float>(cx + std::sin(angle) * (radius + 4.0f),
                                        pivotY - std::cos(angle) * (radius + 4.0f));
    g.setColour(juce::Colour(0xff1a1a1a));
    g.drawLine(cx, pivotY, tip.x, tip.y, 2.4f);
    g.fillEllipse(cx - 4.5f, pivotY - 4.5f, 9.0f, 9.0f);

    // Numeric input level under the face.
    g.setColour(ink);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(dB <= -99.0f ? "IN  -inf dB" : "IN  " + juce::String(dB, 1) + " dB",
               juce::Rectangle<float>(x, y + h + 5.0f, w, 16.0f),
               juce::Justification::centred);
}

void VoVoltAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient background(juce::Colour(0xfff2f9ff), 0.0f, 80.0f,
                                    juce::Colour(0xffd8e9fa), 0.0f, 560.0f, false);
    g.setGradientFill(background);
    g.fillAll();

    const float sx = static_cast<float>(getWidth()) / 1024.0f;
    const float sy = static_cast<float>(getHeight()) / 560.0f;
    juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(juce::AffineTransform::scale(sx, sy));
    g.setColour(navy);
    g.fillRect(0.0f, 0.0f, 1024.0f, 80.0f);
    g.setColour(blue);
    juce::Path pulse;
    pulse.startNewSubPath(20.0f, 40.0f);
    pulse.lineTo(33.0f, 40.0f); pulse.lineTo(40.0f, 24.0f);
    pulse.lineTo(47.0f, 55.0f); pulse.lineTo(54.0f, 33.0f);
    pulse.lineTo(62.0f, 40.0f); pulse.lineTo(74.0f, 40.0f);
    g.strokePath(pulse, juce::PathStrokeType(2.2f));
    g.setColour(juce::Colour(0xff6886a6));
    g.drawVerticalLine(205, 19.0f, 62.0f);
    g.setColour(juce::Colour(0xff7893b1));
    g.drawRoundedRectangle(428.0f, 16.0f, 218.0f, 48.0f, 23.0f, 1.4f);
    g.setColour(juce::Colour(0xffadc3dc));
    g.drawVerticalLine(88, 110.0f, 416.0f);
    g.drawVerticalLine(935, 110.0f, 416.0f);
    paintMeter(g, 37.0f, 157.0f, inMeter, "IN");
    paintMeter(g, 962.0f, 157.0f, outMeter, "OUT");
    paintVuMeter(g, 396.0f, 362.0f, 256.0f, 76.0f, vuLevel);
    g.setColour(juce::Colour(0xffb6d7f4).withAlpha(0.38f));
    g.fillEllipse(397.0f, 121.0f, 232.0f, 232.0f);
}

void VoVoltAudioProcessorEditor::resized()
{
    const float sx = static_cast<float>(getWidth()) / 1024.0f;
    const float sy = static_cast<float>(getHeight()) / 560.0f;
    auto place = [sx, sy](juce::Component& c, int x, int y, int w, int h)
    {
        c.setBounds(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                    juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };
    place(heading, 84, 14, 160, 49);
    place(subtitle, 225, 20, 194, 40);
    place(presetName, 438, 19, 198, 40);
    place(bypass, 827, 18, 159, 43);
    place(characterLabel, 132, 137, 260, 35);
    for (int i = 0; i < 4; ++i) place(modes[static_cast<size_t>(i)], 137 + 65 * i, 193, 64, 43);
    place(drive, 399, 128, 225, 229);
    place(driveLabel, 466, 223, 93, 30);
    place(driveValue, 459, 255, 109, 46);
    place(toneLabel, 115, 345, 220, 28);
    place(tone, 105, 377, 285, 48);
    place(mixLabel, 672, 137, 205, 35);
    place(mix, 658, 190, 256, 54);
    place(outputLabel, 666, 320, 220, 35);
    place(output, 654, 363, 262, 54);
    place(lowCutLabel, 35, 468, 150, 22);
    place(lowCut, 25, 490, 165, 38);
    place(thumpLabel, 205, 468, 130, 22);
    place(thump, 195, 490, 150, 38);
    place(punish, 360, 490, 112, 38);
    place(highCutLabel, 485, 468, 145, 22);
    place(highCut, 475, 490, 165, 38);
    place(steep, 655, 490, 105, 38);
    place(autoGain, 775, 490, 150, 38);
}

juce::AudioProcessorEditor* VoVoltAudioProcessor::createEditor()
{
    return new VoVoltAudioProcessorEditor(*this);
}
