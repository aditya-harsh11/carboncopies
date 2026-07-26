#pragma once

#include <inttypes.h>


enum VoxelState : uint8_t {
    VoxelState_EMPTY = 0,
    VoxelState_INTERIOR = 1,
    VoxelState_BORDER = 2,
    VoxelState_BLACK = 3,
    VoxelState_WHITE = 4,
    VoxelState_OUT_OF_BOUNDS = 5
};

struct VoxelEM {
    VoxelState State_;
    uint8_t DistanceToEdge_vox_;
    uint64_t ParentUID;
};
