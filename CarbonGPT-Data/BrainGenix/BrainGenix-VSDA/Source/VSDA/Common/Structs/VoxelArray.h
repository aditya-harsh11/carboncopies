//=================================//
// This file is part of BrainGenix //
//=================================//

#pragma once

// Standard Libraries
#include <inttypes.h>
#include <memory>
#include <atomic>
#include <cmath>
#include <stdexcept>

// SYCL
#include <sycl/sycl.hpp>

// Internal Libraries
#include <VSDA/Common/Structs/BoundingBox.h>
#include <VSDA/Common/Structs/ScanRegion.h>

#include <VSDA/EM/Structs/VoxelEM.h>
#include <VSDA/CA/Structs/VoxelCA.h>

// Device-friendly voxel array descriptor
template<typename VoxelType>
struct VoxelArrayDesc {
    VoxelType* Data;
    uint64_t SizeX;
    uint64_t SizeY;
    uint64_t SizeZ;
    uint64_t MaxLength;
    float VoxelScale_um;
    float BBoxMinX;
    float BBoxMinY;
    float BBoxMinZ;
    float BBoxMaxX;
    float BBoxMaxY;
    float BBoxMaxZ;
};

// SYCL device functions for voxel access
template<typename VoxelType>
SYCL_EXTERNAL uint64_t SYCLGetVoxelIndex(const VoxelArrayDesc<VoxelType>& desc, int X, int Y, int Z) {
    return Z * desc.SizeY * desc.SizeX + Y * desc.SizeX + X;
}

template<typename VoxelType>
SYCL_EXTERNAL VoxelType& SYCLGetVoxelAtIndex(const VoxelArrayDesc<VoxelType>& desc, uint64_t index) {
    return desc.Data[index];
}

template<typename VoxelType>
SYCL_EXTERNAL VoxelType& SYCLGetVoxelAtCoord(const VoxelArrayDesc<VoxelType>& desc, int X, int Y, int Z) {
    return desc.Data[SYCLGetVoxelIndex(desc, X, Y, Z)];
}

template<typename VoxelType>
SYCL_EXTERNAL int SYCLGetXIndexAtPosition(const VoxelArrayDesc<VoxelType>& desc, float X_Worldspace_um) {
    return std::round((X_Worldspace_um - desc.BBoxMinX) / desc.VoxelScale_um);
}

template<typename VoxelType>
SYCL_EXTERNAL int SYCLGetYIndexAtPosition(const VoxelArrayDesc<VoxelType>& desc, float Y_Worldspace_um) {
    return std::round((Y_Worldspace_um - desc.BBoxMinY) / desc.VoxelScale_um);
}

template<typename VoxelType>
SYCL_EXTERNAL int SYCLGetZIndexAtPosition(const VoxelArrayDesc<VoxelType>& desc, float Z_Worldspace_um) {
    return std::round((Z_Worldspace_um - desc.BBoxMinZ) / desc.VoxelScale_um);
}

/**
 * @brief Host-side Voxel Array management class
 */
template<typename VoxelType>
class VoxelArray {
private:
    sycl::queue* deviceQueue_ = nullptr;
    VoxelType* deviceData_ = nullptr;
    
    uint64_t SizeX_ = 0;
    uint64_t SizeY_ = 0;
    uint64_t SizeZ_ = 0;
    uint64_t DataMaxLength_ = 0;
    
    float VoxelScale_um = 0.0f;
    BoundingBox BoundingBox_;
    VoxelArrayDesc<VoxelType> deviceDesc_;

    // Private helper methods
    uint64_t GetIndex(int X, int Y, int Z) const {
        return Z * SizeY_ * SizeX_ + Y * SizeX_ + X;
    }

    void UpdateDeviceDesc();

public:
    /**
     * @brief Construct a new Voxel Array object
     */
    VoxelArray(sycl::queue* queue, BoundingBox BB, float voxelScaleUm);
    VoxelArray(sycl::queue* queue, ScanRegion region, float voxelScaleUm);
    
    /**
     * @brief Destroy the Voxel Array object
     */
    ~VoxelArray();

    // Disallow copying
    VoxelArray(const VoxelArray&) = delete;
    VoxelArray& operator=(const VoxelArray&) = delete;

    // Memory management
    void ClearArray();
    void ClearArrayAsync();

    // Getters
    VoxelType* getDeviceData() const { return deviceData_; }
    const VoxelArrayDesc<VoxelType>& getDeviceDesc() const { return deviceDesc_; }
    void getDimensions(uint64_t& x, uint64_t& y, uint64_t& z) const;
    
    // Coordinate conversion
    int GetXIndexAtPosition(float X_Worldspace_um) const;
    int GetYIndexAtPosition(float Y_Worldspace_um) const;
    int GetZIndexAtPosition(float Z_Worldspace_um) const;
    
    float GetXPositionAtIndex(int XIndex) const;
    float GetYPositionAtIndex(int YIndex) const;
    float GetZPositionAtIndex(int ZIndex) const;

    // Bounding box operations
    bool SetBB(BoundingBox newBoundingBox);
    bool SetBB(ScanRegion newBoundingBox);
    BoundingBox GetBoundingBox() const;

    // Range checking
    bool IsInRangeX(float X) const;
    bool IsInRangeY(float Y) const;
    bool IsInRangeZ(float Z) const;

    // Size operations
    uint64_t GetSize() const;
    int GetX() const;
    int GetY() const;
    int GetZ() const;
    float GetResolution() const;
};

// Include template implementation
#include "VoxelArray.tpp"