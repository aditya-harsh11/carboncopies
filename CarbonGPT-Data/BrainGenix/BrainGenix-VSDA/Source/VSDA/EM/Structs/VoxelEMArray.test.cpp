#include <gtest/gtest.h>
#include <sycl/sycl.hpp>
#include <vector>
#include <algorithm>
#include <random>

#include <VSDA/Common/Structs/VoxelArray.h>
#include <VSDA/EM/Structs/VoxelEM.h>

#include <SYCLTestFixture.h>

// Test fixture for VoxelArray<VoxelEM>
class VoxelEMArrayTest : public SYCLTestFixture {
protected:
    void SetUp() override {
        // Call parent SetUp to initialize the queue
        SYCLTestFixture::SetUp();
        
        if (!queue) {
            GTEST_SKIP() << "No SYCL queue available";
        }
        
        // Create test bounding box
        testBB.bb_point1[0] = 0.0f;
        testBB.bb_point1[1] = 0.0f;
        testBB.bb_point1[2] = 0.0f;
        testBB.bb_point2[0] = 100.0f;
        testBB.bb_point2[1] = 100.0f;
        testBB.bb_point2[2] = 100.0f;
        
        // Create voxel array using the queue from parent
        voxelArray = new VoxelArray<VoxelEM>(queue.get(), testBB, 1.0f);
    }
    
    void TearDown() override {
        delete voxelArray;
        // Note: queue is automatically managed by unique_ptr in parent
    }
    
    BoundingBox testBB;
    VoxelArray<VoxelEM>* voxelArray;
};

TEST_F(VoxelEMArrayTest, ConstructorAndBasicProperties) {
    EXPECT_EQ(voxelArray->GetX(), 100);
    EXPECT_EQ(voxelArray->GetY(), 100);
    EXPECT_EQ(voxelArray->GetZ(), 100);
    EXPECT_EQ(voxelArray->GetSize(), 100 * 100 * 100);
    EXPECT_FLOAT_EQ(voxelArray->GetResolution(), 1.0f);
    
    // Test device descriptor
    auto desc = voxelArray->getDeviceDesc();
    EXPECT_EQ(desc.SizeX, 100);
    EXPECT_EQ(desc.SizeY, 100);
    EXPECT_EQ(desc.SizeZ, 100);
    EXPECT_EQ(desc.MaxLength, 100 * 100 * 100);
    EXPECT_FLOAT_EQ(desc.VoxelScale_um, 1.0f);
    EXPECT_FLOAT_EQ(desc.BBoxMinX, 0.0f);
    EXPECT_FLOAT_EQ(desc.BBoxMinY, 0.0f);
    EXPECT_FLOAT_EQ(desc.BBoxMinZ, 0.0f);
    EXPECT_FLOAT_EQ(desc.BBoxMaxX, 100.0f);
    EXPECT_FLOAT_EQ(desc.BBoxMaxY, 100.0f);
    EXPECT_FLOAT_EQ(desc.BBoxMaxZ, 100.0f);
    EXPECT_NE(desc.Data, nullptr);
}

TEST_F(VoxelEMArrayTest, ConstructorWithScanRegion) {
    ScanRegion scanRegion;
    scanRegion.Point1X_um = 10.0f;
    scanRegion.Point1Y_um = 20.0f;
    scanRegion.Point1Z_um = 30.0f;
    scanRegion.Point2X_um = 110.0f;
    scanRegion.Point2Y_um = 120.0f;
    scanRegion.Point2Z_um = 130.0f;
    
    VoxelArray<VoxelEM> scanArray(queue.get(), scanRegion, 1.0f);
    
    EXPECT_EQ(scanArray.GetX(), 100);
    EXPECT_EQ(scanArray.GetY(), 100);
    EXPECT_EQ(scanArray.GetZ(), 100);
    EXPECT_FLOAT_EQ(scanArray.GetResolution(), 1.0f);
    
    auto bbox = scanArray.GetBoundingBox();
    EXPECT_FLOAT_EQ(bbox.bb_point1[0], 10.0f);
    EXPECT_FLOAT_EQ(bbox.bb_point1[1], 20.0f);
    EXPECT_FLOAT_EQ(bbox.bb_point1[2], 30.0f);
    EXPECT_FLOAT_EQ(bbox.bb_point2[0], 110.0f);
    EXPECT_FLOAT_EQ(bbox.bb_point2[1], 120.0f);
    EXPECT_FLOAT_EQ(bbox.bb_point2[2], 130.0f);
}

