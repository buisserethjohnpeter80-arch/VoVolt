#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include "VoltCore.h"

class VoVoltAudioProcessor final : public juce::AudioProcessor
{
public:
    VoVoltAudioProcessor();
    ~VoVoltAudioProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VoVolt"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> inputLevel { 0.0f }, outputLevel { 0.0f };
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    juce::LinearSmoothedValue<float> driveSmoothed, mixSmoothed, toneSmoothed, outputSmoothed, bypassSmoothed;
    std::vector<float> tapeState, toneState;
    juce::AudioBuffer<float> dryCopy;
    int preparedBlockSize = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::vector<std::vector<float>> dryDelay;
    size_t delayPosition = 0;
    int delaySize = 1;
    float latencySamples = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoVoltAudioProcessor)
};
