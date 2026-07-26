#pragma once

#include <cstddef>

struct VoxelCA {
    size_t CompartmentID_ = 0;
    bool IsFilled_ = false;
    bool IsBorder_ = false;
};
