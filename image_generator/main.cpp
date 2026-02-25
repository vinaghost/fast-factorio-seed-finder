#include "noise.hpp"
#include <chrono>
#include <print>
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        std::println("You must specify a seed.");
        return 1;
    }

    uint32_t seed0 = std::stoul(std::string(argv[1]));
    std::println("seed: {}", seed0);

    constexpr int IMG_SIZE = 896;

    MapGenSettings settings;
    for (ResourceType t = IRON; t < NB_RESOURCE_TYPE; ++t) {
        settings.frequencies[t] = 6.f;
        settings.sizes[t] = 6.f;
        settings.richness[t] = 6.f;
    }
    NoisePrecompute* precompute = new NoisePrecompute(settings);
    NoiseCache* cache = new NoiseCache;

    auto patches = regular_patches(*precompute, *cache, seed0);
    delete precompute;
    delete cache;

    // Output buffer: RGB bytes
    std::vector<unsigned char> img(IMG_SIZE * IMG_SIZE * 3, 255);

    const float shift = IMG_SIZE / 2.f ;
    for (int py = 0; py < IMG_SIZE; ++py) {
        for (int px = 0; px < IMG_SIZE; ++px) {
            // Map pixel -> world coordinates used by generate_candidates
            float wx = px - shift;
            float wy = py - shift;

            float best_val = -std::numeric_limits<float>::infinity();
            ResourceType best_type = NB_RESOURCE_TYPE;

            for (ResourceType type = IRON; type < NB_RESOURCE_TYPE; ++type) {
                float best_for_type = -std::numeric_limits<float>::infinity();
                for (auto it = patches[type].begin(); it != patches[type].end(); ++it) {
                    const Patch &p = *it;
                    float dx = wx - (float)p.x;
                    float dy = wy - (float)p.y;
                    float dist = std::sqrtf(dx*dx + dy*dy);

                    float slope = 0.f;
                    if (p.radius > 0.f) slope = 3.f * p.quantity / (M_PI * p.radius * p.radius * p.radius);

                    float val = (p.radius - dist) * slope;
                    if (val > best_for_type) best_for_type = val;
                }

                if (best_for_type > best_val) {
                    best_val = best_for_type;
                    best_type = type;
                }
            }

            size_t idx = (py * IMG_SIZE + px) * 3;
            if (best_val <= 0.f || best_type == NB_RESOURCE_TYPE) {
                // background white
                img[idx+0] = 255; img[idx+1] = 255; img[idx+2] = 255;
            } else {
                switch (best_type) {
                    case IRON:   img[idx+0]=0;   img[idx+1]=0;   img[idx+2]=255; break; // blue
                    case COPPER: img[idx+0]=255; img[idx+1]=0;   img[idx+2]=0;   break; // red
                    case COAL:   img[idx+0]=0;   img[idx+1]=0;   img[idx+2]=0;   break; // black
                    case STONE:  img[idx+0]=128; img[idx+1]=128; img[idx+2]=128; break; // gray
                    default:     img[idx+0]=255; img[idx+1]=255; img[idx+2]=255; break;
                }
            }
        }
    }

    std::ofstream ofs("/home/ness056/CodeProjets/factorio-reverse-engineering/build/patches.ppm", std::ios::binary);
    if (!ofs) {
        std::cerr << "Failed to open output file\n";
        return 1;
    }

    ofs << "P6\n" << IMG_SIZE << " " << IMG_SIZE << "\n255\n";
    ofs.write((char*)img.data(), img.size());
    ofs.close();

    std::cout << "Wrote patches.ppm\n";
    return 0;
}