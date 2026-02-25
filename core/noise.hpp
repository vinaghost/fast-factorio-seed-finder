#pragma once

#include "utils.hpp"
#include "constants.hpp"
#include "random.hpp"
#include <algorithm>
#include <cassert>

class NoisePrecompute {
public:
    NoisePrecompute(const MapGenSettings& settings) {
        for (int32_t i = 0; i < MAX_REGION_SIZE; i++) {
            for (int32_t j = 0; j < MAX_REGION_SIZE; j++) {
                int32_t x = i - (MAX_REGION_SIZE / 2);
                int32_t y = j - (MAX_REGION_SIZE / 2);

                Random::random_penalty_between<MAX_NB_SPOTS>(
                    _penalties[i + j*MAX_REGION_SIZE], x, y, RANDOM_SPOT_SIZE_MINIMUM, RANDOM_SPOT_SIZE_MAXIMUM, 1
                );
            }
        }

        for (ResourceType type = IRON; type < NB_RESOURCE_TYPE; type++) {
            _base_regular_densities[type] = base_regular_density(settings, type);
            _base_regular_quantities[type] = base_regular_quantity(settings, type);
        }
    }

    static constexpr float base_regular_density(const MapGenSettings& settings, const ResourceType type) {
        /*
        size_effective_distance_at = if(has_starting_area_placement == -1, distance, distance - regular_patch_fade_in_distance)

        regular_density_at(distance) = "base_density * frequency_multiplier * size_multiplier * \z
                        if(has_starting_area_placement == -1, 1, clamp((distance - starting_resource_placement_radius) / regular_patch_fade_in_distance, 0, 1)) * \z
                        (1 + clamp(size_effective_distance_at(distance) / double_density_distance, 0, 1))"
        */

        return BASE_DENSITIES[type] * settings.frequencies[type] * settings.sizes[type];
    }

    static constexpr float base_regular_quantity(const MapGenSettings& settings, const ResourceType type) {
        /*
        regular_spot_quantity_base_at = "1000000 / base_spots_per_km2 / frequency_multiplier * regular_density_at(distance)"

        regular_spot_quantity_expression = "random_penalty_between(random_spot_size_minimum, random_spot_size_maximum, 1) * \z
                                            regular_spot_quantity_base_at(distance)"
        */

        return 1000000.f / BASE_SPOTS_PER_KM2 / settings.frequencies[type];
    }

    inline const auto& get_penalties() const { return _penalties; }
    inline const auto& get_base_regular_densities() const { return _base_regular_densities; }
    inline const auto& get_base_regular_quantities() const { return _base_regular_quantities; }

private:
    std::array<std::array<float, MAX_NB_SPOTS>, MAX_REGION_SIZE*MAX_REGION_SIZE> _penalties;
    std::array<float, NB_RESOURCE_TYPE> _base_regular_densities;
    std::array<float, NB_RESOURCE_TYPE> _base_regular_quantities;
};

class Noise {
public:
    Noise(uint32_t seed0);

    float noise(uint8_t seed1, float x, float y, float input_scale, float output_scale, float offset_x, float offset_y);
    float multioctave_noise(uint8_t seed1, float x, float y, float persistence, float octaves,
        float input_scale, float output_scale, float offset_x, float offset_y);
    float persistence_multioctave_noise(uint8_t seed1, float x, float y, float persistence, float octaves,
        float input_scale, float output_scale, float offset_x, float offset_y);
    float amplitude_corrected_multioctave_noise(uint8_t seed1, float x, float y, float persistence, float octaves,
        float input_scale, float output_scale, float offset_x, float offset_y);
    
    float finish_elevation(const MapGenSettings&, const NoisePrecompute&, float elevation, float x, float y);
    float elevation_lakes(const MapGenSettings&, const NoisePrecompute&, float x, float y);

private:
    std::pair<float, float> _gradient(uint8_t p1, uint8_t x, uint8_t y);

    std::array<uint8_t, 256> _p1;
    std::array<uint8_t, 256> _p2;
    std::array<uint8_t, 256> _p3;
    std::array<std::pair<float, float>, 256> _grad;
};

