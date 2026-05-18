#define _USE_MATH_DEFINES
#include "stages.hpp"

#include "noise.hpp"
#include "algorithm.hpp"

namespace {

constexpr float MIN_IRON_RADIUS = 10.f;
constexpr float MIN_COPPER_RADIUS = 10.f;
constexpr int32_t MAX_PATCH_DISTANCE_FROM_SPAWN = 320;
constexpr int32_t MAX_PATCH_PAIR_DISTANCE = 220;
constexpr int32_t MAX_IRON_COPPER_PAIR_CENTER_DISTANCE = 160;
constexpr int32_t NOISE_SAMPLING_DISTANCE = 4;
constexpr int32_t MAX_COAL_WATER_DISTANCE = 45;
constexpr float COAL_WATER_DISTANCE_WEIGHT = 0.2f;

inline int64_t cross_product(PositionI32 a, PositionI32 b, PositionI32 c) {
    int64_t abx = (int64_t)b.x - a.x;
    int64_t aby = (int64_t)b.y - a.y;
    int64_t acx = (int64_t)c.x - a.x;
    int64_t acy = (int64_t)c.y - a.y;
    return abx * acy - aby * acx;
}

inline bool on_segment(PositionI32 a, PositionI32 b, PositionI32 p) {
    return std::min(a.x, b.x) <= p.x && p.x <= std::max(a.x, b.x)
        && std::min(a.y, b.y) <= p.y && p.y <= std::max(a.y, b.y);
}

inline bool segments_intersect(PositionI32 a, PositionI32 b, PositionI32 c, PositionI32 d) {
    int64_t c1 = cross_product(a, b, c);
    int64_t c2 = cross_product(a, b, d);
    int64_t c3 = cross_product(c, d, a);
    int64_t c4 = cross_product(c, d, b);

    if ((c1 > 0) != (c2 > 0) && (c3 > 0) != (c4 > 0)) {
        return true;
    }

    if (c1 == 0 && on_segment(a, b, c)) return true;
    if (c2 == 0 && on_segment(a, b, d)) return true;
    if (c3 == 0 && on_segment(c, d, a)) return true;
    if (c4 == 0 && on_segment(c, d, b)) return true;

    return false;
}

} // namespace