TEST_F(VoxelEMArrayTest, CoordinateConversion) {
    // Test index to position conversion
    EXPECT_EQ(voxelArray->GetXIndexAtPosition(50.0f), 50);
    EXPECT_EQ(voxelArray->GetYIndexAtPosition(25.0f), 25);
    EXPECT_EQ(voxelArray->GetZIndexAtPosition(75.0f), 75);
    
    // Test position to index conversion
    EXPECT_FLOAT_EQ(voxelArray->GetXPositionAtIndex(10), 10.0f);
    EXPECT_FLOAT_EQ(voxelArray->GetYPositionAtIndex(30), 30.0f);
    EXPECT_FLOAT_EQ(voxelArray->GetZPositionAtIndex(50), 50.0f);
    
    // Test edge cases
    EXPECT_EQ(voxelArray->GetXIndexAtPosition(0.0f), 0);
    EXPECT_EQ(voxelArray->GetXIndexAtPosition(100.0f), 100);
    EXPECT_FLOAT_EQ(voxelArray->GetXPositionAtIndex(0), 0.0f);
    EXPECT_FLOAT_EQ(voxelArray->GetXPositionAtIndex(100), 100.0f);
    
    // Test fractional positions (should round to nearest integer)
    EXPECT_EQ(voxelArray->GetXIndexAtPosition(10.4f), 10);
    EXPECT_EQ(voxelArray->GetXIndexAtPosition(10.5f), 11);
    EXPECT_EQ(voxelArray->GetXIndexAtPosition(10.6f), 11);
}

TEST_F(VoxelEMArrayTest, RangeChecking) {
    EXPECT_TRUE(voxelArray->IsInRangeX(50.0f));
    EXPECT_TRUE(voxelArray->IsInRangeY(25.0f));
    EXPECT_TRUE(voxelArray->IsInRangeZ(75.0f));
    
    EXPECT_FALSE(voxelArray->IsInRangeX(-10.0f));
    EXPECT_FALSE(voxelArray->IsInRangeY(150.0f));
    EXPECT_FALSE(voxelArray->IsInRangeZ(-5.0f));
    
    // Test boundaries
    EXPECT_TRUE(voxelArray->IsInRangeX(0.0f));
    EXPECT_TRUE(voxelArray->IsInRangeX(100.0f));
    EXPECT_TRUE(voxelArray->IsInRangeY(0.0f));
    EXPECT_TRUE(voxelArray->IsInRangeY(100.0f));
    EXPECT_TRUE(voxelArray->IsInRangeZ(0.0f));
    EXPECT_TRUE(voxelArray->IsInRangeZ(100.0f));
}

TEST_F(VoxelEMArrayTest, BoundingBoxOperations) {
    BoundingBox newBB;
    newBB.bb_point1[0] = 0.0f;
    newBB.bb_point1[1] = 0.0f;
    newBB.bb_point1[2] = 0.0f;
    newBB.bb_point2[0] = 50.0f;
    newBB.bb_point2[1] = 50.0f;
    newBB.bb_point2[2] = 50.0f;
    
    EXPECT_TRUE(voxelArray->SetBB(newBB));
    EXPECT_EQ(voxelArray->GetX(), 50);
    EXPECT_EQ(voxelArray->GetY(), 50);
    EXPECT_EQ(voxelArray->GetZ(), 50);
    
    // Test getting bounding box
    BoundingBox retrievedBB = voxelArray->GetBoundingBox();
    EXPECT_FLOAT_EQ(retrievedBB.bb_point1[0], 0.0f);
    EXPECT_FLOAT_EQ(retrievedBB.bb_point1[1], 0.0f);
    EXPECT_FLOAT_EQ(retrievedBB.bb_point1[2], 0.0f);
    EXPECT_FLOAT_EQ(retrievedBB.bb_point2[0], 50.0f);
    EXPECT_FLOAT_EQ(retrievedBB.bb_point2[1], 50.0f);
    EXPECT_FLOAT_EQ(retrievedBB.bb_point2[2], 50.0f);
    
    // Test with ScanRegion
    ScanRegion scanRegion;
    scanRegion.Point1X_um = 10.0f;
    scanRegion.Point1Y_um = 20.0f;
    scanRegion.Point1Z_um = 30.0f;
    scanRegion.Point2X_um = 60.0f;
    scanRegion.Point2Y_um = 70.0f;
    scanRegion.Point2Z_um = 80.0f;
    
    EXPECT_TRUE(voxelArray->SetBB(scanRegion));
    retrievedBB = voxelArray->GetBoundingBox();
    EXPECT_FLOAT_EQ(retrievedBB.bb_point1[0], 10.0f);
    EXPECT_FLOAT_EQ(retrievedBB.bb_point1[1], 20.0f);
    EXPECT_FLOAT_EQ(retrievedBB.bb_point1[2], 30.0f);
    EXPECT_FLOAT_EQ(retrievedBB.bb_point2[0], 60.0f);
    EXPECT_FLOAT_EQ(retrievedBB.bb_point2[1], 70.0f);
    EXPECT_FLOAT_EQ(retrievedBB.bb_point2[2], 80.0f);
}

