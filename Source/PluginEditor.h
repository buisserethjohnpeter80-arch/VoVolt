#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VoVoltLookAndFeel.h"
#include <array>

class VoVoltAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoVoltAudioProcessorEditor(VoVoltAudioProcessor&);
    ~VoVoltAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void setCharacter(int index);
    void paintMeter(juce::Graphics&, float x, float y, float level, const juce::String& label) const;
    VoVoltAudioProcessor& processor;
    VoVoltLookAndFeel lookAndFeel;
    juce::Slider drive, tone, mix, output;
    juce::Label heading, subtitle, characterLabel, driveLabel, toneLabel, mixLabel, outputLabel;
    juce::Label presetName, driveValue;
    std::array<juce::TextButton, 4> modes;
    juce::TextButton bypass { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttach, toneAttach, mixAttach, outputAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;
    float inMeter = 0.0f, outMeter = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoVoltAudioProcessorEditor)
};
