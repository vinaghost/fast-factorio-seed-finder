#pragma once

#include "utils.hpp"
#include <cstdint>
#include <array>

constexpr int c_max_candidate_count = 32;

static constexpr int32_t c_starter_region_size = 240;
static constexpr int32_t c_regular_region_size = 1024;

static constexpr int c_span = 6.f;
static constexpr int c_nb_starter_spot_to_generate = 32 * c_span + 3;
static constexpr int c_nb_regular_spot_to_generate = 22 * c_span + 1;

static constexpr float c_starter_suggested_distance = 32.f;
static constexpr float c_regular_suggested_distance = 45.254833995939045f;

static constexpr uint8_t c_starter_seed1 = 101;
static constexpr uint8_t c_regular_seed1 = 100;

static constexpr int c_starter_candidate_count = 32;
static constexpr std::array<int, NB_PATCH_TYPE> c_regular_candidate_counts{
    22, 22, 21, 21
};

static constexpr std::array<float, NB_PATCH_TYPE> c_base_densities{
    10, 8, 8, 4
};

static constexpr std::array<float, NB_PATCH_TYPE> c_regular_rq_factors{
    0.11f, 0.11f, 0.10f, 0.10f
};
static constexpr std::array<float, NB_PATCH_TYPE> c_starter_rq_factors{
    1.5f/7.f, 1.2f/7.f, 1.1f/7.f, 1.1f/7.f
};

static constexpr std::array<int, NB_PATCH_TYPE> c_offsets{
    0, 1, 2, 3
};
static constexpr float c_regular_patch_fade_in_distance = 300.f;
static constexpr float c_starting_resource_placement_radius = 120.f;
static constexpr float c_base_spots_per_km2 = 2.5f;
static constexpr float c_double_density_distance = 1300.f;
static constexpr float c_random_spot_size_minimum = 0.25f;
static constexpr float c_random_spot_size_maximum = 2.f;