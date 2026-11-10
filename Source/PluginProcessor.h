#pragma once
#include <JuceHeader.h>
#include <array>
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
    void processChunk(juce::AudioBuffer<float>&, int start, int count, int channels,
                      float& inputPeak, float& outputPeak) noexcept;

    juce::LinearSmoothedValue<float> driveSmoothed, mixSmoothed, toneSmoothed, outputSmoothed, bypassSmoothed;
    juce::LinearSmoothedValue<float> lowCutSmoothed, thumpSmoothed, highCutSmoothed, punishSmoothed, autoGainSmoothed, steepSmoothed;
    std::vector<std::array<vovolt::ChannelState, 4>> characterState;
    std::vector<float> toneState, dcPreviousInput, dcPreviousOutput;
    std::vector<float> preCutPreviousInput, preCutPreviousOutput, thumpState;
    std::vector<float> highCutState1, highCutState2;
    float linkedDryRms = 1.0e-8f, linkedWetRms = 1.0e-8f, linkedAutoGain = 1.0f;
    juce::AudioBuffer<float> dryCopy;
    int preparedBlockSize = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::vector<std::vector<float>> dryDelay;
    size_t delayPosition = 0;
    int delaySize = 1;
    float dcCoefficient = 0.0f;
    float autoGainAttack = 0.0f, autoGainRelease = 0.0f, autoGainSmoothing = 0.0f;
    vovolt::Character activeCharacter = vovolt::Character::tube;
    std::array<float, 4> characterWeights { 0.0f, 1.0f, 0.0f, 0.0f };
    float characterSmoothingCoeff = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoVoltAudioProcessor)
};