/*
elevation_lakes = finish_elevation{elevation = make_0_12like_lakes{x = x,\z
                                                                   y = y,\z
                                                                   bias = 20,\z
                                                                   terrain_octaves = 8,\z
                                                                   segmentation_multiplier = segmentation_multiplier},\z
                                   segmentation_multiplier = segmentation_multiplier}

finish_elevation = min((elevation - water_level) / segmentation_multiplier,\z
                      basis_noise{x = x, y = y, seed0 = map_seed, seed1 = 123, input_scale = 1/8, output_scale = 1.5} + \z
                      starting_lake_distance / 4 - 4,\z
                      -1 + (starting_lake_distance + starting_lake_noise) / 16,\z
                      max(2, 2 + starting_lake_distance / 16 + starting_lake_noise / 2))

{
    type = "noise-expression",
    name = "water_level",
    expression = "10 * log2(control:water:size)"
},

starting_lake_distance = "distance_from_nearest_point{x = x, y = y, points = starting_lake_positions, maximum_distance = 1024}",
starting_lake_noise = "quick_multioctave_noise_persistence{x = x,\z
                                                            y = y,\z
                                                            seed0 = map_seed,\z
                                                            seed1 = 14,\z
                                                            input_scale = 1/8,\z
                                                            output_scale = 1,\z
                                                            octaves = 5,\z
                                                            octave_input_scale_multiplier = 0.5,\z
                                                            persistence = 0.75}"

  {
    type = "noise-function",
    name = "quick_multioctave_noise_persistence",
    parameters = {"x", "y", "seed0", "seed1", "input_scale", "output_scale", "octaves", "octave_input_scale_multiplier", "persistence"},
    expression = "quick_multioctave_noise{x = x,\z
                                          y = y,\z
                                          seed0 = seed0,\z
                                          seed1 = seed1,\z
                                          input_scale = input_scale * octave_input_scale_multiplier ^ (octaves - 1),\z
                                          output_scale = output_scale * 2 ^ (octaves - 1),\z
                                          octaves = octaves,\z
                                          octave_output_scale_multiplier = persistence,\z
                                          octave_input_scale_multiplier = 1 / octave_input_scale_multiplier}"
  },

  {
    type = "noise-function",
    name = "make_0_12like_lakes",
    parameters = {"x", "y", "bias", "terrain_octaves", "segmentation_multiplier"},
    expression = "max(bias + variable_persistence_multioctave_noise{x = x,\z
                                                                    y = y,\z
                                                                    seed0 = map_seed,\z
                                                                    seed1 = 1,\z
                                                                    input_scale = input_scale,\z
                                                                    output_scale = 0.125,\z
                                                                    offset_x = offset_x,\z
                                                                    octaves = terrain_octaves,\z
                                                                    persistence = persistence},\z
                      20 + water_level - 0.1 * segmentation_multiplier * distance + \z
                      variable_persistence_multioctave_noise{x = x,\z
                                                             y = y,\z
                                                             seed0 = map_seed,\z
                                                             seed1 = 2,\z
                                                             input_scale = input_scale,\z
                                                             output_scale = 0.125,\z
                                                             offset_x = offset_x,\z
                                                             octaves = 6,\z
                                                             persistence = persistence})",
    local_expressions =
    {
      input_scale = "segmentation_multiplier / 2",
      offset_x = "10000 / segmentation_multiplier",
      persistence = "clamp(amplitude_corrected_multioctave_noise{x = x,\z
                                                                 y = y,\z
                                                                 seed0 = map_seed,\z
                                                                 seed1 = 1,\z
                                                                 octaves = terrain_octaves - 2,\z
                                                                 input_scale = input_scale,\z
                                                                 offset_x = offset_x,\z
                                                                 persistence = 0.7,\z
                                                                 amplitude = 0.5} + 0.3,\z
                          0.1, 0.9)"
    }

  {
    type = "noise-function",
    name = "amplitude_corrected_multioctave_noise",
    parameters = {"x", "y", "seed0", "seed1", "input_scale", "offset_x", "octaves", "persistence", "amplitude"},
    expression = "variable_persistence_multioctave_noise{x = x,\z
                                                         y = y,\z
                                                         seed0 = seed0,\z
                                                         seed1 = seed1,\z
                                                         input_scale = input_scale,\z
                                                         output_scale = (1 - persistence) / 2 ^ octaves / (1 - persistence ^ octaves) * amplitude,\z
                                                         offset_x = offset_x,\z
                                                         octaves = octaves,\z
                                                         persistence = persistence}"
  },
*/

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
    std::array<Patch, MAX_NB_SPOTS> data;
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

// NOT thread safe.
class NoiseCache {
public:
    template<int32_t NbCandidates, int32_t ChunkCount>
    using Chunks = std::array<PseudoCandidateArray<NbCandidates>, ChunkCount*ChunkCount>;

    inline uint32_t get_id() { return ++_id; }
    template<PatchType Type>
    inline auto& get_chunks() {
        if constexpr (Type == STARTER) return _starter_chunks;
        else return _regular_chunks;
    }

private:
    uint32_t _id = 0;
    Chunks<NB_CANDIDATES[STARTER], CHUNK_COUNTS[STARTER]> _starter_chunks;
    Chunks<NB_CANDIDATES[REGULAR], CHUNK_COUNTS[REGULAR]> _regular_chunks;
};

using Patches = std::array<PatchArray, NB_RESOURCE_TYPE>;

Patches starter_patches(const NoisePrecompute&, NoiseCache&, uint32_t seed0);
Patches regular_patches(const NoisePrecompute&, NoiseCache&, uint32_t seed0);