// Parameters settings, precompute and noise_cache are required by some functions in noise.hpp.
// seed and settings are the current seed, water settings and elevation type we are looking evaluating
Finder<SeedCache>::EvalResult stage1_eval(
    const MapGenSettings&, const NoisePrecompute& precompute, NoiseCache& noise_cache, uint32_t seed, SeedCache* seed_cache
) {
    // Gives a high score to seeds with little ore. Only looks at regular patches.

    // The game generates regular patches by regions (or chunks) of 1024x1024 tiles. Region (0, 0) is centered on spawn.
    // For example region (-1, 1) would be all patches in the box of center (-1024, 1024) and of side length 1024.
    Patches patches = regular_patches(precompute, noise_cache, seed, { 0, 0 });

    const PatchArray& iron_patches = patches[IRON];
    const PatchArray& copper_patches = patches[COPPER];

    float stage1_area = union_area(patches);
    float best_score = -INFINITY;
    bool any_valid_layout = false;

    for (size_t i = 0; i < iron_patches.size(); i++) {
        const Patch& iron1 = iron_patches[i];
        if (iron1.radius < MIN_IRON_RADIUS) continue;
        if (iron1.pos.chebyshev_length() - (int32_t)iron1.radius > MAX_PATCH_DISTANCE_FROM_SPAWN) continue;

        for (size_t j = i + 1; j < iron_patches.size(); j++) {
            const Patch& iron2 = iron_patches[j];
            if (iron2.radius < MIN_IRON_RADIUS) continue;
            if (iron2.pos.chebyshev_length() - (int32_t)iron2.radius > MAX_PATCH_DISTANCE_FROM_SPAWN) continue;

            float iron_pair_distance = PositionI32::distance(iron1.pos, iron2.pos);
            if (iron_pair_distance > (float)MAX_PATCH_PAIR_DISTANCE) continue;

            PositionI32 iron_pair_center = (iron1.pos + iron2.pos) / 2;

            for (size_t k = 0; k < copper_patches.size(); k++) {
                const Patch& copper1 = copper_patches[k];
                if (copper1.radius < MIN_COPPER_RADIUS) continue;
                if (copper1.pos.chebyshev_length() - (int32_t)copper1.radius > MAX_PATCH_DISTANCE_FROM_SPAWN) continue;

                for (size_t l = k + 1; l < copper_patches.size(); l++) {
                    const Patch& copper2 = copper_patches[l];
                    if (copper2.radius < MIN_COPPER_RADIUS) continue;
                    if (copper2.pos.chebyshev_length() - (int32_t)copper2.radius > MAX_PATCH_DISTANCE_FROM_SPAWN) continue;

                    float copper_pair_distance = PositionI32::distance(copper1.pos, copper2.pos);
                    if (copper_pair_distance > (float)MAX_PATCH_PAIR_DISTANCE) continue;

                    PositionI32 copper_pair_center = (copper1.pos + copper2.pos) / 2;
                    float pair_centers_distance = PositionI32::distance(iron_pair_center, copper_pair_center);
                    if (pair_centers_distance > (float)MAX_IRON_COPPER_PAIR_CENTER_DISTANCE) continue;

                    if (segments_intersect(iron1.pos, iron2.pos, copper1.pos, copper2.pos)) {
                        continue;
                    }

                    float score =
                        iron1.radius + iron2.radius + copper1.radius + copper2.radius
                        - 0.01f * (iron_pair_distance + copper_pair_distance)
                        - 0.02f * pair_centers_distance;

                    if (!any_valid_layout || score > best_score) {
                        any_valid_layout = true;
                        best_score = score;
                    }
                }
            }
        }
    }

    // The seed cache is used to pass data to the next stages.
    // If the next stage introduces the twin seed, water settings or elevation types, then the cache will get copied to each configuration.
    //
    // Note that if the SeedCache template is void then seed_cache will be a nullptr.
    seed_cache->stage1_area = stage1_area;

    // .eliminate is the pass-or-fail method to eliminate seeds I talked about in stages.hpp.
    // If it is set to true, the seed will not make it through to the next stage.
    // If set to false, then the top n method will be used. If n is greater than the number of seeds that
    // were not eliminated then all of them will make it to the next stage.

    return { .eliminate = !any_valid_layout, .score = any_valid_layout ? best_score : -INFINITY };
}

Finder<SeedCache>::EvalResult stage2_eval(
    const MapGenSettings& settings, const NoisePrecompute& precompute, NoiseCache& noise_cache, uint32_t seed, SeedCache* seed_cache, Stage2Cache &
) {
    // Same pattern as stage1, but on starter patches with an exact patch count requirement.
    Noise noise(seed, true, false);
    Patches patches = starter_patches(settings, precompute, noise, noise_cache, seed);

    if (
        patches[IRON].size() != 2 ||
        patches[COPPER].size() != 2 ||
        patches[COAL].size() != 1 ||
        patches[STONE].size() != 1
    ) {
        return { .eliminate = true, .score = -INFINITY };
    }

    const Patch& iron1 = patches[IRON][0];
    const Patch& iron2 = patches[IRON][1];
    const Patch& copper1 = patches[COPPER][0];
    const Patch& copper2 = patches[COPPER][1];

    if (
        iron1.radius < MIN_IRON_RADIUS ||
        iron2.radius < MIN_IRON_RADIUS ||
        copper1.radius < MIN_COPPER_RADIUS ||
        copper2.radius < MIN_COPPER_RADIUS
    ) {
        return { .eliminate = true, .score = -INFINITY };
    }

    if (
        iron1.pos.chebyshev_length() - (int32_t)iron1.radius > MAX_PATCH_DISTANCE_FROM_SPAWN ||
        iron2.pos.chebyshev_length() - (int32_t)iron2.radius > MAX_PATCH_DISTANCE_FROM_SPAWN ||
        copper1.pos.chebyshev_length() - (int32_t)copper1.radius > MAX_PATCH_DISTANCE_FROM_SPAWN ||
        copper2.pos.chebyshev_length() - (int32_t)copper2.radius > MAX_PATCH_DISTANCE_FROM_SPAWN
    ) {
        return { .eliminate = true, .score = -INFINITY };
    }

    float iron_pair_distance = PositionI32::distance(iron1.pos, iron2.pos);
    float copper_pair_distance = PositionI32::distance(copper1.pos, copper2.pos);
    if (
        iron_pair_distance > (float)MAX_PATCH_PAIR_DISTANCE ||
        copper_pair_distance > (float)MAX_PATCH_PAIR_DISTANCE
    ) {
        return { .eliminate = true, .score = -INFINITY };
    }

    PositionI32 iron_pair_center = (iron1.pos + iron2.pos) / 2;
    PositionI32 copper_pair_center = (copper1.pos + copper2.pos) / 2;
    float pair_centers_distance = PositionI32::distance(iron_pair_center, copper_pair_center);
    if (pair_centers_distance > (float)MAX_IRON_COPPER_PAIR_CENTER_DISTANCE) {
        return { .eliminate = true, .score = -INFINITY };
    }

    if (segments_intersect(iron1.pos, iron2.pos, copper1.pos, copper2.pos)) {
        return { .eliminate = true, .score = -INFINITY };
    }

    float score =
        iron1.radius + iron2.radius + copper1.radius + copper2.radius
        - 0.01f * (iron_pair_distance + copper_pair_distance)
        - 0.02f * pair_centers_distance;

    // Keep stage1 cache meaningful for potential downstream heuristics.
    seed_cache->stage1_area = union_area(patches);

    return { .eliminate = false, .score = score };
}

