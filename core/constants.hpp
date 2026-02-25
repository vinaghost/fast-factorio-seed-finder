#pragma once

#include "utils.hpp"
#include <cstdint>
#include <array>

static constexpr uint32_t SEED_MAX = 4294967295;

static constexpr std::array<int32_t, NB_PATCH_TYPE> REGION_SIZES{
    240, 1024
};
static constexpr int32_t MAX_REGION_SIZE = 1024;

static constexpr int32_t SPAN = 6.f;

static constexpr std::array<int32_t, NB_PATCH_TYPE> NB_CANDIDATES{
    32 * SPAN + 3, 22 * SPAN + 1
};
static constexpr int32_t MAX_NB_CANDIDATES = std::max(NB_CANDIDATES[STARTER], NB_CANDIDATES[REGULAR]);

static constexpr std::array<float, NB_PATCH_TYPE> SUGGESTED_DISTANCES{
    32.f, 45.254833995939045f
};

static constexpr std::array<int32_t, NB_PATCH_TYPE> CHUNK_SIZES{
    (int32_t)std::ceil(SUGGESTED_DISTANCES[STARTER]),
    (int32_t)std::ceil(SUGGESTED_DISTANCES[REGULAR]),
};

static constexpr std::array<int32_t, NB_PATCH_TYPE> CHUNK_COUNTS{
    1 + REGION_SIZES[STARTER] / CHUNK_SIZES[STARTER],
    1 + REGION_SIZES[REGULAR] / CHUNK_SIZES[REGULAR]
};

static constexpr std::array<uint8_t, NB_PATCH_TYPE> SEEDS1{
    101, 100
};

static constexpr std::array<std::array<int32_t, NB_RESOURCE_TYPE>, NB_PATCH_TYPE> NB_SPOTS{
    std::array<int32_t, NB_RESOURCE_TYPE>{ 32, 32, 32, 32 },
    std::array<int32_t, NB_RESOURCE_TYPE>{ 22, 22, 21, 21 }
};
constexpr int MAX_NB_SPOTS = 32;

static constexpr std::array<float, NB_RESOURCE_TYPE> BASE_DENSITIES{
    10, 8, 8, 4
};

static constexpr std::array<std::array<float, NB_RESOURCE_TYPE>, NB_PATCH_TYPE> RQ_FACTORS{
    std::array<float, NB_RESOURCE_TYPE>{ 1.5f/7.f, 1.2f/7.f, 1.1f/7.f, 1.1f/7.f },
    std::array<float, NB_RESOURCE_TYPE>{ 0.11f, 0.11f, 0.10f, 0.10f }
};

static constexpr std::array<int, NB_RESOURCE_TYPE> OFFSETS{
    0, 1, 2, 3
};
static constexpr float REGULAR_PATCH_FADE_IN_DISTANCE = 300.f;
static constexpr float STARTING_RESOURCE_PLACEMENT_RADIUS = 120.f;
static constexpr float BASE_SPOTS_PER_KM2 = 2.5f;
static constexpr float DOUBLE_DENSITY_DISTANCE = 1300.f;
static constexpr float RANDOM_SPOT_SIZE_MINIMUM = 0.25f;
static constexpr float RANDOM_SPOT_SIZE_MAXIMUM = 2.f;