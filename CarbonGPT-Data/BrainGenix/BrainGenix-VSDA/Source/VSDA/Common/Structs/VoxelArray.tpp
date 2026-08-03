template<typename VoxelType>
VoxelArray<VoxelType>::VoxelArray(sycl::queue* queue, BoundingBox BB, float voxelScaleUm)
    : deviceQueue_(queue), VoxelScale_um(voxelScaleUm), BoundingBox_(BB) {
    
    // Calculate dimensions using Geometry types
    float sizeX = std::abs(BB.bb_point1[0] - BB.bb_point2[0]);
    float sizeY = std::abs(BB.bb_point1[1] - BB.bb_point2[1]);
    float sizeZ = std::abs(BB.bb_point1[2] - BB.bb_point2[2]);
    
    SizeX_ = static_cast<uint64_t>(sizeX / voxelScaleUm);
    SizeY_ = static_cast<uint64_t>(sizeY / voxelScaleUm);
    SizeZ_ = static_cast<uint64_t>(sizeZ / voxelScaleUm);
    DataMaxLength_ = SizeX_ * SizeY_ * SizeZ_;

    // Only allocate device memory if we have a non-zero size
    if (DataMaxLength_ > 0) {
        deviceData_ = sycl::malloc_device<VoxelType>(DataMaxLength_, *deviceQueue_);
        
        if (deviceData_ == nullptr) {
            throw std::runtime_error("Failed to allocate device memory");
        }

        // Initialize memory and wait for completion
        ClearArray();
    } else {
        deviceData_ = nullptr;
    }

    // Update device descriptor (even for zero-size arrays)
    UpdateDeviceDesc();
}

template<typename VoxelType>
VoxelArray<VoxelType>::VoxelArray(sycl::queue* queue, ScanRegion region, float voxelScaleUm)
    : deviceQueue_(queue), VoxelScale_um(voxelScaleUm) {
    
    // Convert ScanRegion to BoundingBox using Geometry types
    BoundingBox_.bb_point1[0] = region.Point1X_um;
    BoundingBox_.bb_point1[1] = region.Point1Y_um;
    BoundingBox_.bb_point1[2] = region.Point1Z_um;
    BoundingBox_.bb_point2[0] = region.Point2X_um;
    BoundingBox_.bb_point2[1] = region.Point2Y_um;
    BoundingBox_.bb_point2[2] = region.Point2Z_um;

    // Calculate dimensions
    float sizeX = std::abs(BoundingBox_.bb_point1[0] - BoundingBox_.bb_point2[0]);
    float sizeY = std::abs(BoundingBox_.bb_point1[1] - BoundingBox_.bb_point2[1]);
    float sizeZ = std::abs(BoundingBox_.bb_point1[2] - BoundingBox_.bb_point2[2]);
    
    SizeX_ = static_cast<uint64_t>(sizeX / voxelScaleUm);
    SizeY_ = static_cast<uint64_t>(sizeY / voxelScaleUm);
    SizeZ_ = static_cast<uint64_t>(sizeZ / voxelScaleUm);
    DataMaxLength_ = SizeX_ * SizeY_ * SizeZ_;

    // Only allocate device memory if we have a non-zero size
    if (DataMaxLength_ > 0) {
        deviceData_ = sycl::malloc_device<VoxelType>(DataMaxLength_, *deviceQueue_);
        
        if (deviceData_ == nullptr) {
            throw std::runtime_error("Failed to allocate device memory");
        }

        // Initialize memory and wait for completion
        ClearArray();
    } else {
        deviceData_ = nullptr;
    }

    // Update device descriptor (even for zero-size arrays)
    UpdateDeviceDesc();
}

template<typename VoxelType>
VoxelArray<VoxelType>::~VoxelArray() {
    if (deviceData_) {
        sycl::free(deviceData_, *deviceQueue_);
    }
}

template<typename VoxelType>
void VoxelArray<VoxelType>::ClearArray() {
    if (deviceData_ == nullptr) return; // Skip for zero-size arrays
    
    VoxelType zeroVal{};
    deviceQueue_->submit([&](sycl::handler& h) {
        h.fill(deviceData_, zeroVal, DataMaxLength_);
    });
    deviceQueue_->wait(); // Wait for completion
}

template<typename VoxelType>
void VoxelArray<VoxelType>::ClearArrayAsync() {
    if (deviceData_ == nullptr) return; // Skip for zero-size arrays
    
    VoxelType zeroVal{};
    deviceQueue_->submit([&](sycl::handler& h) {
        h.fill(deviceData_, zeroVal, DataMaxLength_);
    });
}

template<typename VoxelType>
void VoxelArray<VoxelType>::UpdateDeviceDesc() {
    deviceDesc_.Data = deviceData_;
    deviceDesc_.SizeX = SizeX_;
    deviceDesc_.SizeY = SizeY_;
    deviceDesc_.SizeZ = SizeZ_;
    deviceDesc_.MaxLength = DataMaxLength_;
    deviceDesc_.VoxelScale_um = VoxelScale_um;
    deviceDesc_.BBoxMinX = BoundingBox_.bb_point1[0];
    deviceDesc_.BBoxMinY = BoundingBox_.bb_point1[1];
    deviceDesc_.BBoxMinZ = BoundingBox_.bb_point1[2];
    deviceDesc_.BBoxMaxX = BoundingBox_.bb_point2[0];
    deviceDesc_.BBoxMaxY = BoundingBox_.bb_point2[1];
    deviceDesc_.BBoxMaxZ = BoundingBox_.bb_point2[2];
}

