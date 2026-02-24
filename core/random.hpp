#pragma once

#include <array>
#include <cstdint>
#include <cmath>

class Random {
public:
    constexpr Random(uint32_t seed) {
        seed = std::max((uint32_t)341, seed);

        _a = seed;
        _b = seed;
        _c = seed;
    }

    constexpr uint32_t random() {
        _a = ((_a & 0xfffffffe) << 0xc) | (((_a << 0xd) ^ _a) >> 0x13);
        _b = ((_b & 0xffffff8) << 4) | (((_b * 4) ^ _b) >> 0x19);
        _c = ((_c & 0xfffffff0) << 0x11) | (((_c * 8) ^ _c) >> 0xb);

        return _a ^ _b ^ _c;
    }

    template<typename T>
    void shuffle(std::array<T, 256> &a) {
        for (int i = 255; i >= 0; i--) {
            int j;
            if (i < 1) {
                j = 0;
            } else {
                j = random() % (i + 1);
            }

            std::swap(a[i], a[j]);
        }
    }

    template<int Size>
    static constexpr void random_penalty(std::array<float, Size>& out, int32_t x, int32_t y,
        float source, float amplitude = 1.f, uint32_t seed = 1
    ) {
        if (source <= 0) return;

        uint32_t seed_ = ((uint32_t)y + seed) * 0x1ee3 + (uint32_t)x * 0x1eef + 0x3fbe2c;
        Random random(seed_);
        
        for (auto it = out.rbegin(); it != out.rend(); it++) {
            *it = source - (float)random.random() * std::exp2f(-32.f) * amplitude;
        }
    }

    /**
     * {
     *   type = "noise-function",
     *   name = "random_penalty_between",
     *   parameters = {"from", "to", "seed"},
     *   expression = "random_penalty{x = x, y = y, seed = seed, source = to, amplitude = to - from}"
     * },
     */
    template<int Size>
    static constexpr void random_penalty_between(std::array<float, Size>& out, int32_t x, int32_t y,
        float from, float to, uint32_t seed
    ) {
        random_penalty<Size>(out, x, y, to, to - from, seed);
    }

private:
    uint32_t _a;
    uint32_t _b;
    uint32_t _c;
};