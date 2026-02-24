#pragma once

#include "random.hpp"

class Noise {
public:
    Noise(uint32_t seed0, uint8_t seed1);

    float noise(float x, float y, float input_scale, float output_scale, float offset_x, float offset_y);
    float multioctave_noise(float x, float y, float persistence, float octaves,
        float input_scale, float output_scale, float offset_x, float offset_y);
    float persistence_multioctave_noise(float x, float y, float persistence, float octaves,
        float input_scale, float output_scale, float offset_x, float offset_y);

private:
    std::pair<float, float> _gradient(uint8_t x, uint8_t y);

    uint8_t _p1;
    std::array<uint8_t, 256> _p2;
    std::array<uint8_t, 256> _p3;
    std::array<std::pair<float, float>, 256> _grad;
};