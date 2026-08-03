#include <gtest/gtest.h>
#include <sycl/sycl.hpp>

#include <VSDA/CA/Structs/VoxelCA.h>

#include <SYCLTestFixture.h>

class VoxelCAStructTest : public SYCLTestFixture {   
};

TEST_F(VoxelCAStructTest, StructSizeAndAlignment) {
    EXPECT_EQ(sizeof(VoxelCA), 16);
    EXPECT_EQ(alignof(VoxelCA), 8);
}

TEST_F(VoxelCAStructTest, DefaultValues) {
    VoxelCA voxel{};
    EXPECT_FALSE(voxel.IsFilled_);
    EXPECT_EQ(voxel.CompartmentID_, 0);
    EXPECT_FALSE(voxel.IsBorder_);
}

TEST_F(VoxelCAStructTest, DeviceMemoryAllocation) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    VoxelCA* deviceVoxel = sycl::malloc_device<VoxelCA>(1, *queue);
    ASSERT_NE(deviceVoxel, nullptr);
    
    queue->submit([&](sycl::handler& h) {
        h.single_task([=]() {
            deviceVoxel[0].IsFilled_ = true;
            deviceVoxel[0].CompartmentID_ = 42;
            deviceVoxel[0].IsBorder_ = true;
        });
    });
    
    queue->wait_and_throw();
    sycl::free(deviceVoxel, *queue);
    SUCCEED();
}

TEST_F(VoxelCAStructTest, HostToDeviceCopy) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    VoxelCA hostVoxel{999, true, true};
    VoxelCA* deviceVoxel = sycl::malloc_device<VoxelCA>(1, *queue);
    
    queue->memcpy(deviceVoxel, &hostVoxel, sizeof(VoxelCA)).wait();
    
    VoxelCA result;
    queue->memcpy(&result, deviceVoxel, sizeof(VoxelCA)).wait();
    
    EXPECT_TRUE(result.IsFilled_);
    EXPECT_EQ(result.CompartmentID_, 999);
    EXPECT_TRUE(result.IsBorder_);
    
    sycl::free(deviceVoxel, *queue);
}

TEST_F(VoxelCAStructTest, ArrayOfVoxels) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    const int numVoxels = 100;
    VoxelCA* deviceVoxels = sycl::malloc_device<VoxelCA>(numVoxels, *queue);
    
    queue->submit([&](sycl::handler& h) {
        h.parallel_for(sycl::range<1>(numVoxels), [=](sycl::id<1> idx) {
            size_t i = idx[0]; // Extract the index value
            deviceVoxels[i].IsFilled_ = (i % 2 == 0);
            deviceVoxels[i].CompartmentID_ = i * 10;
            deviceVoxels[i].IsBorder_ = (i % 3 == 0);
        });
    });
    
    std::vector<VoxelCA> hostVoxels(numVoxels);
    queue->memcpy(hostVoxels.data(), deviceVoxels, numVoxels * sizeof(VoxelCA)).wait();
    
    for (int i = 0; i < numVoxels; ++i) {
        EXPECT_EQ(hostVoxels[i].IsFilled_, (i % 2 == 0));
        EXPECT_EQ(hostVoxels[i].CompartmentID_, i * 10);
        EXPECT_EQ(hostVoxels[i].IsBorder_, (i % 3 == 0));
    }
    
    sycl::free(deviceVoxels, *queue);
}