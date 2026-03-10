#define _USE_MATH_DEFINES
#include "stages.hpp"
#include "algorithm.hpp"

constexpr int16_t MAX_MIXER_COPPER_LINE_WIDTH = 30;
constexpr int16_t MAX_MIXER_IRON_LINE_WIDTH = 30;
constexpr float MIN_MALL_IRON_AREA = 50.f * 50 + 28.f * 21 + 20;
constexpr int32_t MAX_COAL_WATER_DISTANCE = 30;
constexpr float MAX_OIL_DISTANCE = 200.f;
constexpr float MIN_NEAR_WATER_COAL_RADIUS = 12.f;
constexpr float MIN_MIXERS = 10.f;
const float MIN_MIXERS_IRON = std::ceil(MIN_MIXERS / 2.f);

constexpr int32_t NOISE_SAMPLING_DISTANCE = 3;

Finder<SeedCache>::EvalResult stage1_eval(
    const MapGenSettings&, const NoisePrecompute& precompute, NoiseCache& noise_cache, uint32_t seed, SeedCache* seed_cache
) {
    Patches patches = regular_patches(precompute, noise_cache, seed, { 0, 0 });

    float best_score = 0.f;

    for (Direction direction = EAST; direction < NB_DIRECTIONS; direction = next_direction(direction)) {
        BoxI16 mixer_copper_box = BoxI16(150, -90, 300, 90).rotated(direction);
        std::pair<float, BoxI16> copper_pair = count_ore_lines(patches[COPPER], mixer_copper_box, MAX_MIXER_COPPER_LINE_WIDTH, direction);
        float copper_score = copper_pair.first;
        BoxI16 copper_bounding_box = copper_pair.second;
        
        if (copper_score < MIN_MIXERS) continue;

        auto check_perpendicular = [&](Direction perpendicular) -> std::tuple<float, BoxI16, BoxI16> {
            Direction perpendicular_rotated = perpendicular - direction;
            BoxI16 copper_bounding_box_rotated = copper_bounding_box.rotated_counter_clockwise(direction);
            int16_t copper_distance_from_spawn = copper_bounding_box_rotated.side_position(WEST);

            int16_t y1_align = perpendicular_rotated == NORTH ? copper_bounding_box_rotated.left_top.y - 50 : copper_bounding_box_rotated.right_bottom.y;
            int16_t y2_align = perpendicular_rotated == NORTH ? copper_bounding_box_rotated.left_top.y : copper_bounding_box_rotated.right_bottom.y + 50;
            BoxI16 mixer_iron_box_align = BoxI16(
                copper_distance_from_spawn + 10, y1_align,
                copper_distance_from_spawn + 54, y2_align
            ).rotated(direction);

            int16_t y1_knight_move = perpendicular_rotated == NORTH ? copper_bounding_box_rotated.left_top.y - 100 : copper_bounding_box_rotated.right_bottom.y + 65;
            int16_t y2_knight_move = perpendicular_rotated == NORTH ? copper_bounding_box_rotated.left_top.y - 65 : copper_bounding_box_rotated.right_bottom.y + 100;
            BoxI16 mixer_iron_box_knight_move = BoxI16(
                copper_distance_from_spawn - 120, y1_knight_move,
                copper_distance_from_spawn, y2_knight_move
            ).rotated(direction);

            int16_t y1_coal = perpendicular_rotated == NORTH ? copper_bounding_box_rotated.left_top.y - 100 : copper_bounding_box_rotated.right_bottom.y;
            int16_t y2_coal = perpendicular_rotated == NORTH ? copper_bounding_box_rotated.left_top.y : copper_bounding_box_rotated.right_bottom.y + 100;
            BoxI16 mixer_coal_box = BoxI16(
                copper_distance_from_spawn - 120, y1_coal,
                copper_distance_from_spawn + 54, y2_coal
            ).rotated(direction);

            std::pair<float, BoxI16> align_pair = count_ore_lines(patches[IRON], mixer_iron_box_align, MAX_MIXER_IRON_LINE_WIDTH, direction);
            std::pair<float, BoxI16> knight_move_pair = count_ore_lines(patches[IRON], mixer_iron_box_knight_move, MAX_MIXER_IRON_LINE_WIDTH, perpendicular);

            BoxI16 coal_box;
            bool any_coal = false;
            for (const auto& p : patches[COAL]) {
                if (mixer_coal_box.contains(p.pos) && p.radius >= 13.f) {
                    any_coal = true;
                    coal_box = BoxI16(p.pos, (int16_t)p.radius);
                }
            }

            if (!any_coal) return { 0.f, BoxI16(), BoxI16() };

            if (align_pair.first > knight_move_pair.first) {
                return { align_pair.first, align_pair.second, coal_box };
            } else {
                return { knight_move_pair.first, knight_move_pair.second, coal_box };
            }
        };

        Direction perpendicular = direction;
        perpendicular++;

        auto iron_tuple = check_perpendicular(perpendicular);
        auto iron_tuple2 = check_perpendicular(-perpendicular);
        if (std::get<0>(iron_tuple2) > std::get<0>(iron_tuple)) {
            iron_tuple = iron_tuple2;
        }

        if (std::get<0>(iron_tuple) < MIN_MIXERS_IRON) continue;

        float score = std::min(copper_score, MIN_MIXERS + 2.f) + std::min(std::get<0>(iron_tuple), MIN_MIXERS_IRON + 1.f);

        if (best_score < score) {
            best_score = score;
            seed_cache->mixer_direction = direction;
            seed_cache->mixer_copper_bounding_box = copper_bounding_box;
            seed_cache->mixer_iron_bounding_box = std::get<1>(iron_tuple);
            seed_cache->mixer_coal_bounding_box = std::get<2>(iron_tuple);
        }
    }

    PositionI32 lake = starter_lake_position(seed);
    PositionI32 estimated_mall_pos = -lake + PositionI32(seed_cache->mixer_direction) * 40;

    bool any_oil = false;
    for (const auto& p : patches[OIL]) {
        if (PositionI32::distance_2(p.pos, estimated_mall_pos) < MAX_OIL_DISTANCE*MAX_OIL_DISTANCE) {
            any_oil = true;
            break;
        }
    }

    return { !any_oil || best_score == 0.f, best_score };
}

