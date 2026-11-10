#include "VoltCore.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
void check(bool condition, const std::string& label)
{
    if (!condition) throw std::runtime_error(label);
}

bool near(float a, float b, float tolerance = 1.0e-5f)
{
    return std::abs(a - b) <= tolerance;
}

float measure(const vovolt::Character mode, float x, float drive, float sampleRate = 192000.0f)
{
    vovolt::ChannelState state;
    state.prepare(sampleRate);
    return vovolt::VoltCore::shape(x, mode, drive, state);
}

void testDriveZero()
{
    using namespace vovolt;
    for (int mode = 0; mode < 4; ++mode)
        for (int i = -100; i <= 100; ++i)
        {
            ChannelState state;
            state.prepare(176400.0f);
            const float x = static_cast<float>(i) / 50.0f;
            check(VoltCore::shape(x, static_cast<Character>(mode), 0.0f, state) == x,
                  "Drive=0 should be mathematically transparent");
        }
}

void testFiniteRanges()
{
    using namespace vovolt;
    for (const float fs : {44100.0f, 48000.0f, 96000.0f, 176400.0f, 192000.0f, 384000.0f})
        for (int mode = 0; mode < 4; ++mode)
            for (float drive : {0.0f, 0.01f, 0.25f, 0.5f, 0.75f, 1.0f})
            {
                ChannelState state;
                state.prepare(fs);
                for (int i = -1600; i <= 1600; ++i)
                {
                    const float x = static_cast<float>(i) / 400.0f;
                    const float y = VoltCore::shape(x, static_cast<Character>(mode), drive, state);
                    check(std::isfinite(y), "Non-finite DSP output");
                    check(std::abs(y) < 15.0f, "Uncontrolled DSP gain");
                }
            }
}

void testOddSymmetricModes()
{
    using namespace vovolt;
    for (auto mode : {Character::soft, Character::hard})
        for (float x : {0.05f, 0.25f, 0.7f, 1.4f})
            check(near(measure(mode, x, 0.7f), -measure(mode, -x, 0.7f)),
                  "Soft and Hard must be odd-symmetric");
}

void testHardKnee()
{
    using namespace vovolt;
    const float d = 0.8f;
    const float gain = 1.0f + 7.0f * d;
    const float threshold = 1.0f - 0.62f * d;
    const float knee = 0.16f - 0.10f * d;
    for (float boundary : {(threshold - knee * 0.5f) / gain,
                           (threshold + knee * 0.5f) / gain})
    {
        const float h = 0.00005f;
        const float l = (measure(Character::hard, boundary, d) -
                         measure(Character::hard, boundary - h, d)) / h;
        const float r = (measure(Character::hard, boundary + h, d) -
                         measure(Character::hard, boundary, d)) / h;
        check(std::abs(l - r) < 0.035f, "Hard knee slope discontinuity");
    }
    check(near(measure(Character::hard, 2.0f, 1.0f), measure(Character::hard, 3.0f, 1.0f)),
          "Hard mode must clip at maximum drive");
}

void testTubeAsymmetry()
{
    using namespace vovolt;
    const float positive = measure(Character::tube, 0.35f, 0.85f);
    const float negative = measure(Character::tube, -0.35f, 0.85f);
    check(std::abs(positive + negative) > 0.002f, "Tube asymmetry missing");
    check(near(measure(Character::tube, 0.0f, 0.85f), 0.0f), "Tube zero-input bias");
}

void testTapeStateAndRate()
{
    using namespace vovolt;
    for (float fs : {176400.0f, 192000.0f, 384000.0f})
    {
        ChannelState fresh, driven;
        fresh.prepare(fs);
        driven.prepare(fs);
        for (int i = 0; i < static_cast<int>(fs * 0.015f); ++i)
            VoltCore::shape(0.75f, Character::tape, 0.75f, driven);
        const float yFresh = VoltCore::shape(0.1f, Character::tape, 0.75f, fresh);
        const float yMemory = VoltCore::shape(0.1f, Character::tape, 0.75f, driven);
        check(std::abs(yFresh - yMemory) > 0.004f, "Tape memory not affecting output");
        driven.reset();
        check(near(VoltCore::shape(0.1f, Character::tape, 0.75f, driven), yFresh),
              "Tape reset should restore initial state");
    }
}

