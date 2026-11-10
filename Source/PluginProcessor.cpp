#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

VoVoltAudioProcessor::VoVoltAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "VoVoltParameters", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout VoVoltAudioProcessor::createLayout()
{
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
    // Two oversampling stages = 4x; polyphase IIR with host latency reporting.
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(channels), 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing(static_cast<size_t>(juce::jmax(1, samplesPerBlock)));
    oversampler->reset();
    latencySamples = oversampler->getLatencyInSamples();
    setLatencySamples(static_cast<int>(std::round(latencySamples)));

    // Dry path delay must match the reported integer host latency.
    delaySize = juce::jmax(1, getLatencySamples());
    dryDelay.assign(static_cast<size_t>(channels), std::vector<float>(static_cast<size_t>(delaySize), 0.0f));
    delayPosition = 0;
    tapeState.assign(static_cast<size_t>(channels), 0.0f);
    toneState.assign(static_cast<size_t>(channels), 0.0f);

    driveSmoothed.reset(sampleRate * 4.0, 0.025);
    toneSmoothed.reset(sampleRate * 4.0, 0.025);
    mixSmoothed.reset(sampleRate, 0.025);
    outputSmoothed.reset(sampleRate, 0.025);
    bypassSmoothed.reset(sampleRate, 0.025);
    driveSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("drive")->load());
    mixSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("mix")->load());
    toneSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("tone")->load());
    outputSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(parameters.getRawParameterValue("output")->load()));
    bypassSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("bypass")->load());
}

void VoVoltAudioProcessor::releaseResources()
{
    oversampler.reset();
    dryDelay.clear();
    dryCopy.setSize(0, 0);
    tapeState.clear();
    toneState.clear();
}

void VoVoltAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin(buffer.getNumChannels(), getTotalNumInputChannels());
    for (int c = channels; c < buffer.getNumChannels(); ++c) buffer.clear(c, 0, n);
    if (n <= 0 || channels <= 0 || !oversampler) return;
    if (n > preparedBlockSize) return; // Fail safe, preserve original audio.
    for (int c = 0; c < channels; ++c)
        dryCopy.copyFrom(c, 0, buffer, c, 0, n);

    driveSmoothed.setTargetValue(parameters.getRawParameterValue("drive")->load());
    mixSmoothed.setTargetValue(parameters.getRawParameterValue("mix")->load());
    toneSmoothed.setTargetValue(parameters.getRawParameterValue("tone")->load());
    outputSmoothed.setTargetValue(juce::Decibels::decibelsToGain(parameters.getRawParameterValue("output")->load()));
    bypassSmoothed.setTargetValue(parameters.getRawParameterValue("bypass")->load());
    const auto mode = static_cast<vovolt::Character>(juce::jlimit(0, 3,
        static_cast<int>(parameters.getRawParameterValue("character")->load())));


    float inPeak = 0.0f;
    for (int c = 0; c < channels; ++c)
        inPeak = juce::jmax(inPeak, buffer.getMagnitude(c, 0, n));

    // Input copied to preallocated dryCopy before oversampling.
    juce::dsp::AudioBlock<float> block(buffer);
    auto up = oversampler->processSamplesUp(block);
    const size_t upN = up.getNumSamples();
    const float oversampleFactor = static_cast<float>(upN) / static_cast<float>(n);
    const float baseRate = static_cast<float>(getSampleRate());
    const float fs = juce::jmax(1.0f, baseRate * oversampleFactor);
    // Tone controls the one-pole lowpass cutoff of the saturated branch, 3kHz-18kHz.
    for (size_t i = 0; i < upN; ++i)
    {
        const float drive = driveSmoothed.getNextValue();
        const float tone = toneSmoothed.getNextValue();
        const float cutoff = 3000.0f + 15000.0f * tone;
        const float a = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / fs);
        for (int c = 0; c < channels; ++c)
        {
            auto* ptr = up.getChannelPointer(static_cast<size_t>(c));
            const float shaped = vovolt::VoltCore::shape(ptr[i], mode, drive, tapeState[static_cast<size_t>(c)]);
            float& state = toneState[static_cast<size_t>(c)];
            state = (1.0f - a) * shaped + a * state;
            ptr[i] = state;
        }
    }
    oversampler->processSamplesDown(block);

    // Delay the dry branch to align with the oversampled wet branch.
    float outPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float mix = mixSmoothed.getNextValue();
        const float output = outputSmoothed.getNextValue();
        const float bypass = bypassSmoothed.getNextValue();
        for (int c = 0; c < channels; ++c)
        {
            const float wet = buffer.getSample(c, i);
            const float dry = dryCopy.getSample(c, i);
            auto& ring = dryDelay[static_cast<size_t>(c)];
            const float delayed = getLatencySamples() == 0 ? dry : ring[delayPosition];
            ring[delayPosition] = dry;
            const float mixed = delayed + (wet - delayed) * mix;
            const float value = mixed * output * (1.0f - bypass) + delayed * bypass;
            buffer.setSample(c, i, value);
            outPeak = juce::jmax(outPeak, std::abs(value));
        }
        delayPosition = (delayPosition + 1) % static_cast<size_t>(delaySize);
    }
    inputLevel.store(inPeak, std::memory_order_relaxed);
    outputLevel.store(outPeak, std::memory_order_relaxed);
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
