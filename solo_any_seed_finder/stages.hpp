#pragma once

#include "finder.hpp"

constexpr Finder::StageSettings stage1_settings{
    .check_twin_seeds = false,
    .seed_nb_to_next_stage = 1000000
};

Finder::EvalResult stage1_eval(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, Finder::SeedScorePair pair);