void testInvalidInputRecovery()
{
    using namespace vovolt;
    for (int mode = 0; mode < 4; ++mode)
    {
        ChannelState state;
        state.prepare(192000.0f);
        const auto character = static_cast<Character>(mode);
        VoltCore::shape(0.5f, character, 0.9f, state);
        check(VoltCore::shape(std::numeric_limits<float>::quiet_NaN(), character, 0.9f, state) == 0.0f,
              "Non-finite input must be contained");
        check(std::isfinite(VoltCore::shape(0.25f, character, 0.9f, state)),
              "DSP must recover after non-finite input");
    }
}

void testHarmonicContent()
{
    using namespace vovolt;
    constexpr float fs = 192000.0f;
    constexpr float pi = 3.14159265358979323846f;
    constexpr int count = 19200; // exactly 100 cycles of a 1-kHz sine
    auto harmonic = [](Character mode, float drive, int harmonicNumber)
    {
        ChannelState state;
        state.prepare(fs);
        constexpr float frequency = 1000.0f;
        // Warm the state up first, then analyze a steady-state periodic window.
        for (int i = 0; i < count; ++i)
            VoltCore::shape(0.45f * std::sin(2.0f * pi * frequency * static_cast<float>(i) / fs),
                            mode, drive, state);
        double re = 0.0, im = 0.0;
        for (int i = 0; i < count; ++i)
        {
            const float phase = 2.0f * pi * frequency * static_cast<float>(i) / fs;
            const float y = VoltCore::shape(0.45f * std::sin(phase), mode, drive, state);
            re += y * std::cos(static_cast<double>(harmonicNumber) * phase);
            im += y * std::sin(static_cast<double>(harmonicNumber) * phase);
        }
        return 2.0 * std::sqrt(re * re + im * im) / static_cast<double>(count);
    };
    check(harmonic(Character::soft, 0.0f, 3) < 0.0001, "Zero-drive soft invented harmonics");
    check(harmonic(Character::soft, 0.9f, 3) > 0.005, "Soft missing third harmonic");
    check(harmonic(Character::hard, 0.9f, 3) > 0.005, "Hard missing third harmonic");
    check(harmonic(Character::tube, 0.9f, 2) > 0.003, "Tube missing second harmonic");
}

void testSegmentationIndependence()
{
    using namespace vovolt;
    for (int mode = 0; mode < 4; ++mode)
    {
        ChannelState whole, chunked;
        whole.prepare(192000.0f);
        chunked.prepare(192000.0f);
        std::vector<float> a, b;
        for (int i = 0; i < 1200; ++i)
        {
            const float sample = std::sin(0.035f * static_cast<float>(i)) * 0.65f;
            a.push_back(VoltCore::shape(sample, static_cast<Character>(mode), 0.65f, whole));
        }
        // Equivalent stream processed as irregular 127/256/817 sample chunks.
        const int lengths[] = {127, 256, 817};
        int position = 0;
        for (int length : lengths)
            for (int i = 0; i < length; ++i)
            {
                const float sample = std::sin(0.035f * static_cast<float>(position)) * 0.65f;
                b.push_back(VoltCore::shape(sample, static_cast<Character>(mode), 0.65f, chunked));
                ++position;
            }
        check(a.size() == b.size(), "Incorrect test chunk count");
        for (size_t i = 0; i < a.size(); ++i)
            check(a[i] == b[i], "DSP changed with processing segmentation");
    }
}
}

int main()
{
    try
    {
        testDriveZero();
        testFiniteRanges();
        testOddSymmetricModes();
        testHardKnee();
        testTubeAsymmetry();
        testTapeStateAndRate();
        testSegmentationIndependence();
        testHarmonicContent();
        testInvalidInputRecovery();
        std::cout << "VoVolt DSP v0.4: 9 test groups passed\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "VoVolt DSP test failed: " << e.what() << '\n';
        return 1;
    }
}
