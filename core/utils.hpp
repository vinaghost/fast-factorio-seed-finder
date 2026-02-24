#pragma once

#include <array>
#include <cstdint>
#include <cmath>

enum PatchType {
    IRON,
    COPPER,
    COAL,
    STONE,
    NB_PATCH_TYPE
};

constexpr PatchType& operator++(PatchType& type) {
    return type = (PatchType)(1 + ((int)type));
}

constexpr PatchType operator++(PatchType& type, int) {
    PatchType tmp(type);
    ++type;
    return tmp;
}

struct MapGenSettings {
    std::array<float, NB_PATCH_TYPE> frequencies;
    std::array<float, NB_PATCH_TYPE> sizes;
    std::array<float, NB_PATCH_TYPE> richness;
};