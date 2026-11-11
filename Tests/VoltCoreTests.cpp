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

void testSoftUnityGainAndCompression()
{
    using namespace vovolt;
    for (float drive : {0.25f, 0.5f, 0.75f, 1.0f})
    {
        const float smallInput = 0.001f;
        const float smallOutput = measure(Character::soft, smallInput, drive);
        check(std::abs((smallOutput / smallInput) - 1.0f) < 0.001f,
              "Soft must not boost small-signal gain");

        const float medium = measure(Character::soft, 0.25f, drive);
        const float large = measure(Character::soft, 0.75f, drive);
        check(medium > 0.0f && large > medium,
              "Soft transfer must remain monotonic for positive input");
        check(large < 0.75f,
              "Soft must progressively round larger peaks without positive gain");
    }

    const float atLowDrive = measure(Character::soft, 0.45f, 0.25f);
    const float atHighDrive = measure(Character::soft, 0.45f, 0.75f);
    check(atHighDrive < atLowDrive,
          "Increasing Soft Drive must increase peak compression");
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

void testHardOddHarmonicOrder()
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
    // Decapitator P (pentode) reference: odd harmonics dominate and decay.
    const double h2 = harmonic(Character::hard, 0.9f, 2);
    const double h3 = harmonic(Character::hard, 0.9f, 3);
    const double h5 = harmonic(Character::hard, 0.9f, 5);
    const double h7 = harmonic(Character::hard, 0.9f, 7);
    check(h2 < 0.001, "Hard must stay odd-symmetric (no even harmonics)");
    check(h3 > 0.05 && h3 > h5 && h5 > h7 && h7 > 0.01,
          "Hard odd harmonics must dominate and decay (H3 > H5 > H7)");
}

float measureRms(vovolt::Character mode, float amp, float drive, float fs = 192000.0f)
{
    vovolt::ChannelState state;
    state.prepare(fs);
    constexpr int count = 19200;
    constexpr float pi = 3.14159265358979323846f;
    double sum = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const float x = amp * std::sin(2.0f * pi * 1000.0f * static_cast<float>(i) / fs);
        const float y = vovolt::VoltCore::shape(x, mode, drive, state);
        sum += static_cast<double>(y) * y;
    }
    return static_cast<float>(std::sqrt(sum / count));
}

void testHardLevelRelationship()
{
    using namespace vovolt;
    // Regression guard for the 2026-11-10 finding: the FL report "HARD much
    // quieter than SOFT" was caused by the OLD Soft curve (pre-calibration,
    // +17 dB small-signal boost), NOT by Hard being weak. After the Soft
    // calibration, Hard is the louder mode at high Drive, as hard clipping
    // should be. No makeup gain belongs in Hard: adding +4.5..6 dB would
    // make Hard ~+11 dB louder than Soft. Measured 2026-11-10 (1 kHz sine):
    // drive 0.75, amp 0.45 -> Soft 0.2335, Hard 0.4598 (ratio 1.97).
    for (float drive : {0.75f, 1.0f})
    {
        const float soft = measureRms(Character::soft, 0.45f, drive);
        const float hard = measureRms(Character::hard, 0.45f, drive);
        check(hard > soft, "Hard must be the louder mode at high Drive");
        check(hard < 3.0f * soft, "Hard level must stay within sane bounds");
    }
}

void testTubeEvenHarmonicOrder()
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
        // Warm the envelope up first, then analyze a steady-state window.
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
    // Decapitator T (triode) reference: even harmonics dominate, 2nd strongest.
    for (float drive : {0.25f, 0.5f, 0.75f, 0.9f, 1.0f})
    {
        const double h2 = harmonic(Character::tube, drive, 2);
        const double h3 = harmonic(Character::tube, drive, 3);
        check(h2 > h3, "Tube 2nd harmonic must dominate the 3rd (triode)");
    }
    check(harmonic(Character::tube, 0.5f, 2) > 0.01, "Tube 2nd harmonic missing at moderate Drive");
    check(harmonic(Character::tube, 0.75f, 4) > 0.001, "Tube 4th harmonic missing");
}

