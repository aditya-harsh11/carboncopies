#pragma once

#include <gtest/gtest.h>
#include <sycl/sycl.hpp>

class SYCLTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        try {
            // Use in-order queue for consistent behavior across devices
            queue = std::make_unique<sycl::queue>(
                sycl::gpu_selector_v,
                sycl::property_list{sycl::property::queue::in_order{}}
            );
        } catch (const sycl::exception& e) {
            try {
                queue = std::make_unique<sycl::queue>(
                    sycl::cpu_selector_v,
                    sycl::property_list{sycl::property::queue::in_order{}}
                );
                GTEST_LOG_(INFO) << "Falling back to CPU device";
            } catch (const sycl::exception& e) {
                GTEST_SKIP() << "No SYCL devices available: " << e.what();
            }
        }
    }
    
    std::unique_ptr<sycl::queue> queue;
};