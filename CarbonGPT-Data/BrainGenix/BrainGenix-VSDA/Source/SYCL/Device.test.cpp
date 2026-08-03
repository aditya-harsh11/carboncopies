//=================================//
// This file is part of BrainGenix //
//=================================//

/*
    Description: Unit tests for SYCL Device functionality
    Additional Notes: None
    Date Created: 2025-08-28
    Author(s): Thomas Liao

    Copyright (C) 2025  Thomas Liao

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <gtest/gtest.h>
#include <sycl/sycl.hpp>

#include <SYCL/Device.h>

#include <SYCLTestFixture.h>

class SYCLDeviceTest : public SYCLTestFixture {
};

TEST_F(SYCLDeviceTest, EnumValues) {
    EXPECT_EQ(SYCL_DEVICE_TYPE_GPU, 0);
    EXPECT_EQ(SYCL_DEVICE_TYPE_CPU, 1);
    EXPECT_EQ(SYCL_DEVICE_TYPE_OTHER, 2);
}

TEST_F(SYCLDeviceTest, SYCLDeviceStructLayout) {
    // Test that the struct has expected layout and member types
    SYCLDevice device;
    
    // Test that Queue is a unique_ptr to sycl::queue
    EXPECT_TRUE((std::is_same_v<decltype(device.Queue), std::unique_ptr<sycl::queue>>));
    
    // Test that Device is a sycl::device
    EXPECT_TRUE((std::is_same_v<decltype(device.Device), sycl::device>));
    
    // Test that Name and Vendor are std::string
    EXPECT_TRUE((std::is_same_v<decltype(device.Name), std::string>));
    EXPECT_TRUE((std::is_same_v<decltype(device.Vendor), std::string>));
    
    // Test that VRAM_MB is size_t
    EXPECT_TRUE((std::is_same_v<decltype(device.VRAM_MB), size_t>));
    
    // Test that Type is SYCLDeviceType enum
    EXPECT_TRUE((std::is_same_v<decltype(device.Type), SYCLDeviceType>));
}

TEST_F(SYCLDeviceTest, InitSYCLDeviceWithValidDevice) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    // Get the device from the test fixture queue
    sycl::device dev = queue->get_device();
    
    SYCLDevice device_info = InitSYCLDevice(dev);
    
    // Verify the device info is populated correctly
    EXPECT_NE(device_info.Queue, nullptr);
    EXPECT_TRUE(device_info.Device == dev);
    EXPECT_FALSE(device_info.Name.empty());
    EXPECT_FALSE(device_info.Vendor.empty());
    EXPECT_GT(device_info.VRAM_MB, 0);
    
    // Verify the queue is explicitly in-order (required for test correctness)
    ASSERT_TRUE(device_info.Queue->has_property<sycl::property::queue::in_order>())
        << "Queue must be in-order for test correctness. This test will break with out-of-order queues.";
    
    // Verify the device type is correctly identified
    if (dev.is_gpu()) {
        EXPECT_EQ(device_info.Type, SYCL_DEVICE_TYPE_GPU);
    } else if (dev.is_cpu()) {
        EXPECT_EQ(device_info.Type, SYCL_DEVICE_TYPE_CPU);
    } else {
        EXPECT_EQ(device_info.Type, SYCL_DEVICE_TYPE_OTHER);
    }
}

TEST_F(SYCLDeviceTest, InitSYCLDeviceQueueFunctionality) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    sycl::device dev = queue->get_device();
    SYCLDevice device_info = InitSYCLDevice(dev);
    
    // CRITICAL: Verify queue is in-order (required for test correctness)
    ASSERT_TRUE(device_info.Queue->has_property<sycl::property::queue::in_order>())
        << "Queue must be in-order for test correctness. This test will break with out-of-order queues.";
    
    // Test that the queue is functional by submitting a simple kernel
    const int test_value = 42;
    int* device_data = sycl::malloc_device<int>(1, *device_info.Queue);
    int host_result = 0;
    
    device_info.Queue->submit([&](sycl::handler& h) {
        h.single_task([=]() {
            device_data[0] = test_value;
        });
    });
    
    device_info.Queue->memcpy(&host_result, device_data, sizeof(int)).wait();
    
    EXPECT_EQ(host_result, test_value);
    sycl::free(device_data, *device_info.Queue);
}

TEST_F(SYCLDeviceTest, MultipleDeviceInitialization) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    // Get all available devices
    auto platforms = sycl::platform::get_platforms();
    std::vector<sycl::device> all_devices;
    
    for (const auto& platform : platforms) {
        auto devices = platform.get_devices();
        all_devices.insert(all_devices.end(), devices.begin(), devices.end());
    }
    
    // Initialize SYCLDevice for each available device
    std::vector<SYCLDevice> device_infos;
    for (const auto& dev : all_devices) {
        device_infos.push_back(InitSYCLDevice(dev));
    }
    
    // Verify all devices were initialized correctly
    for (size_t i = 0; i < device_infos.size(); ++i) {
        const auto& device_info = device_infos[i];
        const auto& dev = all_devices[i];
        
        EXPECT_NE(device_info.Queue, nullptr);
        EXPECT_TRUE(device_info.Device == dev);
        EXPECT_FALSE(device_info.Name.empty());
        EXPECT_FALSE(device_info.Vendor.empty());
        
        // CRITICAL: Verify queue is in-order (required for test correctness)
        ASSERT_TRUE(device_info.Queue->has_property<sycl::property::queue::in_order>())
            << "Queue must be in-order for test correctness. This test will break with out-of-order queues.";
        
        // Type should match the SYCL device type
        if (dev.is_gpu()) {
            EXPECT_EQ(device_info.Type, SYCL_DEVICE_TYPE_GPU);
        } else if (dev.is_cpu()) {
            EXPECT_EQ(device_info.Type, SYCL_DEVICE_TYPE_CPU);
        } else {
            EXPECT_EQ(device_info.Type, SYCL_DEVICE_TYPE_OTHER);
        }
    }
}

TEST_F(SYCLDeviceTest, DeviceMoveSemantics) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    sycl::device dev = queue->get_device();
    SYCLDevice device_info = InitSYCLDevice(dev);
    
    // CRITICAL: Verify queue is in-order (required for test correctness)
    ASSERT_TRUE(device_info.Queue->has_property<sycl::property::queue::in_order>())
        << "Queue must be in-order for test correctness. This test will break with out-of-order queues.";
    
    // Test move constructor
    SYCLDevice moved_device = std::move(device_info);
    
    EXPECT_NE(moved_device.Queue, nullptr);
    EXPECT_TRUE(moved_device.Device == dev);
    EXPECT_FALSE(moved_device.Name.empty());
    
    // Original should be in moved-from state (Queue should be null)
    EXPECT_EQ(device_info.Queue, nullptr);
}

TEST_F(SYCLDeviceTest, DeviceCopySemantics) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    sycl::device dev = queue->get_device();
    SYCLDevice original = InitSYCLDevice(dev);
    
    // SYCLDevice should not be copyable due to unique_ptr member
    // This test verifies that copy construction is disabled
    EXPECT_FALSE(std::is_copy_constructible_v<SYCLDevice>);
    EXPECT_FALSE(std::is_copy_assignable_v<SYCLDevice>);
}

TEST_F(SYCLDeviceTest, DefaultHostDeviceInitialization) {
    // Test with default host device (if available)
    try {
        sycl::device host_device(sycl::host_selector{});
        SYCLDevice device_info = InitSYCLDevice(host_device);
        
        EXPECT_NE(device_info.Queue, nullptr);
        EXPECT_FALSE(device_info.Name.empty());
        
        // CRITICAL: Verify queue is in-order (required for test correctness)
        ASSERT_TRUE(device_info.Queue->has_property<sycl::property::queue::in_order>())
            << "Queue must be in-order for test correctness. This test will break with out-of-order queues.";
        
        EXPECT_EQ(device_info.Type, SYCL_DEVICE_TYPE_CPU); // Host device should be CPU type
    } catch (const sycl::exception&) {
        // Host device might not be available, skip the test
        GTEST_SKIP() << "Host device not available";
    }
}

TEST_F(SYCLDeviceTest, DeviceInfoConsistency) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    sycl::device dev = queue->get_device();
    SYCLDevice device_info = InitSYCLDevice(dev);
    
    // CRITICAL: Verify queue is in-order (required for test correctness)
    ASSERT_TRUE(device_info.Queue->has_property<sycl::property::queue::in_order>())
        << "Queue must be in-order for test correctness. This test will break with out-of-order queues.";
    
    // Verify that the info in SYCLDevice matches the original sycl::device
    EXPECT_EQ(device_info.Name, dev.get_info<sycl::info::device::name>());
    EXPECT_EQ(device_info.Vendor, dev.get_info<sycl::info::device::vendor>());
    EXPECT_EQ(device_info.VRAM_MB, dev.get_info<sycl::info::device::global_mem_size>() / (1024 * 1024));
}