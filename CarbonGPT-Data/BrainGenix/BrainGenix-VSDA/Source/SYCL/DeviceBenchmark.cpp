//=================================//
// This file is part of BrainGenix //
//=================================//

#include <SYCL/DeviceBenchmark.h>
#include <chrono>
#include <cmath>
#include <vector>
#include <atomic>
#include <thread>
#include <algorithm>
#include <random>

// Benchmark parameters - optimized for modern GPUs
constexpr size_t MEM_BENCH_SIZE = 256 * 1024 * 1024; // 1024 MB
constexpr size_t COMPUTE_WORK_ITEMS = 4 * 1024 * 1024; // 64M work items
constexpr int COMPUTE_ITERATIONS = 16; // Inner loop iterations
constexpr int WARMUP_RUNS = 2;
constexpr int MEASURED_RUNS = 6;

/**
 * @brief Memory bandwidth benchmark using parallel kernel
 */
double MemoryBenchmarkKernel(sycl::queue& queue) {
    try {
        const size_t elements = MEM_BENCH_SIZE / sizeof(float);
        const size_t work_group_size = queue.get_device().get_info<sycl::info::device::max_work_group_size>();
        const size_t global_size = (elements + work_group_size - 1) / work_group_size * work_group_size;
        
        // Allocate memory using USM
        float* src = sycl::malloc_device<float>(elements, queue);
        float* dst = sycl::malloc_device<float>(elements, queue);
        
        if (!src || !dst) {
            if (src) sycl::free(src, queue);
            if (dst) sycl::free(dst, queue);
            return 0.0;
        }
        
        // Initialize source buffer
        queue.fill(src, 1.0f, elements).wait();
        queue.fill(dst, 0.0f, elements).wait();
        
        // Warmup
        for (int i = 0; i < WARMUP_RUNS; i++) {
            queue.submit([&](sycl::handler& h) {
                h.parallel_for(sycl::nd_range<1>(global_size, work_group_size), 
                [=](sycl::nd_item<1> item) {
                    const size_t idx = item.get_global_id(0);
                    if (idx < elements) {
                        dst[idx] = src[idx];
                    }
                });
            }).wait();
        }
        
        // Benchmark
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < MEASURED_RUNS; i++) {
            queue.submit([&](sycl::handler& h) {
                h.parallel_for(sycl::nd_range<1>(global_size, work_group_size), 
                [=](sycl::nd_item<1> item) {
                    const size_t idx = item.get_global_id(0);
                    if (idx < elements) {
                        dst[idx] = src[idx];
                    }
                });
            }).wait();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        
        // Calculate bandwidth (GB/s): (bytes copied) / (time in seconds) / 1e9
        double bandwidth = (MEASURED_RUNS * MEM_BENCH_SIZE) / diff.count() / 1e9;
        
        // Clean up
        sycl::free(src, queue);
        sycl::free(dst, queue);
        
        return bandwidth;
    } catch (...) {
        return 0.0;
    }
}

/**
 * @brief Compute performance benchmark using optimized kernel
 */
