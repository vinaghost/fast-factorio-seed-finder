#pragma once

#include "utils.hpp"
#include "spot_noise_constants.hpp"
#include "random.hpp"
#include <algorithm>
#include <cassert>

struct Candidate {
    int32_t x;
    int32_t y;
    float density;
};

struct PseudoCandidate {
    int32_t x;
    int32_t y;
};

template<int NbCandidates>
struct PseudoCandidateArray {
    std::array<PseudoCandidate, NbCandidates> data;
    uint32_t id = 0;
    size_t count = 0;

    inline void insert(int32_t x, int32_t y) {
        assert(count < data.size());
        data[count] = {x, y};
        count++;
    }

    inline void clear() {
        count = 0;
    }

    inline auto begin() {
        return data.begin();
    }

    inline auto end() {
        return data.begin() + count;
    }
};

struct Patch {
    int32_t x;
    int32_t y;
    float radius;
    float quantity;
};

struct PatchArray {
    std::array<Patch, c_max_candidate_count> data;
    size_t count = 0;

    inline void insert(int32_t x, int32_t y, float radius, float quantity) {
        assert(count < data.size());
        data[count] = {x, y, radius, quantity};
        count++;
    }

    inline void clear() {
        count = 0;
    }

    inline auto begin() {
        return data.begin();
    }

    inline auto end() {
        return data.begin() + count;
    }
};

template<int NbCandidates, float SuggestedDistance, int32_t RegionSize, int NbSpots>
class SpotNoiseCache {
public:
    SpotNoiseCache(const MapGenSettings& settings) {
        for (int32_t i = 0; i < RegionSize; i++) {
            for (int32_t j = 0; j < RegionSize; j++) {
                int32_t x = i - (RegionSize / 2);
                int32_t y = j - (RegionSize / 2);

                Random::random_penalty_between<NbCandidates>(
                    _penalties[i + j*RegionSize], x, y, c_random_spot_size_minimum, c_random_spot_size_maximum, 1
                );
            }
        }

        for (PatchType type = IRON; type < NB_PATCH_TYPE; type++) {
            _base_regular_densities[type] = base_regular_density(settings, type);
            _base_regular_quantities[type] = base_regular_quantity(settings, type);
        }
    }

    static constexpr float base_regular_density(const MapGenSettings& settings, const PatchType type) {
        /*
        size_effective_distance_at = if(has_starting_area_placement == -1, distance, distance - regular_patch_fade_in_distance)

        regular_density_at(distance) = "base_density * frequency_multiplier * size_multiplier * \z
                        if(has_starting_area_placement == -1, 1, clamp((distance - starting_resource_placement_radius) / regular_patch_fade_in_distance, 0, 1)) * \z
                        (1 + clamp(size_effective_distance_at(distance) / double_density_distance, 0, 1))"
        */

        return c_base_densities[type] * settings.frequencies[type] * settings.sizes[type];
    }

    static constexpr float base_regular_quantity(const MapGenSettings& settings, const PatchType type) {
        /*
        regular_spot_quantity_base_at = "1000000 / base_spots_per_km2 / frequency_multiplier * regular_density_at(distance)"

        regular_spot_quantity_expression = "random_penalty_between(random_spot_size_minimum, random_spot_size_maximum, 1) * \z
                                            regular_spot_quantity_base_at(distance)"
        */

        return 1000000.f / c_base_spots_per_km2 / settings.frequencies[type];
    }

    inline const auto& get_penalties() const { return _penalties; }
    inline const auto& get_base_regular_densities() const { return _base_regular_densities; }
    inline const auto& get_base_regular_quantities() const { return _base_regular_quantities; }
    inline auto& get_chunks() { return _chunks; }
    inline uint32_t get_id() { return ++_id; }

    static constexpr int32_t c_chunk_size = (int32_t)std::ceil(SuggestedDistance);
    static constexpr int32_t c_chunk_count = RegionSize / c_chunk_size + 1;
    static constexpr int nb_spots = NbSpots;
    
private:
    std::array<std::array<float, NbCandidates>, RegionSize*RegionSize> _penalties;
    std::array<float, NB_PATCH_TYPE> _base_regular_densities;
    std::array<float, NB_PATCH_TYPE> _base_regular_quantities;

    std::array<PseudoCandidateArray<NbCandidates>, c_chunk_count*c_chunk_count> _chunks;
    uint32_t _id = 0;
};

using StarterSpotNoiseCache = SpotNoiseCache<
    c_nb_starter_spot_to_generate,
    c_starter_suggested_distance,
    c_starter_region_size,
    32
>;

using RegularSpotNoiseCache = SpotNoiseCache<
    c_nb_regular_spot_to_generate,
    c_regular_suggested_distance,
    c_regular_region_size,
    22
>;

using Patches = std::array<PatchArray, NB_PATCH_TYPE>;

Patches starter_patches(const MapGenSettings& settings, StarterSpotNoiseCache& cache, uint32_t seed0);
Patches regular_patches(const MapGenSettings& settings, RegularSpotNoiseCache& cache, uint32_t seed0);