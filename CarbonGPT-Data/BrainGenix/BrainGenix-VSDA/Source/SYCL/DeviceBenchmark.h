//=================================//
// This file is part of BrainGenix //
//=================================//

#pragma once

#include <SYCL/Device.h>
#include <vector>

/**
 * @brief Performance metrics for a SYCL device
 */
struct SYCLDevicePerformanceInfo {
    double MemoryBandwidthGBs;  // Memory bandwidth in GB/s
    double GFLOPsFP32;          // Single-precision compute performance in GFLOPs
    double GFLOPsFP16;          // Half-precision compute performance in GFLOPs
};

/**
 * @brief Benchmarks all devices and returns performance metrics
 * @param Devices Vector of devices to benchmark
 * @return Vector of performance info corresponding to input devices
 */
std::vector<SYCLDevicePerformanceInfo> BenchmarkDevices(const std::vector<SYCLDevice>& Devices);
