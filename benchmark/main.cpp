#include "noise.hpp"
#include "spot_noise.hpp"
#include <chrono>
#include <print>
#include <iostream>
#include <fstream>

void benchmark(int n) {
    Noise noise(250, 22);
    float sum = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; i++) {
        for (float x = 0; x < 1000.f; x++) {
            for (float y = 0; y < 1000.f; y++) {
                sum += noise.noise(x, y, 0.01, 10, 300, 200);
            }
        }
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::println("{} {}", sum, std::chrono::duration_cast<std::chrono::milliseconds>(end - start));
}

void benchmark_2(int n) {
    MapGenSettings settings;
    for (PatchType type = IRON; type < NB_PATCH_TYPE; type++) {
        settings.frequencies[type] = 6.f;
        settings.sizes[type] = 6.f;
        settings.frequencies[type] = 6.f;
    }

    auto start = std::chrono::high_resolution_clock::now();
    RegularSpotNoiseCache* cache = new RegularSpotNoiseCache(settings);
    auto end = std::chrono::high_resolution_clock::now();
    std::println("cache init {}", std::chrono::duration_cast<std::chrono::milliseconds>(end - start));

    float sum = 0.f;

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; i++) {
        auto patches = regular_patches(settings, *cache, i);
        for (const auto& patch : patches[1]) {
            sum += patch.radius;
        }
    }
    end = std::chrono::high_resolution_clock::now();

    std::println("{} {}", sum, std::chrono::duration_cast<std::chrono::milliseconds>(end - start));

    delete cache;
}

int main() {
    if (false) {
        benchmark_2(100000);
    } else {
        while (true) benchmark_2(1000000);
    }

    // 7.8s for 1000000
    // 6.7s => chunk_size = ceil(suggested_distance)
    // ~7-11s => penalties, quality and density giant lookup table
    // ~6.7-9s => penalties and density giant lookup table
    // 6.4s => only penalties lookup table
    // 6.1s => penalties lookup table and precomputed base density/quantity
    
    return 0;
}
