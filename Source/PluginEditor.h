#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

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
    VoVoltAudioProcessor& processor;
    juce::Slider drive, tone, mix, output;
    juce::Label driveLabel, toneLabel, mixLabel, outputLabel, heading, subtitle, characterLabel;
    std::array<juce::TextButton, 4> modes;
    juce::ToggleButton bypass { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttach, toneAttach, mixAttach, outputAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;
    float inMeter = 0.0f, outMeter = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoVoltAudioProcessorEditor)
};
