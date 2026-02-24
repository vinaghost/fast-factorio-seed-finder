#include "noise.hpp"
#include "gradients.hpp"
#include <numeric>
#include <cmath>

Noise::Noise(uint32_t seed0, uint8_t seed1) {
    Random random(seed0);

    std::array<uint8_t, 256> seeder;
    std::iota(seeder.begin(), seeder.end(), 0);
    std::iota(_p2.begin(), _p2.end(), 0);
    std::iota(_p3.begin(), _p3.end(), 0);
    _grad = default_gradients();

    random.shuffle(seeder);
    random.shuffle(_p2);
    random.shuffle(_p3);
    random.shuffle(_grad);

    _p1 = seeder[seed1];
}

std::pair<float, float> Noise::_gradient(uint8_t x, uint8_t y) {
    uint8_t y_permutation = _p1 ^ _p2[y];
    uint8_t xy_permutation = y_permutation ^ _p3[x];
    return _grad[xy_permutation];
}

float Noise::noise(float x, float y, float input_scale, float output_scale, float offset_x, float offset_y) {
    float x_scaled = (x + offset_x) * input_scale;
    float y_scaled = (y + offset_y) * input_scale;

    float x_floor = std::floorf(x_scaled);
    float y_floor = std::floorf(y_scaled);
    float x_ceil = x_floor + 1.f;
    float y_ceil = y_floor + 1.f;
    float x_frac = x_scaled - x_floor;
    float y_frac = y_scaled - y_floor;

    std::array<std::pair<float, float>, 4> points{
        std::make_pair( x_floor, y_floor ),
        std::make_pair( x_ceil, y_floor ),
        std::make_pair( x_floor, y_ceil ),
        std::make_pair( x_ceil, y_ceil )
    };

    std::array<std::pair<float, float>, 4> dcba{
        std::make_pair( 0.f, 0.f ),
        std::make_pair( 1.f, 0.f ),
        std::make_pair( 0.f, 1.f ),
        std::make_pair( 1.f, 1.f )
    };

    float sum = 0.f;

    for (int i = 0; i < 4; i++) {
        auto gradient = _gradient(points[i].first, points[i].second);

        float x_frac_offset = x_frac - dcba[i].first;
        float y_frac_offset = y_frac - dcba[i].second;

        float d2 = 1.f - std::min(1.f, x_frac_offset*x_frac_offset + y_frac_offset*y_frac_offset);

        sum += (
            (x_frac_offset * gradient.first) +
            (y_frac_offset * gradient.second)
        ) * d2*d2*d2;
    }

    return sum * output_scale;
}

// float Noise::multioctave_noise(float x, float y, float persistence, float octaves,
//     float input_scale, float output_scale, float offset_x, float offset_y
// ) {
//     return 0;
// }

float Noise::persistence_multioctave_noise(float x, float y, float persistence, float octaves,
    float input_scale, float output_scale, float offset_x, float offset_y
) {
    input_scale *= 0.5f;
    output_scale *= std::powf(2.f, octaves);

    float sum = 0.f;
    for (int i = 1; i < octaves; i++) {
        sum += noise(x, y, input_scale, 1.f, offset_x, offset_y);
        sum *= persistence;

        input_scale *= 0.5f;
    }

    sum += noise(x, y, input_scale, 1.f, offset_x, offset_y);

    return sum * output_scale;
}