Finder<SeedCache>::EvalResult stage3_eval(
    const MapGenSettings& settings, const NoisePrecompute& precompute, NoiseCache& noise_cache, uint32_t seed, SeedCache*
) {
    // This stage tries to see if there is any water under patches and reduce the area accordingly

    // Here we regenerate patches, it could be possible to transmit this from previous stages using the noise_cache.
    // However it would probably not be faster because it would require much more memory.
    Noise noise(seed, true, settings.elevation_type == ELEVATION_2_0);
    Patches s_patches = starter_patches(settings, precompute, noise, noise_cache, seed);
    Patches r_patches = regular_patches(precompute, noise_cache, seed, {0, 0});

    if (
        s_patches[IRON].size() != 2 ||
        s_patches[COPPER].size() != 2 ||
        s_patches[COAL].size() != 1 ||
        s_patches[STONE].size() != 1
    ) {
        return { true, -INFINITY };
    }

    auto transform_patches = [&](Patch &p, ResourceType, size_t) {
        // Water can partially override patch usability, so reduce effective radius.
        if (noise.is_tile_water(settings, precompute, p.pos)) {
            p.radius *= 0.5f;
        }
    };

    iterate_patches(s_patches, transform_patches);
    iterate_patches(r_patches, transform_patches);

    const Patch& iron1 = s_patches[IRON][0];
    const Patch& iron2 = s_patches[IRON][1];
    const Patch& copper1 = s_patches[COPPER][0];
    const Patch& copper2 = s_patches[COPPER][1];
    const Patch& coal = s_patches[COAL][0];

    if (
        iron1.radius < MIN_IRON_RADIUS * 0.5f ||
        iron2.radius < MIN_IRON_RADIUS * 0.5f ||
        copper1.radius < MIN_COPPER_RADIUS * 0.5f ||
        copper2.radius < MIN_COPPER_RADIUS * 0.5f
    ) {
        return { true, -INFINITY };
    }

    if (segments_intersect(iron1.pos, iron2.pos, copper1.pos, copper2.pos)) {
        return { true, -INFINITY };
    }

    float coal_water_distance = INFINITY;
    BoxI32 coal_box(coal.pos, MAX_COAL_WATER_DISTANCE);
    for (int32_t x = coal_box.left_top.x; x < coal_box.right_bottom.x; x += NOISE_SAMPLING_DISTANCE) {
        for (int32_t y = coal_box.left_top.y; y < coal_box.right_bottom.y; y += NOISE_SAMPLING_DISTANCE) {
            PositionI32 pos(x, y);
            if (noise.is_tile_water(settings, precompute, pos)) {
                coal_water_distance = std::min(coal_water_distance, PositionI32::distance(coal.pos, pos) - coal.radius);
            }
        }
    }

    float area = union_area(s_patches) + union_area(r_patches);

    float score = -area;
    if (std::isfinite(coal_water_distance)) {
        score -= COAL_WATER_DISTANCE_WEIGHT * std::abs(coal_water_distance) / MAX_COAL_WATER_DISTANCE;
    }

    return {
        !std::isfinite(coal_water_distance) ||
        coal_water_distance > MAX_COAL_WATER_DISTANCE,
        score
    };
}