TEST_F(VoxelEMArrayTest, ClearArray) {
    // First set some data
    std::vector<VoxelEM> initialData(voxelArray->GetSize());
    for (auto& voxel : initialData) {
        voxel.State_ = VoxelState_BORDER;
        voxel.DistanceToEdge_vox_ = 42;
        voxel.ParentUID = 123456789;
    }
    
    queue->memcpy(voxelArray->getDeviceData(), initialData.data(), 
                 voxelArray->GetSize() * sizeof(VoxelEM)).wait();
    
    // Test synchronous clear
    voxelArray->ClearArray();
    
    // Verify all voxels are zeroed
    std::vector<VoxelEM> clearedData(voxelArray->GetSize());
    queue->memcpy(clearedData.data(), voxelArray->getDeviceData(), 
                 voxelArray->GetSize() * sizeof(VoxelEM)).wait();
    
    for (const auto& voxel : clearedData) {
        EXPECT_EQ(voxel.State_, VoxelState_EMPTY);
        EXPECT_EQ(voxel.DistanceToEdge_vox_, 0);
        EXPECT_EQ(voxel.ParentUID, 0);
    }
    
    // Test asynchronous clear
    // First set some data again
    queue->memcpy(voxelArray->getDeviceData(), initialData.data(), 
                 voxelArray->GetSize() * sizeof(VoxelEM)).wait();
    
    voxelArray->ClearArrayAsync();
    queue->wait(); // Wait for completion
    
    // Verify all voxels are zeroed again
    queue->memcpy(clearedData.data(), voxelArray->getDeviceData(), 
                 voxelArray->GetSize() * sizeof(VoxelEM)).wait();
    
    for (const auto& voxel : clearedData) {
        EXPECT_EQ(voxel.State_, VoxelState_EMPTY);
        EXPECT_EQ(voxel.DistanceToEdge_vox_, 0);
        EXPECT_EQ(voxel.ParentUID, 0);
    }
}

TEST_F(VoxelEMArrayTest, GetDimensions) {
    uint64_t x, y, z;
    voxelArray->getDimensions(x, y, z);
    
    EXPECT_EQ(x, 100);
    EXPECT_EQ(y, 100);
    EXPECT_EQ(z, 100);
}

TEST_F(VoxelEMArrayTest, DeviceDescriptorAccess) {
    auto desc = voxelArray->getDeviceDesc();
    
    // Test that all descriptor fields are accessible in device code
    queue->submit([=](sycl::handler& h) {
        h.single_task([=]() {
            // Test that we can access all descriptor fields
            uint64_t sizeX = desc.SizeX;
            uint64_t sizeY = desc.SizeY;
            uint64_t sizeZ = desc.SizeZ;
            uint64_t maxLength = desc.MaxLength;
            float scale = desc.VoxelScale_um;
            float minX = desc.BBoxMinX;
            float minY = desc.BBoxMinY;
            float minZ = desc.BBoxMinZ;
            float maxX = desc.BBoxMaxX;
            float maxY = desc.BBoxMaxY;
            float maxZ = desc.BBoxMaxZ;
            
            // Ensure data pointer is valid
            if (desc.Data != nullptr) {
                // Do nothing, just test access
            }
        });
    });
    
    queue->wait_and_throw();
    SUCCEED();
}

