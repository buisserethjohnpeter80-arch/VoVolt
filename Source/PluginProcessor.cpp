#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

VoVoltAudioProcessor::VoVoltAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "VoVoltParameters", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout VoVoltAudioProcessor::createLayout()
{
    // Keep the IDs, ranges, order and defaults identical to v0.1 for preset compatibility.
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"character", 1}, "Character",
        juce::StringArray{"Soft", "Tube", "Tape", "Hard"}, 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"drive", 1}, "Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.25f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"tone", 1}, "Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"mix", 1}, "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.6f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"output", 1}, "Output",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass", 1}, "Bypass", false));
    return layout;
}

bool VoVoltAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo())
        && layouts.getMainOutputChannelSet() == in;
}

void VoVoltAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const int channels = juce::jmax(1, getTotalNumInputChannels());
    preparedBlockSize = juce::jmax(1, samplesPerBlock);
    dryCopy.setSize(channels, preparedBlockSize, false, true, false);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(channels), 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing(static_cast<size_t>(preparedBlockSize));
    oversampler->reset();
    // useIntegerLatency=true: keeps wet/dry delay sample-aligned at host rate.
    setLatencySamples(juce::roundToInt(oversampler->getLatencyInSamples()));
    delaySize = juce::jmax(1, getLatencySamples());
    dryDelay.assign(static_cast<size_t>(channels), std::vector<float>(static_cast<size_t>(delaySize), 0.0f));
    delayPosition = 0;

    const float internalRate = static_cast<float>(sampleRate * 4.0);
    characterState.resize(static_cast<size_t>(channels));
    for (auto& stateByCharacter : characterState)
        for (auto& state : stateByCharacter)
            state.prepare(internalRate);
    toneState.assign(static_cast<size_t>(channels), 0.0f);
    dcPreviousInput.assign(static_cast<size_t>(channels), 0.0f);
    dcPreviousOutput.assign(static_cast<size_t>(channels), 0.0f);
    dcCoefficient = std::exp(-2.0f * juce::MathConstants<float>::pi * 8.0f / internalRate);

    driveSmoothed.reset(internalRate, 0.025);
    toneSmoothed.reset(internalRate, 0.025);
    mixSmoothed.reset(sampleRate, 0.025);
    outputSmoothed.reset(sampleRate, 0.025);
    bypassSmoothed.reset(sampleRate, 0.025);
    driveSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("drive")->load());
    mixSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("mix")->load());
    toneSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("tone")->load());
    outputSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(parameters.getRawParameterValue("output")->load()));
    bypassSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("bypass")->load());

    activeCharacter = static_cast<vovolt::Character>(juce::jlimit(0, 3,
        static_cast<int>(parameters.getRawParameterValue("character")->load())));
    characterWeights.fill(0.0f);
    characterWeights[static_cast<size_t>(activeCharacter)] = 1.0f;
    characterSmoothingCoeff = 1.0f - std::exp(-1.0f / (0.006f * internalRate));
    inputLevel.store(0.0f, std::memory_order_relaxed);
    outputLevel.store(0.0f, std::memory_order_relaxed);
}

void VoVoltAudioProcessor::releaseResources()
{
    oversampler.reset();
    dryDelay.clear();
    dryCopy.setSize(0, 0);
    characterState.clear();
    toneState.clear();
    dcPreviousInput.clear();
    dcPreviousOutput.clear();
}

void VoVoltAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin(buffer.getNumChannels(), getTotalNumInputChannels());
    for (int c = channels; c < buffer.getNumChannels(); ++c)
        buffer.clear(c, 0, n);
    if (n <= 0 || channels <= 0 || !oversampler || preparedBlockSize <= 0)
        return;

    driveSmoothed.setTargetValue(parameters.getRawParameterValue("drive")->load());
    mixSmoothed.setTargetValue(parameters.getRawParameterValue("mix")->load());
    toneSmoothed.setTargetValue(parameters.getRawParameterValue("tone")->load());
    outputSmoothed.setTargetValue(juce::Decibels::decibelsToGain(parameters.getRawParameterValue("output")->load()));
    bypassSmoothed.setTargetValue(parameters.getRawParameterValue("bypass")->load());
    const auto requested = static_cast<vovolt::Character>(juce::jlimit(0, 3,
        static_cast<int>(parameters.getRawParameterValue("character")->load())));
    activeCharacter = requested; // Weights interpolate without restarting a crossfade.

    float inputPeak = 0.0f, outputPeak = 0.0f;
    for (int start = 0; start < n; start += preparedBlockSize)
        processChunk(buffer, start, juce::jmin(preparedBlockSize, n - start), channels, inputPeak, outputPeak);
    inputLevel.store(inputPeak, std::memory_order_relaxed);
    outputLevel.store(outputPeak, std::memory_order_relaxed);
}

