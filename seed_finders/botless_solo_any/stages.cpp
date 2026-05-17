#define _USE_MATH_DEFINES
#include "stages.hpp"
#include "algorithm.hpp"

constexpr int32_t MAX_OIL_SPAWN_DISTANCE = 120 + 100;
constexpr float BACKSIDE_IRON_MIN_RADIUS = 22.f;
constexpr float BACKSIDE_COPPER_MIN_RADIUS = 22.f;
constexpr float BACKSIDE_COAL_MIN_RADIUS = 13.f;

constexpr int32_t NOISE_SAMPLING_DISTANCE = 4;

constexpr float MALL_WEIGHT = 1.f;
constexpr float COAL_WATER_DISTANCE_WEIGHT = 0.3f;
// constexpr float FRONT_COAL_WEIGHT = 0.4f;
// constexpr float OIL_WEIGHT = 0.15f;

constexpr int32_t BACKSIDE_MAX_DISTANCE = 245;
constexpr int32_t BACKSIDE_MAX_DISTANCE_BETWEEN_PATCHES = 20;
constexpr BoxI32 BACKSIDE_BOX(-165, -35, -145, 45);

constexpr std::array<ResourceBox, 10> MALL_BOXES{
    ResourceBox{ STONE,  { -25, -70,  70,   0 }, .001f, 0.f },
    ResourceBox{ IRON,   { -24,  -5,  24,   6 }, .1f,   1.f }, // Mall iron 3 + steel
    ResourceBox{ IRON,   { -24,  17,  24,  23 }, .1f,   1.f }, // Mall iron 2
    ResourceBox{ IRON,   { -24,  34,  24,  40 }, .1f,   1.f }, // Mall iron 1
    ResourceBox{ IRON,   { -24,  48,  -4,  52 }, .1f,   1.f }, // Burners
    ResourceBox{ COPPER, { -24,  53,  -4,  55 }, .1f,   1.f }, // Burners
    ResourceBox{ COPPER, { -24,  65,  24,  71 }, .1f,   1.f }, // Mall copper
    ResourceBox{ COPPER, { -24,  75,  24, 102 }, .1f,   1.f }, // RC copper
    ResourceBox{ IRON,   { -24, 104, 120, 140 }, .001f, 0.f }, // RC iron
    ResourceBox{ COAL,   { -36,  40, -28,  65 }, .1f,   0.f }  // Burners
};
constexpr PositionI32 BURNERS_POS(-28, 53);
constexpr std::array<BoxI32, 2> BASE_BOXES{
    BoxI32(24, 65, 154, 90),
    BoxI32(24,  5,  76, 65)
};
constexpr BoxI32 OIL_BOX(10, -120, 150, 200);

constexpr int32_t MAX_COAL_WATER_DISTANCE = 30;

Finder<SeedCache>::EvalResult stage1_eval(
    const MapGenSettings& settings, const NoisePrecompute&, NoiseCache& noise_cache, uint32_t seed, SeedCache* seed_cache
) {
    Patches patches = regular_patches(settings, noise_cache, seed, { 0, 0 });

    bool any_oil = false;
    for (const auto& oil : patches[OIL]) {
        if (oil.pos.chebyshev_length() < MAX_OIL_SPAWN_DISTANCE && oil.radius > 0) {
            any_oil = true;
            break;
        }
    }

    if (!any_oil) return { true, 0.f };

    for (const auto& copper : patches[COPPER]) {
        if (
            copper.pos.chebyshev_length() - (int32_t)copper.radius > BACKSIDE_MAX_DISTANCE ||
            copper.radius < BACKSIDE_COPPER_MIN_RADIUS
        ) continue;

        bool any_iron = false, any_coal = false;
        Patch iron_patch, coal_patch;
        for (const auto& iron : patches[IRON]) {
            int32_t distance_with_copper = PositionI32::chebyshev_distance(copper.pos, iron.pos) - (int32_t)copper.radius - (int32_t)iron.radius;
            if (
                iron.pos.chebyshev_length() - (int32_t)iron.radius < BACKSIDE_MAX_DISTANCE &&
                distance_with_copper < BACKSIDE_MAX_DISTANCE_BETWEEN_PATCHES &&
                iron.radius > BACKSIDE_IRON_MIN_RADIUS
            ) {
                any_iron = true;
                iron_patch = iron;
                break;
            }
        }

        for (const auto& coal : patches[COAL]) {
            int32_t distance_with_copper = PositionI32::chebyshev_distance(copper.pos, coal.pos) - (int32_t)copper.radius - (int32_t)coal.radius;
            if (
                coal.pos.chebyshev_length() - (int32_t)coal.radius < BACKSIDE_MAX_DISTANCE &&
                distance_with_copper < BACKSIDE_MAX_DISTANCE_BETWEEN_PATCHES &&
                coal.radius > BACKSIDE_COAL_MIN_RADIUS
            ) {
                any_coal = true;
                coal_patch = coal;
                break;
            }
        }

        if (any_iron && any_coal) {
            seed_cache->back_iron_patch = iron_patch;
            seed_cache->back_copper_patch = copper;
            seed_cache->back_coal_patch = coal_patch;
            return { false, 0.f };
        }
    }

    return { true, 0.f };
}

