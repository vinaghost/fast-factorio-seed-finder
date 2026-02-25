#include "noise.hpp"
#include "gradients.hpp"
#include <numeric>
#include <cmath>

Noise::Noise(uint32_t seed0) {
    Random random(seed0);

    std::iota(_p1.begin(), _p1.end(), 0);
    std::iota(_p2.begin(), _p2.end(), 0);
    std::iota(_p3.begin(), _p3.end(), 0);
    _grad = default_gradients();

    random.shuffle(_p1);
    random.shuffle(_p2);
    random.shuffle(_p3);
    random.shuffle(_grad);
}

std::pair<float, float> Noise::_gradient(uint8_t p1, uint8_t x, uint8_t y) {
    uint8_t y_permutation = p1 ^ _p2[y];
    uint8_t xy_permutation = y_permutation ^ _p3[x];
    return _grad[xy_permutation];
}

float Noise::noise(uint8_t seed1, float x, float y, float input_scale, float output_scale, float offset_x, float offset_y) {
    const uint8_t p1 = _p1[seed1];
    
    const float x_scaled = (x + offset_x) * input_scale;
    const float y_scaled = (y + offset_y) * input_scale;

    const float x_floor = std::floorf(x_scaled);
    const float y_floor = std::floorf(y_scaled);
    const float x_ceil = x_floor + 1.f;
    const float y_ceil = y_floor + 1.f;
    const float x_frac = x_scaled - x_floor;
    const float y_frac = y_scaled - y_floor;

    const std::array<std::pair<float, float>, 4> points{
        std::make_pair( x_floor, y_floor ),
        std::make_pair( x_ceil, y_floor ),
        std::make_pair( x_floor, y_ceil ),
        std::make_pair( x_ceil, y_ceil )
    };

    const std::array<std::pair<float, float>, 4> dcba{
        std::make_pair( 0.f, 0.f ),
        std::make_pair( 1.f, 0.f ),
        std::make_pair( 0.f, 1.f ),
        std::make_pair( 1.f, 1.f )
    };

    float sum = 0.f;

    for (int i = 0; i < 4; i++) {
        const auto gradient = _gradient(p1, points[i].first, points[i].second);

        const float x_frac_offset = x_frac - dcba[i].first;
        const float y_frac_offset = y_frac - dcba[i].second;

        const float d2 = 1.f - std::min(1.f, x_frac_offset*x_frac_offset + y_frac_offset*y_frac_offset);

        sum += (
            (x_frac_offset * gradient.first) +
            (y_frac_offset * gradient.second)
        ) * d2*d2*d2;
    }

    return sum * output_scale;
}

// float Noise::multioctave_noise(float x, float y, float persistence, float octaves,
//     float input_scale, float output_scale, float offset_x, float offset_y
// ) {
//     return 0;
// }

float Noise::persistence_multioctave_noise(uint8_t seed1, float x, float y, float persistence, float octaves,
    float input_scale, float output_scale, float offset_x, float offset_y
) {
    input_scale *= 0.5f;
    output_scale *= std::powf(2.f, octaves);

    float sum = 0.f;
    for (int i = 1; i < octaves; i++) {
        sum += noise(seed1, x, y, input_scale, 1.f, offset_x, offset_y);
        sum *= persistence;

        input_scale *= 0.5f;
    }

    sum += noise(seed1, x, y, input_scale, 1.f, offset_x, offset_y);

    return sum * output_scale;
}

float Noise::finish_elevation(const MapGenSettings& settings, const NoisePrecompute& precompute, float elevation, float x, float y) {
    float dx = x - precompute.starter_lake.x;
    float dy = y - precompute.starter_lake.y;
    float starting_lake_distance = std::sqrt(dx*dx + dy*dy);

    constexpr float input_scale = 1.f/8;
    constexpr float output_scale = 1.f;
    constexpr int octaves = 5;
    constexpr float octave_input_scale_multiplier = 0.5f;
    constexpr float persistence = 0.75f;
    float starting_lake_noise = quick_multioctave_noise(
        
    )
    
    return std::min(
        (elevation - precompute.water_level) / settings.water_scale,
        noise(123, x, y, 1/8, 1.5) + starting_lake_distance / 4 - 4,
        -1 + (starting_lake_distance + starting_lake_noise) / 16,
        max(2, 2 + starting_lake_distance / 16 + starting_lake_noise / 2)
    )
}

float Noise::elevation_lakes(const MapGenSettings& settings, const NoisePrecompute& precompute, float x, float y) {
    float lakes = make_0_12like_lakes()
    return finish_elevation(lakes)
}

