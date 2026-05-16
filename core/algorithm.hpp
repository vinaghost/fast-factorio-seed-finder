#pragma once

#define _USE_MATH_DEFINES
#include "noise.hpp"
#include "bitset"
#include <print>

// -/+ 20% approximation
float union_area(const PatchArray& patches);
float union_area(const Patches& patches, bool include_oil = false);

bool is_point_in_patch(const PatchArray&, PositionI32 position);

// Returns a number between 0 and 1;
float area_covered_by_patch(const PatchArray&, BoxI32, int32_t sampling_distance);

// Will overestimate of patches are overlapping
std::pair<float, BoxI32> count_ore_lines(const PatchArray& patches, BoxI32 box, int32_t max_width, Direction belt_direction);

struct ResourceBox {
    ResourceType type;
    BoxI32 box;
    float min;
    float weight;
};

template<size_t NbBoxes, std::array<ResourceBox, NbBoxes> Boxes, BoxI32 MainOffsets>
std::pair<PositionI32, float> evaluate_boxes(const Patches& patches, const Patches* patches2, PositionI32 offset, Direction direction, bool flipped) {
    static_assert(NbBoxes > 0);

    for (const ResourceBox& r_box : Boxes) {
        BoxI32 extended_box = r_box.box;
        extended_box.left_top += MainOffsets.left_top;
        extended_box.right_bottom += MainOffsets.right_bottom;

        extended_box = extended_box.rotated(direction).flipped(direction, flipped) + offset;
        for (const auto& p : patches[r_box.type]) {
            if (extended_box.distance_2(p.pos) < p.radius*p.radius) goto found;
        }
        if (patches2) {
            for (const auto& p : (*patches2)[r_box.type]) {
                if (extended_box.distance_2(p.pos) < p.radius*p.radius) goto found;
            }
        }
        return { PositionI32(0, 0), 0.f };

        found:;
    }

    auto get_search_box = [] consteval {
        BoxI32 search_box = Boxes[0].box;
        for (auto it = Boxes.begin() + 1; it != Boxes.end(); it++) {
            search_box = BoxI32::combined(search_box, it->box);
        }
        search_box.left_top += MainOffsets.left_top;
        search_box.right_bottom += MainOffsets.right_bottom;
        return search_box;
    };

    constexpr BoxI32 search_box = get_search_box();
    constexpr PositionI32 search_box_size = search_box.size();
    std::array<std::array<std::bitset<search_box_size.x>, search_box_size.y>, NB_RESOURCE_TYPE> ore_bits;
    BoxI32 r_search_box = search_box.rotated(direction).flipped(direction, flipped) + offset;

    for (ResourceType type = IRON; type < NB_RESOURCE_TYPE; type++) {
        for (const auto& p : patches[type]) {
            int32_t r = (int32_t)p.radius;
            for (int32_t iy = -r; iy <= r; iy++) {
                int32_t y = p.pos.y + iy;

                int32_t dx = (int32_t)std::sqrt(p.radius*p.radius - iy*iy);
                for (int32_t ix = -dx; ix <= dx; ix++) {
                    int32_t x = p.pos.x + ix;
                    PositionI32 pos(x, y);
                    if (!r_search_box.contains(pos)) continue;

                    PositionI32 idx = (pos - offset).flipped(direction, flipped).rotated_counter_clockwise(direction) - search_box.left_top;
                    ore_bits[type][idx.y].set(idx.x);
                }
            }
        }

        if (patches2) {
            for (const auto& p : (*patches2)[type]) {
                int32_t r = (int32_t)p.radius;
                for (int32_t iy = -r; iy <= r; iy++) {
                    int32_t y = p.pos.y + iy;

                    int32_t dx = (int32_t)std::sqrt(p.radius*p.radius - iy*iy);
                    for (int32_t ix = -dx; ix <= dx; ix++) {
                        int32_t x = p.pos.x + ix;
                        PositionI32 pos(x, y);
                        if (!r_search_box.contains(pos)) continue;

                        PositionI32 idx = (pos - offset).flipped(direction, flipped).rotated_counter_clockwise(direction) - search_box.left_top;
                        ore_bits[type][idx.y].set(idx.x);
                    }
                }
            }
        }
    }

    std::array<std::array<std::bitset<search_box_size.x>, search_box_size.y>, NB_RESOURCE_TYPE> inv_ore_bits;
    for (ResourceType type = IRON; type < NB_RESOURCE_TYPE; type++) {
        for (int y = 0; y < search_box_size.y; y++) {
            inv_ore_bits[type][y] = ore_bits[((int)type + 1) % 4][y] | ore_bits[((int)type + 2) % 4][y] | ore_bits[((int)type + 3) % 4][y];
        }
    }

    std::array<std::bitset<search_box_size.x>, NbBoxes> masks;
    for (size_t i = 0; i < NbBoxes; i++) {
        for (int32_t x = Boxes[i].box.left_top.x; x < Boxes[i].box.right_bottom.x; x++) {
            masks[i].set(MainOffsets.left_top.x - search_box.left_top.x + x);
        }
    }

    float best = 0.f;
    PositionI32 best_offset;

    for (int32_t x = MainOffsets.left_top.x; x < MainOffsets.right_bottom.x; x++) {
        std::array<int32_t, NbBoxes> counts{0};
        for (size_t i = 0; i < NbBoxes; i++) {
            const ResourceBox& box = Boxes[i];
            for (int32_t y = box.box.left_top.y; y < box.box.right_bottom.y; y++) {
                size_t y_idx = MainOffsets.left_top.y - search_box.left_top.y + y;
                counts[i] += (int32_t)(ore_bits[box.type][y_idx] & masks[i]).count();
                counts[i] -= (int32_t)(inv_ore_bits[box.type][y_idx] & masks[i]).count() / 2;
            }
        }

        for (int32_t y = MainOffsets.left_top.y; y < MainOffsets.right_bottom.y; y++) {
            float score = 0.f;
            for (size_t i = 0; i < NbBoxes; i++) {
                float surface = (float)counts[i] / Boxes[i].box.area();
                assert(surface <= 1.f);
                if (surface < Boxes[i].min) {
                    score = 0.f;
                    break;
                }
                score += surface * Boxes[i].weight;
            }
            if (best < score) {
                best = score;
                best_offset = PositionI32(x, y);
            }

            if (y + 1 >= MainOffsets.right_bottom.y) break;
            for (size_t i = 0; i < NbBoxes; i++) {
                const ResourceBox& box = Boxes[i];
                size_t y_idx_base = -search_box.left_top.y + y;
                counts[i] -= (int32_t)(ore_bits[box.type][box.box.left_top.y + y_idx_base] & masks[i]).count();
                counts[i] += (int32_t)(inv_ore_bits[box.type][box.box.left_top.y + y_idx_base] & masks[i]).count() / 2;
                counts[i] += (int32_t)(ore_bits[box.type][box.box.right_bottom.y + y_idx_base] & masks[i]).count();
                counts[i] -= (int32_t)(inv_ore_bits[box.type][box.box.right_bottom.y + y_idx_base] & masks[i]).count() / 2;
            }
        }

        if (x + 1 >= MainOffsets.right_bottom.x) break;
        for (size_t i = 0; i < NbBoxes; i++) {
            masks[i].reset(Boxes[i].box.left_top.x - search_box.left_top.x + x);
            masks[i].set(Boxes[i].box.right_bottom.x - search_box.left_top.x + x);
        }
    }

    return std::make_pair(best_offset.rotated(direction).flipped(direction, flipped) + offset, best);
}