#pragma once
#include <algorithm>
#include <cmath>

namespace vovolt
{
enum class Character : int { soft = 0, tube = 1, tape = 2, hard = 3 };

// DSP math is JUCE-independent and safe to call on the audio thread.
struct VoltCore
{
    static float shape(float x, Character mode, float drive, float& tapeMemory) noexcept
    {
        const float gain = 1.0f + 15.0f * std::clamp(drive, 0.0f, 1.0f);
        const float z = x * gain;
        switch (mode)
        {
            case Character::soft:
                return std::tanh(z) / std::tanh(gain);
            case Character::tube:
            {
                // Asymmetric transfer; subtract zero-input bias to avoid a static DC offset.
                constexpr float bias = 0.23f;
                const float dc = std::tanh(bias);
                return (std::tanh(z + bias) - dc) / (1.0f + dc);
            }
            case Character::tape:
            {
                // Stateful pre-emphasis/memory; an original approximation, not hardware emulation.
                const float previous = tapeMemory;
                tapeMemory = 0.94f * tapeMemory + 0.06f * x;
                const float memorySignal = x + 0.18f * (x - previous);
                const float v = memorySignal * gain;
                return (2.0f / 3.14159265358979323846f) * std::atan(v);
            }
            case Character::hard:
                return std::clamp(z * 0.65f, -1.0f, 1.0f);
        }
        return x;
    }
};
}
