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
    // Additive v0.5 controls. Existing parameter IDs/ranges/defaults remain unchanged.
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lowCut", 1}, "Low Cut",
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.35f), 20.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"thump", 1}, "Thump",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"punish", 1}, "Punish", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"highCut", 1}, "High Cut",
        juce::NormalisableRange<float>(4000.0f, 20000.0f, 1.0f, 0.45f), 20000.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"steep", 1}, "Steep", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"autoGain", 1}, "Auto Gain", false));
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
    preCutPreviousInput.assign(static_cast<size_t>(channels), 0.0f);
    preCutPreviousOutput.assign(static_cast<size_t>(channels), 0.0f);
    thumpState.assign(static_cast<size_t>(channels), 0.0f);
    highCutState1.assign(static_cast<size_t>(channels), 0.0f);
    highCutState2.assign(static_cast<size_t>(channels), 0.0f);
    linkedDryRms = linkedWetRms = 1.0e-8f;
    linkedAutoGain = 1.0f;
    dcCoefficient = std::exp(-2.0f * juce::MathConstants<float>::pi * 8.0f / internalRate);
    autoGainAttack = 1.0f - std::exp(-1.0f / (0.050f * static_cast<float>(sampleRate)));
    autoGainRelease = 1.0f - std::exp(-1.0f / (0.500f * static_cast<float>(sampleRate)));
    autoGainSmoothing = 1.0f - std::exp(-1.0f / (0.100f * static_cast<float>(sampleRate)));

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
    lowCutSmoothed.reset(internalRate, 0.025);
    thumpSmoothed.reset(internalRate, 0.025);
    highCutSmoothed.reset(sampleRate, 0.025);
    punishSmoothed.reset(internalRate, 0.025);
    autoGainSmoothed.reset(sampleRate, 0.025);
    steepSmoothed.reset(sampleRate, 0.025);
    lowCutSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("lowCut")->load());
    thumpSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("thump")->load());
    highCutSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("highCut")->load());
    punishSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("punish")->load());
    autoGainSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("autoGain")->load());
    steepSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("steep")->load());

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
    preCutPreviousInput.clear(); preCutPreviousOutput.clear(); thumpState.clear();
    highCutState1.clear(); highCutState2.clear();

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
    lowCutSmoothed.setTargetValue(parameters.getRawParameterValue("lowCut")->load());
    thumpSmoothed.setTargetValue(parameters.getRawParameterValue("thump")->load());
    highCutSmoothed.setTargetValue(parameters.getRawParameterValue("highCut")->load());
    punishSmoothed.setTargetValue(parameters.getRawParameterValue("punish")->load());
    autoGainSmoothed.setTargetValue(parameters.getRawParameterValue("autoGain")->load());
    steepSmoothed.setTargetValue(parameters.getRawParameterValue("steep")->load());
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
        const float thump = thumpSmoothed.getNextValue();
        const float punish = punishSmoothed.getNextValue();
        const float lowCut = lowCutSmoothed.getNextValue();
        const float lowCutCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * lowCut / internalRate);
        const float thumpCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * 115.0f / internalRate);
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
            float input = ptr[i];
            const auto channel = static_cast<size_t>(c);
            // Pre-saturation high-pass. 20 Hz is the practical bypass position.
            if (lowCut > 20.5f)
            {
                const float previous = preCutPreviousInput[channel];
                const float hp = lowCutCoeff * (preCutPreviousOutput[channel] + input - previous);
                preCutPreviousInput[channel] = input;
                preCutPreviousOutput[channel] = hp;
                input = hp;
            }
            else
            {
                preCutPreviousInput[channel] = input;
                preCutPreviousOutput[channel] = input;
            }
            // Thump adds a controlled low-frequency resonant-style component before saturation.
            thumpState[channel] = (1.0f - thumpCoeff) * input + thumpCoeff * thumpState[channel];
            input += thump * 0.65f * thumpState[channel];
            // Punish is an intentional extra input-stage push, not a post-output volume boost.
            input *= (1.0f + 2.5f * punish);
            auto& modes = characterState[channel];
            float wet = 0.0f;
            for (size_t modeIndex = 0; modeIndex < characterWeights.size(); ++modeIndex)
            {
                const float weight = characterWeights[modeIndex];
                if (weight > 0.00001f)
                    wet += weight * vovolt::VoltCore::shape(
                        input, static_cast<vovolt::Character>(modeIndex), drive, modes[modeIndex]);
            }
            // Existing Tone control retained, followed by a gentle DC blocker.
            float& lowpass = toneState[channel];
            lowpass = (1.0f - filterCoeff) * wet + filterCoeff * lowpass;
            const float hp = lowpass - dcPreviousInput[channel] + dcCoefficient * dcPreviousOutput[channel];
            dcPreviousInput[channel] = lowpass;
            dcPreviousOutput[channel] = hp;
            ptr[i] = hp;
        }
    }
    oversampler->processSamplesDown(block);

    for (int i = 0; i < count; ++i)
    {
        const float mix = mixSmoothed.getNextValue();
        const float output = outputSmoothed.getNextValue();
        const float bypass = bypassSmoothed.getNextValue();
        const float highCut = highCutSmoothed.getNextValue();
        const float steep = steepSmoothed.getNextValue();
        const float autoGain = autoGainSmoothed.getNextValue();
        const float highCutCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * highCut
                                             / static_cast<float>(getSampleRate()));
        float dryPowerSum = 0.0f;
        float wetPowerSum = 0.0f;
        // Apply post-saturation filtering before the Auto Gain detector.
        for (int c = 0; c < channels; ++c)
        {
            const auto channel = static_cast<size_t>(c);
            const float rawDry = dryCopy.getSample(c, i);
            float filteredWet = buffer.getSample(c, start + i);
            highCutState1[channel] = (1.0f - highCutCoeff) * filteredWet
                                   + highCutCoeff * highCutState1[channel];
            filteredWet = highCutState1[channel];
            highCutState2[channel] = (1.0f - highCutCoeff) * filteredWet
                                   + highCutCoeff * highCutState2[channel];
            filteredWet += steep * (highCutState2[channel] - filteredWet);
            buffer.setSample(c, start + i, filteredWet);
            dryPowerSum += rawDry * rawDry;
            wetPowerSum += filteredWet * filteredWet;
        }
        const float inverseChannels = 1.0f / static_cast<float>(channels);
        dryPowerSum *= inverseChannels;
        wetPowerSum *= inverseChannels;
        const float dryCoeff = dryPowerSum > linkedDryRms ? autoGainAttack : autoGainRelease;
        const float wetCoeff = wetPowerSum > linkedWetRms ? autoGainAttack : autoGainRelease;
        linkedDryRms += dryCoeff * (dryPowerSum - linkedDryRms);
        linkedWetRms += wetCoeff * (wetPowerSum - linkedWetRms);
        const float targetGain = juce::jlimit(0.25f, 4.0f,
            std::sqrt((linkedDryRms + 1.0e-9f) / (linkedWetRms + 1.0e-9f)));
        linkedAutoGain += autoGainSmoothing * (targetGain - linkedAutoGain);
        const float linkedGain = 1.0f + autoGain * (linkedAutoGain - 1.0f);
        for (int c = 0; c < channels; ++c)
        {
            const auto channel = static_cast<size_t>(c);
            float wet = buffer.getSample(c, start + i);
            const float dry = dryCopy.getSample(c, i);
            auto& ring = dryDelay[channel];
            const float delayed = getLatencySamples() == 0 ? dry : ring[delayPosition];
            ring[delayPosition] = dry;

            // The same detector gain is applied to all channels to preserve stereo image.
            wet *= linkedGain;

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
