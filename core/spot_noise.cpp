#include "spot_noise.hpp"

#include <cmath>

template<int NbCandidates, float SuggestedDistance, int32_t RegionSize, int NbSpots>
static std::array<Candidate, NbCandidates> generate_candidates(
    SpotNoiseCache<NbCandidates, SuggestedDistance, RegionSize, NbSpots> &cache, const uint32_t seed0, const uint8_t seed1
) {
    std::array<Candidate, NbCandidates> candidates;

    const uint32_t id = cache.get_id();
    const auto chunk_count = cache.c_chunk_count;
    const auto chunk_size = cache.c_chunk_size;
    auto& chunks = cache.get_chunks();

    // const uint32_t seed = (region_y * 0x1ee3 + region_x * 0x1eef + seed1 * 0x1ef7 + 0x3fbe2c) ^ seed0;
    const uint32_t seed = (seed1 * 0x1ef7 + 0x3fbe2c) ^ seed0;
    Random random(seed);

    float suggested_distance2 = SuggestedDistance * SuggestedDistance;

    constexpr int32_t half_region_size = RegionSize / 2;
    // const int32_t offset_x = RegionSize * region_x - half_region_size;
    // const int32_t offset_y = RegionSize * region_y - half_region_size;

    int i = 0;
    while (i < NbCandidates) {
        int32_t x = random.random() % RegionSize;
        int32_t y = random.random() % RegionSize;
        const int32_t chunk_x = x / chunk_size;
        const int32_t chunk_y = y / chunk_size;
        
        x -= half_region_size;
        y -= half_region_size;

        const int other_chunk_y_min = std::max(0, chunk_y - 1);
        const int other_chunk_x_max = std::min(chunk_count - 1, chunk_x + 1);
        const int other_chunk_y_max = std::min(chunk_count - 1, chunk_y + 1);

        bool valid = true;

        for (int other_chunk_x = std::max(0, chunk_x - 1); other_chunk_x <= other_chunk_x_max; other_chunk_x++) {
            for (int other_chunk_y = other_chunk_y_min; other_chunk_y <= other_chunk_y_max; other_chunk_y++) {
                auto& other_array = chunks[other_chunk_x + other_chunk_y * chunk_count];
                if (other_array.id != id) continue;

                for (auto& other : other_array) {
                    int32_t dx = x - other.x;
                    int32_t dy = y - other.y;
                    if (dx*dx + dy*dy < suggested_distance2) {
                        suggested_distance2 *= 0.9375f;
                        valid = false;
                        goto invalid;
                    }
                }
            }
        }
        invalid:;

        if (valid) {
            auto& candidate = candidates[i];
            candidate.x = x;
            candidate.y = y;

            auto& other_array = chunks[chunk_x + chunk_y * chunk_count];
            if (other_array.id != id) {
                other_array.clear();
                other_array.id = id;
            }
            other_array.insert(x, y);
            i++;
        }
    }

    return candidates;
}

Patches starter_patches(const MapGenSettings& settings, const StarterSpotNoiseCache& cache, const uint32_t seed0);

static constexpr float regular_density_modifier(const int32_t x, const int32_t y) {
    const float d = std::sqrtf((float)x*x + (float)y*y);

    const float closeness_penalty = std::clamp((d - c_starting_resource_placement_radius) / c_regular_patch_fade_in_distance, 0.f, 1.f);

    const float size_effective_distance_at = d - c_regular_patch_fade_in_distance;
    const float distance_bonus = 1 + std::clamp(size_effective_distance_at / c_double_density_distance, 0.f, 1.f);

    return closeness_penalty * distance_bonus;
}

static constexpr float regular_quantity(const float base_quantity, const float density, const float penalty) {
    return base_quantity * density * penalty;
}

static constexpr float regular_radius(const float rq_factor, const float quantity) {
    /*
    spot_radius_expression = min(32, regular_rq_factor * regular_spot_quantity_expression ^ (1/3))
    */

    return std::min(32.f, rq_factor * std::cbrtf(quantity));
}

Patches regular_patches(const MapGenSettings&, RegularSpotNoiseCache& cache, const uint32_t seed0) {
    Patches patches;

    auto candidates = generate_candidates<c_nb_regular_spot_to_generate,
        c_regular_suggested_distance, c_regular_region_size, 22>(cache, seed0, c_regular_seed1);

    for (PatchType type = IRON; type < NB_PATCH_TYPE; type++) {
        float base_density = cache.get_base_regular_densities()[type];
        float base_quantity = cache.get_base_regular_quantities()[type];
        float rq_factor = c_regular_rq_factors[type];

        float total_density = 0.f;
        const auto count = c_regular_candidate_counts[type];
        const auto offset = c_offsets[type];

        for (int i = 0; i < count; i++) {
            auto &candidate = candidates[i*c_span + offset];
            const float density = base_density * regular_density_modifier(candidate.x, candidate.y);

            candidate.density = density;
            total_density += density;
        }

        // In the original algorithm we sort the candidates using the favorability expression,
        // but for the regular patches it is always 1, so we can skip that sort.

        int penalties_offset = 22 - count;
        const auto& penalties = cache.get_penalties()[candidates[offset].x+512 + (candidates[offset].y+512)*1024];

        total_density = c_regular_region_size*c_regular_region_size * total_density / count;
        float total_quantity = 0.f;

        for (int i = 0; i < count; i++) {
            const auto &candidate = candidates[i*c_span + offset];
            const float quantity = regular_quantity(base_quantity, candidate.density, penalties[i + penalties_offset]);
            const float radius = regular_radius(rq_factor, quantity);

            patches[type].insert(candidate.x, candidate.y, radius, quantity);
            total_quantity += quantity;

            if (total_quantity >= total_density) break;
        }
    }

    return patches;
}