float Noise::amplitude_corrected_multioctave_noise(uint8_t seed1, float x, float y, float persistence, float octaves,
    float input_scale, float output_scale, float offset_x, float offset_y
) {
    const float corrected_output_scale = (1 - persistence) / std::exp2(octaves) / (1 - std::pow(persistence, octaves)) * output_scale;

    return persistence_multioctave_noise(
        seed1, x, y, persistence, octaves, input_scale, corrected_output_scale, offset_x, offset_y
    );
}

template<PatchType Type>
static std::array<Candidate, NB_CANDIDATES[Type]> generate_candidates(
    NoiseCache& cache, const uint32_t seed0
) {
    std::array<Candidate, NB_CANDIDATES[Type]> candidates;

    const uint32_t id = cache.get_id();
    auto& chunks = cache.get_chunks<Type>();
    constexpr auto chunk_count = CHUNK_COUNTS[Type];
    constexpr auto chunk_size = CHUNK_SIZES[Type];

    // const uint32_t seed = (region_y * 0x1ee3 + region_x * 0x1eef + seed1 * 0x1ef7 + 0x3fbe2c) ^ seed0;
    constexpr uint32_t seed_base = SEEDS1[Type] * 0x1ef7 + 0x3fbe2c;
    const uint32_t seed = seed_base ^ seed0;
    Random random(seed);

    constexpr float c_suggested_distance2 = SUGGESTED_DISTANCES[Type] * SUGGESTED_DISTANCES[Type];
    float suggested_distance2 = c_suggested_distance2;

    constexpr int32_t half_region_size = REGION_SIZES[Type] / 2;
    // const int32_t offset_x = RegionSize * region_x - half_region_size;
    // const int32_t offset_y = RegionSize * region_y - half_region_size;

    int i = 0;
    while (i < NB_CANDIDATES[Type]) {
        int32_t x = random.random() % REGION_SIZES[Type];
        int32_t y = random.random() % REGION_SIZES[Type];
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

Patches starter_patches(const NoisePrecompute&, NoiseCache&, const uint32_t) {
    assert(false && "TODO");
    Patches patches;
    return patches;
}

static constexpr float regular_density_modifier(const int32_t x, const int32_t y) {
    const float d = std::sqrtf((float)x*x + (float)y*y);

    const float closeness_penalty = std::clamp((d - STARTING_RESOURCE_PLACEMENT_RADIUS) / REGULAR_PATCH_FADE_IN_DISTANCE, 0.f, 1.f);

    const float size_effective_distance_at = d - REGULAR_PATCH_FADE_IN_DISTANCE;
    const float distance_bonus = 1 + std::clamp(size_effective_distance_at / DOUBLE_DENSITY_DISTANCE, 0.f, 1.f);

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

Patches regular_patches(const NoisePrecompute& precompute, NoiseCache& cache, const uint32_t seed0) {
    Patches patches;

    auto candidates = generate_candidates<REGULAR>(cache, seed0);

    for (ResourceType type = IRON; type < NB_RESOURCE_TYPE; type++) {
        const float base_density = precompute.get_base_regular_densities()[type];
        const float base_quantity = precompute.get_base_regular_quantities()[type];
        const float rq_factor = RQ_FACTORS[REGULAR][type];

        float total_density = 0.f;
        const auto nb_spots = NB_SPOTS[REGULAR][type];
        const auto offset = OFFSETS[type];

        for (int i = 0; i < nb_spots; i++) {
            auto &candidate = candidates[i*SPAN + offset];
            const float density = base_density * regular_density_modifier(candidate.x, candidate.y);

            candidate.density = density;
            total_density += density;
        }

        // In the original algorithm we sort the candidates using the favorability expression,
        // but for the regular patches it is always 1, so we can skip that sort.

        int penalties_offset = MAX_NB_SPOTS - nb_spots;
        const auto& penalties = precompute.get_penalties()[candidates[offset].x+512 + (candidates[offset].y+512)*1024];

        total_density = REGION_SIZES[REGULAR]*REGION_SIZES[REGULAR] * total_density / nb_spots;
        float total_quantity = 0.f;

        for (int i = 0; i < nb_spots; i++) {
            const auto &candidate = candidates[i*SPAN + offset];
            const float quantity = regular_quantity(base_quantity, candidate.density, penalties[i + penalties_offset]);
            const float radius = regular_radius(rq_factor, quantity);

            patches[type].insert(candidate.x, candidate.y, radius, quantity);
            total_quantity += quantity;

            if (total_quantity >= total_density) break;
        }
    }

    return patches;
}