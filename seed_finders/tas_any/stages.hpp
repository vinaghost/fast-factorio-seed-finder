#pragma once

#include "finder.hpp"

// Has to be copy/move assignable/constructible.
struct SeedCache {
    float stage1_area;
};

constexpr Finder<SeedCache>::StageSettings stage1_settings{
    // Here the first step only looks that regular patches, so we don't need to check twin seeds, water settings or elevation types
    .check_twin_seeds = false,
    .check_water_settings = false,
    .check_elevation_types = false,

    // There are 2 ways of eliminating seeds, either we rank all seeds with their score and keep only the top seed_nb_to_next_stage,
    // or we can use a pass-or-fail method (see stages.cpp for the second option)
    // For this stage I will use the top n option. Only the top 10 000 seeds will make it to the 2nd stage.
    .seed_nb_to_next_stage = 10'000
};

Finder<SeedCache>::EvalResult stage1_eval(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, uint32_t seed, SeedCache*);

constexpr Finder<SeedCache>::StageSettings stage2_settings{
    // This stage looks that starter patches so we add in twin seeds and water settings. Elevation types is not useful yet.
    .check_twin_seeds = true,
    .check_water_settings = true,
    .check_elevation_types = false,

    .seed_nb_to_next_stage = 500'000'000     // Here we use the pass-or-fail method so I set this a very high number
};

// Has to be copy/move assignable/constructible.
struct Stage2Cache {
    std::array<float, 1'000'000> big_array;
};

Finder<SeedCache>::EvalResult stage2_eval(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, uint32_t seed, SeedCache*, Stage2Cache &stage_cache);

constexpr Finder<SeedCache>::StageSettings stage3_settings{
    // Finally we add elevation to check for water.
    .check_twin_seeds = true,
    .check_water_settings = true,
    .check_elevation_types = true,

    .seed_nb_to_next_stage = 1'000    // For this stage I will use the top n option. Only the top 1000 seeds will make it to the output file.
};

Finder<SeedCache>::EvalResult stage3_eval(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, uint32_t seed, SeedCache*);