void ComputeBenchmarkKernel(sycl::queue& queue, double& gflopsFP32, double& gflopsFP16) {
    try {
        const size_t work_items = COMPUTE_WORK_ITEMS;
        const size_t work_group_size = queue.get_device().get_info<sycl::info::device::max_work_group_size>();
        const size_t global_size = (work_items + work_group_size - 1) / work_group_size * work_group_size;
        
        // Allocate memory using USM
        float* a = sycl::malloc_device<float>(work_items, queue);
        float* b = sycl::malloc_device<float>(work_items, queue);
        float* c = sycl::malloc_device<float>(work_items, queue);
        
        if (!a || !b || !c) {
            if (a) sycl::free(a, queue);
            if (b) sycl::free(b, queue);
            if (c) sycl::free(c, queue);
            gflopsFP32 = 0.0;
            gflopsFP16 = 0.0;
            return;
        }
        
        // Initialize buffers with random data to prevent optimization
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.1f, 1.0f);
        
        std::vector<float> host_a(work_items), host_b(work_items), host_c(work_items);
        for (size_t i = 0; i < work_items; i++) {
            host_a[i] = dis(gen);
            host_b[i] = dis(gen);
            host_c[i] = 0.0f;
        }
        
        queue.memcpy(a, host_a.data(), work_items * sizeof(float)).wait();
        queue.memcpy(b, host_b.data(), work_items * sizeof(float)).wait();
        queue.memcpy(c, host_c.data(), work_items * sizeof(float)).wait();
        
        // Warmup
        for (int i = 0; i < WARMUP_RUNS; i++) {
            queue.submit([&](sycl::handler& h) {
                h.parallel_for(sycl::nd_range<1>(global_size, work_group_size), 
                [=](sycl::nd_item<1> item) {
                    const size_t idx = item.get_global_id(0);
                    if (idx < work_items) {
                        float sum = c[idx];
                        for (int j = 0; j < COMPUTE_ITERATIONS; j++) {
                            sum = sycl::mad(a[idx], b[idx], sum);
                        }
                        c[idx] = sum;
                    }
                });
            }).wait();
        }
        
        // Benchmark FP32
        auto startFP32 = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < MEASURED_RUNS; i++) {
            queue.submit([&](sycl::handler& h) {
                h.parallel_for(sycl::nd_range<1>(global_size, work_group_size), 
                [=](sycl::nd_item<1> item) {
                    const size_t idx = item.get_global_id(0);
                    if (idx < work_items) {
                        float sum = c[idx];
                        for (int j = 0; j < COMPUTE_ITERATIONS; j++) {
                            sum = sycl::mad(a[idx], b[idx], sum);
                        }
                        c[idx] = sum;
                    }
                });
            }).wait();
        }
        
        auto endFP32 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diffFP32 = endFP32 - startFP32;
        
        // Calculate FP32 GFLOPs
        // 2 operations per FMA × iterations × work items × runs
        double totalOpsFP32 = 2.0 * COMPUTE_ITERATIONS * work_items * MEASURED_RUNS;
        gflopsFP32 = totalOpsFP32 / diffFP32.count() / 1e9;
        
        // Benchmark FP16 if supported
        gflopsFP16 = 0.0;
        
        try {
            // Check if device supports FP16
            auto device = queue.get_device();
            if (device.has(sycl::aspect::fp16)) {
                // Allocate half precision memory
                sycl::half* a_half = sycl::malloc_device<sycl::half>(work_items, queue);
                sycl::half* b_half = sycl::malloc_device<sycl::half>(work_items, queue);
                sycl::half* c_half = sycl::malloc_device<sycl::half>(work_items, queue);
                
                if (a_half && b_half && c_half) {
                    // Initialize half-precision buffers
                    std::vector<sycl::half> host_a_half(work_items), host_b_half(work_items), host_c_half(work_items);
                    for (size_t i = 0; i < work_items; i++) {
                        host_a_half[i] = static_cast<sycl::half>(host_a[i]);
                        host_b_half[i] = static_cast<sycl::half>(host_b[i]);
                        host_c_half[i] = sycl::half(0.0f);
                    }
                    
                    queue.memcpy(a_half, host_a_half.data(), work_items * sizeof(sycl::half)).wait();
                    queue.memcpy(b_half, host_b_half.data(), work_items * sizeof(sycl::half)).wait();
                    queue.memcpy(c_half, host_c_half.data(), work_items * sizeof(sycl::half)).wait();
                    
                    // Warmup
                    for (int i = 0; i < WARMUP_RUNS; i++) {
                        queue.submit([&](sycl::handler& h) {
                            h.parallel_for(sycl::nd_range<1>(global_size, work_group_size), 
                            [=](sycl::nd_item<1> item) {
                                const size_t idx = item.get_global_id(0);
                                if (idx < work_items) {
                                    sycl::half sum = c_half[idx];
                                    for (int j = 0; j < COMPUTE_ITERATIONS; j++) {
                                        sum = sycl::mad(a_half[idx], b_half[idx], sum);
                                    }
                                    c_half[idx] = sum;
                                }
                            });
                        }).wait();
                    }
                    
                    // Benchmark FP16
                    auto startFP16 = std::chrono::high_resolution_clock::now();
                    
                    for (int i = 0; i < MEASURED_RUNS; i++) {
                        queue.submit([&](sycl::handler& h) {
                            h.parallel_for(sycl::nd_range<1>(global_size, work_group_size), 
                            [=](sycl::nd_item<1> item) {
                                const size_t idx = item.get_global_id(0);
                                if (idx < work_items) {
                                    sycl::half sum = c_half[idx];
                                    for (int j = 0; j < COMPUTE_ITERATIONS; j++) {
                                        sum = sycl::mad(a_half[idx], b_half[idx], sum);
                                    }
                                    c_half[idx] = sum;
                                }
                            });
                        }).wait();
                    }
                    
                    auto endFP16 = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double> diffFP16 = endFP16 - startFP16;
                    
                    // Calculate FP16 GFLOPs
                    double totalOpsFP16 = 2.0 * COMPUTE_ITERATIONS * work_items * MEASURED_RUNS;
                    gflopsFP16 = totalOpsFP16 / diffFP16.count() / 1e9;
                    
                    sycl::free(a_half, queue);
                    sycl::free(b_half, queue);
                    sycl::free(c_half, queue);
                }
            }
        } catch (...) {
            // FP16 not supported or failed
        }
        
        // Clean up
        sycl::free(a, queue);
        sycl::free(b, queue);
        sycl::free(c, queue);
        
    } catch (...) {
        gflopsFP32 = 0.0;
        gflopsFP16 = 0.0;
    }
}

std::vector<SYCLDevicePerformanceInfo> BenchmarkDevices(const std::vector<SYCLDevice>& Devices) {
    std::vector<SYCLDevicePerformanceInfo> results(Devices.size());
    
    for (size_t i = 0; i < Devices.size(); ++i) {
        try {
            SYCLDevicePerformanceInfo info;
            
            // Memory bandwidth benchmark
            info.MemoryBandwidthGBs = MemoryBenchmarkKernel(*Devices[i].Queue);
            
            // Compute performance benchmark
            double gflopsFP32, gflopsFP16;
            ComputeBenchmarkKernel(*Devices[i].Queue, gflopsFP32, gflopsFP16);
            
            info.GFLOPsFP32 = gflopsFP32;
            info.GFLOPsFP16 = gflopsFP16;
            
            results[i] = info;
            
        } catch (const std::exception& e) {
            SYCLDevicePerformanceInfo info;
            info.MemoryBandwidthGBs = 0.0;
            info.GFLOPsFP32 = 0.0;
            info.GFLOPsFP16 = 0.0;
            results[i] = info;
        }
    }
    
    return results;
}