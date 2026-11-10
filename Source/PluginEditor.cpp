#include "PluginEditor.h"

namespace
{
const juce::Colour navy(0xff182c40), blue(0xff188afa), ink(0xff142c4a), light(0xffe9f4ff);
void styleSlider(juce::Slider& slider, bool rotary)
{
    slider.setSliderStyle(rotary ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, rotary ? 90 : 100, 24);
    slider.setColour(juce::Slider::thumbColourId, blue);
    slider.setColour(juce::Slider::rotarySliderFillColourId, blue);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xffbed2e8));
    slider.setColour(juce::Slider::trackColourId, blue);
    slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xffbed2e8));
    slider.setColour(juce::Slider::textBoxTextColourId, ink);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}
void styleLabel(juce::Label& label, float size, bool bold = false)
{
    label.setFont(juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)));
    label.setColour(juce::Label::textColourId, ink);
    label.setJustificationType(juce::Justification::centred);
}
}

VoVoltAudioProcessorEditor::VoVoltAudioProcessorEditor(VoVoltAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(960, 430);
    setResizable(true, true);
    setResizeLimits(760, 350, 1440, 645);

    heading.setText("VoVolt", juce::dontSendNotification);
    styleLabel(heading, 30.0f, true);
    heading.setColour(juce::Label::textColourId, juce::Colours::white);
    heading.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(heading);
    subtitle.setText("VOCAL SATURATION", juce::dontSendNotification);
    styleLabel(subtitle, 13.0f);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xffbcd7f2));
    addAndMakeVisible(subtitle);

    characterLabel.setText("CHARACTER", juce::dontSendNotification);
    styleLabel(characterLabel, 17.0f, true);
    addAndMakeVisible(characterLabel);
    const char* names[] = {"SOFT", "TUBE", "TAPE", "HARD"};
    for (int i = 0; i < 4; ++i)
    {
        modes[static_cast<size_t>(i)].setButtonText(names[i]);
        modes[static_cast<size_t>(i)].onClick = [this, i] { setCharacter(i); };
        addAndMakeVisible(modes[static_cast<size_t>(i)]);
    }

    for (auto* s : {&drive, &tone, &mix, &output}) addAndMakeVisible(*s);
    styleSlider(drive, true);
    styleSlider(tone, false);
    styleSlider(mix, false);
    styleSlider(output, false);
    drive.setTextValueSuffix(" %");
    tone.setTextValueSuffix(" %");
    mix.setTextValueSuffix(" %");
    output.setTextValueSuffix(" dB");

    auto setup = [this](juce::Label& l, const char* text)
    {
        l.setText(text, juce::dontSendNotification);
        styleLabel(l, 17.0f, true);
        addAndMakeVisible(l);
    };
    setup(driveLabel, "DRIVE");
    setup(toneLabel, "TONE");
    setup(mixLabel, "MIX");
    setup(outputLabel, "OUTPUT");

    driveAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "drive", drive);
    toneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "tone", tone);
    mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "mix", mix);
    outputAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "output", output);
    bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, "bypass", bypass);
    bypass.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(bypass);
    startTimerHz(25);
}

VoVoltAudioProcessorEditor::~VoVoltAudioProcessorEditor() { stopTimer(); }

void VoVoltAudioProcessorEditor::setCharacter(int index)
{
    if (auto* param = processor.parameters.getParameter("character"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(static_cast<float>(index) / 3.0f);
        param->endChangeGesture();
    }
}

void VoVoltAudioProcessorEditor::timerCallback()
{
    inMeter = juce::jmax(processor.inputLevel.load(std::memory_order_relaxed), inMeter * 0.88f);
    outMeter = juce::jmax(processor.outputLevel.load(std::memory_order_relaxed), outMeter * 0.88f);
    const int selected = juce::jlimit(0, 3, static_cast<int>(processor.parameters.getRawParameterValue("character")->load()));
    for (int i = 0; i < 4; ++i)
    {
        auto& b = modes[static_cast<size_t>(i)];
        b.setColour(juce::TextButton::buttonColourId, i == selected ? blue : juce::Colour(0xffd5e7f8));
        b.setColour(juce::TextButton::textColourOffId, i == selected ? juce::Colours::white : ink);
    }
    repaint();
}

void VoVoltAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto r = getLocalBounds();
    juce::ColourGradient gradient(juce::Colour(0xfff4faff), 0.0f, 72.0f,
                                  juce::Colour(0xffd9eaff), 0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(gradient);
    g.fillAll();
    g.setColour(navy);
    g.fillRect(r.removeFromTop(80));

    const float scale = static_cast<float>(getWidth()) / 960.0f;
    auto meter = [&](float x, float level, const juce::String& title)
    {
        const float y = 140.0f * scale;
        const float h = 194.0f * scale;
        const float w = 18.0f * scale;
        g.setColour(juce::Colour(0xffb8cce0));
        g.fillRoundedRectangle(x * scale, y, w, h, 7.0f);
        const float frac = juce::jlimit(0.0f, 1.0f, level);
        g.setColour(frac > 0.9f ? juce::Colour(0xffe5b348) : juce::Colour(0xff20cba0));
        g.fillRoundedRectangle(x * scale, y + h * (1.0f - frac), w, h * frac, 6.0f);
        g.setColour(ink);
        g.setFont(juce::FontOptions(15.0f * scale));
        g.drawText(title, juce::Rectangle<float>((x - 18) * scale, (y - 35.0f * scale), 56 * scale, 28 * scale),
                   juce::Justification::centred);
    };
    meter(28.0f, inMeter, "IN");
    meter(916.0f, outMeter, "OUT");
    g.setColour(juce::Colour(0xffb8cee4));
    g.drawVerticalLine(static_cast<int>(83 * scale), 115.0f * scale, 375.0f * scale);
    g.drawVerticalLine(static_cast<int>(878 * scale), 115.0f * scale, 375.0f * scale);
}

void VoVoltAudioProcessorEditor::resized()
{
    const float sx = static_cast<float>(getWidth()) / 960.0f;
    const float sy = static_cast<float>(getHeight()) / 430.0f;
    auto place = [sx, sy](juce::Component& c, int x, int y, int w, int h)
    {
        c.setBounds(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                    juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };
    place(heading, 30, 10, 225, 54);
    place(subtitle, 255, 22, 170, 34);
    place(bypass, 815, 20, 120, 42);
    place(characterLabel, 125, 125, 265, 28);
    for (int i = 0; i < 4; ++i) place(modes[static_cast<size_t>(i)], 113 + 74 * i, 166, 70, 44);
    place(driveLabel, 432, 125, 100, 30);
    place(drive, 397, 150, 170, 180);
    place(toneLabel, 130, 265, 220, 28);
    place(tone, 135, 300, 220, 75);
    place(mixLabel, 625, 125, 205, 28);
    place(mix, 625, 160, 205, 75);
    place(outputLabel, 625, 265, 205, 28);
    place(output, 625, 300, 205, 75);
}

juce::AudioProcessorEditor* VoVoltAudioProcessor::createEditor()
{
    return new VoVoltAudioProcessorEditor(*this);
}
