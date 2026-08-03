#include <VSDA/Common/Structs/BoundingBox.h>

#include <iostream>
#include <cmath>


SYCL_EXTERNAL std::string BoundingBox::Dimensions() const {
    float sizeX, sizeY, sizeZ;
    GetDimensions(sizeX, sizeY, sizeZ);
    
    return std::to_string(sizeX) + "X, " +
           std::to_string(sizeY) + "Y, " +
           std::to_string(sizeZ) + "Z";
}

SYCL_EXTERNAL std::string BoundingBox::ToString() const {
    return std::to_string(bb_point1[0]) + "X1, " +
           std::to_string(bb_point1[1]) + "Y1, " +
           std::to_string(bb_point1[2]) + "Z1 | " +
           std::to_string(bb_point2[0]) + "X2, " +
           std::to_string(bb_point2[1]) + "Y2, " +
           std::to_string(bb_point2[2]) + "Z2";
}

SYCL_EXTERNAL bool BoundingBox::IsIntersecting(const BoundingBox& otherBox) const {
    float xMax1 = std::max(bb_point1[0], bb_point2[0]);
    float yMax1 = std::max(bb_point1[1], bb_point2[1]);
    float zMax1 = std::max(bb_point1[2], bb_point2[2]);
    float xMax2 = std::max(otherBox.bb_point1[0], otherBox.bb_point2[0]);
    float yMax2 = std::max(otherBox.bb_point1[1], otherBox.bb_point2[1]);
    float zMax2 = std::max(otherBox.bb_point1[2], otherBox.bb_point2[2]);

    float xMin1 = std::min(bb_point1[0], bb_point2[0]);
    float yMin1 = std::min(bb_point1[1], bb_point2[1]);
    float zMin1 = std::min(bb_point1[2], bb_point2[2]);
    float xMin2 = std::min(otherBox.bb_point1[0], otherBox.bb_point2[0]);
    float yMin2 = std::min(otherBox.bb_point1[1], otherBox.bb_point2[1]);
    float zMin2 = std::min(otherBox.bb_point1[2], otherBox.bb_point2[2]);

    return IsOverlapping1D(xMax1, xMin1, xMax2, xMin2) &&
           IsOverlapping1D(yMax1, yMin1, yMax2, yMin2) &&
           IsOverlapping1D(zMax1, zMin1, zMax2, zMin2);
}

SYCL_EXTERNAL uint64_t BoundingBox::GetVoxelSize(float voxelScaleUm) const {
    float sizeX, sizeY, sizeZ;
    GetDimensions(sizeX, sizeY, sizeZ);
    
    uint64_t sizeX_vox = static_cast<uint64_t>(sizeX / voxelScaleUm);
    uint64_t sizeY_vox = static_cast<uint64_t>(sizeY / voxelScaleUm);
    uint64_t sizeZ_vox = static_cast<uint64_t>(sizeZ / voxelScaleUm);

    return sizeX_vox * sizeY_vox * sizeZ_vox;
}

SYCL_EXTERNAL bool BoundingBox::ContainsPoint(float x, float y, float z) const {
    float minX = std::min(bb_point1[0], bb_point2[0]);
    float maxX = std::max(bb_point1[0], bb_point2[0]);
    float minY = std::min(bb_point1[1], bb_point2[1]);
    float maxY = std::max(bb_point1[1], bb_point2[1]);
    float minZ = std::min(bb_point1[2], bb_point2[2]);
    float maxZ = std::max(bb_point1[2], bb_point2[2]);

    return (x >= minX && x <= maxX) &&
           (y >= minY && y <= maxY) &&
           (z >= minZ && z <= maxZ);
}

SYCL_EXTERNAL void BoundingBox::GetDimensions(float& sizeX, float& sizeY, float& sizeZ) const {
    sizeX = std::abs(bb_point2[0] - bb_point1[0]);
    sizeY = std::abs(bb_point2[1] - bb_point1[1]);
    sizeZ = std::abs(bb_point2[2] - bb_point1[2]);
}

SYCL_EXTERNAL void BoundingBox::GetCenter(float& centerX, float& centerY, float& centerZ) const {
    centerX = (bb_point1[0] + bb_point2[0]) * 0.5f;
    centerY = (bb_point1[1] + bb_point2[1]) * 0.5f;
    centerZ = (bb_point1[2] + bb_point2[2]) * 0.5f;
}

SYCL_EXTERNAL bool BoundingBox::IsOverlapping1D(float max1, float min1, float max2, float min2) {
    return (max1 >= min2) && (max2 >= min1);
}