void testTubeLevelRelationship()
{
    using namespace vovolt;
    // Regression guard for the 2026-11-11 finding: the old biased-tanh Tube
    // was +10.9 dB louder than Soft at Drive 0.75 (RMS 0.82 vs 0.23) and its
    // output exceeded full scale (RMS 1.10 at Drive 1.0). The Koren triode
    // stays level-matched with Soft. Measured 2026-11-11 (1 kHz sine):
    // drive 0.75, amp 0.45 -> Soft 0.2335, Tube 0.2358 (ratio 1.01);
    // drive 1.0 -> Soft 0.1790, Tube 0.2676 (ratio 1.50).
    for (float drive : {0.75f, 1.0f})
    {
        const float soft = measureRms(Character::soft, 0.45f, drive);
        const float tube = measureRms(Character::tube, 0.45f, drive);
        check(tube > 0.5f * soft && tube < 2.0f * soft,
              "Tube level must stay within 0.5x..2.0x of Soft");
    }
}

void testTubeSmallSignalGain()
{
    using namespace vovolt;
    // The old Tube boosted quiet material (2.62x at Drive 0.5); the Koren
    // triode keeps small-signal gain near unity. Measured 2026-11-11:
    // 0.85 at Drive 0.5, 1.12 at Drive 1.0.
    for (float drive : {0.5f, 1.0f})
    {
        const float gain = measure(Character::tube, 0.001f, drive) / 0.001f;
        check(gain > 0.7f && gain < 1.2f, "Tube small-signal gain must stay near unity");
    }
}

void testTubeThdCurve()
{
    using namespace vovolt;
    // Culture Vulture anchors (published specs / Sound on Sound review):
    // THD 0.2% at the cleanest setting (bias 0.25-0.3 mA), distortion up to
    // ~99.9% overdriven. Our own triode must start subtle and grow smoothly.
    // Measured 2026-11-11 (1 kHz sine, amp 0.45, dynamic envelope):
    // Drive 0.05 -> 0.34%, 0.10 -> 0.86%, 0.25 -> 3.57%, 0.50 -> 12.35%,
    // 0.75 -> 25.38%, 1.00 -> 39.90%.
    constexpr float fs = 192000.0f;
    constexpr float pi = 3.14159265358979323846f;
    constexpr int count = 19200; // exactly 100 cycles of a 1-kHz sine
    auto harmonic = [](float drive, int harmonicNumber)
    {
        ChannelState state;
        state.prepare(fs);
        constexpr float frequency = 1000.0f;
        for (int i = 0; i < count; ++i) // warm the envelope up first
            VoltCore::shape(0.45f * std::sin(2.0f * pi * frequency * static_cast<float>(i) / fs),
                            Character::tube, drive, state);
        double re = 0.0, im = 0.0;
        for (int i = 0; i < count; ++i)
        {
            const float phase = 2.0f * pi * frequency * static_cast<float>(i) / fs;
            const float y = VoltCore::shape(0.45f * std::sin(phase), Character::tube, drive, state);
            re += y * std::cos(static_cast<double>(harmonicNumber) * phase);
            im += y * std::sin(static_cast<double>(harmonicNumber) * phase);
        }
        return 2.0 * std::sqrt(re * re + im * im) / static_cast<double>(count);
    };
    auto thd = [&](float drive)
    {
        const double h1 = harmonic(drive, 1);
        double sum = 0.0;
        for (int h = 2; h <= 6; ++h)
        {
            const double m = harmonic(drive, h);
            sum += m * m;
        }
        return 100.0 * std::sqrt(sum) / h1;
    };
    check(thd(0.05f) < 1.0, "Tube must start subtle (THD < 1% at Drive 0.05)");
    check(thd(0.10f) < 2.0, "Tube THD must stay subtle at Drive 0.10");
    check(thd(1.0f) > 30.0, "Tube must reach heavy distortion at Drive 1.0");
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
        testSoftUnityGainAndCompression();
        testHardKnee();
        testTubeAsymmetry();
        testTapeStateAndRate();
        testSegmentationIndependence();
        testHarmonicContent();
        testHardOddHarmonicOrder();
        testHardLevelRelationship();
        testTubeEvenHarmonicOrder();
        testTubeLevelRelationship();
        testTubeSmallSignalGain();
        testTubeThdCurve();
        testInvalidInputRecovery();
        std::cout << "VoVolt DSP core v0.5-dev: 16 test groups passed\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "VoVolt DSP test failed: " << e.what() << '\n';
        return 1;
    }
}
