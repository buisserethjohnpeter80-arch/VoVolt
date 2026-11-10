#pragma once

#include <algorithm>
#include <cmath>

namespace vovolt
{
enum class Character : int { soft = 0, tube = 1, tape = 2, hard = 3 };

// Independent state for EACH channel and EACH character.
// No dynamic allocations or sample-rate calculations occur in shape().
struct ChannelState
{
    float tubeEnvelope = 0.0f;
    float tapeLowpass = 0.0f;
    float tapeMagnetisation = 0.0f;
    float tubeAttackCoeff = 0.0f;
    float tubeReleaseCoeff = 0.0f;
    float tapeLowpassCoeff = 0.0f;
    float tapeMemoryCoeff = 0.0f;

    void prepare(float oversampledRate) noexcept
    {
        const float rate = std::max(8000.0f, oversampledRate);
        tubeAttackCoeff = 1.0f - std::exp(-1.0f / (0.004f * rate));
        tubeReleaseCoeff = 1.0f - std::exp(-1.0f / (0.060f * rate));
        tapeLowpassCoeff = 1.0f - std::exp(-2.0f * 3.14159265358979323846f * 5500.0f / rate);
        tapeMemoryCoeff = 1.0f - std::exp(-1.0f / (0.00045f * rate));
        reset();
    }

    void reset() noexcept
    {
        tubeEnvelope = 0.0f;
        tapeLowpass = 0.0f;
        tapeMagnetisation = 0.0f;
    }
};

// Original VoVolt character models. Tube and Tape are *inspired by* analog
// behaviour; neither is a component-level or hardware-measured emulation.
struct VoltCore
{
    static float shape(float x, Character mode, float drive, ChannelState& state) noexcept
    {
        if (!std::isfinite(x) || !std::isfinite(drive))
        {
            state.reset();
            return 0.0f;
        }
        const float d = std::clamp(drive, 0.0f, 1.0f);
        // No clipping, colour or nonlinear gain at zero drive.
        // The outer Tone filter may still colour the wet signal.
        if (d == 0.0f)
            return x;

        switch (mode)
        {
            case Character::soft:
            {
                // Unity small-signal gain: tanh(g*x)/g has a derivative of 1 at x=0.
                // The previous /tanh(g) normalization boosted quiet material as Drive rose,
                // which made level-matched A/B comparisons misleading. This curve keeps
                // low-level gain near unity while progressively rounding larger peaks.
                const float gain = 1.0f + 3.5f * d;
                const float saturated = std::tanh(gain * x) / gain;
                return x + d * (saturated - x);
            }
            case Character::hard:
            {
                // Smooth quadratic knee continuous in value AND slope.
                // Drive increases input gain and lowers clipping threshold.
                const float gain = 1.0f + 7.0f * d;
                const float threshold = 1.0f - 0.62f * d;
                const float knee = 0.16f - 0.10f * d;
                const float amplitude = std::abs(x * gain);
                const float kneeStart = threshold - 0.5f * knee;
                const float kneeEnd = threshold + 0.5f * knee;
                float clipped = 0.0f;
                if (amplitude <= kneeStart)
                    clipped = amplitude;
                else if (amplitude >= kneeEnd)
                    clipped = threshold;
                else
                {
                    const float distance = amplitude - kneeStart;
                    clipped = amplitude - (distance * distance) / (2.0f * knee);
                }
                // Preserves exact identity at d=0 and fades in the hard character.
                const float signedClip = std::copysign(clipped, x);
                return x + d * (signedClip - x);
            }
            case Character::tube:
            {
                // Signal-dependent bias, asymmetric transfer and envelope memory.
                const float level = std::abs(x);
                const float coefficient = level > state.tubeEnvelope
                    ? state.tubeAttackCoeff : state.tubeReleaseCoeff;
                state.tubeEnvelope += coefficient * (level - state.tubeEnvelope);
                const float bias = d * (0.14f + 0.12f * std::min(state.tubeEnvelope, 1.5f));
                const float gain = 1.0f + 6.0f * d;
                const float offset = std::tanh(bias);
                const float scale = std::max(0.1f, std::tanh(gain + bias) - offset);
                const float saturated = (std::tanh(gain * x + bias) - offset) / scale;
                return x + d * (saturated - x);
            }
            case Character::tape:
            {
                // Stateful magnetic-memory approximation, with pre-emphasis.
                // These are perceptual model terms, NOT Jiles-Atherton physics.
                state.tapeLowpass += state.tapeLowpassCoeff * (x - state.tapeLowpass);
                const float preemphasised = x + 0.16f * d * (x - state.tapeLowpass);
                const float excitation = (1.0f + 7.0f * d) * preemphasised;
                const float field = excitation + 0.70f * d * state.tapeMagnetisation;
                const float target = std::tanh(field);
                state.tapeMagnetisation += state.tapeMemoryCoeff * (target - state.tapeMagnetisation);
                const float saturated = 0.82f * target + 0.18f * state.tapeMagnetisation;
                return x + d * (saturated - x);
            }
        }
        return x;
    }
};
}
