#include "VoltCore.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    using namespace vovolt;
    for (int mode = 0; mode < 4; ++mode)
    {
        float state = 0.0f;
        for (int i = -2000; i <= 2000; ++i)
        {
            const float x = i / 1000.0f;
            const float y = VoltCore::shape(x, static_cast<Character>(mode), 0.5f, state);
            assert(std::isfinite(y));
            assert(std::abs(y) < 4.0f);
        }
    }
    for (auto mode : {Character::soft, Character::tube, Character::hard})
    {
        float state = 0.0f;
        assert(std::abs(VoltCore::shape(0.0f, mode, 0.4f, state)) < 0.00001f);
    }
    std::cout << "VoVolt core DSP smoke tests passed\n";
}
