#include "algorithm.hpp"

const float UNION_FACTOR = 4.f * std::sqrtf(2) / 3.f;
float union_area(const PatchArray& patches) {
    float sum = 0;

    for (const auto& p : patches) {
        sum += (float)M_PI * p.radius*p.radius;
    }

    for (size_t i = 0; i < patches.size(); i++) {
        const auto& p1 = patches[i];
        for (size_t j = i + 1; j < patches.size(); j++) {
            const auto& p2 = patches[j];
            float rr = p1.radius + p2.radius;

            int32_t distance_2 = PositionI32::distance_2(p1.pos, p2.pos);
            if (distance_2 > rr*rr) continue;
            
            float distance = std::sqrt((float)distance_2);
            float w = rr - distance;
            float union_ = UNION_FACTOR * std::sqrt(w*w*w * p1.radius * p2.radius / rr);
            sum -= union_;
        }
    }

    return sum;
}

float union_area(const Patches& patches, bool include_oil) {
    float sum = 0;

    iterate_patches(patches, [&](const Patch& p, ResourceType type, size_t) {
        if (include_oil || type != OIL) {
            sum += (float)M_PI * p.radius*p.radius;
        }
    });

    iterate_patches(patches, [&](const Patch& p1, ResourceType type1, size_t i) {
        if (!include_oil && type1 == OIL) return;
        iterate_patches(patches, [&](const Patch& p2, ResourceType type2, size_t j) {
            if (j <= i || (!include_oil && type2 == OIL)) return;

            float rr = p1.radius + p2.radius;

            int32_t distance_2 = PositionI32::distance_2(p1.pos, p2.pos);
            if (distance_2 > rr*rr) return;
            
            float distance = std::sqrt((float)distance_2);
            float w = rr - distance;
            float union_ = UNION_FACTOR * std::sqrt(w*w*w * p1.radius * p2.radius / rr);
            sum -= union_;
        });
    });

    return sum;
}

bool is_point_in_patch(const PatchArray& patches, PositionI32 position) {
    for (const auto& p : patches) {
        if (PositionI32::distance_2(p.pos, position) <= p.radius*p.radius) return true;
    }

    return false;
}

float area_covered_by_patch(const PatchArray& patches, BoxI32 box, int32_t sampling_distance) {
    int32_t inside = 0;
    int32_t total = 0;

    for (int32_t x = box.left_top.x; x <= box.right_bottom.x; x += sampling_distance) {
        for (int32_t y = box.left_top.y; y <= box.right_bottom.y; y += sampling_distance) {
            if (is_point_in_patch(patches, { x, y })) inside++;
            total++;
        }
    }

    return (float)inside / total;
}

std::pair<float, BoxI32> count_ore_lines(const PatchArray& patches, BoxI32 box, int32_t max_width, Direction belt_direction) {
    int32_t max_width_2 = max_width / 2;
    PatchArray valid_coppers;
    for (const auto& p : patches) {
        if (box.contains(p.pos)) valid_coppers.insert(p);
    }

    float best_count = 0.f;
    BoxI32 best_bounding_box;
    for (size_t i = 0; i < valid_coppers.size(); i++) {
        float count = 0.f;
        BoxI32 bounding_box = BoxI32(INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN);

        const auto& p1 = valid_coppers[i];
        for (size_t j = i; j < valid_coppers.size(); j++) {
            const auto& p2 = valid_coppers[j];
            int32_t forward1, forward2;
            if (belt_direction == EAST || belt_direction == WEST) {
                forward1 = p1.pos.x;
                forward2 = p2.pos.x;
            } else {
                forward1 = p1.pos.y;
                forward2 = p2.pos.y;
            }

            if (std::abs(forward1 - forward2) < max_width_2) {
                float scaled_radius = p2.radius + 4;
                if (scaled_radius <= 22.5) continue;

                count += 2.f / 7 * std::sqrt(scaled_radius*scaled_radius - 22.5f*22.5f);
                bounding_box = BoxI32::combined(bounding_box, BoxI32(p2.pos, (int32_t)p2.radius));
            }
        }   

        if (best_count < count) {
            best_count = count;
            best_bounding_box = bounding_box;
        }
    }

    return { best_count, best_bounding_box };
}