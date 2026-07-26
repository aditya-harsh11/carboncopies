#include <gtest/gtest.h>
#include <sycl/sycl.hpp>

#include <VSDA/EM/Structs/VoxelEM.h>

#include <SYCLTestFixture.h>

class VoxelEMStructTest : public SYCLTestFixture {   
};


TEST_F(VoxelEMStructTest, StructSizeAndAlignment) {
    EXPECT_EQ(sizeof(VoxelEM), 16);
    EXPECT_EQ(alignof(VoxelEM), 8);
}

TEST_F(VoxelEMStructTest, DefaultValues) {
    VoxelEM voxel{};
    EXPECT_EQ(voxel.State_, VoxelState_EMPTY);
    EXPECT_EQ(voxel.DistanceToEdge_vox_, 0);
    EXPECT_EQ(voxel.ParentUID, 0);
}

TEST_F(VoxelEMStructTest, EnumValues) {
    EXPECT_EQ(VoxelState_EMPTY, 0);
    EXPECT_EQ(VoxelState_INTERIOR, 1);
    EXPECT_EQ(VoxelState_BORDER, 2);
    EXPECT_EQ(VoxelState_BLACK, 3);
    EXPECT_EQ(VoxelState_WHITE, 4);
    EXPECT_EQ(VoxelState_OUT_OF_BOUNDS, 5);
}

TEST_F(VoxelEMStructTest, DeviceMemoryAllocation) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    VoxelEM* deviceVoxel = sycl::malloc_device<VoxelEM>(1, *queue);
    ASSERT_NE(deviceVoxel, nullptr);
    
    queue->submit([&](sycl::handler& h) {
        h.single_task([=]() {
            deviceVoxel[0].State_ = VoxelState_BORDER;
            deviceVoxel[0].DistanceToEdge_vox_ = 42;
            deviceVoxel[0].ParentUID = 123456789;
        });
    });
    
    queue->wait_and_throw();
    sycl::free(deviceVoxel, *queue);
    SUCCEED();
}

TEST_F(VoxelEMStructTest, HostToDeviceCopy) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    VoxelEM hostVoxel{VoxelState_INTERIOR, 10, 999};
    VoxelEM* deviceVoxel = sycl::malloc_device<VoxelEM>(1, *queue);
    
    queue->memcpy(deviceVoxel, &hostVoxel, sizeof(VoxelEM)).wait();
    
    VoxelEM result;
    queue->memcpy(&result, deviceVoxel, sizeof(VoxelEM)).wait();
    
    EXPECT_EQ(result.State_, VoxelState_INTERIOR);
    EXPECT_EQ(result.DistanceToEdge_vox_, 10);
    EXPECT_EQ(result.ParentUID, 999);
    
    sycl::free(deviceVoxel, *queue);
}

TEST_F(VoxelEMStructTest, ArrayOfVoxels) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    const int numVoxels = 100;
    VoxelEM* deviceVoxels = sycl::malloc_device<VoxelEM>(numVoxels, *queue);
    
    queue->submit([&](sycl::handler& h) {
        h.parallel_for(sycl::range<1>(numVoxels), [=](sycl::id<1> idx) {
            size_t i = idx[0]; // Extract the index value
            deviceVoxels[i].State_ = static_cast<VoxelState>(i % 6);
            deviceVoxels[i].DistanceToEdge_vox_ = static_cast<uint8_t>(i);
            deviceVoxels[i].ParentUID = i * 1000;
        });
    });
    
    std::vector<VoxelEM> hostVoxels(numVoxels);
    queue->memcpy(hostVoxels.data(), deviceVoxels, numVoxels * sizeof(VoxelEM)).wait();
    
    for (int i = 0; i < numVoxels; ++i) {
        EXPECT_EQ(hostVoxels[i].State_, static_cast<VoxelState>(i % 6));
        EXPECT_EQ(hostVoxels[i].DistanceToEdge_vox_, static_cast<uint8_t>(i));
        EXPECT_EQ(hostVoxels[i].ParentUID, i * 1000);
    }
    
    sycl::free(deviceVoxels, *queue);
}