TEST_F(VoxelEMArrayTest, KernelVoxelAccess) {
    const int testX = 10, testY = 20, testZ = 30;
    const uint64_t expectedIndex = testZ * 100 * 100 + testY * 100 + testX;
    
    auto desc = voxelArray->getDeviceDesc();
    
    // Write to specific voxel using kernel
    queue->submit([=](sycl::handler& h) {
        h.single_task([=]() {
            uint64_t index = SYCLGetVoxelIndex(desc, testX, testY, testZ);
            VoxelEM& voxel = SYCLGetVoxelAtIndex(desc, index);
            
            voxel.State_ = VoxelState_BORDER;
            voxel.DistanceToEdge_vox_ = 42;
            voxel.ParentUID = 123456789;
        });
    });
    
    // Read back and verify
    std::vector<VoxelEM> hostData(voxelArray->GetSize());
    queue->memcpy(hostData.data(), voxelArray->getDeviceData(), 
                 voxelArray->GetSize() * sizeof(VoxelEM)).wait();
    
    EXPECT_EQ(hostData[expectedIndex].State_, VoxelState_BORDER);
    EXPECT_EQ(hostData[expectedIndex].DistanceToEdge_vox_, 42);
    EXPECT_EQ(hostData[expectedIndex].ParentUID, 123456789);
}

TEST_F(VoxelEMArrayTest, KernelCoordinateConversion) {
    auto desc = voxelArray->getDeviceDesc();
    const float testX = 25.5f, testY = 35.5f, testZ = 45.5f;
    
    // Store results from device to verify on host
    struct Result {
        int xIndex, yIndex, zIndex;
        VoxelState state;
    };
    Result* deviceResult = sycl::malloc_device<Result>(1, *queue);
    
    queue->submit([=](sycl::handler& h) {
        h.single_task([=]() {
            // Test device coordinate conversion functions
            int xIndex = SYCLGetXIndexAtPosition(desc, testX);
            int yIndex = SYCLGetYIndexAtPosition(desc, testY);
            int zIndex = SYCLGetZIndexAtPosition(desc, testZ);
            
            // Access the voxel
            VoxelEM& voxel = SYCLGetVoxelAtCoord(desc, xIndex, yIndex, zIndex);
            voxel.State_ = VoxelState_INTERIOR;
            
            // Store results for host verification
            deviceResult[0].xIndex = xIndex;
            deviceResult[0].yIndex = yIndex;
            deviceResult[0].zIndex = zIndex;
            deviceResult[0].state = voxel.State_;
        });
    });
    
    // Read back and verify on host
    Result hostResult;
    queue->memcpy(&hostResult, deviceResult, sizeof(Result)).wait();
    
    EXPECT_EQ(hostResult.xIndex, 26); // 25.5 rounds to 26
    EXPECT_EQ(hostResult.yIndex, 36); // 35.5 rounds to 36
    EXPECT_EQ(hostResult.zIndex, 46); // 45.5 rounds to 46
    EXPECT_EQ(hostResult.state, VoxelState_INTERIOR);
    
    sycl::free(deviceResult, *queue);
}

TEST_F(VoxelEMArrayTest, MultipleVoxelAccessPatterns) {
    auto desc = voxelArray->getDeviceDesc();
    const int numVoxels = 1000;
    
    // Test writing to multiple voxels using different access patterns
    queue->submit([=](sycl::handler& h) {
        h.parallel_for(sycl::range<1>(numVoxels), [=](sycl::id<1> idx) {
            size_t i = idx[0]; // Extract index value
            int x = i % desc.SizeX;
            int y = (i / desc.SizeX) % desc.SizeY;
            int z = i / (desc.SizeX * desc.SizeY);
            
            if (x < desc.SizeX && y < desc.SizeY && z < desc.SizeZ) {
                uint64_t index = SYCLGetVoxelIndex(desc, x, y, z);
                VoxelEM& voxel = SYCLGetVoxelAtIndex(desc, index);
                
                voxel.State_ = static_cast<VoxelState>((x + y + z) % 6);
                voxel.DistanceToEdge_vox_ = static_cast<uint8_t>(i % 256);
                voxel.ParentUID = x * 1000000 + y * 1000 + z;
            }
        });
    });
    
    // Read back and verify a sample of voxels
    std::vector<VoxelEM> hostData(voxelArray->GetSize());
    queue->memcpy(hostData.data(), voxelArray->getDeviceData(), 
                 voxelArray->GetSize() * sizeof(VoxelEM)).wait();
    
    // Check a few random voxels to ensure they were set correctly
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, numVoxels - 1);
    
    for (int i = 0; i < 10; i++) {
        int testIdx = dis(gen);
        int x = testIdx % desc.SizeX;
        int y = (testIdx / desc.SizeX) % desc.SizeY;
        int z = testIdx / (desc.SizeX * desc.SizeY);
        
        if (x < desc.SizeX && y < desc.SizeY && z < desc.SizeZ) {
            uint64_t index = z * desc.SizeY * desc.SizeX + y * desc.SizeX + x;
            VoxelState expectedState = static_cast<VoxelState>((x + y + z) % 6);
            uint8_t expectedDistance = static_cast<uint8_t>(testIdx % 256);
            uint64_t expectedParentUID = x * 1000000 + y * 1000 + z;
            
            EXPECT_EQ(hostData[index].State_, expectedState);
            EXPECT_EQ(hostData[index].DistanceToEdge_vox_, expectedDistance);
            EXPECT_EQ(hostData[index].ParentUID, expectedParentUID);
        }
    }
}

