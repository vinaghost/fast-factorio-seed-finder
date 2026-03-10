#pragma once

#include "finder.hpp"

struct SeedCache {
    Direction mixer_direction;
    BoxI16 mixer_copper_bounding_box;
    BoxI16 mixer_iron_bounding_box;
    BoxI16 mixer_coal_bounding_box;
    BoxI16 mall_iron_bounding_box;
    float iron_area;
};

constexpr Finder<SeedCache>::StageSettings stage1_settings{
    .check_twin_seeds = false,
    .check_water_settings = false,
    .check_elevation_types = false,
    .seed_nb_to_next_stage = 500'000'000
};

Finder<SeedCache>::EvalResult stage1_eval(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, uint32_t seed, SeedCache*);

constexpr Finder<SeedCache>::StageSettings stage2_settings{
    .check_twin_seeds = true,
    .check_water_settings = true,
    .check_elevation_types = false,
    .seed_nb_to_next_stage = 500'000'000
};

Finder<SeedCache>::EvalResult stage2_eval(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, uint32_t seed, SeedCache*);

constexpr Finder<SeedCache>::StageSettings stage3_settings{
    .check_twin_seeds = true,
    .check_water_settings = true,
    .check_elevation_types = true,
    .seed_nb_to_next_stage = 500'000'000
};

Finder<SeedCache>::EvalResult stage3_eval(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, uint32_t seed, SeedCache*);