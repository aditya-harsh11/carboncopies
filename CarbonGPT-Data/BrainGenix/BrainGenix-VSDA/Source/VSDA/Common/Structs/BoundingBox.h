//=================================================================//
// This file is part of the BrainGenix-NES Neuron Emulation System //
//=================================================================//

#pragma once

// Standard Libraries
#include <cstdint>
#include <string>
#include <algorithm>

// SYCL
#include <sycl/sycl.hpp>

/**
 * @brief Defines a rectangular bounding box between the two points given.
 * Note that point 1 should be smaller than point 2.
 */
struct BoundingBox {
    float bb_point1[3];
    float bb_point2[3];

    /**
     * @brief Prints the dimensions of the box in X,Y,Z
     */
    SYCL_EXTERNAL std::string Dimensions() const;

    /**
     * @brief Prints the two points of the bounding box, X1,Y1,Z1, X2,Y2,Z2
     */
    SYCL_EXTERNAL std::string ToString() const;

    /**
     * @brief Helper that checks if this box intersects with another.
     */
    SYCL_EXTERNAL bool IsIntersecting(const BoundingBox& otherBox) const;

    /**
     * @brief Calculates the number of voxels that would fit in this bounding box
     */
    SYCL_EXTERNAL uint64_t GetVoxelSize(float voxelScaleUm) const;

    /**
     * @brief SYCL-compatible 1D overlap check
     */
    SYCL_EXTERNAL static bool IsOverlapping1D(float max1, float min1, float max2, float min2);

    /**
     * @brief Get the center point of the bounding box
     */
    SYCL_EXTERNAL void GetCenter(float& centerX, float& centerY, float& centerZ) const;
    
    /**
     * @brief Get the dimensions of the bounding box
     */
    SYCL_EXTERNAL void GetDimensions(float& sizeX, float& sizeY, float& sizeZ) const;

    /**
     * @brief Check if a point is inside the bounding box
     */
    SYCL_EXTERNAL bool ContainsPoint(float x, float y, float z) const;

};