TEST_F(VoxelEMArrayTest, OutOfBoundsSafety) {
    auto desc = voxelArray->getDeviceDesc();
    
    // Test accessing out of bounds coordinates (should not crash)
    queue->submit([=](sycl::handler& h) {
        h.single_task([=]() {
            // These should be safe even if out of bounds
            int x = desc.SizeX + 10; // Out of bounds
            int y = desc.SizeY + 5;  // Out of bounds
            int z = desc.SizeZ + 3;  // Out of bounds
            
            // The SYCL functions should handle this gracefully
            uint64_t index = SYCLGetVoxelIndex(desc, x, y, z);
            // Don't actually access the data at this index to avoid crashes
            
            // Test negative indices too
            x = -5;
            y = -10;
            z = -3;
            index = SYCLGetVoxelIndex(desc, x, y, z);
        });
    });
    
    queue->wait_and_throw();
    SUCCEED();
}

TEST_F(VoxelEMArrayTest, AllVoxelStates) {
    auto desc = voxelArray->getDeviceDesc();
    
    // Test setting all possible voxel states
    queue->submit([=](sycl::handler& h) {
        h.single_task([=]() {
            for (int i = 0; i < 6; i++) {
                VoxelEM& voxel = SYCLGetVoxelAtCoord(desc, i, 0, 0);
                voxel.State_ = static_cast<VoxelState>(i);
                voxel.DistanceToEdge_vox_ = i * 10;
                voxel.ParentUID = 1000 + i;
            }
        });
    });
    
    // Read back and verify
    std::vector<VoxelEM> hostData(voxelArray->GetSize());
    queue->memcpy(hostData.data(), voxelArray->getDeviceData(), 
                 voxelArray->GetSize() * sizeof(VoxelEM)).wait();
    
    for (int i = 0; i < 6; i++) {
        uint64_t index = i;
        EXPECT_EQ(hostData[index].State_, static_cast<VoxelState>(i));
        EXPECT_EQ(hostData[index].DistanceToEdge_vox_, i * 10);
        EXPECT_EQ(hostData[index].ParentUID, 1000 + i);
    }
}

TEST_F(VoxelEMArrayTest, LargeCoordinateValues) {
    auto desc = voxelArray->getDeviceDesc();
    
    // Test with very large coordinate values
    queue->submit([=](sycl::handler& h) {
        h.single_task([=]() {
            // Test large positive values
            int x = INT_MAX;
            int y = INT_MAX;
            int z = INT_MAX;
            uint64_t index = SYCLGetVoxelIndex(desc, x, y, z);
            
            // Test large negative values
            x = INT_MIN;
            y = INT_MIN;
            z = INT_MIN;
            index = SYCLGetVoxelIndex(desc, x, y, z);
        });
    });
    
    queue->wait_and_throw();
    SUCCEED();
}

TEST_F(VoxelEMArrayTest, ZeroSizeArray) {
    // Test creating a zero-size array (edge case)
    BoundingBox zeroBB;
    zeroBB.bb_point1[0] = 0.0f;
    zeroBB.bb_point1[1] = 0.0f;
    zeroBB.bb_point1[2] = 0.0f;
    zeroBB.bb_point2[0] = 0.0f;
    zeroBB.bb_point2[1] = 0.0f;
    zeroBB.bb_point2[2] = 0.0f;
    
    VoxelArray<VoxelEM> zeroArray(queue.get(), zeroBB, 1.0f);
    
    EXPECT_EQ(zeroArray.GetX(), 0);
    EXPECT_EQ(zeroArray.GetY(), 0);
    EXPECT_EQ(zeroArray.GetZ(), 0);
    EXPECT_EQ(zeroArray.GetSize(), 0);
}