Finder<SeedCache>::EvalResult stage2_eval(
    const MapGenSettings& settings, const NoisePrecompute& precompute, NoiseCache& noise_cache, uint32_t seed, SeedCache* seed_cache
) {
    Noise noise(seed, true, false);
    Patches patches = starter_patches(settings, precompute, noise, noise_cache, seed, { 0, 0 });

    Direction direction = seed_cache->mixer_direction;
    BoxI16 rotated_box = seed_cache->mixer_copper_bounding_box.rotated_counter_clockwise(direction);
    PositionI16 center = rotated_box.center();
    int16_t copper_distance_from_spawn = rotated_box.side_position(WEST);

    BoxI16 iron_box = BoxI16(-172 + copper_distance_from_spawn, center.y - 60, -152 + copper_distance_from_spawn, center.y + 60).rotated(direction);

    BoxI16 iron_bounding_box;
    PatchArray valid_irons;
    for (const auto& p : patches[IRON]) {
        if (iron_box.contains(p.pos)) {
            valid_irons.insert(p.pos, p.radius + 4.f, p.quantity);
            iron_bounding_box = BoxI16::combine(iron_bounding_box, BoxI16(p.pos, (int16_t)p.radius));
        }
    }

    float iron_area = union_area(valid_irons);
    seed_cache->iron_area = iron_area;
    seed_cache->mall_iron_bounding_box = iron_bounding_box;

    return { iron_area < MIN_MALL_IRON_AREA, iron_area };
}

Finder<SeedCache>::EvalResult stage3_eval(
    const MapGenSettings& settings, const NoisePrecompute& precompute, NoiseCache& noise_cache, uint32_t seed, SeedCache* seed_cache
) {
    Noise noise(seed, true, settings.elevation_type == ELEVATION_2_0 ? true : false);
    Patches s_patches = starter_patches(settings, precompute, noise, noise_cache, seed, { 0, 0 });
    Patches r_patches = regular_patches(precompute, noise_cache, seed, { 0, 0 });

    Direction direction = seed_cache->mixer_direction;

    float coal_water_distance = INFINITY;
    for (int i = 0; i < s_patches[COAL].size(); i++) {
        const auto& p = s_patches[COAL][i];

        if (p.radius < MIN_NEAR_WATER_COAL_RADIUS) {
            bool connected = false;
            for (int j = 0; j < s_patches[COAL].size(); j++) {
                const auto& p2 = s_patches[COAL][j];
                float r = p.radius + p2.radius + 25;
                if (i != j && PositionI32::distance_2(p.pos, p2.pos) < r*r) {
                    connected = true;
                    break;
                }
            }

            if (!connected) continue;
        }

        BoxI32 box(p.pos, MAX_COAL_WATER_DISTANCE);
        for (int32_t x = box.left_top.x; x < box.right_bottom.x; x += NOISE_SAMPLING_DISTANCE) {
            for (int32_t y = box.left_top.y; y < box.right_bottom.y; y += NOISE_SAMPLING_DISTANCE) {
                PositionI32 pos(x, y);
                if (noise.is_tile_water(settings, precompute, pos)) {
                    coal_water_distance = std::min(coal_water_distance, PositionI32::distance(p.pos, pos) - p.radius);
                }
            }
        }
    }

    BoxI16 modified_mall_iron_box = seed_cache->mall_iron_bounding_box.rotated_counter_clockwise(direction);
    modified_mall_iron_box.right_bottom.x += 120;
    modified_mall_iron_box = modified_mall_iron_box.rotated(direction);

    PositionI16 mall_iron_box_center = modified_mall_iron_box.center();
    float oil_distance = INFINITY;
    for (const auto& p : r_patches[OIL]) {
        if (noise.elevation(settings, precompute, p.pos) > -10) {
            oil_distance = std::min(oil_distance, PositionI32::distance(p.pos, mall_iron_box_center));
        }
    }

    float score = (
        seed_cache->iron_area / MIN_MALL_IRON_AREA -
        std::abs(coal_water_distance) / MAX_COAL_WATER_DISTANCE
    );

    return {
        oil_distance > MAX_OIL_DISTANCE ||
        coal_water_distance > MAX_COAL_WATER_DISTANCE ||
        noise.any_water_in_box(settings, precompute, seed_cache->mixer_copper_bounding_box, (int16_t)NOISE_SAMPLING_DISTANCE) ||
        noise.any_water_in_box(settings, precompute, seed_cache->mixer_iron_bounding_box, (int16_t)NOISE_SAMPLING_DISTANCE) ||
        noise.any_water_in_box(settings, precompute, seed_cache->mixer_coal_bounding_box, (int16_t)NOISE_SAMPLING_DISTANCE) ||
        noise.any_water_in_box(settings, precompute, modified_mall_iron_box, (int16_t)NOISE_SAMPLING_DISTANCE),
        score
    };
}