void VoVoltAudioProcessor::processChunk(juce::AudioBuffer<float>& buffer, int start, int count,
                                         int channels, float& inputPeak, float& outputPeak) noexcept
{
    for (int c = 0; c < channels; ++c)
    {
        // Reject poisoned host samples BEFORE oversampling filter state sees them.
        float* samples = buffer.getWritePointer(c, start);
        for (int i = 0; i < count; ++i)
            if (!std::isfinite(samples[i]))
                samples[i] = 0.0f;
        dryCopy.copyFrom(c, 0, buffer, c, start, count);
        inputPeak = juce::jmax(inputPeak, buffer.getMagnitude(c, start, count));
    }

    juce::dsp::AudioBlock<float> fullBlock(buffer);
    auto block = fullBlock.getSubBlock(static_cast<size_t>(start), static_cast<size_t>(count));
    auto up = oversampler->processSamplesUp(block);
    const size_t upN = up.getNumSamples();
    const float internalRate = static_cast<float>(getSampleRate() * 4.0);

    for (size_t i = 0; i < upN; ++i)
    {
        const float drive = driveSmoothed.getNextValue();
        const float tone = toneSmoothed.getNextValue();
        const float cutoff = 3000.0f + 15000.0f * tone;
        const float filterCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / internalRate);
        // A one-pole equal-gain transition handles fast repeated character changes
        // without resetting a 2-character crossfade and causing an audible jump.
        for (size_t modeIndex = 0; modeIndex < characterWeights.size(); ++modeIndex)
        {
            const float target = modeIndex == static_cast<size_t>(activeCharacter) ? 1.0f : 0.0f;
            characterWeights[modeIndex] += characterSmoothingCoeff * (target - characterWeights[modeIndex]);
        }
        for (int c = 0; c < channels; ++c)
        {
            auto* ptr = up.getChannelPointer(static_cast<size_t>(c));
            const float input = ptr[i];
            auto& modes = characterState[static_cast<size_t>(c)];
            float wet = 0.0f;
            for (size_t modeIndex = 0; modeIndex < characterWeights.size(); ++modeIndex)
            {
                const float weight = characterWeights[modeIndex];
                if (weight > 0.00001f)
                    wet += weight * vovolt::VoltCore::shape(
                        input, static_cast<vovolt::Character>(modeIndex), drive, modes[modeIndex]);
            }
            // Existing Tone control retained, followed by a gentle DC blocker.
            float& lowpass = toneState[static_cast<size_t>(c)];
            lowpass = (1.0f - filterCoeff) * wet + filterCoeff * lowpass;
            const float hp = lowpass - dcPreviousInput[static_cast<size_t>(c)]
                + dcCoefficient * dcPreviousOutput[static_cast<size_t>(c)];
            dcPreviousInput[static_cast<size_t>(c)] = lowpass;
            dcPreviousOutput[static_cast<size_t>(c)] = hp;
            ptr[i] = hp;
        }
    }
    oversampler->processSamplesDown(block);

    for (int i = 0; i < count; ++i)
    {
        const float mix = mixSmoothed.getNextValue();
        const float output = outputSmoothed.getNextValue();
        const float bypass = bypassSmoothed.getNextValue();
        for (int c = 0; c < channels; ++c)
        {
            const float wet = buffer.getSample(c, start + i);
            const float dry = dryCopy.getSample(c, i);
            auto& ring = dryDelay[static_cast<size_t>(c)];
            const float delayed = getLatencySamples() == 0 ? dry : ring[delayPosition];
            ring[delayPosition] = dry;
            const float mixed = delayed + (wet - delayed) * mix;
            const float value = mixed * output * (1.0f - bypass) + delayed * bypass;
            buffer.setSample(c, start + i, value);
            outputPeak = juce::jmax(outputPeak, std::abs(value));
        }
        delayPosition = (delayPosition + 1) % static_cast<size_t>(delaySize);
    }
}

void VoVoltAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = parameters.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}

void VoVoltAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoVoltAudioProcessor(); }
