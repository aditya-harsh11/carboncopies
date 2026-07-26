//=================================//
// This file is part of BrainGenix //
//=================================//

/*
    Description: Unit tests for DeviceDetector functionality
    Additional Notes: None
    Date Created: 2025-08-29
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
#include <vector>
#include <memory>
#include <stdexcept>

#include <SYCL/DeviceDetector.h>
#include <SYCL/Device.h>
#include <SYCLTestFixture.h>

class DeviceDetectorTest : public SYCLTestFixture {
protected:
    // Helper function to check device types
    bool HasDeviceType(sycl::info::device_type type) {
        auto platforms = sycl::platform::get_platforms();
        for (const auto& platform : platforms) {
            auto devices = platform.get_devices();
            for (const auto& device : devices) {
                if (device.get_info<sycl::info::device::device_type>() == type) {
                    return true;
                }
            }
        }
        return false;
    }
    
    bool HasGPUDevices() {
        return HasDeviceType(sycl::info::device_type::gpu);
    }
    
    bool HasCPUDevices() {
        return HasDeviceType(sycl::info::device_type::cpu);
    }
};

TEST_F(DeviceDetectorTest, DetectDevices_FindsAvailableDevices) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    // Create a dummy logger (just to fulfill the parameter requirement)
    BG::Common::Logger::LoggingSystem dummyLogger(false);
    
    // Run detection
    auto devices = DetectDevices(&dummyLogger);
    
    // Verify at least one device was found
    ASSERT_FALSE(devices.empty()) << "No devices detected";
}

TEST_F(DeviceDetectorTest, DetectDevices_PrioritizesGPUs) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    if (!HasGPUDevices()) {
        GTEST_SKIP() << "No GPU devices available for testing";
    }
    
    // Create a dummy logger
    BG::Common::Logger::LoggingSystem dummyLogger(false);
    
    // Run detection
    auto devices = DetectDevices(&dummyLogger);
    
    // Verify all detected devices are GPUs
    for (const auto& device : devices) {
        EXPECT_EQ(device.Type, SYCL_DEVICE_TYPE_GPU) 
            << "Detected non-GPU device when GPUs are available";
    }
}

TEST_F(DeviceDetectorTest, DetectDevices_FallbacksToCPU) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    // Check if we have GPUs
    bool hasGPUs = HasGPUDevices();
    
    // If we have GPUs, we can't test CPU fallback
    if (hasGPUs) {
        GTEST_SKIP() << "GPU devices are available, skipping CPU fallback test";
    }
    
    // Check if we have CPUs
    bool hasCPUs = HasCPUDevices();
    if (!hasCPUs) {
        GTEST_SKIP() << "No CPU devices available for testing";
    }
    
    // Create a dummy logger
    BG::Common::Logger::LoggingSystem dummyLogger(false);
    
    // Run detection
    auto devices = DetectDevices(&dummyLogger);
    
    // Verify we got CPU device(s)
    ASSERT_FALSE(devices.empty()) << "No devices detected";
    
    // We should only have one CPU device as fallback
    EXPECT_EQ(devices.size(), 1) << "Expected exactly one CPU device as fallback";
    
    // Verify it's a CPU
    EXPECT_EQ(devices[0].Type, SYCL_DEVICE_TYPE_CPU) 
        << "Expected CPU device when no GPUs are available";
}

TEST_F(DeviceDetectorTest, DetectDevices_UsesFirstDevice) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    // Check if we have GPUs or CPUs
    bool hasGPUs = HasGPUDevices();
    bool hasCPUs = HasCPUDevices();
    
    // If we have GPUs or CPUs, we can't test the fallback to first device
    if (hasGPUs || hasCPUs) {
        GTEST_SKIP() << "GPU or CPU devices are available, skipping fallback test";
    }
    
    // Create a dummy logger
    BG::Common::Logger::LoggingSystem dummyLogger(false);
    
    // Run detection
    auto devices = DetectDevices(&dummyLogger);
    
    // Verify we got at least one device
    ASSERT_FALSE(devices.empty()) << "No devices detected";
}

TEST_F(DeviceDetectorTest, DetectDevices_ReturnsCorrectDeviceCount) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    // Get all available devices
    auto allDevices = sycl::device::get_devices();
    if (allDevices.empty()) {
        GTEST_SKIP() << "No SYCL devices available for testing";
    }
    
    // Count GPUs
    int gpuCount = 0;
    for (const auto& dev : allDevices) {
        if (dev.is_gpu()) gpuCount++;
    }
    
    // Create a dummy logger
    BG::Common::Logger::LoggingSystem dummyLogger(false);
    
    // Run detection
    auto devices = DetectDevices(&dummyLogger);
    
    // If GPUs were available, we should have all of them
    if (gpuCount > 0) {
        EXPECT_EQ(devices.size(), static_cast<size_t>(gpuCount))
            << "Expected to detect all GPU devices";
    } 
    // If no GPUs but CPUs, we should have 1 CPU
    else {
        int cpuCount = 0;
        for (const auto& dev : allDevices) {
            if (dev.is_cpu()) cpuCount++;
        }
        
        if (cpuCount > 0) {
            EXPECT_EQ(devices.size(), 1)
                << "Expected exactly one CPU device as fallback";
        }
    }
}

TEST_F(DeviceDetectorTest, DeviceConsistency) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    // Create a dummy logger
    BG::Common::Logger::LoggingSystem dummyLogger(false);
    
    // Run detection
    auto devices = DetectDevices(&dummyLogger);
    
    // Should have at least one device
    ASSERT_FALSE(devices.empty()) << "No devices detected";
    
    // Check the first device
    const auto& deviceInfo = devices[0];
    
    // Verify name matches
    EXPECT_FALSE(deviceInfo.Name.empty()) << "Device name should not be empty";
    
    // Verify vendor matches
    EXPECT_FALSE(deviceInfo.Vendor.empty()) << "Device vendor should not be empty";
    
    // Verify VRAM is positive
    EXPECT_GT(deviceInfo.VRAM_MB, 0) << "Device VRAM should be greater than 0";
    
    // Verify device type matches expectations
    if (deviceInfo.Device.is_gpu()) {
        EXPECT_EQ(deviceInfo.Type, SYCL_DEVICE_TYPE_GPU) 
            << "GPU device should have correct type";
    } else if (deviceInfo.Device.is_cpu()) {
        EXPECT_EQ(deviceInfo.Type, SYCL_DEVICE_TYPE_CPU) 
            << "CPU device should have correct type";
    } else {
        EXPECT_EQ(deviceInfo.Type, SYCL_DEVICE_TYPE_OTHER) 
            << "Other device should have correct type";
    }
}

TEST_F(DeviceDetectorTest, LogDevices_HandlesValidDevices) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    // Create a dummy logger
    BG::Common::Logger::LoggingSystem dummyLogger(false);
    
    // Create test devices
    auto devices = DetectDevices(&dummyLogger);
    ASSERT_FALSE(devices.empty()) << "No devices to test with";
    
    // Log the devices (we're not verifying the logs themselves since that's application behavior)
    LogDevices(&dummyLogger, &devices);
    
    // Just verify no crashes occurred
    SUCCEED() << "LogDevices completed without crashing";
}

TEST_F(DeviceDetectorTest, LogDevices_HandlesNullPointer) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    // Create a dummy logger
    BG::Common::Logger::LoggingSystem dummyLogger(false);
    
    // Log with null pointer (should not crash)
    LogDevices(&dummyLogger, nullptr);
    
    // Just verify no crashes occurred
    SUCCEED() << "LogDevices with null pointer completed without crashing";
}

TEST_F(DeviceDetectorTest, LogDevices_HandlesEmptyVector) {
    if (!queue) GTEST_SKIP() << "No SYCL queue available";
    
    // Create a dummy logger
    BG::Common::Logger::LoggingSystem dummyLogger(false);
    
    // Create empty vector
    std::vector<SYCLDevice> emptyDevices;
    
    // Log empty vector (should not crash)
    LogDevices(&dummyLogger, &emptyDevices);
    
    // Just verify no crashes occurred
    SUCCEED() << "LogDevices with empty vector completed without crashing";
}