TEST_F(VoxelEMArrayTest, DifferentResolutions) {
    // Test creating arrays with different resolutions
    BoundingBox smallBB;
    smallBB.bb_point1[0] = 0.0f;
    smallBB.bb_point1[1] = 0.0f;
    smallBB.bb_point1[2] = 0.0f;
    smallBB.bb_point2[0] = 10.0f;
    smallBB.bb_point2[1] = 10.0f;
    smallBB.bb_point2[2] = 10.0f;
    
    // Test with 0.5um resolution
    VoxelArray<VoxelEM> highResArray(queue.get(), smallBB, 0.5f);
    EXPECT_EQ(highResArray.GetX(), 20);
    EXPECT_EQ(highResArray.GetY(), 20);
    EXPECT_EQ(highResArray.GetZ(), 20);
    EXPECT_FLOAT_EQ(highResArray.GetResolution(), 0.5f);
    
    // Test with 2.0um resolution
    VoxelArray<VoxelEM> lowResArray(queue.get(), smallBB, 2.0f);
    EXPECT_EQ(lowResArray.GetX(), 5);
    EXPECT_EQ(lowResArray.GetY(), 5);
    EXPECT_EQ(lowResArray.GetZ(), 5);
    EXPECT_FLOAT_EQ(lowResArray.GetResolution(), 2.0f);
}

TEST_F(VoxelEMArrayTest, CopyToHostAndBack) {
    // Create test data on host
    std::vector<VoxelEM> hostData(voxelArray->GetSize());
    for (uint64_t i = 0; i < hostData.size(); i++) {
        hostData[i].State_ = static_cast<VoxelState>(i % 6);
        hostData[i].DistanceToEdge_vox_ = static_cast<uint8_t>(i % 256);
        hostData[i].ParentUID = i * 1000;
    }
    
    // Copy to device
    queue->memcpy(voxelArray->getDeviceData(), hostData.data(), 
                 voxelArray->GetSize() * sizeof(VoxelEM)).wait();
    
    // Copy back to host
    std::vector<VoxelEM> verifyData(voxelArray->GetSize());
    queue->memcpy(verifyData.data(), voxelArray->getDeviceData(), 
                 voxelArray->GetSize() * sizeof(VoxelEM)).wait();
    
    // Verify data matches
    for (uint64_t i = 0; i < hostData.size(); i++) {
        EXPECT_EQ(verifyData[i].State_, hostData[i].State_);
        EXPECT_EQ(verifyData[i].DistanceToEdge_vox_, hostData[i].DistanceToEdge_vox_);
        EXPECT_EQ(verifyData[i].ParentUID, hostData[i].ParentUID);
    }
}

TEST_F(VoxelEMArrayTest, StressTestLargeArray) {
    // Test with a larger array to stress test memory management
    BoundingBox largeBB;
    largeBB.bb_point1[0] = 0.0f;
    largeBB.bb_point1[1] = 0.0f;
    largeBB.bb_point1[2] = 0.0f;
    largeBB.bb_point2[0] = 200.0f;
    largeBB.bb_point2[1] = 200.0f;
    largeBB.bb_point2[2] = 200.0f;
    
    VoxelArray<VoxelEM> largeArray(queue.get(), largeBB, 1.0f);
    
    EXPECT_EQ(largeArray.GetX(), 200);
    EXPECT_EQ(largeArray.GetY(), 200);
    EXPECT_EQ(largeArray.GetZ(), 200);
    EXPECT_EQ(largeArray.GetSize(), 200 * 200 * 200);
    
    // Test basic operations on the large array
    EXPECT_TRUE(largeArray.IsInRangeX(100.0f));
    EXPECT_FALSE(largeArray.IsInRangeX(250.0f));
    EXPECT_EQ(largeArray.GetXIndexAtPosition(150.0f), 150);
    EXPECT_FLOAT_EQ(largeArray.GetXPositionAtIndex(150), 150.0f);
    
    // Test clearing the array
    largeArray.ClearArray();
    SUCCEED();
}