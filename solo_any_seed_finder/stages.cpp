#define _USE_MATH_DEFINES
#include "stages.hpp"
#include "algorithm.hpp"

constexpr BoxI32 COPPER_SEARCH_BOX(150, -90, 300, 90);

constexpr int32_t MIN_EDGE_MALL_MIXER_PATCHES_DISTANCE = 155;
constexpr int32_t MAX_EDGE_MALL_MIXER_PATCHES_DISTANCE = MIN_EDGE_MALL_MIXER_PATCHES_DISTANCE + 25;
constexpr int32_t MAX_MALL_MIXER_PATCHES_SHIFT = 30;
constexpr int32_t MAX_MIXER_COPPER_LINE_WIDTH = 30;
constexpr int32_t MAX_MIXER_IRON_LINE_WIDTH = 30;

constexpr int32_t MAX_COAL_WATER_DISTANCE = 50;
constexpr float MIN_NEAR_WATER_COAL_RADIUS = 12.f;
constexpr float MIN_MIXERS = 10.f;
const float MIN_MIXERS_IRON = std::ceil(MIN_MIXERS / 2.f);

constexpr std::array<std::tuple<ResourceType, BoxI32, float>, 6> MALL_BOXES{
    std::make_tuple(COPPER, BoxI32(-42, -40, -20, -32), 1.f), // main copper
    std::make_tuple(IRON,   BoxI32(-45, -25, -20, -21), 1.f), // iron burners
    std::make_tuple(IRON,   BoxI32(-45, -11,   0,  -6), 0.5f), // 1st snake
    std::make_tuple(IRON,   BoxI32(-45,   6,   0,  12), 0.5f), // 2nd snake
    std::make_tuple(IRON,   BoxI32(-45,  23,   0,  35), 0.5f), // 3rd snake + steel
    std::make_tuple(IRON,   BoxI32(-43,  47, -23,  74), 0.5f), // gear/pipe iron
};
constexpr int32_t MAX_COAL_BURNER_DISTANCE = 30;
constexpr std::array<PositionI32, 2> OIL_POSITIONS{
    PositionI32(10, 45),
    PositionI32(10, -45)
};
constexpr int32_t MAX_OIL_DISTANCE = 100;
constexpr BoxI32 BASE_BOX(0, -40, MAX_EDGE_MALL_MIXER_PATCHES_DISTANCE, 70);

constexpr int32_t MALL_OFFSET_SAMPLING_DISTANCE = 3;
constexpr int32_t MALL_AREA_COMPUTE_SAMPLING_DISTANCE1 = 3;
constexpr int32_t MALL_AREA_COMPUTE_SAMPLING_DISTANCE2 = 4;
constexpr int32_t NOISE_SAMPLING_DISTANCE = 3;
constexpr int32_t MAX_WATER_BURNER_DISTANCE = 75;

constexpr float MALL_PATCHES_WEIGHT = 1.f;
constexpr float BURNERS_WEIGHT = 0.25f;
constexpr float COAL_WATER_DISTANCE_WEIGHT = 0.15f;
constexpr float OIL_WEIGHT = 0.15f;