template<typename VoxelType>
void VoxelArray<VoxelType>::getDimensions(uint64_t& x, uint64_t& y, uint64_t& z) const {
    x = SizeX_;
    y = SizeY_;
    z = SizeZ_;
}

template<typename VoxelType>
int VoxelArray<VoxelType>::GetXIndexAtPosition(float X_Worldspace_um) const {
    return std::round((X_Worldspace_um - BoundingBox_.bb_point1[0]) / VoxelScale_um);
}

template<typename VoxelType>
int VoxelArray<VoxelType>::GetYIndexAtPosition(float Y_Worldspace_um) const {
    return std::round((Y_Worldspace_um - BoundingBox_.bb_point1[1]) / VoxelScale_um);
}

template<typename VoxelType>
int VoxelArray<VoxelType>::GetZIndexAtPosition(float Z_Worldspace_um) const {
    return std::round((Z_Worldspace_um - BoundingBox_.bb_point1[2]) / VoxelScale_um);
}

template<typename VoxelType>
float VoxelArray<VoxelType>::GetXPositionAtIndex(int XIndex) const {
    return BoundingBox_.bb_point1[0] + XIndex * VoxelScale_um;
}

template<typename VoxelType>
float VoxelArray<VoxelType>::GetYPositionAtIndex(int YIndex) const {
    return BoundingBox_.bb_point1[1] + YIndex * VoxelScale_um;
}

template<typename VoxelType>
float VoxelArray<VoxelType>::GetZPositionAtIndex(int ZIndex) const {
    return BoundingBox_.bb_point1[2] + ZIndex * VoxelScale_um;
}

template<typename VoxelType>
bool VoxelArray<VoxelType>::SetBB(BoundingBox newBoundingBox) {
    // Check if new size matches current allocation
    float sizeX = std::abs(newBoundingBox.bb_point1[0] - newBoundingBox.bb_point2[0]);
    float sizeY = std::abs(newBoundingBox.bb_point1[1] - newBoundingBox.bb_point2[1]);
    float sizeZ = std::abs(newBoundingBox.bb_point1[2] - newBoundingBox.bb_point2[2]);
    
    uint64_t newSize = static_cast<uint64_t>(sizeX / VoxelScale_um) *
                        static_cast<uint64_t>(sizeY / VoxelScale_um) *
                        static_cast<uint64_t>(sizeZ / VoxelScale_um);
    
    if (newSize > DataMaxLength_) {
        return false; // Cannot resize beyond allocated memory
    }
    
    BoundingBox_ = newBoundingBox;
    
    // Update dimensions
    SizeX_ = static_cast<uint64_t>(sizeX / VoxelScale_um);
    SizeY_ = static_cast<uint64_t>(sizeY / VoxelScale_um);
    SizeZ_ = static_cast<uint64_t>(sizeZ / VoxelScale_um);
    
    // Update device descriptor
    UpdateDeviceDesc();
    
    return true;
}

template<typename VoxelType>
bool VoxelArray<VoxelType>::SetBB(ScanRegion newBoundingBox) {
    BoundingBox bb;
    bb.bb_point1[0] = newBoundingBox.Point1X_um;
    bb.bb_point1[1] = newBoundingBox.Point1Y_um;
    bb.bb_point1[2] = newBoundingBox.Point1Z_um;
    bb.bb_point2[0] = newBoundingBox.Point2X_um;
    bb.bb_point2[1] = newBoundingBox.Point2Y_um;
    bb.bb_point2[2] = newBoundingBox.Point2Z_um;
    
    return SetBB(bb);
}

template<typename VoxelType>
BoundingBox VoxelArray<VoxelType>::GetBoundingBox() const {
    return BoundingBox_;
}

template<typename VoxelType>
bool VoxelArray<VoxelType>::IsInRangeX(float X) const {
    return X >= BoundingBox_.bb_point1[0] && X <= BoundingBox_.bb_point2[0];
}

template<typename VoxelType>
bool VoxelArray<VoxelType>::IsInRangeY(float Y) const {
    return Y >= BoundingBox_.bb_point1[1] && Y <= BoundingBox_.bb_point2[1];
}

template<typename VoxelType>
bool VoxelArray<VoxelType>::IsInRangeZ(float Z) const {
    return Z >= BoundingBox_.bb_point1[2] && Z <= BoundingBox_.bb_point2[2];
}

template<typename VoxelType>
uint64_t VoxelArray<VoxelType>::GetSize() const {
    return DataMaxLength_;
}

template<typename VoxelType>
int VoxelArray<VoxelType>::GetX() const {
    return static_cast<int>(SizeX_);
}

template<typename VoxelType>
int VoxelArray<VoxelType>::GetY() const {
    return static_cast<int>(SizeY_);
}

template<typename VoxelType>
int VoxelArray<VoxelType>::GetZ() const {
    return static_cast<int>(SizeZ_);
}

template<typename VoxelType>
float VoxelArray<VoxelType>::GetResolution() const {
    return VoxelScale_um;
}