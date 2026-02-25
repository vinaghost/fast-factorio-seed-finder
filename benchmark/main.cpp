#include "noise.hpp"
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
    for (ResourceType t = IRON; t < NB_RESOURCE_TYPE; ++t) {
        settings.frequencies[t] = 6.f;
        settings.sizes[t] = 6.f;
        settings.richness[t] = 6.f;
    }
    auto start = std::chrono::high_resolution_clock::now();
    NoisePrecompute* precompute = new NoisePrecompute(settings);
    NoiseCache* cache = new NoiseCache;
    auto end = std::chrono::high_resolution_clock::now();
    std::println("cache/precompute init {}", std::chrono::duration_cast<std::chrono::milliseconds>(end - start));

    float sum = 0.f;

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; i++) {
        auto patches = regular_patches(*precompute, *cache, i);
        for (const auto& patch : patches[1]) {
            sum += patch.radius;
        }
    }
    end = std::chrono::high_resolution_clock::now();

    std::println("{} {}", sum, std::chrono::duration_cast<std::chrono::milliseconds>(end - start));

    delete precompute;
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