Finder<SeedCache>::EvalResult stage1_eval(
    const MapGenSettings&, const NoisePrecompute& precompute, NoiseCache& noise_cache, uint32_t seed, SeedCache* seed_cache
) {
    Patches patches = regular_patches(precompute, noise_cache, seed, { 0, 0 });

    float best_score = 0.f;

    for (Direction direction = EAST; direction < NB_DIRECTIONS; direction = next_direction(direction)) {
        BoxI32 mixer_copper_box = COPPER_SEARCH_BOX.rotated(direction);
        std::pair<float, BoxI32> copper_pair = count_ore_lines(patches[COPPER], mixer_copper_box, MAX_MIXER_COPPER_LINE_WIDTH, direction);
        float copper_score = copper_pair.first;
        BoxI32 copper_bounding_box = copper_pair.second;
        
        if (copper_score < MIN_MIXERS) continue;

        auto check_perpendicular = [&](Direction perpendicular) -> std::tuple<float, BoxI32, BoxI32> {
            Direction perpendicular_rotated = perpendicular - direction;
            BoxI32 copper_bounding_box_rotated = copper_bounding_box.rotated_counter_clockwise(direction);
            int32_t copper_distance_from_spawn = copper_bounding_box_rotated.side_position(WEST);

            int32_t y1_align = perpendicular_rotated == NORTH ? copper_bounding_box_rotated.left_top.y - 50 : copper_bounding_box_rotated.right_bottom.y;
            int32_t y2_align = perpendicular_rotated == NORTH ? copper_bounding_box_rotated.left_top.y : copper_bounding_box_rotated.right_bottom.y + 50;
            BoxI32 mixer_iron_box_align = BoxI32(
                copper_distance_from_spawn + 10, y1_align,
                copper_distance_from_spawn + 54, y2_align
            ).rotated(direction);

            int32_t y1_knight_move = perpendicular_rotated == NORTH ? copper_bounding_box_rotated.left_top.y - 100 : copper_bounding_box_rotated.right_bottom.y + 65;
            int32_t y2_knight_move = perpendicular_rotated == NORTH ? copper_bounding_box_rotated.left_top.y - 65 : copper_bounding_box_rotated.right_bottom.y + 100;
            BoxI32 mixer_iron_box_knight_move = BoxI32(
                copper_distance_from_spawn - 120, y1_knight_move,
                copper_distance_from_spawn, y2_knight_move
            ).rotated(direction);

            int32_t y1_coal = perpendicular_rotated == NORTH ? copper_bounding_box_rotated.left_top.y - 100 : copper_bounding_box_rotated.right_bottom.y;
            int32_t y2_coal = perpendicular_rotated == NORTH ? copper_bounding_box_rotated.left_top.y : copper_bounding_box_rotated.right_bottom.y + 100;
            BoxI32 mixer_coal_box = BoxI32(
                copper_distance_from_spawn - 120, y1_coal,
                copper_distance_from_spawn + 54, y2_coal
            ).rotated(direction);

            std::pair<float, BoxI32> align_pair = count_ore_lines(patches[IRON], mixer_iron_box_align, MAX_MIXER_IRON_LINE_WIDTH, direction);
            std::pair<float, BoxI32> knight_move_pair = count_ore_lines(patches[IRON], mixer_iron_box_knight_move, MAX_MIXER_IRON_LINE_WIDTH, perpendicular);

            BoxI32 coal_box;
            bool any_coal = false;
            for (const auto& p : patches[COAL]) {
                if (mixer_coal_box.contains(p.pos) && p.radius >= 13.f) {
                    any_coal = true;
                    coal_box = BoxI32(p.pos, (int32_t)p.radius);
                }
            }

            if (!any_coal) return { 0.f, BoxI32(), BoxI32() };

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

    return { best_score == 0.f, best_score };
}

Finder<SeedCache>::EvalResult stage2_eval(
    const MapGenSettings& settings, const NoisePrecompute& precompute, NoiseCache& noise_cache, uint32_t seed, SeedCache* seed_cache
) {
    Noise noise(seed, true, false);
    Patches s_patches = starter_patches(settings, precompute, noise, noise_cache, seed, { 0, 0 });
    Patches r_patches = regular_patches(precompute, noise_cache, seed, { 0, 0 });

    Direction direction = seed_cache->mixer_direction;
    BoxI32 rotated_box = seed_cache->mixer_copper_bounding_box.rotated_counter_clockwise(direction);
    PositionI32 center = rotated_box.center();
    int32_t copper_distance_from_spawn = rotated_box.side_position(WEST);
    PositionI32 copper_edge_center(copper_distance_from_spawn, center.y);

    float boxes_factors_sum = 0.f;
    for (const auto& t : MALL_BOXES) {
        boxes_factors_sum += std::get<float>(t);
    }

    PositionI32 best_mall_boxes_offset;
    float best_score = 0;
    auto check_boxes = [&](bool flipped) {
        for (int32_t x = MIN_EDGE_MALL_MIXER_PATCHES_DISTANCE; x < MAX_EDGE_MALL_MIXER_PATCHES_DISTANCE; x += MALL_OFFSET_SAMPLING_DISTANCE) {
            for (int32_t y = -MAX_MALL_MIXER_PATCHES_SHIFT; y < MAX_MALL_MIXER_PATCHES_SHIFT; y += MALL_OFFSET_SAMPLING_DISTANCE) {
                PositionI32 mall_boxes_offset = copper_edge_center - PositionI32(x, y);
                float score = 0;

                BoxI32 base_burner_box = flipped ? std::get<BoxI32>(MALL_BOXES[1]).flipped(EAST) : std::get<BoxI32>(MALL_BOXES[1]);
                BoxI32 burner_box = (base_burner_box + mall_boxes_offset).rotated(direction);
                int32_t coal_distance_2 = INT32_MAX;
                for (const auto& p : s_patches[COAL]) {
                    coal_distance_2 = std::min(coal_distance_2, burner_box.distance_2(p.pos) - (int32_t)p.radius);
                }

                if (coal_distance_2 > MAX_COAL_BURNER_DISTANCE*MAX_COAL_BURNER_DISTANCE) continue;
                float coal_distance = std::sqrt((float)coal_distance_2);
                score -= BURNERS_WEIGHT * coal_distance / MAX_COAL_BURNER_DISTANCE;

                int32_t best_oil_distance = INT32_MAX;
                for (auto oil_pos : OIL_POSITIONS) {
                    auto oil_pos_rotated = (oil_pos + mall_boxes_offset).rotated(direction);
                    for (const auto& p : r_patches[OIL]) {
                        best_oil_distance = std::min(best_oil_distance, PositionI32::manhattan_distance(p.pos, oil_pos_rotated) - (int32_t)p.radius);
                    }
                }

                if (best_oil_distance - 50 > MAX_OIL_DISTANCE) continue;

                for (const auto& tuple : MALL_BOXES) {
                    BoxI32 base_box = flipped ? std::get<BoxI32>(tuple).flipped(EAST) : std::get<BoxI32>(tuple);
                    BoxI32 box = (base_box + mall_boxes_offset).rotated(direction);
                    float good_area = area_covered_by_patch(s_patches[std::get<ResourceType>(tuple)], box, MALL_AREA_COMPUTE_SAMPLING_DISTANCE1);
                    if (good_area <= 0.f) {
                        score = -INFINITY;
                        break;
                    }
                    
                    for (ResourceType resource = IRON; resource < NB_RESOURCE_TYPE; resource++) {
                        if (resource == std::get<ResourceType>(tuple)) continue;
                        good_area -= 0.6f * area_covered_by_patch(s_patches[resource], box, MALL_AREA_COMPUTE_SAMPLING_DISTANCE2);
                    }
                    if (good_area <= 0.f) {
                        score = -INFINITY;
                        break;
                    }

                    score += good_area * std::get<float>(tuple);
                }

                score /= MALL_PATCHES_WEIGHT * boxes_factors_sum;
                
                if (best_score < score) {
                    best_score = score;
                    best_mall_boxes_offset = mall_boxes_offset;
                }
            }
        }
    };

    check_boxes(false);
    check_boxes(true);

    seed_cache->mall_boxes_offset = best_mall_boxes_offset;
    seed_cache->mall_score = best_score;
    return { best_score < 0.3f, best_score };
}

Finder<SeedCache>::EvalResult stage3_eval(
    const MapGenSettings& settings, const NoisePrecompute& precompute, NoiseCache& noise_cache, uint32_t seed, SeedCache* seed_cache
) {
    Noise noise(seed, true, settings.elevation_type == ELEVATION_2_0 ? true : false);
    Patches s_patches = starter_patches(settings, precompute, noise, noise_cache, seed, { 0, 0 });
    Patches r_patches = regular_patches(precompute, noise_cache, seed, { 0, 0 });

    Direction direction = seed_cache->mixer_direction;

    BoxI32 burner_box = (std::get<BoxI32>(MALL_BOXES[1]) + seed_cache->mall_boxes_offset).rotated(direction);

    float coal_water_distance = INFINITY;
    for (int i = 0; i < s_patches[COAL].size(); i++) {
        const auto& p = s_patches[COAL][i];

        if (burner_box.distance_2(p.pos) - (int32_t)p.radius > MAX_WATER_BURNER_DISTANCE*MAX_WATER_BURNER_DISTANCE) continue;

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

    int32_t best_oil_distance = INT32_MAX;
    for (auto oil_pos : OIL_POSITIONS) {
        auto oil_pos_rotated = (oil_pos + seed_cache->mall_boxes_offset).rotated(direction);
        for (const auto& p : r_patches[OIL]) {
            if (noise.elevation(settings, precompute, p.pos) > -10) {
                best_oil_distance = std::min(best_oil_distance, PositionI32::manhattan_distance(p.pos, oil_pos_rotated) - (int32_t)p.radius);
            }
        }
    }

    BoxI32 estimated_base_box = (BASE_BOX + seed_cache->mall_boxes_offset).rotated(direction);

    float score = (
        seed_cache->mall_score -
        COAL_WATER_DISTANCE_WEIGHT * std::abs(coal_water_distance) / MAX_COAL_WATER_DISTANCE -
        OIL_WEIGHT * std::clamp((float)(best_oil_distance - 50) / MAX_OIL_DISTANCE, 0.f, 1.f)
    );

    return {
        best_oil_distance - 50 > MAX_OIL_DISTANCE ||
        coal_water_distance > MAX_COAL_WATER_DISTANCE ||
        noise.any_water_in_box(settings, precompute, seed_cache->mixer_copper_bounding_box, NOISE_SAMPLING_DISTANCE) ||
        noise.any_water_in_box(settings, precompute, seed_cache->mixer_iron_bounding_box, NOISE_SAMPLING_DISTANCE) ||
        noise.any_water_in_box(settings, precompute, seed_cache->mixer_iron_bounding_box, NOISE_SAMPLING_DISTANCE) ||
        noise.any_water_in_box(settings, precompute, estimated_base_box, NOISE_SAMPLING_DISTANCE),
        score
    };
}