#include "stages.hpp"

Finder::EvalResult stage1_eval(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, Finder::SeedScorePair pair) {
    // auto patches = regular_patches(precompute, cache, pair.first);
    // return patches[0].data[0].quantity;
    return { pair.first % 100 != 0, (float)pair.first};
}