Finder<SeedCache>::EvalResult stage2_eval(
    const MapGenSettings& settings, const NoisePrecompute& precompute, NoiseCache& noise_cache, uint32_t seed, SeedCache* seed_cache
) {
    Noise noise(seed, true, false);
    Patches r_patches = regular_patches(precompute, noise_cache, seed, { 0, 0 });
    Patches s_patches = starter_patches(settings, precompute, noise, noise_cache, seed);
    float best_score = 0.f;

    auto try_box = [&](PositionI32 offset, Direction direction, bool flipped) {
        auto pair = evaluate_boxes<MALL_BOXES.size(), MALL_BOXES, BACKSIDE_BOX>(s_patches, &r_patches, offset, direction, flipped);

        if (best_score < pair.second) {
            best_score = pair.second;
            BoxI32 rotated_box0 = BASE_BOXES[0].rotated(direction);
            seed_cache->base_boxes[0] = (flipped ? rotated_box0.flipped(direction) : rotated_box0) + pair.first;
            BoxI32 rotated_box1 = BASE_BOXES[1].rotated(direction);
            seed_cache->base_boxes[1] = (flipped ? rotated_box1.flipped(direction) : rotated_box1) + pair.first;
            BoxI32 oil_rotated_box = OIL_BOX.rotated(direction);
            seed_cache->oil_box = (flipped ? oil_rotated_box.flipped(direction) : oil_rotated_box) + pair.first;
            PositionI32 rotated_burners_pos = BURNERS_POS.rotated(direction);
            seed_cache->burners_pos = (flipped ? rotated_burners_pos.flipped(direction) : rotated_burners_pos) + pair.first;
        }
    };

    auto copper = seed_cache->back_copper_patch;
    for (Direction direction = EAST; direction < NB_DIRECTIONS; direction = next_direction(direction)) {
        PositionI32 offset = copper.pos - PositionI32(direction)*(int32_t)copper.radius;
        try_box(offset, direction, false);
        try_box(offset, direction, true);
    }

    seed_cache->mall_score = best_score;
    return { best_score == 0.f, best_score };
}

Finder<SeedCache>::EvalResult stage3_eval(
    const MapGenSettings& settings, const NoisePrecompute& precompute, NoiseCache& noise_cache, uint32_t seed, SeedCache* seed_cache
) {
    Noise noise(seed, true, settings.elevation_type == ELEVATION_2_0 ? true : false);
    Patches s_patches = starter_patches(settings, precompute, noise, noise_cache, seed);
    Patches r_patches = regular_patches(precompute, noise_cache, seed, { 0, 0 });

    float coal_water_distance = INFINITY;
    BoxI32 box(seed_cache->burners_pos + PositionI32(-70, -70), seed_cache->burners_pos + PositionI32(0, 70));
    for (int32_t x = box.left_top.x; x < box.right_bottom.x; x += NOISE_SAMPLING_DISTANCE) {
        for (int32_t y = box.left_top.y; y < box.right_bottom.y; y += NOISE_SAMPLING_DISTANCE) {
            PositionI32 pos(x, y);
            if (noise.is_tile_water(settings, precompute, pos)) {
                for (auto& coal : s_patches[COAL]) {
                    coal_water_distance = std::min(
                        coal_water_distance,
                        (PositionI32::distance(coal.pos, pos) - coal.radius)
                    );
                }
            }
        }
    }

    bool any_oil = false;
    for (const auto& oil : r_patches[OIL]) {
        if (noise.elevation(settings, precompute, oil.pos) > -5 && seed_cache->oil_box.contains(oil.pos) && oil.radius > 0) {
            any_oil = true;
            break;
        }
    }

    if (!any_oil) return { true, 0.f };

    float score = (
        MALL_WEIGHT * seed_cache->mall_score
        - COAL_WATER_DISTANCE_WEIGHT * std::abs(coal_water_distance) / MAX_COAL_WATER_DISTANCE
        // - OIL_WEIGHT * std::clamp((float)(best_oil_distance - 50) / MAX_OIL_SPAWN_DISTANCE, 0.f, 1.f)
    );

    return {
        coal_water_distance > MAX_COAL_WATER_DISTANCE ||
        noise.any_water_in_box(settings, precompute, BoxI32(seed_cache->back_iron_patch.pos, (int32_t)seed_cache->back_iron_patch.radius), NOISE_SAMPLING_DISTANCE) ||
        noise.any_water_in_box(settings, precompute, BoxI32(seed_cache->back_copper_patch.pos, (int32_t)seed_cache->back_copper_patch.radius), NOISE_SAMPLING_DISTANCE) ||
        noise.any_water_in_box(settings, precompute, BoxI32(seed_cache->back_coal_patch.pos, (int32_t)seed_cache->back_coal_patch.radius), NOISE_SAMPLING_DISTANCE) ||
        noise.any_water_in_box(settings, precompute, seed_cache->base_boxes[0], NOISE_SAMPLING_DISTANCE) ||
        noise.any_water_in_box(settings, precompute, seed_cache->base_boxes[1], NOISE_SAMPLING_DISTANCE),
        score
    };
}